/** \ingroup rpmbuild
 * \file build/files.c
 *  The post-build, pre-packaging file tree walk to assemble the package
 *  manifest.
 */

#include "system.h"

#define	MYALLPERMS	07777

#include <errno.h>
#include <regex.h>
#if WITH_CAP
#include <sys/capability.h>
#endif

#include <rpm/rpmpgp.h>
#include <rpm/argv.h>
#include <rpm/rpmfc.h>
#include <rpm/rpmfileutil.h>	/* rpmDoDigest() */
#include <rpm/rpmlog.h>
#include <rpm/rpmbase64.h>

#include "rpmio/rpmio_internal.h"	/* XXX rpmioSlurp */
#include "misc/fts.h"
#include "lib/rpmfi_internal.h"	/* XXX fi->apath */
#include "lib/rpmug.h"
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"

#include "debug.h"
#include <libgen.h>

#define SKIPSPACE(s) { while (*(s) && risspace(*(s))) (s)++; }
#define	SKIPWHITE(_x)	{while(*(_x) && (risspace(*_x) || *(_x) == ',')) (_x)++;}
#define	SKIPNONWHITE(_x){while(*(_x) &&!(risspace(*_x) || *(_x) == ',')) (_x)++;}

/**
 */
enum specfFlags_e {
    SPECD_DEFFILEMODE	= (1 << 0),
    SPECD_DEFDIRMODE	= (1 << 1),
    SPECD_DEFUID	= (1 << 2),
    SPECD_DEFGID	= (1 << 3),
    SPECD_DEFVERIFY	= (1 << 4),

    SPECD_FILEMODE	= (1 << 8),
    SPECD_DIRMODE	= (1 << 9),
    SPECD_UID		= (1 << 10),
    SPECD_GID		= (1 << 11),
    SPECD_VERIFY	= (1 << 12)
};

typedef rpmFlags specfFlags;

/* internal %files parsing state attributes */
enum parseAttrs_e {
    RPMFILE_EXCLUDE	= (1 << 16),	/*!< from %%exclude */
    RPMFILE_DOCDIR	= (1 << 17),	/*!< from %%docdir */
    RPMFILE_DIR		= (1 << 18),	/*!< from %%dir */
    RPMFILE_SPECIALDIR	= (1 << 19),	/*!< from special %%doc */
};

/* bits up to 15 (for now) reserved for exported rpmfileAttrs */
#define PARSEATTR_MASK 0x0000ffff

/**
 */
typedef struct FileListRec_s {
    struct stat fl_st;
#define	fl_dev	fl_st.st_dev
#define	fl_ino	fl_st.st_ino
#define	fl_mode	fl_st.st_mode
#define	fl_nlink fl_st.st_nlink
#define	fl_uid	fl_st.st_uid
#define	fl_gid	fl_st.st_gid
#define	fl_rdev	fl_st.st_rdev
#define	fl_size	fl_st.st_size
#define	fl_mtime fl_st.st_mtime

    char *diskPath;		/* get file from here       */
    char *cpioPath;		/* filename in cpio archive */
    rpmsid uname;
    rpmsid gname;
    unsigned	flags;
    specfFlags	specdFlags;	/* which attributes have been explicitly specified. */
    rpmVerifyFlags verifyFlags;
    char *langs;		/* XXX locales separated with | */
    char *caps;
} * FileListRec;

/**
 */
typedef struct AttrRec_s {
    rpmsid	ar_fmodestr;
    rpmsid	ar_dmodestr;
    rpmsid	ar_user;
    rpmsid	ar_group;
    mode_t	ar_fmode;
    mode_t	ar_dmode;
} * AttrRec;

/* list of files */
static StringBuf check_fileList = NULL;

typedef struct specialDir_s {
    char * dirname;
    ARGV_t files;
    struct AttrRec_s ar;
    struct AttrRec_s def_ar;
    rpmFlags sdtype;
} * specialDir;

typedef struct FileEntry_s {
    rpmfileAttrs attrFlags;
    specfFlags specdFlags;
    rpmVerifyFlags verifyFlags;
    struct AttrRec_s ar;

    ARGV_t langs;
    char *caps;

    /* these are only ever relevant for current entry */
    unsigned devtype;
    unsigned devmajor;
    int devminor;
    int isDir;
} * FileEntry;

typedef struct FileRecords_s {
    FileListRec recs;
    int alloced;
    int used;
} * FileRecords;

/**
 * Package file tree walk data.
 */
typedef struct FileList_s {
    /* global filelist state */
    char * buildRoot;
    size_t buildRootLen;
    int processingFailed;
    int haveCaps;
    int largeFiles;
    ARGV_t docDirs;
    rpmBuildPkgFlags pkgFlags;
    rpmstrPool pool;

    /* actual file records */
    struct FileRecords_s files;

    /* active defaults */
    struct FileEntry_s def;

    /* current file-entry state */
    struct FileEntry_s cur;
} * FileList;

static void nullAttrRec(AttrRec ar)
{
    memset(ar, 0, sizeof(*ar));
}

static void dupAttrRec(const AttrRec oar, AttrRec nar)
{
    if (oar == nar)
	return;
    *nar = *oar; /* struct assignment */
}

static void FileEntryFree(FileEntry entry)
{
    argvFree(entry->langs);
    memset(entry, 0, sizeof(*entry));
}

/**
 * strtokWithQuotes.
 * @param s
 * @param delim
 */
static char *strtokWithQuotes(char *s, const char *delim)
{
    static char *olds = NULL;
    char *token;

    if (s == NULL)
	s = olds;
    if (s == NULL)
	return NULL;

    /* Skip leading delimiters */
    s += strspn(s, delim);
    if (*s == '\0')
	return NULL;

    /* Find the end of the token.  */
    token = s;
    if (*token == '"') {
	token++;
	/* Find next " char */
	s = strchr(token, '"');
    } else {
	s = strpbrk(token, delim);
    }

    /* Terminate it */
    if (s == NULL) {
	/* This token finishes the string */
	olds = strchr(token, '\0');
    } else {
	/* Terminate the token and make olds point past it */
	*s = '\0';
	olds = s+1;
    }

    return token;
}

/**
 */
typedef const struct VFA {
    const char * attribute;
    int	flag;
} VFA_t;

/**
 */
static VFA_t const verifyAttrs[] = {
    { "md5",		RPMVERIFY_FILEDIGEST },
    { "filedigest",	RPMVERIFY_FILEDIGEST },
    { "size",		RPMVERIFY_FILESIZE },
    { "link",		RPMVERIFY_LINKTO },
    { "user",		RPMVERIFY_USER },
    { "owner",		RPMVERIFY_USER },
    { "group",		RPMVERIFY_GROUP },
    { "mtime",		RPMVERIFY_MTIME },
    { "mode",		RPMVERIFY_MODE },
    { "rdev",		RPMVERIFY_RDEV },
    { "caps",		RPMVERIFY_CAPS },
    { NULL, 0 }
};

static rpmFlags vfaMatch(VFA_t *attrs, const char *token, rpmFlags *flags)
{
    VFA_t *vfa;

    for (vfa = attrs; vfa->attribute != NULL; vfa++) {
	if (rstreq(token, vfa->attribute)) {
	    *flags |= vfa->flag;
	    break;
	}
    }
    return vfa->flag;
}

/**
 * Parse %verify and %defverify from file manifest.
 * @param buf		current spec file line
 * @param def		parse for %defverify or %verify?
 * @param entry		file entry data (current or default)
 * @return		RPMRC_OK on success
 */
static rpmRC parseForVerify(char * buf, int def, FileEntry entry)
{
    char *p, *pe, *q = NULL;
    const char *name = def ? "%defverify" : "%verify";
    int negated = 0;
    rpmVerifyFlags verifyFlags = RPMVERIFY_NONE;
    rpmRC rc = RPMRC_FAIL;

    if ((p = strstr(buf, name)) == NULL)
	return RPMRC_OK;

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';

    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	goto exit;
    }

    /* Bracket %*verify args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	goto exit;
    }

    /* Localize. Erase parsed string */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    for (p = q; *p != '\0'; p = pe) {
	SKIPWHITE(p);
	if (*p == '\0')
	    break;
	pe = p;
	SKIPNONWHITE(pe);
	if (*pe != '\0')
	    *pe++ = '\0';

	if (vfaMatch(verifyAttrs, p, &verifyFlags))
	    continue;

	if (rstreq(p, "not")) {
	    negated ^= 1;
	} else {
	    rpmlog(RPMLOG_ERR, _("Invalid %s token: %s\n"), name, p);
	    goto exit;
	}
    }

    entry->verifyFlags = negated ? ~(verifyFlags) : verifyFlags;
    entry->specdFlags |= SPECD_VERIFY;
    rc = RPMRC_OK;

exit:
    free(q);

    return rc;
}

static int isAttrDefault(rpmstrPool pool, rpmsid arsid)
{
    const char *ars = rpmstrPoolStr(pool, arsid);
    return (ars && ars[0] == '-' && ars[1] == '\0');
}

/**
 * Parse %dev from file manifest.
 * @param buf		current spec file line
 * @param cur		current file entry data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForDev(char * buf, FileEntry cur)
{
    const char * name;
    const char * errstr = NULL;
    char *p, *pe, *q = NULL;
    rpmRC rc = RPMRC_FAIL;	/* assume error */

    if ((p = strstr(buf, (name = "%dev"))) == NULL)
	return RPMRC_OK;

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';
    SKIPSPACE(pe);

    if (*pe != '(') {
	errstr = "'('";
	goto exit;
    }

    /* Bracket %dev args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};
    if (*pe != ')') {
	errstr = "')'";
	goto exit;
    }

    /* Localize. Erase parsed string */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    p = q; SKIPWHITE(p);
    pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
    if (*p == 'b')
	cur->devtype = 'b';
    else if (*p == 'c')
	cur->devtype = 'c';
    else {
	errstr = "devtype";
	goto exit;
    }

    p = pe; SKIPWHITE(p);
    pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe = '\0';
    for (pe = p; *pe && risdigit(*pe); pe++)
	{} ;
    if (*pe == '\0') {
	cur->devmajor = atoi(p);
	if (!(cur->devmajor >= 0 && cur->devmajor < 256)) {
	    errstr = "devmajor";
	    goto exit;
	}
	pe++;
    } else {
	errstr = "devmajor";
	goto exit;
    }

    p = pe; SKIPWHITE(p);
    pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe = '\0';
    for (pe = p; *pe && risdigit(*pe); pe++)
	{} ;
    if (*pe == '\0') {
	cur->devminor = atoi(p);
	if (!(cur->devminor >= 0 && cur->devminor < 256)) {
	    errstr = "devminor";
	    goto exit;
	}
    } else {
	errstr = "devminor";
	goto exit;
    }

    rc = RPMRC_OK;

exit:
    if (rc) {
	rpmlog(RPMLOG_ERR, _("Missing %s in %s %s\n"), errstr, name, p);
    }
    free(q);
    return rc;
}

/**
 * Parse %attr and %defattr from file manifest.
 * @param pool		string pool
 * @param buf		current spec file line
 * @param def		parse for %defattr or %attr?
 * @param entry		file entry data (current / default)
 * @return		0 on success
 */
static rpmRC parseForAttr(rpmstrPool pool, char * buf, int def, FileEntry entry)
{
    const char *name = def ? "%defattr" : "%attr";
    char *p, *pe, *q = NULL;
    int x;
    struct AttrRec_s arbuf;
    AttrRec ar = &arbuf;
    rpmRC rc = RPMRC_FAIL;

    if ((p = strstr(buf, name)) == NULL)
	return RPMRC_OK;

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';

    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	goto exit;
    }

    /* Bracket %*attr args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (def) {	/* %defattr */
	char *r = pe;
	r++;
	SKIPSPACE(r);
	if (*r != '\0') {
	    rpmlog(RPMLOG_ERR,
		     _("Non-white space follows %s(): %s\n"), name, r);
	    goto exit;
	}
    }

    /* Localize. Erase parsed string */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    nullAttrRec(ar);

    p = q; SKIPWHITE(p);
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_fmodestr = rpmstrPoolId(pool, p, 1);
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_user = rpmstrPoolId(pool, p, 1);
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_group = rpmstrPoolId(pool, p, 1);
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0' && def) {	/* %defattr */
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_dmodestr = rpmstrPoolId(pool, p, 1);
	p = pe; SKIPWHITE(p);
    }

    if (!(ar->ar_fmodestr && ar->ar_user && ar->ar_group) || *p != '\0') {
	rpmlog(RPMLOG_ERR, _("Bad syntax: %s(%s)\n"), name, q);
	goto exit;
    }

    /* Do a quick test on the mode argument and adjust for "-" */
    if (ar->ar_fmodestr && !isAttrDefault(pool, ar->ar_fmodestr)) {
	unsigned int ui;
	x = sscanf(rpmstrPoolStr(pool, ar->ar_fmodestr), "%o", &ui);
	if ((x == 0) || (ar->ar_fmode & ~MYALLPERMS)) {
	    rpmlog(RPMLOG_ERR, _("Bad mode spec: %s(%s)\n"), name, q);
	    goto exit;
	}
	ar->ar_fmode = ui;
    } else {
	ar->ar_fmodestr = 0;
    }

    if (ar->ar_dmodestr && !isAttrDefault(pool, ar->ar_dmodestr)) {
	unsigned int ui;
	x = sscanf(rpmstrPoolStr(pool, ar->ar_dmodestr), "%o", &ui);
	if ((x == 0) || (ar->ar_dmode & ~MYALLPERMS)) {
	    rpmlog(RPMLOG_ERR, _("Bad dirmode spec: %s(%s)\n"), name, q);
	    goto exit;
	}
	ar->ar_dmode = ui;
    } else {
	ar->ar_dmodestr = 0;
    }

    if (!(ar->ar_user && !isAttrDefault(pool, ar->ar_user))) {
	ar->ar_user = 0;
    }

    if (!(ar->ar_group && !isAttrDefault(pool, ar->ar_group))) {
	ar->ar_group = 0;
    }

    dupAttrRec(ar, &(entry->ar));

    /* XXX fix all this */
    entry->specdFlags |= SPECD_UID | SPECD_GID | SPECD_FILEMODE | SPECD_DIRMODE;
    rc = RPMRC_OK;

exit:
    free(q);
    
    return rc;
}

static VFA_t const configAttrs[] = {
    { "missingok",	RPMFILE_MISSINGOK },
    { "noreplace",	RPMFILE_NOREPLACE },
    { NULL, 0 }
};

/**
 * Parse %config from file manifest.
 * @param buf		current spec file line
 * @param cur		current file entry data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForConfig(char * buf, FileEntry cur)
{
    char *p, *pe, *q = NULL;
    const char *name;
    rpmRC rc = RPMRC_FAIL;

    if ((p = strstr(buf, (name = "%config"))) == NULL)
	return RPMRC_OK;

    cur->attrFlags |= RPMFILE_CONFIG;

    /* Erase "%config" token. */
    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';
    SKIPSPACE(pe);
    if (*pe != '(')
	return RPMRC_OK;

    /* Bracket %config args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	goto exit;
    }

    /* Localize. Erase parsed string. */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    for (p = q; *p != '\0'; p = pe) {
	SKIPWHITE(p);
	if (*p == '\0')
	    break;
	pe = p;
	SKIPNONWHITE(pe);
	if (*pe != '\0')
	    *pe++ = '\0';
	if (!vfaMatch(configAttrs, p, &(cur->attrFlags))) {
	    rpmlog(RPMLOG_ERR, _("Invalid %s token: %s\n"), name, p);
	    goto exit;
	}
    }
    rc = RPMRC_OK;
    
exit:
    free(q);

    return rc;
}

static rpmRC addLang(ARGV_t *av, const char *lang, size_t n, const char *ent)
{
    rpmRC rc = RPMRC_FAIL;
    char lbuf[n + 1];
    rstrlcpy(lbuf, lang, sizeof(lbuf));
    SKIPWHITE(ent);

    /* Sanity check locale length */
    if (n < 1 || (n == 1 && *lang != 'C') || n >= 32) {
	rpmlog(RPMLOG_ERR, _("Unusual locale length: \"%s\" in %%lang(%s)\n"),
		lbuf, ent);
	goto exit;
    }

    /* Check for duplicate locales */
    if (argvSearch(*av, lbuf, NULL)) {
	rpmlog(RPMLOG_WARNING, _("Duplicate locale %s in %%lang(%s)\n"),
		lbuf, ent);
    } else {
	argvAdd(av, lbuf);
	argvSort(*av, NULL);
    }
    rc = RPMRC_OK;

exit:
    return rc;
}

/**
 * Parse %lang from file manifest.
 * @param buf		current spec file line
 * @param cur		current file entry data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForLang(char * buf, FileEntry cur)
{
    char *p, *pe, *q = NULL;
    const char *name;
    rpmRC rc = RPMRC_FAIL;

  while ((p = strstr(buf, (name = "%lang"))) != NULL) {

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';
    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	goto exit;
    }

    /* Bracket %lang args */
    *pe = ' ';
    for (pe = p; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	goto exit;
    }

    /* Localize. Erase parsed string. */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    /* Parse multiple arguments from %lang */
    for (p = q; *p != '\0'; p = pe) {
	SKIPWHITE(p);
	pe = p;
	SKIPNONWHITE(pe);

	if (addLang(&(cur->langs), p, (pe-p), q))
	    goto exit;

	if (*pe == ',') pe++;	/* skip , if present */
    }
  }

    rc = RPMRC_OK;

exit:
    free(q);

    return rc;
}

/**
 * Parse %caps from file manifest.
 * @param buf		current spec file line
 * @param cur		current file entry data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForCaps(char * buf, FileEntry cur)
{
    char *p, *pe, *q = NULL;
    const char *name;
    rpmRC rc = RPMRC_FAIL;

    if ((p = strstr(buf, (name = "%caps"))) == NULL)
	return RPMRC_OK;

    /* Erase "%caps" token. */
    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';
    SKIPSPACE(pe);
    if (*pe != '(')
	return RPMRC_OK;

    /* Bracket %caps args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	goto exit;
    }

    /* Localize. Erase parsed string. */
    q = xmalloc((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

#if WITH_CAP
    {
	char *captxt = NULL;
	cap_t fcaps = cap_from_text(q);
	if (fcaps == NULL) {
	    rpmlog(RPMLOG_ERR, _("Invalid capability: %s\n"), q);
	    goto exit;
	}
	/* run our string through cap_to_text() to get libcap presentation */
	captxt = cap_to_text(fcaps, NULL);
	cur->caps = xstrdup(captxt);
	cap_free(captxt);
	cap_free(fcaps);
    }
#else
	rpmlog(RPMLOG_ERR, _("File capability support not built in\n"));
	goto exit;
#endif

    rc = RPMRC_OK;
    
exit:
    free(q);

    return rc;
}
/**
 */
static VFA_t const virtualAttrs[] = {
    { "%dir",		RPMFILE_DIR },
    { "%docdir",	RPMFILE_DOCDIR },
    { "%doc",		RPMFILE_DOC },
    { "%ghost",		RPMFILE_GHOST },
    { "%exclude",	RPMFILE_EXCLUDE },
    { "%readme",	RPMFILE_README },
    { "%license",	RPMFILE_LICENSE },
    { "%pubkey",	RPMFILE_PUBKEY },
    { NULL, 0 }
};

/**
 * Parse simple attributes (e.g. %dir) from file manifest.
 * @param buf		current spec file line
 * @param cur		current file entry data
 * @retval *fileNames	file names
 * @return		RPMRC_OK on success
 */
static rpmRC parseForSimple(char * buf, FileEntry cur, ARGV_t * fileNames)
{
    char *s, *t;
    rpmRC res = RPMRC_OK;
    int allow_relative = (RPMFILE_PUBKEY|RPMFILE_DOC|RPMFILE_LICENSE);

    t = buf;
    while ((s = strtokWithQuotes(t, " \t\n")) != NULL) {
	t = NULL;

    	/* Set flags for virtual file attributes */
	if (vfaMatch(virtualAttrs, s, &(cur->attrFlags)))
	    continue;

	/* normally paths need to be absolute */
	if (*s != '/') {
	   if (!(cur->attrFlags & allow_relative)) {
		rpmlog(RPMLOG_ERR, _("File must begin with \"/\": %s\n"), s);
		res = RPMRC_FAIL;
		continue;
	    }
	    /* non-absolute %doc and %license paths are special */
	    if (cur->attrFlags & (RPMFILE_DOC | RPMFILE_LICENSE))
		cur->attrFlags |= RPMFILE_SPECIALDIR;
	}
	argvAdd(fileNames, s);
    }

    return res;
}

/**
 */
static int compareFileListRecs(const void * ap, const void * bp)	
{
    const char *a = ((FileListRec)ap)->cpioPath;
    const char *b = ((FileListRec)bp)->cpioPath;
    return strcmp(a, b);
}

/**
 * Test if file is located in a %docdir.
 * @param docDirs	doc dirs
 * @param fileName	file path
 * @return		1 if doc file, 0 if not
 */
static int isDoc(ARGV_const_t docDirs, const char * fileName)	
{
    size_t k, l;

    k = strlen(fileName);
    for (ARGV_const_t dd = docDirs; *dd; dd++) {
	l = strlen(*dd);
	if (l < k && rstreqn(fileName, *dd, l) && fileName[l] == '/')
	    return 1;
    }
    return 0;
}

static int isHardLink(FileListRec flp, FileListRec tlp)
{
    return ((S_ISREG(flp->fl_mode) && S_ISREG(tlp->fl_mode)) &&
	    ((flp->fl_nlink > 1) && (flp->fl_nlink == tlp->fl_nlink)) &&
	    (flp->fl_ino == tlp->fl_ino) && 
	    (flp->fl_dev == tlp->fl_dev));
}

/**
 * Verify that file attributes scope over hardlinks correctly.
 * If partial hardlink sets are possible, then add tracking dependency.
 * @param files		package file records
 * @return		1 if partial hardlink sets can exist, 0 otherwise.
 */
static int checkHardLinks(FileRecords files)
{
    FileListRec ilp, jlp;
    int i, j;

    for (i = 0;  i < files->used; i++) {
	ilp = files->recs + i;
	if (!(S_ISREG(ilp->fl_mode) && ilp->fl_nlink > 1))
	    continue;

	for (j = i + 1; j < files->used; j++) {
	    jlp = files->recs + j;
	    if (isHardLink(ilp, jlp)) {
		return 1;
	    }
	}
    }
    return 0;
}

static int seenHardLink(FileRecords files, FileListRec flp, rpm_ino_t *fileid)
{
    for (FileListRec ilp = files->recs; ilp < flp; ilp++) {
	if (isHardLink(flp, ilp)) {
	    *fileid = ilp - files->recs;
	    return 1;
	}
    }
    return 0;
}

/**
 * Add file entries to header.
 * @todo Should directories have %doc/%config attributes? (#14531)
 * @todo Remove RPMTAG_OLDFILENAMES, add dirname/basename instead.
 * @param fl		package file tree walk data
 * @param pkg		(sub) package
 * @param isSrc		pass 1 for source packages 0 otherwise
 */
static void genCpioListAndHeader(FileList fl, Package pkg, int isSrc)
{
    FileListRec flp;
    char buf[BUFSIZ];
    int i, npaths = 0;
    uint32_t defaultalgo = PGPHASHALGO_MD5, digestalgo;
    rpm_loff_t totalFileSize = 0;
    Header h = pkg->header; /* just a shortcut */

    /*
     * See if non-md5 file digest algorithm is requested. If not
     * specified, quietly assume md5. Otherwise check if supported type.
     */
    digestalgo = rpmExpandNumeric(isSrc ? "%{_source_filedigest_algorithm}" :
					  "%{_binary_filedigest_algorithm}");
    if (digestalgo == 0) {
	digestalgo = defaultalgo;
    }

    if (rpmDigestLength(digestalgo) == 0) {
	rpmlog(RPMLOG_WARNING,
		_("Unknown file digest algorithm %u, falling back to MD5\n"), 
		digestalgo);
	digestalgo = defaultalgo;
    }
    
    /* Adjust paths if needed */
    for (i = 0, flp = fl->files.recs; i < fl->files.used; i++, flp++) {
	int changed = 0;
	char * cpiopath = flp->cpioPath;

	if (!isSrc && pkg->removePostfixes)
	for (ARGV_const_t postfix_p = pkg->removePostfixes; *postfix_p; postfix_p++) {
	    int len = strlen(*postfix_p);
	    int plen = strlen(cpiopath);
	    if (len <= plen && !strncmp(cpiopath+plen-len, *postfix_p, len)) {
		cpiopath[plen-len] = '\0';
		changed = 1;
		if (plen-len > 0 && cpiopath[plen-len-1] == '/') {
		    cpiopath[plen-len-1] = '\0';
		}
	    }
	}
	if (changed) {
	    char * tmp = xstrdup(cpiopath);
	    _free(flp->cpioPath);
	    flp->cpioPath = tmp;
	}
    }

    /* Sort the big list */
    qsort(fl->files.recs, fl->files.used,
	  sizeof(*(fl->files.recs)), compareFileListRecs);
    
    pkg->dpaths = xmalloc((fl->files.used + 1) * sizeof(*pkg->dpaths));

    /* Generate the header. */
    for (i = 0, flp = fl->files.recs; i < fl->files.used; i++, flp++) {
	rpm_ino_t fileid = flp - fl->files.recs;

 	/* Merge duplicate entries. */
	while (i < (fl->files.used - 1) &&
	    rstreq(flp->cpioPath, flp[1].cpioPath)) {

	    /* Two entries for the same file found, merge the entries. */
	    /* Note that an %exclude is a duplication of a file reference */

	    /* file flags */
	    flp[1].flags |= flp->flags;	

	    if (!(flp[1].flags & RPMFILE_EXCLUDE))
		rpmlog(RPMLOG_WARNING, _("File listed twice: %s\n"),
			flp->cpioPath);
   
	    /* file mode */
	    if (S_ISDIR(flp->fl_mode)) {
		if ((flp[1].specdFlags & (SPECD_DIRMODE | SPECD_DEFDIRMODE)) <
		    (flp->specdFlags & (SPECD_DIRMODE | SPECD_DEFDIRMODE)))
			flp[1].fl_mode = flp->fl_mode;
	    } else {
		if ((flp[1].specdFlags & (SPECD_FILEMODE | SPECD_DEFFILEMODE)) <
		    (flp->specdFlags & (SPECD_FILEMODE | SPECD_DEFFILEMODE)))
			flp[1].fl_mode = flp->fl_mode;
	    }

	    /* uid */
	    if ((flp[1].specdFlags & (SPECD_UID | SPECD_DEFUID)) <
		(flp->specdFlags & (SPECD_UID | SPECD_DEFUID)))
	    {
		flp[1].fl_uid = flp->fl_uid;
		flp[1].uname = flp->uname;
	    }

	    /* gid */
	    if ((flp[1].specdFlags & (SPECD_GID | SPECD_DEFGID)) <
		(flp->specdFlags & (SPECD_GID | SPECD_DEFGID)))
	    {
		flp[1].fl_gid = flp->fl_gid;
		flp[1].gname = flp->gname;
	    }

	    /* verify flags */
	    if ((flp[1].specdFlags & (SPECD_VERIFY | SPECD_DEFVERIFY)) <
		(flp->specdFlags & (SPECD_VERIFY | SPECD_DEFVERIFY)))
		    flp[1].verifyFlags = flp->verifyFlags;

	    /* XXX to-do: language */

	    flp++; i++;
	}

	/* Skip files that were marked with %exclude. */
	if (flp->flags & RPMFILE_EXCLUDE) continue;

	/* Collect on-disk paths for archive creation */
	pkg->dpaths[npaths++] = xstrdup(flp->diskPath);

	headerPutString(h, RPMTAG_OLDFILENAMES, flp->cpioPath);
	headerPutString(h, RPMTAG_FILEUSERNAME,
			rpmstrPoolStr(fl->pool, flp->uname));
	headerPutString(h, RPMTAG_FILEGROUPNAME,
			rpmstrPoolStr(fl->pool, flp->gname));

	/* Only use 64bit filesizes tag if required. */
	if (fl->largeFiles) {
	    rpm_loff_t rsize64 = (rpm_loff_t)flp->fl_size;
	    headerPutUint64(h, RPMTAG_LONGFILESIZES, &rsize64, 1);
            (void) rpmlibNeedsFeature(pkg, "LargeFiles", "4.12.0-1");
	} else {
	    rpm_off_t rsize32 = (rpm_off_t)flp->fl_size;
	    headerPutUint32(h, RPMTAG_FILESIZES, &rsize32, 1);
	}
	/* Excludes and dupes have been filtered out by now. */
	if (S_ISREG(flp->fl_mode)) {
	    if (flp->fl_nlink == 1 || !seenHardLink(&fl->files, flp, &fileid)) {
		totalFileSize += flp->fl_size;
	    }
	}
	
	/*
 	 * For items whose size varies between systems, always explicitly 
 	 * cast to the header type before inserting.
 	 * TODO: check and warn if header type overflows for each case.
 	 */
	{   rpm_time_t rtime = (rpm_time_t) flp->fl_mtime;
	    headerPutUint32(h, RPMTAG_FILEMTIMES, &rtime, 1);
	}

	{   rpm_mode_t rmode = (rpm_mode_t) flp->fl_mode;
	    headerPutUint16(h, RPMTAG_FILEMODES, &rmode, 1);
	}

	{   rpm_rdev_t rrdev = (rpm_rdev_t) flp->fl_rdev;
	    headerPutUint16(h, RPMTAG_FILERDEVS, &rrdev, 1);
	}
	
	/*
	 * To allow rpmbuild to work on filesystems with 64bit inodes numbers,
	 * remap them into 32bit integers based on filelist index, just
	 * preserving semantics for determining hardlinks.
	 * Start at 1 as inode zero as that could be considered as an error.
	 * Since we flatten all the inodes to appear within a single fs,
	 * we also need to flatten the devices.
	 */
	{   rpm_ino_t rino = fileid + 1;
	    rpm_dev_t rdev = flp->fl_dev ? 1 : 0;
	    headerPutUint32(h, RPMTAG_FILEINODES, &rino, 1);
	    headerPutUint32(h, RPMTAG_FILEDEVICES, &rdev, 1);
	}
	
	headerPutString(h, RPMTAG_FILELANGS, flp->langs);

	if (fl->haveCaps) {
	    headerPutString(h, RPMTAG_FILECAPS, flp->caps);
	}
	
	buf[0] = '\0';
	if (S_ISREG(flp->fl_mode))
	    (void) rpmDoDigest(digestalgo, flp->diskPath, 1, 
			       (unsigned char *)buf, NULL);
	headerPutString(h, RPMTAG_FILEDIGESTS, buf);
	
	buf[0] = '\0';
	if (S_ISLNK(flp->fl_mode)) {
	    ssize_t llen = readlink(flp->diskPath, buf, BUFSIZ-1);
	    if (llen == -1) {
		rpmlog(RPMLOG_ERR, _("reading symlink %s failed: %s\n"),
			flp->diskPath, strerror(errno));
		fl->processingFailed = 1;
	    } else {
		buf[llen] = '\0';
		if (buf[0] == '/' && !rstreq(fl->buildRoot, "/") &&
			rstreqn(buf, fl->buildRoot, fl->buildRootLen)) {
		    rpmlog(RPMLOG_ERR,
				_("Symlink points to BuildRoot: %s -> %s\n"),
				flp->cpioPath, buf);
		    fl->processingFailed = 1;
		}
	    }
	}
	headerPutString(h, RPMTAG_FILELINKTOS, buf);
	
	if (flp->flags & RPMFILE_GHOST) {
	    flp->verifyFlags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE |
				RPMVERIFY_LINKTO | RPMVERIFY_MTIME);
	}
	headerPutUint32(h, RPMTAG_FILEVERIFYFLAGS, &(flp->verifyFlags),1);
	
	if (!isSrc && isDoc(fl->docDirs, flp->cpioPath))
	    flp->flags |= RPMFILE_DOC;
	/* XXX Should directories have %doc/%config attributes? (#14531) */
	if (S_ISDIR(flp->fl_mode))
	    flp->flags &= ~(RPMFILE_CONFIG|RPMFILE_DOC|RPMFILE_LICENSE);
	/* Strip internal parse data */
	flp->flags &= PARSEATTR_MASK;

	headerPutUint32(h, RPMTAG_FILEFLAGS, &(flp->flags) ,1);
    }
    pkg->dpaths[npaths] = NULL;

    if (totalFileSize < UINT32_MAX) {
	rpm_off_t totalsize = totalFileSize;
	headerPutUint32(h, RPMTAG_SIZE, &totalsize, 1);
    } else {
	rpm_loff_t totalsize = totalFileSize;
	headerPutUint64(h, RPMTAG_LONGSIZE, &totalsize, 1);
    }

    if (digestalgo != defaultalgo) {
	headerPutUint32(h, RPMTAG_FILEDIGESTALGO, &digestalgo, 1);
	rpmlibNeedsFeature(pkg, "FileDigests", "4.6.0-1");
    }

    if (fl->haveCaps) {
	rpmlibNeedsFeature(pkg, "FileCaps", "4.6.1-1");
    }

    if (!isSrc)
	(void) rpmlibNeedsFeature(pkg, "PayloadFilesHavePrefix", "4.0-1");

    /* rpmfiNew() only groks compressed filelists */
    headerConvert(h, HEADERCONV_COMPRESSFILELIST);
    pkg->cpioList = rpmfilesNew(NULL, h, RPMTAG_BASENAMES,
			    (RPMFI_NOFILEUSER|RPMFI_NOFILEGROUP));

    if (pkg->cpioList == NULL || rpmfilesFC(pkg->cpioList) != npaths) {
	fl->processingFailed = 1;
    }

    if (fl->pkgFlags & RPMBUILD_PKG_NODIRTOKENS) {
	/* Uncompress filelist if legacy format requested */
	headerConvert(h, HEADERCONV_EXPANDFILELIST);
    } else {
	/* Binary packages with dirNames cannot be installed by legacy rpm. */
	(void) rpmlibNeedsFeature(pkg, "CompressedFileNames", "3.0.4-1");
    }
}

static FileRecords FileRecordsFree(FileRecords files)
{
    for (int i = 0; i < files->used; i++) {
	free(files->recs[i].diskPath);
	free(files->recs[i].cpioPath);
	free(files->recs[i].langs);
	free(files->recs[i].caps);
    }
    free(files->recs);
    return NULL;
}

static void FileListFree(FileList fl)
{
    FileEntryFree(&(fl->cur));
    FileEntryFree(&(fl->def));
    FileRecordsFree(&(fl->files));
    free(fl->buildRoot);
    argvFree(fl->docDirs);
    rpmstrPoolFree(fl->pool);
}

/* forward ref */
static rpmRC recurseDir(FileList fl, const char * diskPath);

/* Hack up a stat structure for a %dev or non-existing %ghost */
static struct stat * fakeStat(FileEntry cur, struct stat * statp)
{
    time_t now = time(NULL);

    if (cur->devtype) {
	statp->st_rdev = ((cur->devmajor & 0xff) << 8) | (cur->devminor & 0xff);
	statp->st_dev = statp->st_rdev;
	statp->st_mode = (cur->devtype == 'b' ? S_IFBLK : S_IFCHR);
    } else {
	/* non-existing %ghost file or directory */
	statp->st_mode = cur->isDir ? S_IFDIR : S_IFREG;
	/* can't recurse into non-existing directory */
	if (cur->isDir)
	    cur->isDir = 1;
    }
    statp->st_mode |= (cur->ar.ar_fmode & 0777);
    statp->st_atime = now;
    statp->st_mtime = now;
    statp->st_ctime = now;
    statp->st_nlink = 1;
    return statp;
}

/**
 * Add a file to the package manifest.
 * @param fl		package file tree walk data
 * @param diskPath	path to file
 * @param statp		file stat (possibly NULL)
 * @return		RPMRC_OK on success
 */
static rpmRC addFile(FileList fl, const char * diskPath,
		struct stat * statp)
{
    size_t plen = strlen(diskPath);
    char buf[plen + 1];
    const char *cpioPath;
    struct stat statbuf;
    mode_t fileMode;
    uid_t fileUid;
    gid_t fileGid;
    const char *fileUname;
    const char *fileGname;
    rpmRC rc = RPMRC_FAIL; /* assume failure */

    /* Strip trailing slash. The special case of '/' path is handled below. */
    if (plen > 0 && diskPath[plen - 1] == '/') {
	diskPath = strcpy(buf, diskPath);
	buf[plen - 1] = '\0';
    }
    cpioPath = diskPath;
	
    if (strncmp(diskPath, fl->buildRoot, fl->buildRootLen)) {
	rpmlog(RPMLOG_ERR, _("Path is outside buildroot: %s\n"), diskPath);
	goto exit;
    }
    
    /* Path may have prepended buildRoot, so locate the original filename. */
    /*
     * XXX There are 3 types of entry into addFile:
     *
     *	From			diskUrl			statp
     *	=====================================================
     *  processBinaryFile	path			NULL
     *  processBinaryFile	glob result path	NULL
     *  myftw			path			stat
     *
     */
    if (fl->buildRoot && !rstreq(fl->buildRoot, "/"))
    	cpioPath += fl->buildRootLen;

    /* XXX make sure '/' can be packaged also */
    if (*cpioPath == '\0')
	cpioPath = "/";

    /*
     * Unless recursing, we dont have stat() info at hand. Handle the
     * various cases, preserving historical behavior wrt %dev():
     * - for %dev() entries we fake it up whether the file exists or not
     * - otherwise try to grab the data by lstat()
     * - %ghost entries might not exist, fake it up
     */
    if (statp == NULL) {
	memset(&statbuf, 0, sizeof(statbuf));

	if (fl->cur.devtype) {
	    statp = fakeStat(&(fl->cur), &statbuf);
	} else if (lstat(diskPath, &statbuf) == 0) {
	    statp = &statbuf;
	} else if (fl->cur.attrFlags & RPMFILE_GHOST) {
	    statp = fakeStat(&(fl->cur), &statbuf);
	} else {
	    int lvl = RPMLOG_ERR;
	    const char *msg = fl->cur.isDir ? _("Directory not found: %s\n") :
					      _("File not found: %s\n");
	    if (fl->cur.attrFlags & RPMFILE_EXCLUDE) {
		lvl = RPMLOG_WARNING;
		rc = RPMRC_OK;
	    }
	    rpmlog(lvl, msg, diskPath);
	    goto exit;
	}
    }

    /* Error out when a non-directory is specified as one in spec */
    if (fl->cur.isDir && (statp == &statbuf) && !S_ISDIR(statp->st_mode)) {
	rpmlog(RPMLOG_ERR, _("Not a directory: %s\n"), diskPath);
	goto exit;
    }

    /* Don't recurse into explicit %dir, don't double-recurse from fts */
    if ((fl->cur.isDir != 1) && (statp == &statbuf) && S_ISDIR(statp->st_mode)) {
	return recurseDir(fl, diskPath);
    }

    fileMode = statp->st_mode;
    fileUid = statp->st_uid;
    fileGid = statp->st_gid;

    /* Explicit %attr() always wins */
    if (fl->cur.ar.ar_fmodestr) {
	if (S_ISLNK(fileMode)) {
	    rpmlog(RPMLOG_WARNING,
		   "Explicit %%attr() mode not applicaple to symlink: %s\n",
		   diskPath);
	} else {
	    fileMode &= S_IFMT;
	    fileMode |= fl->cur.ar.ar_fmode;
	}
    } else {
	/* ...but %defattr() for directories and files is different */
	if (S_ISDIR(fileMode)) {
	    if (fl->def.ar.ar_dmodestr) {
		fileMode &= S_IFMT;
		fileMode |= fl->def.ar.ar_dmode;
	    }
	} else if (!S_ISLNK(fileMode) && fl->def.ar.ar_fmodestr) {
	    fileMode &= S_IFMT;
	    fileMode |= fl->def.ar.ar_fmode;
	}
    }
    if (fl->cur.ar.ar_user) {
	fileUname = rpmstrPoolStr(fl->pool, fl->cur.ar.ar_user);
    } else if (fl->def.ar.ar_user) {
	fileUname = rpmstrPoolStr(fl->pool, fl->def.ar.ar_user);
    } else {
	fileUname = rpmugUname(fileUid);
    }
    if (fl->cur.ar.ar_group) {
	fileGname = rpmstrPoolStr(fl->pool, fl->cur.ar.ar_group);
    } else if (fl->def.ar.ar_group) {
	fileGname = rpmstrPoolStr(fl->pool, fl->def.ar.ar_group);
    } else {
	fileGname = rpmugGname(fileGid);
    }
	
    /* Default user/group to builder's user/group */
    if (fileUname == NULL)
	fileUname = rpmugUname(getuid());
    if (fileGname == NULL)
	fileGname = rpmugGname(getgid());
    
    /* S_XXX macro must be consistent with type in find call at check-files script */
    if (check_fileList && (S_ISREG(fileMode) || S_ISLNK(fileMode))) {
	appendStringBuf(check_fileList, diskPath);
	appendStringBuf(check_fileList, "\n");
    }

    /* Add to the file list */
    if (fl->files.used == fl->files.alloced) {
	fl->files.alloced += 128;
	fl->files.recs = xrealloc(fl->files.recs,
			fl->files.alloced * sizeof(*(fl->files.recs)));
    }
	    
    {	FileListRec flp = &fl->files.recs[fl->files.used];

	flp->fl_st = *statp;	/* structure assignment */
	flp->fl_mode = fileMode;
	flp->fl_uid = fileUid;
	flp->fl_gid = fileGid;

	flp->cpioPath = xstrdup(cpioPath);
	flp->diskPath = xstrdup(diskPath);
	flp->uname = rpmstrPoolId(fl->pool, fileUname, 1);
	flp->gname = rpmstrPoolId(fl->pool, fileGname, 1);

	if (fl->cur.langs) {
	    flp->langs = argvJoin(fl->cur.langs, "|");
	} else {
	    flp->langs = xstrdup("");
	}

	if (fl->cur.caps) {
	    flp->caps = xstrdup(fl->cur.caps);
	} else {
	    flp->caps = xstrdup("");
	}

	flp->flags = fl->cur.attrFlags;
	flp->specdFlags = fl->cur.specdFlags;
	flp->verifyFlags = fl->cur.verifyFlags;

	if (!(flp->flags & RPMFILE_EXCLUDE) && S_ISREG(flp->fl_mode)) {
	    if (flp->fl_size >= UINT32_MAX) {
		fl->largeFiles = 1;
	    }
	}
    }

    rc = RPMRC_OK;
    fl->files.used++;

exit:
    if (rc != RPMRC_OK)
	fl->processingFailed = 1;

    return rc;
}

/**
 * Add directory (and all of its files) to the package manifest.
 * @param fl		package file tree walk data
 * @param diskPath	path to file
 * @return		RPMRC_OK on success
 */
static rpmRC recurseDir(FileList fl, const char * diskPath)
{
    char * ftsSet[2];
    FTS * ftsp;
    FTSENT * fts;
    int myFtsOpts = (FTS_COMFOLLOW | FTS_NOCHDIR | FTS_PHYSICAL);
    rpmRC rc = RPMRC_FAIL;

    ftsSet[0] = (char *) diskPath;
    ftsSet[1] = NULL;
    ftsp = Fts_open(ftsSet, myFtsOpts, NULL);
    while ((fts = Fts_read(ftsp)) != NULL) {
	switch (fts->fts_info) {
	case FTS_D:		/* preorder directory */
	case FTS_F:		/* regular file */
	case FTS_SL:		/* symbolic link */
	case FTS_SLNONE:	/* symbolic link without target */
	case FTS_DEFAULT:	/* none of the above */
	    rc = addFile(fl, fts->fts_accpath, fts->fts_statp);
	    break;
	case FTS_DOT:		/* dot or dot-dot */
	case FTS_DP:		/* postorder directory */
	    rc = RPMRC_OK;
	    break;
	case FTS_NS:		/* stat(2) failed */
	case FTS_DNR:		/* unreadable directory */
	case FTS_ERR:		/* error; errno is set */
	case FTS_DC:		/* directory that causes cycles */
	case FTS_NSOK:		/* no stat(2) requested */
	case FTS_INIT:		/* initialized only */
	case FTS_W:		/* whiteout object */
	default:
	    rc = RPMRC_FAIL;
	    break;
	}
	if (rc)
	    break;
    }
    (void) Fts_close(ftsp);

    return rc;
}

/**
 * Add a pubkey/icon to a binary package.
 * @param pkg
 * @param fl		package file tree walk data
 * @param fileName	path to file, relative is builddir, absolute buildroot.
 * @param tag		tag to add
 * @return		RPMRC_OK on success
 */
static rpmRC processMetadataFile(Package pkg, FileList fl, 
				 const char * fileName, rpmTagVal tag)
{
    const char * buildDir = "%{_builddir}/%{?buildsubdir}/";
    char * fn = NULL;
    char * apkt = NULL;
    uint8_t * pkt = NULL;
    ssize_t pktlen = 0;
    int absolute = 0;
    rpmRC rc = RPMRC_FAIL;
    int xx;

    if (*fileName == '/') {
	fn = rpmGenPath(fl->buildRoot, NULL, fileName);
	absolute = 1;
    } else
	fn = rpmGenPath(buildDir, NULL, fileName);

    switch (tag) {
    default:
	rpmlog(RPMLOG_ERR, _("%s: can't load unknown tag (%d).\n"),
		fn, tag);
	goto exit;
	break;
    case RPMTAG_PUBKEYS: {
	if ((xx = pgpReadPkts(fn, &pkt, (size_t *)&pktlen)) <= 0) {
	    rpmlog(RPMLOG_ERR, _("%s: public key read failed.\n"), fn);
	    goto exit;
	}
	if (xx != PGPARMOR_PUBKEY) {
	    rpmlog(RPMLOG_ERR, _("%s: not an armored public key.\n"), fn);
	    goto exit;
	}
	apkt = pgpArmorWrap(PGPARMOR_PUBKEY, pkt, pktlen);
	break;
    }
    }

    if (!apkt) {
	rpmlog(RPMLOG_ERR, _("%s: failed to encode\n"), fn);
	goto exit;
    }

    headerPutString(pkg->header, tag, apkt);
    rc = RPMRC_OK;

    if (absolute)
	rc = addFile(fl, fn, NULL);

exit:
    free(apkt);
    free(pkt);
    free(fn);
    if (rc) {
	fl->processingFailed = 1;
	rc = RPMRC_FAIL;
    }
    return rc;
}

/**
 * Add a file to a binary package.
 * @param pkg
 * @param fl		package file tree walk data
 * @param fileName	file to add
 * @return		RPMRC_OK on success
 */
static rpmRC processBinaryFile(Package pkg, FileList fl, const char * fileName)
{
    int quote = 1;	/* XXX permit quoted glob characters. */
    int doGlob;
    char *diskPath = NULL;
    rpmRC rc = RPMRC_OK;
    size_t fnlen = strlen(fileName);
    int trailing_slash = (fnlen > 0 && fileName[fnlen-1] == '/');

    /* XXX differentiate other directories from explicit %dir */
    if (trailing_slash && !fl->cur.isDir)
	fl->cur.isDir = -1;
    
    doGlob = rpmIsGlob(fileName, quote);

    /* Check that file starts with leading "/" */
    if (*fileName != '/') {
	rpmlog(RPMLOG_ERR, _("File needs leading \"/\": %s\n"), fileName);
    	rc = RPMRC_FAIL;
    	goto exit;
    }
    
    /* Copy file name or glob pattern removing multiple "/" chars. */
    /*
     * Note: rpmGetPath should guarantee a "canonical" path. That means
     * that the following pathologies should be weeded out:
     *		//bin//sh
     *		//usr//bin/
     *		/.././../usr/../bin//./sh
     */
    diskPath = rpmGenPath(fl->buildRoot, NULL, fileName);
    /* Arrange trailing slash on directories */
    if (fl->cur.isDir)
	diskPath = rstrcat(&diskPath, "/");

    if (doGlob) {
	ARGV_t argv = NULL;
	int argc = 0;
	int i;

	if (fl->cur.devtype) {
	    rpmlog(RPMLOG_ERR, _("%%dev glob not permitted: %s\n"), diskPath);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	if (rpmGlob(diskPath, &argc, &argv) == 0 && argc >= 1) {
	    for (i = 0; i < argc; i++) {
		rc = addFile(fl, argv[i], NULL);
	    }
	    argvFree(argv);
	} else {
	    const char *msg = (fl->cur.isDir) ?
				_("Directory not found by glob: %s. "
				"Trying without globbing.\n") :
				_("File not found by glob: %s. "
				"Trying without globbing.\n");
	    rpmlog(RPMLOG_DEBUG, msg, diskPath);
	    rc = addFile(fl, diskPath, NULL);
	}
    } else {
	rc = addFile(fl, diskPath, NULL);
    }

exit:
    free(diskPath);
    if (rc) {
	fl->processingFailed = 1;
	rc = RPMRC_FAIL;
    }
    return rc;
}

static rpmRC readFilesManifest(rpmSpec spec, Package pkg, const char *path)
{
    char *fn, buf[BUFSIZ];
    FILE *fd = NULL;
    rpmRC rc = RPMRC_FAIL;
    unsigned int nlines = 0;

    if (*path == '/') {
	fn = rpmGetPath(path, NULL);
    } else {
	fn = rpmGetPath("%{_builddir}/",
	    (spec->buildSubdir ? spec->buildSubdir : "") , "/", path, NULL);
    }
    fd = fopen(fn, "r");

    if (fd == NULL) {
	rpmlog(RPMLOG_ERR, _("Could not open %%files file %s: %m\n"), fn);
	goto exit;
    }

    /* XXX unmask %license while parsing files manifest*/
    addMacro(spec->macros, "license", NULL, "%%license", RMIL_SPEC);

    while (fgets(buf, sizeof(buf), fd)) {
	if (handleComments(buf))
	    continue;
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmlog(RPMLOG_ERR, _("line: %s\n"), buf);
	    goto exit;
	}
	argvAdd(&(pkg->fileList), buf);
	nlines++;
    }

    if (nlines == 0) {
	int terminate =
		rpmExpandNumeric("%{?_empty_manifest_terminate_build}");
	rpmlog(terminate ? RPMLOG_ERR : RPMLOG_WARNING,
	       _("Empty %%files file %s\n"), fn);
	if (terminate)
		goto exit;
    }

    if (ferror(fd))
	rpmlog(RPMLOG_ERR, _("Error reading %%files file %s: %m\n"), fn);
    else
	rc = RPMRC_OK;

exit:
    delMacro(NULL, "license");
    if (fd) fclose(fd);
    free(fn);
    return rc;
}

static char * getSpecialDocDir(Header h, rpmFlags sdtype)
{
    const char *errstr = NULL;
    const char *dirtype = (sdtype == RPMFILE_DOC) ? "docdir" : "licensedir";
    const char *fmt_default = "%{NAME}-%{VERSION}";
    char *fmt_macro = rpmExpand("%{?_docdir_fmt}", NULL);
    char *fmt = NULL; 
    char *res = NULL;

    if (fmt_macro && strlen(fmt_macro) > 0) {
	fmt = headerFormat(h, fmt_macro, &errstr);
	if (errstr) {
	    rpmlog(RPMLOG_WARNING, _("illegal _docdir_fmt %s: %s\n"),
		   fmt_macro, errstr);
	}
    }

    if (fmt == NULL)
	fmt = headerFormat(h, fmt_default, &errstr);

    res = rpmGetPath("%{_", dirtype, "}/", fmt, NULL);

    free(fmt);
    free(fmt_macro);
    return res;
}

static specialDir specialDirNew(Header h, rpmFlags sdtype,
				AttrRec ar, AttrRec def_ar)
{
    specialDir sd = xcalloc(1, sizeof(*sd));
    dupAttrRec(ar, &(sd->ar));
    dupAttrRec(def_ar, &(sd->def_ar));
    sd->dirname = getSpecialDocDir(h, sdtype);
    sd->sdtype = sdtype;
    return sd;
}

static specialDir specialDirFree(specialDir sd)
{
    if (sd) {
	argvFree(sd->files);
	free(sd->dirname);
	free(sd);
    }
    return NULL;
}

static void processSpecialDir(rpmSpec spec, Package pkg, FileList fl,
				specialDir sd, int install, int test)
{
    const char *sdenv = (sd->sdtype == RPMFILE_DOC) ? "DOCDIR" : "LICENSEDIR";
    const char *sdname = (sd->sdtype == RPMFILE_DOC) ? "%doc" : "%license";
    char *mkdocdir = rpmExpand("%{__mkdir_p} $", sdenv, NULL);
    StringBuf docScript = newStringBuf();
    char *basepath, **files;

    appendStringBuf(docScript, sdenv);
    appendStringBuf(docScript, "=$RPM_BUILD_ROOT");
    appendLineStringBuf(docScript, sd->dirname);
    appendStringBuf(docScript, "export ");
    appendLineStringBuf(docScript, sdenv);
    appendLineStringBuf(docScript, mkdocdir);

    for (ARGV_const_t fn = sd->files; fn && *fn; fn++) {
	/* Quotes would break globs, escape spaces instead */
	char *efn = rpmEscapeSpaces(*fn);
	appendStringBuf(docScript, "cp -pr ");
	appendStringBuf(docScript, efn);
	appendStringBuf(docScript, " $");
	appendLineStringBuf(docScript, sdenv);
	free(efn);
    }

    if (install) {
	rpmRC rc = doScript(spec, RPMBUILD_STRINGBUF, sdname,
			    getStringBuf(docScript), test);

	if (rc && rpmExpandNumeric("%{?_missing_doc_files_terminate_build}"))
	    fl->processingFailed = 1;
    }

    /* Reset for %doc */
    FileEntryFree(&fl->cur);

    fl->cur.attrFlags |= sd->sdtype;
    fl->cur.verifyFlags = fl->def.verifyFlags;
    dupAttrRec(&(sd->ar), &(fl->cur.ar));
    dupAttrRec(&(sd->def_ar), &(fl->def.ar));

    basepath = rpmGenPath(spec->rootDir, "%{_builddir}", spec->buildSubdir);
    files = sd->files;
    while (*files != NULL) {
	char *origfile = rpmGenPath(basepath, *files, NULL);
	char *eorigfile = rpmEscapeSpaces(origfile);
	ARGV_t globFiles;
	int globFilesCount, i;
	char *newfile;

	if (rpmGlob(eorigfile, &globFilesCount, &globFiles) == 0) {
	    for (i = 0; i < globFilesCount; i++) {
		rasprintf(&newfile, "%s/%s", sd->dirname, basename(globFiles[i]));
		processBinaryFile(pkg, fl, newfile);
		free(newfile);
	    }
	    argvFree(globFiles);
	} else {
	    rpmlog(RPMLOG_ERR, _("File not found by glob: %s\n"), eorigfile);
	    fl->processingFailed = 1;
	}
	free(eorigfile);
	free(origfile);
	files++;
    }
    free(basepath);

    fl->cur.isDir = 1;
    (void) processBinaryFile(pkg, fl, sd->dirname);

    freeStringBuf(docScript);
    free(mkdocdir);
}
				

static rpmRC processPackageFiles(rpmSpec spec, rpmBuildPkgFlags pkgFlags,
				 Package pkg, int installSpecialDoc, int test)
{
    struct AttrRec_s root_ar = { 0, 0, 0, 0, 0, 0 };
    struct FileList_s fl;
    ARGV_t fileNames = NULL;
    specialDir specialDoc = NULL;
    specialDir specialLic = NULL;

    pkg->cpioList = NULL;

    for (ARGV_const_t fp = pkg->fileFile; fp && *fp != NULL; fp++) {
	if (readFilesManifest(spec, pkg, *fp))
	    return RPMRC_FAIL;
    }
    /* Init the file list structure */
    memset(&fl, 0, sizeof(fl));

    fl.pool = rpmstrPoolLink(spec->pool);
    /* XXX spec->buildRoot == NULL, then xstrdup("") is returned */
    fl.buildRoot = rpmGenPath(spec->rootDir, spec->buildRoot, NULL);
    fl.buildRootLen = strlen(fl.buildRoot);

    root_ar.ar_user = rpmstrPoolId(fl.pool, "root", 1);
    root_ar.ar_group = rpmstrPoolId(fl.pool, "root", 1);
    dupAttrRec(&root_ar, &fl.def.ar);	/* XXX assume %defattr(-,root,root) */

    fl.def.verifyFlags = RPMVERIFY_ALL;

    fl.pkgFlags = pkgFlags;

    {	char *docs = rpmGetPath("%{?__docdir_path}", NULL);
	argvSplit(&fl.docDirs, docs, ":");
	free(docs);
    }
    
    for (ARGV_const_t fp = pkg->fileList; *fp != NULL; fp++) {
	char buf[strlen(*fp) + 1];
	const char *s = *fp;
	SKIPSPACE(s);
	if (*s == '\0')
	    continue;
	fileNames = argvFree(fileNames);
	rstrlcpy(buf, s, sizeof(buf));
	
	/* Reset for a new line in %files */
	FileEntryFree(&fl.cur);

	/* turn explicit flags into %def'd ones (gosh this is hacky...) */
	fl.cur.specdFlags = ((unsigned)fl.def.specdFlags) >> 8;
	fl.cur.verifyFlags = fl.def.verifyFlags;

	if (parseForVerify(buf, 0, &fl.cur) ||
	    parseForVerify(buf, 1, &fl.def) ||
	    parseForAttr(fl.pool, buf, 0, &fl.cur) ||
	    parseForAttr(fl.pool, buf, 1, &fl.def) ||
	    parseForDev(buf, &fl.cur) ||
	    parseForConfig(buf, &fl.cur) ||
	    parseForLang(buf, &fl.cur) ||
	    parseForCaps(buf, &fl.cur) ||
	    parseForSimple(buf, &fl.cur, &fileNames))
	{
	    fl.processingFailed = 1;
	    continue;
	}

	for (ARGV_const_t fn = fileNames; fn && *fn; fn++) {
	    if (fl.cur.attrFlags & RPMFILE_SPECIALDIR) {
		rpmFlags oattrs = (fl.cur.attrFlags & ~RPMFILE_SPECIALDIR);
		specialDir *sdp = NULL;
		if (oattrs == RPMFILE_DOC) {
		    sdp = &specialDoc;
		} else if (oattrs == RPMFILE_LICENSE) {
		    sdp = &specialLic;
		}

		if (sdp == NULL || **fn == '/') {
		    rpmlog(RPMLOG_ERR,
			   _("Can't mix special %s with other forms: %s\n"),
			   (oattrs & RPMFILE_DOC) ? "%doc" : "%license", *fn);
		    fl.processingFailed = 1;
		    continue;
		}

		/* save attributes on first special doc/license for later use */
		if (*sdp == NULL) {
		    *sdp = specialDirNew(pkg->header, oattrs,
					 &fl.cur.ar, &fl.def.ar);
		}
		argvAdd(&(*sdp)->files, *fn);
		continue;
	    }

	    /* this is now an artificial limitation */
	    if (fn != fileNames) {
		rpmlog(RPMLOG_ERR, _("More than one file on a line: %s\n"),*fn);
		fl.processingFailed = 1;
		continue;
	    }

	    if (fl.cur.attrFlags & RPMFILE_DOCDIR) {
		argvAdd(&(fl.docDirs), *fn);
	    } else if (fl.cur.attrFlags & RPMFILE_PUBKEY) {
		(void) processMetadataFile(pkg, &fl, *fn, RPMTAG_PUBKEYS);
	    } else {
		if (fl.cur.attrFlags & RPMFILE_DIR)
		    fl.cur.isDir = 1;
		(void) processBinaryFile(pkg, &fl, *fn);
	    }
	}

	if (fl.cur.caps)
	    fl.haveCaps = 1;
    }

    /* Now process special docs and licenses if present */
    if (specialDoc)
	processSpecialDir(spec, pkg, &fl, specialDoc, installSpecialDoc, test);
    if (specialLic)
	processSpecialDir(spec, pkg, &fl, specialLic, installSpecialDoc, test);
    
    if (fl.processingFailed)
	goto exit;

    /* Verify that file attributes scope over hardlinks correctly. */
    if (checkHardLinks(&fl.files))
	(void) rpmlibNeedsFeature(pkg, "PartialHardlinkSets", "4.0.4-1");

    genCpioListAndHeader(&fl, pkg, 0);

exit:
    argvFree(fileNames);
    FileListFree(&fl);
    specialDirFree(specialDoc);
    specialDirFree(specialLic);
    return fl.processingFailed ? RPMRC_FAIL : RPMRC_OK;
}

static void genSourceRpmName(rpmSpec spec)
{
    if (spec->sourceRpmName == NULL) {
	char *nvr = headerGetAsString(spec->packages->header, RPMTAG_NVR);
	rasprintf(&spec->sourceRpmName, "%s.%ssrc.rpm", nvr,
	    	  spec->noSource ? "no" : "");
	free(nvr);
    }
}

rpmRC processSourceFiles(rpmSpec spec, rpmBuildPkgFlags pkgFlags)
{
    struct Source *srcPtr;
    struct FileList_s fl;
    ARGV_t files = NULL;
    Package pkg;
    Package sourcePkg = spec->sourcePackage;
    static char *_srcdefattr;
    static int oneshot;

    if (!oneshot) {
	_srcdefattr = rpmExpand("%{?_srcdefattr}", NULL);
	if (_srcdefattr && !*_srcdefattr)
	    _srcdefattr = _free(_srcdefattr);
	oneshot = 1;
    }

    genSourceRpmName(spec);
    /* Construct the file list and source entries */
    argvAdd(&files, spec->specFile);
    for (srcPtr = spec->sources; srcPtr != NULL; srcPtr = srcPtr->next) {
	char * sfn = rpmGetPath( ((srcPtr->flags & RPMBUILD_ISNO) ? "!" : ""),
		"%{_sourcedir}/", srcPtr->source, NULL);
	argvAdd(&files, sfn);
	free(sfn);
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (srcPtr = pkg->icon; srcPtr != NULL; srcPtr = srcPtr->next) {
	    char * sfn;
	    sfn = rpmGetPath( ((srcPtr->flags & RPMBUILD_ISNO) ? "!" : ""),
		"%{_sourcedir}/", srcPtr->source, NULL);
	    argvAdd(&files, sfn);
	    free(sfn);
	}
    }

    sourcePkg->cpioList = NULL;

    /* Init the file list structure */
    memset(&fl, 0, sizeof(fl));
    fl.pool = rpmstrPoolLink(spec->pool);
    if (_srcdefattr) {
	char *a = rstrscat(NULL, "%defattr ", _srcdefattr, NULL);
	parseForAttr(fl.pool, a, 1, &fl.def);
	free(a);
    }
    fl.files.alloced = spec->numSources + 1;
    fl.files.recs = xcalloc(fl.files.alloced, sizeof(*fl.files.recs));
    fl.pkgFlags = pkgFlags;

    for (ARGV_const_t fp = files; *fp != NULL; fp++) {
	const char *diskPath = *fp;
	char *tmp;
	FileListRec flp;

	SKIPSPACE(diskPath);
	if (! *diskPath)
	    continue;

	flp = &fl.files.recs[fl.files.used];

	/* The first source file is the spec file */
	flp->flags = (fl.files.used == 0) ? RPMFILE_SPECFILE : 0;
	/* files with leading ! are no source files */
	if (*diskPath == '!') {
	    flp->flags |= RPMFILE_GHOST;
	    diskPath++;
	}

	tmp = xstrdup(diskPath); /* basename() might modify */
	flp->diskPath = xstrdup(diskPath);
	flp->cpioPath = xstrdup(basename(tmp));
	flp->verifyFlags = RPMVERIFY_ALL;
	free(tmp);

	if (stat(diskPath, &flp->fl_st)) {
	    rpmlog(RPMLOG_ERR, _("Bad file: %s: %s\n"),
		diskPath, strerror(errno));
	    fl.processingFailed = 1;
	} else {
	    if (S_ISREG(flp->fl_mode) && flp->fl_size >= UINT32_MAX)
		fl.largeFiles = 1;
	}

	if (fl.def.ar.ar_fmodestr) {
	    flp->fl_mode &= S_IFMT;
	    flp->fl_mode |= fl.def.ar.ar_fmode;
	}

	if (fl.def.ar.ar_user) {
	    flp->uname = fl.def.ar.ar_user;
	} else {
	    flp->uname = rpmstrPoolId(fl.pool, rpmugUname(flp->fl_uid), 1);
	}
	if (! flp->uname) {
	    flp->uname = rpmstrPoolId(fl.pool, rpmugUname(getuid()), 1);
	}
	if (! flp->uname) {
	    flp->uname = rpmstrPoolId(fl.pool, "root", 1);
	}

	if (fl.def.ar.ar_group) {
	    flp->gname = fl.def.ar.ar_group;
	} else {
	    flp->gname = rpmstrPoolId(fl.pool, rpmugGname(flp->fl_gid), 1);
	}
	if (! flp->gname) {
	    flp->gname = rpmstrPoolId(fl.pool, rpmugGname(getgid()), 1);
	}
	if (! flp->gname) {
	    flp->gname = rpmstrPoolId(fl.pool, "root", 1);
	}

	flp->langs = xstrdup("");
	fl.files.used++;
    }
    argvFree(files);

    if (! fl.processingFailed) {
	if (sourcePkg->header != NULL) {
	    genCpioListAndHeader(&fl, sourcePkg, 1);
	}
    }

    FileListFree(&fl);
    return fl.processingFailed ? RPMRC_FAIL : RPMRC_OK;
}

/**
 * Check packaged file list against what's in the build root.
 * @param buildRoot	path of build root
 * @param fileList	packaged file list
 * @return		-1 if skipped, 0 on OK, 1 on error
 */
static int checkFiles(const char *buildRoot, StringBuf fileList)
{
    static char * const av_ckfile[] = { "%{?__check_files}", NULL };
    StringBuf sb_stdout = NULL;
    int rc = -1;
    char * s = rpmExpand(av_ckfile[0], NULL);
    
    if (!(s && *s))
	goto exit;

    rpmlog(RPMLOG_NOTICE, _("Checking for unpackaged file(s): %s\n"), s);

    rc = rpmfcExec(av_ckfile, fileList, &sb_stdout, 0, buildRoot);
    if (rc < 0)
	goto exit;
    
    if (sb_stdout) {
	int _unpackaged_files_terminate_build =
		rpmExpandNumeric("%{?_unpackaged_files_terminate_build}");
	const char * t = getStringBuf(sb_stdout);
	if ((*t != '\0') && (*t != '\n')) {
	    rc = (_unpackaged_files_terminate_build) ? 1 : 0;
	    rpmlog((rc ? RPMLOG_ERR : RPMLOG_WARNING),
		_("Installed (but unpackaged) file(s) found:\n%s"), t);
	}
    }
    
exit:
    freeStringBuf(sb_stdout);
    free(s);
    return rc;
}

rpmRC processBinaryFiles(rpmSpec spec, rpmBuildPkgFlags pkgFlags,
			int installSpecialDoc, int test)
{
    Package pkg;
    rpmRC rc = RPMRC_OK;
    
    check_fileList = newStringBuf();
    genSourceRpmName(spec);
    
    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	char *nvr;
	const char *a;
	int header_color;
	int arch_color;

	if (pkg->fileList == NULL)
	    continue;

	headerPutString(pkg->header, RPMTAG_SOURCERPM, spec->sourceRpmName);

	nvr = headerGetAsString(pkg->header, RPMTAG_NVRA);
	rpmlog(RPMLOG_NOTICE, _("Processing files: %s\n"), nvr);
	free(nvr);
		   
	if ((rc = processPackageFiles(spec, pkgFlags, pkg, installSpecialDoc, test)) != RPMRC_OK ||
	    (rc = rpmfcGenerateDepends(spec, pkg)) != RPMRC_OK)
	    goto exit;

	a = headerGetString(pkg->header, RPMTAG_ARCH);
	header_color = headerGetNumber(pkg->header, RPMTAG_HEADERCOLOR);
	if (!rstreq(a, "noarch")) {
	    arch_color = rpmGetArchColor(a);
	    if (arch_color > 0 && header_color > 0 &&
					!(arch_color & header_color)) {
		rpmlog(RPMLOG_WARNING,
		       _("Binaries arch (%d) not matching the package arch (%d).\n"),
		       header_color, arch_color);
	    }
	} else if (header_color != 0) {
	    int terminate = rpmExpandNumeric("%{?_binaries_in_noarch_packages_terminate_build}");
	    rpmlog(terminate ? RPMLOG_ERR : RPMLOG_WARNING, 
		   _("Arch dependent binaries in noarch package\n"));
	    if (terminate) {
		rc = RPMRC_FAIL;
		goto exit;
	    }
	}
    }

    /* Now we have in fileList list of files from all packages.
     * We pass it to a script which does the work of finding missing
     * and duplicated files.
     */
    
    
    if (checkFiles(spec->buildRoot, check_fileList) > 0) {
	rc = RPMRC_FAIL;
    }
exit:
    check_fileList = freeStringBuf(check_fileList);
    
    return rc;
}
