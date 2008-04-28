/** \ingroup rpmbuild
 * \file build/files.c
 *  The post-build, pre-packaging file tree walk to assemble the package
 *  manifest.
 */

#include "system.h"

#define	MYALLPERMS	07777

#include <regex.h>

#include <rpm/rpmbuild.h>
#include <rpm/rpmpgp.h>
#include <rpm/argv.h>
#include <rpm/rpmfc.h>
#include <rpm/rpmfileutil.h>	/* rpmDoDigest() */
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* XXX rpmioSlurp */
#include "rpmio/fts.h"
#include "lib/cpio.h"
#include "lib/rpmfi_internal.h"	/* XXX pretty much all rpmfi internals... */
#include "lib/rpmte_internal.h"	/* XXX rpmte init */
#include "lib/legacy.h"		/* XXX expandFileList, compressFileList */
#include "build/buildio.h"

#include "debug.h"

#define SKIPSPACE(s) { while (*(s) && risspace(*(s))) (s)++; }
#define	SKIPWHITE(_x)	{while(*(_x) && (risspace(*_x) || *(_x) == ',')) (_x)++;}
#define	SKIPNONWHITE(_x){while(*(_x) &&!(risspace(*_x) || *(_x) == ',')) (_x)++;}

/**
 */
typedef enum specdFlags_e {
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
} specdFlags;

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
    const char *uname;
    const char *gname;
    unsigned	flags;
    specdFlags	specdFlags;	/* which attributes have been explicitly specified. */
    rpmVerifyFlags verifyFlags;
    char *langs;		/* XXX locales separated with | */
} * FileListRec;

/**
 */
typedef struct AttrRec_s {
    char *ar_fmodestr;
    char *ar_dmodestr;
    char *ar_user;
    char *ar_group;
    mode_t	ar_fmode;
    mode_t	ar_dmode;
} * AttrRec;

static struct AttrRec_s root_ar = { NULL, NULL, "root", "root", 0, 0 };

/* list of files */
static StringBuf check_fileList = NULL;

/**
 * Package file tree walk data.
 */
typedef struct FileList_s {
    char * buildRoot;
    char * prefix;

    int fileCount;
    rpm_off_t totalFileSize;
    int processingFailed;

    int passedSpecialDoc;
    int isSpecialDoc;

    int noGlob;
    unsigned devtype;
    unsigned devmajor;
    int devminor;
    
    int isDir;
    int inFtw;
    rpmfileAttrs currentFlags;
    specdFlags currentSpecdFlags;
    rpmVerifyFlags currentVerifyFlags;
    struct AttrRec_s cur_ar;
    struct AttrRec_s def_ar;
    specdFlags defSpecdFlags;
    rpmVerifyFlags defVerifyFlags;
    int nLangs;
    char ** currentLangs;
    ARGV_t docDirs;
    
    FileListRec fileList;
    int fileListRecsAlloced;
    int fileListRecsUsed;
} * FileList;

/**
 */
static void nullAttrRec(AttrRec ar)
{
    ar->ar_fmodestr = NULL;
    ar->ar_dmodestr = NULL;
    ar->ar_user = NULL;
    ar->ar_group = NULL;
    ar->ar_fmode = 0;
    ar->ar_dmode = 0;
}

/**
 */
static void freeAttrRec(AttrRec ar)
{
    ar->ar_fmodestr = _free(ar->ar_fmodestr);
    ar->ar_dmodestr = _free(ar->ar_dmodestr);
    ar->ar_user = _free(ar->ar_user);
    ar->ar_group = _free(ar->ar_group);
    /* XXX doesn't free ar (yet) */
    return;
}

/**
 */
static void dupAttrRec(const AttrRec oar, AttrRec nar)
{
    if (oar == nar)
	return;
    freeAttrRec(nar);
    nar->ar_fmodestr = (oar->ar_fmodestr ? xstrdup(oar->ar_fmodestr) : NULL);
    nar->ar_dmodestr = (oar->ar_dmodestr ? xstrdup(oar->ar_dmodestr) : NULL);
    nar->ar_user = (oar->ar_user ? xstrdup(oar->ar_user) : NULL);
    nar->ar_group = (oar->ar_group ? xstrdup(oar->ar_group) : NULL);
    nar->ar_fmode = oar->ar_fmode;
    nar->ar_dmode = oar->ar_dmode;
}

#if 0
/**
 */
static void dumpAttrRec(const char * msg, AttrRec ar)
{
    if (msg)
	fprintf(stderr, "%s:\t", msg);
    fprintf(stderr, "(%s, %s, %s, %s)\n",
	ar->ar_fmodestr,
	ar->ar_user,
	ar->ar_group,
	ar->ar_dmodestr);
}
#endif

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
static void timeCheck(int tc, Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpm_time_t * mtime;
    const char ** files;
    rpmTagType fnt;
    rpm_count_t count, x;
    time_t currentTime = time(NULL);

    x = hge(h, RPMTAG_OLDFILENAMES, &fnt, (rpm_data_t *) &files, &count);
    x = hge(h, RPMTAG_FILEMTIMES, NULL, (rpm_data_t *) &mtime, NULL);
    
    for (x = 0; x < count; x++) {
	if ((currentTime - mtime[x]) > tc)
	    rpmlog(RPMLOG_WARNING, _("TIMECHECK failure: %s\n"), files[x]);
    }
    files = hfd(files, fnt);
}

/**
 */
typedef const struct VFA {
    const char const * attribute;
    int not;
    int	flag;
} VFA_t;

/**
 */
VFA_t const verifyAttrs[] = {
    { "md5",	0,	RPMVERIFY_MD5 },
    { "size",	0,	RPMVERIFY_FILESIZE },
    { "link",	0,	RPMVERIFY_LINKTO },
    { "user",	0,	RPMVERIFY_USER },
    { "group",	0,	RPMVERIFY_GROUP },
    { "mtime",	0,	RPMVERIFY_MTIME },
    { "mode",	0,	RPMVERIFY_MODE },
    { "rdev",	0,	RPMVERIFY_RDEV },
    { NULL, 0,	0 }
};

/**
 * Parse %verify and %defverify from file manifest.
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForVerify(const char * buf, FileList fl)
{
    char *p, *pe, *q;
    const char *name;
    rpmVerifyFlags *resultVerify;
    int negated;
    rpmVerifyFlags verifyFlags;
    specdFlags * specdFlags;

    if ((p = strstr(buf, (name = "%verify"))) != NULL) {
	resultVerify = &(fl->currentVerifyFlags);
	specdFlags = &fl->currentSpecdFlags;
    } else if ((p = strstr(buf, (name = "%defverify"))) != NULL) {
	resultVerify = &(fl->defVerifyFlags);
	specdFlags = &fl->defSpecdFlags;
    } else
	return RPMRC_OK;

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';

    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Bracket %*verify args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Localize. Erase parsed string */
    q = alloca((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    negated = 0;
    verifyFlags = RPMVERIFY_NONE;

    for (p = q; *p != '\0'; p = pe) {
	SKIPWHITE(p);
	if (*p == '\0')
	    break;
	pe = p;
	SKIPNONWHITE(pe);
	if (*pe != '\0')
	    *pe++ = '\0';

	{   VFA_t *vfa;
	    for (vfa = verifyAttrs; vfa->attribute != NULL; vfa++) {
		if (strcmp(p, vfa->attribute))
		    continue;
		verifyFlags |= vfa->flag;
		break;
	    }
	    if (vfa->attribute)
		continue;
	}

	if (!strcmp(p, "not")) {
	    negated ^= 1;
	} else {
	    rpmlog(RPMLOG_ERR, _("Invalid %s token: %s\n"), name, p);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
    }

    *resultVerify = negated ? ~(verifyFlags) : verifyFlags;
    *specdFlags |= SPECD_VERIFY;

    return RPMRC_OK;
}

#define	isAttrDefault(_ars)	((_ars)[0] == '-' && (_ars)[1] == '\0')

/**
 * Parse %dev from file manifest.
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForDev(const char * buf, FileList fl)
{
    const char * name;
    const char * errstr = NULL;
    char *p, *pe, *q = NULL;
    int rc = RPMRC_FAIL;	/* assume error */

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
	fl->devtype = 'b';
    else if (*p == 'c')
	fl->devtype = 'c';
    else {
	errstr = "devtype";
	goto exit;
    }

    p = pe; SKIPWHITE(p);
    pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
    for (pe = p; *pe && risdigit(*pe); pe++)
	{} ;
    if (*pe == '\0') {
	fl->devmajor = atoi(p);
	if (!(fl->devmajor >= 0 && fl->devmajor < 256)) {
	    errstr = "devmajor";
	    goto exit;
	}
	pe++;
    } else {
	errstr = "devmajor";
	goto exit;
    }

    p = pe; SKIPWHITE(p);
    pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
    for (pe = p; *pe && risdigit(*pe); pe++)
	{} ;
    if (*pe == '\0') {
	fl->devminor = atoi(p);
	if (!(fl->devminor >= 0 && fl->devminor < 256)) {
	    errstr = "devminor";
	    goto exit;
	}
	pe++;
    } else {
	errstr = "devminor";
	goto exit;
    }

    fl->noGlob = 1;

    rc = RPMRC_OK;

exit:
    if (rc) {
	rpmlog(RPMLOG_ERR, _("Missing %s in %s %s\n"), errstr, name, p);
	fl->processingFailed = 1;
    }
    free(q);
    return rc;
}

/**
 * Parse %attr and %defattr from file manifest.
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @return		0 on success
 */
static int parseForAttr(const char * buf, FileList fl)
{
    const char *name;
    char *p, *pe, *q;
    int x;
    struct AttrRec_s arbuf;
    AttrRec ar = &arbuf, ret_ar;
    specdFlags * specdFlags;

    if ((p = strstr(buf, (name = "%attr"))) != NULL) {
	ret_ar = &(fl->cur_ar);
	specdFlags = &fl->currentSpecdFlags;
    } else if ((p = strstr(buf, (name = "%defattr"))) != NULL) {
	ret_ar = &(fl->def_ar);
	specdFlags = &fl->defSpecdFlags;
    } else
	return 0;

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';

    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Bracket %*attr args */
    *pe++ = ' ';
    for (p = pe; *pe && *pe != ')'; pe++)
	{};

    if (ret_ar == &(fl->def_ar)) {	/* %defattr */
	q = pe;
	q++;
	SKIPSPACE(q);
	if (*q != '\0') {
	    rpmlog(RPMLOG_ERR,
		     _("Non-white space follows %s(): %s\n"), name, q);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
    }

    /* Localize. Erase parsed string */
    q = alloca((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    nullAttrRec(ar);

    p = q; SKIPWHITE(p);
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_fmodestr = p;
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_user = p;
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0') {
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_group = p;
	p = pe; SKIPWHITE(p);
    }
    if (*p != '\0' && ret_ar == &(fl->def_ar)) {	/* %defattr */
	pe = p; SKIPNONWHITE(pe); if (*pe != '\0') *pe++ = '\0';
	ar->ar_dmodestr = p;
	p = pe; SKIPWHITE(p);
    }

    if (!(ar->ar_fmodestr && ar->ar_user && ar->ar_group) || *p != '\0') {
	rpmlog(RPMLOG_ERR, _("Bad syntax: %s(%s)\n"), name, q);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Do a quick test on the mode argument and adjust for "-" */
    if (ar->ar_fmodestr && !isAttrDefault(ar->ar_fmodestr)) {
	unsigned int ui;
	x = sscanf(ar->ar_fmodestr, "%o", &ui);
	if ((x == 0) || (ar->ar_fmode & ~MYALLPERMS)) {
	    rpmlog(RPMLOG_ERR, _("Bad mode spec: %s(%s)\n"), name, q);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
	ar->ar_fmode = ui;
    } else
	ar->ar_fmodestr = NULL;

    if (ar->ar_dmodestr && !isAttrDefault(ar->ar_dmodestr)) {
	unsigned int ui;
	x = sscanf(ar->ar_dmodestr, "%o", &ui);
	if ((x == 0) || (ar->ar_dmode & ~MYALLPERMS)) {
	    rpmlog(RPMLOG_ERR, _("Bad dirmode spec: %s(%s)\n"), name, q);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
	ar->ar_dmode = ui;
    } else
	ar->ar_dmodestr = NULL;

    if (!(ar->ar_user && !isAttrDefault(ar->ar_user)))
	ar->ar_user = NULL;

    if (!(ar->ar_group && !isAttrDefault(ar->ar_group)))
	ar->ar_group = NULL;

    dupAttrRec(ar, ret_ar);

    /* XXX fix all this */
    *specdFlags |= SPECD_UID | SPECD_GID | SPECD_FILEMODE | SPECD_DIRMODE;
    
    return 0;
}

/**
 * Parse %config from file manifest.
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @return		RPMRC_OK on success
 */
static rpmRC parseForConfig(const char * buf, FileList fl)
{
    char *p, *pe, *q;
    const char *name;

    if ((p = strstr(buf, (name = "%config"))) == NULL)
	return RPMRC_OK;

    fl->currentFlags |= RPMFILE_CONFIG;

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
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Localize. Erase parsed string. */
    q = alloca((pe-p) + 1);
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
	if (!strcmp(p, "missingok")) {
	    fl->currentFlags |= RPMFILE_MISSINGOK;
	} else if (!strcmp(p, "noreplace")) {
	    fl->currentFlags |= RPMFILE_NOREPLACE;
	} else {
	    rpmlog(RPMLOG_ERR, _("Invalid %s token: %s\n"), name, p);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
    }

    return RPMRC_OK;
}

/**
 */
static int langCmp(const void * ap, const void * bp)
{
    return strcmp(*(const char **)ap, *(const char **)bp);
}

/**
 * Parse %lang from file manifest.
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @return		RPMRC_OK on success
 */
static int parseForLang(const char * buf, FileList fl)
{
    char *p, *pe, *q;
    const char *name;

  while ((p = strstr(buf, (name = "%lang"))) != NULL) {

    for (pe = p; (pe-p) < strlen(name); pe++)
	*pe = ' ';
    SKIPSPACE(pe);

    if (*pe != '(') {
	rpmlog(RPMLOG_ERR, _("Missing '(' in %s %s\n"), name, pe);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Bracket %lang args */
    *pe++ = ' ';
    for (pe = p; *pe && *pe != ')'; pe++)
	{};

    if (*pe == '\0') {
	rpmlog(RPMLOG_ERR, _("Missing ')' in %s(%s\n"), name, p);
	fl->processingFailed = 1;
	return RPMRC_FAIL;
    }

    /* Localize. Erase parsed string. */
    q = alloca((pe-p) + 1);
    rstrlcpy(q, p, (pe-p) + 1);
    while (p <= pe)
	*p++ = ' ';

    /* Parse multiple arguments from %lang */
    for (p = q; *p != '\0'; p = pe) {
	char *newp;
	size_t np;
	int i;

	SKIPWHITE(p);
	pe = p;
	SKIPNONWHITE(pe);

	np = pe - p;
	
	/* Sanity check on locale lengths */
	if (np < 1 || (np == 1 && *p != 'C') || np >= 32) {
	    rpmlog(RPMLOG_ERR,
		_("Unusual locale length: \"%.*s\" in %%lang(%s)\n"),
		(int)np, p, q);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}

	/* Check for duplicate locales */
	if (fl->currentLangs != NULL)
	for (i = 0; i < fl->nLangs; i++) {
	    if (strncmp(fl->currentLangs[i], p, np))
		continue;
	    rpmlog(RPMLOG_ERR, _("Duplicate locale %.*s in %%lang(%s)\n"),
		(int)np, p, q);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}

	/* Add new locale */
	fl->currentLangs = xrealloc(fl->currentLangs,
				(fl->nLangs + 1) * sizeof(*fl->currentLangs));
	newp = xmalloc( np+1 );
	rstrlcpy(newp, p, np + 1);
	fl->currentLangs[fl->nLangs++] = newp;
	if (*pe == ',') pe++;	/* skip , if present */
    }
  }

    /* Insure that locales are sorted. */
    if (fl->currentLangs)
	qsort(fl->currentLangs, fl->nLangs, sizeof(*fl->currentLangs), langCmp);

    return RPMRC_OK;
}

/**
 */
static int parseForRegexLang(const char * fileName, char ** lang)
{
    static int initialized = 0;
    static int hasRegex = 0;
    static regex_t compiledPatt;
    char *buf;
    int x;
    regmatch_t matches[2];
    const char *s;

    if (! initialized) {
	char *patt = rpmExpand("%{?_langpatt}", NULL);
	int rc = 0;
	if (!(patt && *patt != '\0'))
	    rc = 1;
	else if (regcomp(&compiledPatt, patt, REG_EXTENDED))
	    rc = -1;
	patt = _free(patt);
	if (rc)
	    return rc;
	hasRegex = 1;
	initialized = 1;
    }
    
    memset(matches, 0, sizeof(matches));
    if (! hasRegex || regexec(&compiledPatt, fileName, 2, matches, REG_NOTEOL))
	return 1;

    /* Got match */
    s = fileName + matches[1].rm_eo - 1;
    x = matches[1].rm_eo - matches[1].rm_so;
    buf = xmalloc(x+1);
    buf[x] = '\0';
    memcpy(buf, s, x);
    if (lang)
	*lang = buf;
    else {
	free(buf);
	return 1;
    }
    return 0;
}

/**
 */
VFA_t virtualFileAttributes[] = {
	{ "%dir",	0,	0 },	/* XXX why not RPMFILE_DIR? */
	{ "%doc",	0,	RPMFILE_DOC },
	{ "%ghost",	0,	RPMFILE_GHOST },
	{ "%exclude",	0,	RPMFILE_EXCLUDE },
	{ "%readme",	0,	RPMFILE_README },
	{ "%license",	0,	RPMFILE_LICENSE },
	{ "%pubkey",	0,	RPMFILE_PUBKEY },
	{ "%policy",	0,	RPMFILE_POLICY },

#if WHY_NOT
	{ "%icon",	0,	RPMFILE_ICON },
	{ "%spec",	0,	RPMFILE_SPEC },
	{ "%config",	0,	RPMFILE_CONFIG },
	{ "%missingok",	0,	RPMFILE_CONFIG|RPMFILE_MISSINGOK },
	{ "%noreplace",	0,	RPMFILE_CONFIG|RPMFILE_NOREPLACE },
#endif

	{ NULL, 0, 0 }
};

/**
 * Parse simple attributes (e.g. %dir) from file manifest.
 * @param spec
 * @param pkg
 * @param buf		current spec file line
 * @param fl		package file tree walk data
 * @retval *fileName	file name
 * @return		RPMRC_OK on success
 */
static rpmRC parseForSimple(rpmSpec spec, Package pkg, char * buf,
			  FileList fl, const char ** fileName)
{
    char *s, *t;
    int res, specialDoc = 0;
    char specialDocBuf[BUFSIZ];

    specialDocBuf[0] = '\0';
    *fileName = NULL;
    res = RPMRC_OK;

    t = buf;
    while ((s = strtokWithQuotes(t, " \t\n")) != NULL) {
	t = NULL;
	if (!strcmp(s, "%docdir")) {
	    s = strtokWithQuotes(NULL, " \t\n");
	
	    if (s == NULL || strtokWithQuotes(NULL, " \t\n")) {
		rpmlog(RPMLOG_ERR, _("Only one arg for %%docdir\n"));
		fl->processingFailed = 1;
		res = RPMRC_FAIL;
	    } else {
		argvAdd(&(fl->docDirs), s);
	    }
	    break;
	}

    /* Set flags for virtual file attributes */
    {	VFA_t *vfa;
	for (vfa = virtualFileAttributes; vfa->attribute != NULL; vfa++) {
	    if (strcmp(s, vfa->attribute))
		continue;
	    if (!vfa->flag) {
		if (!strcmp(s, "%dir"))
		    fl->isDir = 1;	/* XXX why not RPMFILE_DIR? */
	    } else {
		if (vfa->not)
		    fl->currentFlags &= ~vfa->flag;
		else
		    fl->currentFlags |= vfa->flag;
	    }

	    break;
	}
	/* if we got an attribute, continue with next token */
	if (vfa->attribute != NULL)
	    continue;
    }

	if (*fileName) {
	    /* We already got a file -- error */
	    rpmlog(RPMLOG_ERR, _("Two files on one line: %s\n"),
		*fileName);
	    fl->processingFailed = 1;
	    res = RPMRC_FAIL;
	}

	if (*s != '/') {
	    if (fl->currentFlags & RPMFILE_DOC) {
		specialDoc = 1;
		strcat(specialDocBuf, " ");
		strcat(specialDocBuf, s);
	    } else
	    if (fl->currentFlags & (RPMFILE_POLICY|RPMFILE_PUBKEY|RPMFILE_ICON))
	    {
		*fileName = s;
	    } else {
		/* not in %doc, does not begin with / -- error */
		rpmlog(RPMLOG_ERR,
		    _("File must begin with \"/\": %s\n"), s);
		fl->processingFailed = 1;
		res = RPMRC_FAIL;
	    }
	} else {
	    *fileName = s;
	}
    }

    if (specialDoc) {
	if (*fileName || (fl->currentFlags & ~(RPMFILE_DOC))) {
	    rpmlog(RPMLOG_ERR,
		     _("Can't mix special %%doc with other forms: %s\n"),
		     (*fileName ? *fileName : ""));
	    fl->processingFailed = 1;
	    res = RPMRC_FAIL;
	} else {
	/* XXX WATCHOUT: buf is an arg */
	   {
		static const char *_docdir_fmt = NULL;
		static int oneshot = 0;
		char *ddir, *fmt;
	        errmsg_t errstr;
		if (!oneshot) {
		    _docdir_fmt = rpmExpand("%{?_docdir_fmt}", NULL);
		    if (!_docdir_fmt || !*_docdir_fmt)
			_docdir_fmt = "%{NAME}-%{VERSION}";
		    oneshot = 1;
		}
		fmt = headerSprintf(pkg->header, _docdir_fmt, rpmTagTable, rpmHeaderFormats, &errstr);
		if (!fmt) {
		    rpmlog(RPMLOG_ERR, _("illegal _docdir_fmt: %s\n"), errstr);
		    fl->processingFailed = 1;
		    res = RPMRC_FAIL;
		}
		ddir = rpmGetPath("%{_docdir}/", fmt, NULL);
		strcpy(buf, ddir);
		ddir = _free(ddir);
	    }

	/* XXX FIXME: this is easy to do as macro expansion */

	    if (! fl->passedSpecialDoc) {
		pkg->specialDoc = newStringBuf();
		appendStringBuf(pkg->specialDoc, "DOCDIR=$RPM_BUILD_ROOT");
		appendLineStringBuf(pkg->specialDoc, buf);
		appendLineStringBuf(pkg->specialDoc, "export DOCDIR");
		appendLineStringBuf(pkg->specialDoc, "rm -rf $DOCDIR");
		appendLineStringBuf(pkg->specialDoc, RPM_MKDIR_P " $DOCDIR");

		*fileName = buf;
		fl->passedSpecialDoc = 1;
		fl->isSpecialDoc = 1;
	    }

	    appendStringBuf(pkg->specialDoc, "cp -pr ");
	    appendStringBuf(pkg->specialDoc, specialDocBuf);
	    appendLineStringBuf(pkg->specialDoc, " $DOCDIR");
	}
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
 * @param fl		package file tree walk data
 * @param fileName	file path
 * @return		1 if doc file, 0 if not
 */
static int isDoc(FileList fl, const char * fileName)	
{
    size_t k, l;
    ARGV_const_t dd;

    k = strlen(fileName);
    for (dd = fl->docDirs; *dd; dd++) {
	l = strlen(*dd);
	if (l < k && strncmp(fileName, *dd, l) == 0 && fileName[l] == '/')
	    return 1;
    }
    return 0;
}

/**
 * Verify that file attributes scope over hardlinks correctly.
 * If partial hardlink sets are possible, then add tracking dependency.
 * @param fl		package file tree walk data
 * @return		1 if partial hardlink sets can exist, 0 otherwise.
 */
static int checkHardLinks(FileList fl)
{
    FileListRec ilp, jlp;
    int i, j;

    for (i = 0;  i < fl->fileListRecsUsed; i++) {
	ilp = fl->fileList + i;
	if (!(S_ISREG(ilp->fl_mode) && ilp->fl_nlink > 1))
	    continue;

	for (j = i + 1; j < fl->fileListRecsUsed; j++) {
	    jlp = fl->fileList + j;
	    if (!S_ISREG(jlp->fl_mode))
		continue;
	    if (ilp->fl_nlink != jlp->fl_nlink)
		continue;
	    if (ilp->fl_ino != jlp->fl_ino)
		continue;
	    if (ilp->fl_dev != jlp->fl_dev)
		continue;
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
 * @retval *fip		file info for package
 * @param h
 * @param isSrc
 */
static void genCpioListAndHeader(FileList fl,
		rpmfi * fip, Header h, int isSrc)
{
    int _addDotSlash = !(isSrc || rpmExpandNumeric("%{_noPayloadPrefix}"));
    size_t apathlen = 0;
    size_t dpathlen = 0;
    size_t skipLen = 0;
    size_t fnlen;
    FileListRec flp;
    char buf[BUFSIZ];
    int i;
    
    /* Sort the big list */
    qsort(fl->fileList, fl->fileListRecsUsed,
	  sizeof(*(fl->fileList)), compareFileListRecs);
    
    /* Generate the header. */
    if (! isSrc) {
	skipLen = 1;
	if (fl->prefix)
	    skipLen += strlen(fl->prefix);
    }

    for (i = 0, flp = fl->fileList; i < fl->fileListRecsUsed; i++, flp++) {
	const char *s;

 	/* Merge duplicate entries. */
	while (i < (fl->fileListRecsUsed - 1) &&
	    !strcmp(flp->cpioPath, flp[1].cpioPath)) {

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

	/* Omit '/' and/or URL prefix, leave room for "./" prefix */
	apathlen += (strlen(flp->cpioPath) - skipLen + (_addDotSlash ? 3 : 1));

	/* Leave room for both dirname and basename NUL's */
	dpathlen += (strlen(flp->diskPath) + 2);

	/*
	 * Make the header, the OLDFILENAMES will get converted to a 
	 * compressed file list write before we write the actual package to
	 * disk.
	 */
	(void) headerAddOrAppendEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			       &(flp->cpioPath), 1);

      if (sizeof(flp->fl_size) != sizeof(rpm_off_t)) {
	rpm_off_t psize = (rpm_off_t)flp->fl_size;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILESIZES, RPM_INT32_TYPE,
			       &(psize), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILESIZES, RPM_INT32_TYPE,
			       &(flp->fl_size), 1);
      }
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEUSERNAME, RPM_STRING_ARRAY_TYPE,
			       &(flp->uname), 1);
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEGROUPNAME, RPM_STRING_ARRAY_TYPE,
			       &(flp->gname), 1);
      if (sizeof(flp->fl_mtime) != sizeof(rpm_time_t)) {
	rpm_time_t mtime = (rpm_time_t)flp->fl_mtime;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEMTIMES, RPM_INT32_TYPE,
			       &(mtime), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEMTIMES, RPM_INT32_TYPE,
			       &(flp->fl_mtime), 1);
      }
      if (sizeof(flp->fl_mode) != sizeof(rpm_mode_t)) {
	rpm_mode_t pmode = (rpm_mode_t)flp->fl_mode;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEMODES, RPM_INT16_TYPE,
			       &(pmode), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEMODES, RPM_INT16_TYPE,
			       &(flp->fl_mode), 1);
      }
      if (sizeof(flp->fl_rdev) != sizeof(rpm_rdev_t)) {
	rpm_rdev_t prdev = (rpm_rdev_t)flp->fl_rdev;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILERDEVS, RPM_INT16_TYPE,
			       &(prdev), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILERDEVS, RPM_INT16_TYPE,
			       &(flp->fl_rdev), 1);
      }
      if (sizeof(flp->fl_dev) != sizeof(rpm_dev_t)) {
	rpm_dev_t pdevice = (rpm_dev_t)flp->fl_dev;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEDEVICES, RPM_INT32_TYPE,
			       &(pdevice), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEDEVICES, RPM_INT32_TYPE,
			       &(flp->fl_dev), 1);
      }
      if (sizeof(flp->fl_ino) != sizeof(rpm_ino_t)) {
	rpm_ino_t ino = (rpm_ino_t)flp->fl_ino;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEINODES, RPM_INT32_TYPE,
				&(ino), 1);
      } else {
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEINODES, RPM_INT32_TYPE,
				&(flp->fl_ino), 1);
      }

	(void) headerAddOrAppendEntry(h, RPMTAG_FILELANGS, RPM_STRING_ARRAY_TYPE,
			       &(flp->langs),  1);
	
	/* We used to add these, but they should not be needed */
	/* (void) headerAddOrAppendEntry(h, RPMTAG_FILEUIDS,
	 *		   RPM_INT32_TYPE, &(flp->fl_uid), 1);
	 * (void) headerAddOrAppendEntry(h, RPMTAG_FILEGIDS,
	 *		   RPM_INT32_TYPE, &(flp->fl_gid), 1);
	 */
	
	buf[0] = '\0';
	if (S_ISREG(flp->fl_mode))
	    (void) rpmDoDigest(PGPHASHALGO_MD5, flp->diskPath, 1, 
			       (unsigned char *)buf, NULL);
	s = buf;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEMD5S, RPM_STRING_ARRAY_TYPE,
			       &s, 1);
	
	buf[0] = '\0';
	if (S_ISLNK(flp->fl_mode)) {
	    buf[readlink(flp->diskPath, buf, BUFSIZ)] = '\0';
	    if (fl->buildRoot) {
		if (buf[0] == '/' && strcmp(fl->buildRoot, "/") &&
		    !strncmp(buf, fl->buildRoot, strlen(fl->buildRoot))) {
		     rpmlog(RPMLOG_ERR,
				_("Symlink points to BuildRoot: %s -> %s\n"),
				flp->cpioPath, buf);
		    fl->processingFailed = 1;
		}
	    }
	}
	s = buf;
	(void) headerAddOrAppendEntry(h, RPMTAG_FILELINKTOS, RPM_STRING_ARRAY_TYPE,
			       &s, 1);
	
	if (flp->flags & RPMFILE_GHOST) {
	    flp->verifyFlags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE |
				RPMVERIFY_LINKTO | RPMVERIFY_MTIME);
	}
	(void) headerAddOrAppendEntry(h, RPMTAG_FILEVERIFYFLAGS, RPM_INT32_TYPE,
			       &(flp->verifyFlags), 1);
	
	if (!isSrc && isDoc(fl, flp->cpioPath))
	    flp->flags |= RPMFILE_DOC;
	/* XXX Should directories have %doc/%config attributes? (#14531) */
	if (S_ISDIR(flp->fl_mode))
	    flp->flags &= ~(RPMFILE_CONFIG|RPMFILE_DOC);

	(void) headerAddOrAppendEntry(h, RPMTAG_FILEFLAGS, RPM_INT32_TYPE,
			       &(flp->flags), 1);

    }

    (void) headerAddEntry(h, RPMTAG_SIZE, RPM_INT32_TYPE,
		   &(fl->totalFileSize), 1);

    if (_addDotSlash)
	(void) rpmlibNeedsFeature(h, "PayloadFilesHavePrefix", "4.0-1");

    /* Choose how filenames are represented. */
    if (_noDirTokens)
	expandFilelist(h);
    else {
	compressFilelist(h);
	/* Binary packages with dirNames cannot be installed by legacy rpm. */
	(void) rpmlibNeedsFeature(h, "CompressedFileNames", "3.0.4-1");
    }

  { int scareMem = 0;
    rpmts ts = NULL;	/* XXX FIXME drill rpmts ts all the way down here */
    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    char * a, * d;

    if (fi == NULL) return;		/* XXX can't happen */

    fi->te = xcalloc(1, sizeof(*fi->te));
    fi->te->type = TR_ADDED;

    fi->dnl = _free(fi->dnl);
    fi->bnl = _free(fi->bnl);
    if (!scareMem) fi->dil = _free(fi->dil);

    fi->dnl = xmalloc(fi->fc * sizeof(*fi->dnl) + dpathlen);
    d = (char *)(fi->dnl + fi->fc);
    *d = '\0';

    fi->bnl = xmalloc(fi->fc * (sizeof(*fi->bnl) + sizeof(*fi->dil)));
/* FIX: artifact of spoofing headerGetEntry */
    fi->dil = (!scareMem)
	? xcalloc(sizeof(*fi->dil), fi->fc)
	: (int *)(fi->bnl + fi->fc);

    fi->apath = xmalloc(fi->fc * sizeof(*fi->apath) + apathlen + 1);
    a = (char *)(fi->apath + fi->fc);
    *a = '\0';

    fi->actions = xcalloc(sizeof(*fi->actions), fi->fc);
    fi->fmapflags = xcalloc(sizeof(*fi->fmapflags), fi->fc);
    fi->astriplen = 0;
    if (fl->buildRoot)
	fi->astriplen = strlen(fl->buildRoot);
    fi->striplen = 0;
    fi->fuser = NULL;
    fi->fgroup = NULL;

    /* Make the cpio list */
    if (fi->dil != NULL)	/* XXX can't happen */
    for (i = 0, flp = fl->fileList; i < fi->fc; i++, flp++) {
	char * b;

	/* Skip (possible) duplicate file entries, use last entry info. */
	while (((flp - fl->fileList) < (fl->fileListRecsUsed - 1)) &&
		!strcmp(flp->cpioPath, flp[1].cpioPath))
	    flp++;

	if (flp->flags & RPMFILE_EXCLUDE) {
	    i--;
	    continue;
	}

	if ((fnlen = strlen(flp->diskPath) + 1) > fi->fnlen)
	    fi->fnlen = fnlen;

	/* Create disk directory and base name. */
	fi->dil[i] = i;
/* FIX: artifact of spoofing headerGetEntry */
	fi->dnl[fi->dil[i]] = d;
	d = stpcpy(d, flp->diskPath);

	/* Make room for the dirName NUL, find start of baseName. */
	for (b = d; b > fi->dnl[fi->dil[i]] && *b != '/'; b--)
	    b[1] = b[0];
	b++;		/* dirname's end in '/' */
	*b++ = '\0';	/* terminate dirname, b points to basename */
	fi->bnl[i] = b;
	d += 2;		/* skip both dirname and basename NUL's */

	/* Create archive path, normally adding "./" */
	fi->apath[i] = a;
	if (_addDotSlash)
	    a = stpcpy(a, "./");
	a = stpcpy(a, (flp->cpioPath + skipLen));
	a++;		/* skip apath NUL */

	if (flp->flags & RPMFILE_GHOST) {
	    fi->actions[i] = FA_SKIP;
	    continue;
	}
	fi->actions[i] = FA_COPYOUT;
	fi->fmapflags[i] = CPIO_MAP_PATH |
		CPIO_MAP_TYPE | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;
	if (isSrc)
	    fi->fmapflags[i] |= CPIO_FOLLOW_SYMLINKS;

    }
    if (fip)
	*fip = fi;
    else
	fi = rpmfiFree(fi);
  }
}

/**
 */
static FileListRec freeFileList(FileListRec fileList,
			int count)
{
    while (count--) {
	fileList[count].diskPath = _free(fileList[count].diskPath);
	fileList[count].cpioPath = _free(fileList[count].cpioPath);
	fileList[count].langs = _free(fileList[count].langs);
    }
    fileList = _free(fileList);
    return NULL;
}

/* forward ref */
static rpmRC recurseDir(FileList fl, const char * diskPath);

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
    const char *cpioPath = diskPath;
    struct stat statbuf;
    mode_t fileMode;
    uid_t fileUid;
    gid_t fileGid;
    const char *fileUname;
    const char *fileGname;
    
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
    if (fl->buildRoot && strcmp(fl->buildRoot, "/"))
    	cpioPath += strlen(fl->buildRoot);

    /* XXX make sure '/' can be packaged also */
    if (*cpioPath == '\0')
	cpioPath = "/";

    /* If we are using a prefix, validate the file */
    if (!fl->inFtw && fl->prefix) {
	const char *prefixPtr = fl->prefix;

	while (*prefixPtr && *cpioPath && (*cpioPath == *prefixPtr)) {
	    prefixPtr++;
	    cpioPath++;
	}
	if (*prefixPtr || (*cpioPath && *cpioPath != '/')) {
	    rpmlog(RPMLOG_ERR, _("File doesn't match prefix (%s): %s\n"),
		     fl->prefix, cpioPath);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
    }

    if (statp == NULL) {
	statp = &statbuf;
	memset(statp, 0, sizeof(*statp));
	if (fl->devtype) {
	    time_t now = time(NULL);

	    /* XXX hack up a stat structure for a %dev(...) directive. */
	    statp->st_nlink = 1;
	    statp->st_rdev =
		((fl->devmajor & 0xff) << 8) | (fl->devminor & 0xff);
	    statp->st_dev = statp->st_rdev;
	    statp->st_mode = (fl->devtype == 'b' ? S_IFBLK : S_IFCHR);
	    statp->st_mode |= (fl->cur_ar.ar_fmode & 0777);
	    statp->st_atime = now;
	    statp->st_mtime = now;
	    statp->st_ctime = now;
	} else if (lstat(diskPath, statp)) {
	    rpmlog(RPMLOG_ERR, _("File not found: %s\n"), diskPath);
	    fl->processingFailed = 1;
	    return RPMRC_FAIL;
	}
    }

    if ((! fl->isDir) && S_ISDIR(statp->st_mode)) {
/* FIX: fl->buildRoot may be NULL */
	return recurseDir(fl, diskPath);
    }

    fileMode = statp->st_mode;
    fileUid = statp->st_uid;
    fileGid = statp->st_gid;

    if (S_ISDIR(fileMode) && fl->cur_ar.ar_dmodestr) {
	fileMode &= S_IFMT;
	fileMode |= fl->cur_ar.ar_dmode;
    } else if (fl->cur_ar.ar_fmodestr != NULL) {
	fileMode &= S_IFMT;
	fileMode |= fl->cur_ar.ar_fmode;
    }
    if (fl->cur_ar.ar_user) {
	fileUname = getUnameS(fl->cur_ar.ar_user);
    } else {
	fileUname = getUname(fileUid);
    }
    if (fl->cur_ar.ar_group) {
	fileGname = getGnameS(fl->cur_ar.ar_group);
    } else {
	fileGname = getGname(fileGid);
    }
	
    /* Default user/group to builder's user/group */
    if (fileUname == NULL)
	fileUname = getUname(getuid());
    if (fileGname == NULL)
	fileGname = getGname(getgid());
    
    /* S_XXX macro must be consistent with type in find call at check-files script */
    if (check_fileList && (S_ISREG(fileMode) || S_ISLNK(fileMode))) {
	appendStringBuf(check_fileList, diskPath);
	appendStringBuf(check_fileList, "\n");
    }

    /* Add to the file list */
    if (fl->fileListRecsUsed == fl->fileListRecsAlloced) {
	fl->fileListRecsAlloced += 128;
	fl->fileList = xrealloc(fl->fileList,
			fl->fileListRecsAlloced * sizeof(*(fl->fileList)));
    }
	    
    {	FileListRec flp = &fl->fileList[fl->fileListRecsUsed];
	int i;

	flp->fl_st = *statp;	/* structure assignment */
	flp->fl_mode = fileMode;
	flp->fl_uid = fileUid;
	flp->fl_gid = fileGid;

	flp->cpioPath = xstrdup(cpioPath);
	flp->diskPath = xstrdup(diskPath);
	flp->uname = fileUname;
	flp->gname = fileGname;

	if (fl->currentLangs && fl->nLangs > 0) {
	    char * ncl;
	    size_t nl = 0;
	    
	    for (i = 0; i < fl->nLangs; i++)
		nl += strlen(fl->currentLangs[i]) + 1;

	    flp->langs = ncl = xmalloc(nl);
	    for (i = 0; i < fl->nLangs; i++) {
	        const char *ocl;
		if (i)	*ncl++ = '|';
		for (ocl = fl->currentLangs[i]; *ocl != '\0'; ocl++)
			*ncl++ = *ocl;
		*ncl = '\0';
	    }
	} else if (parseForRegexLang(cpioPath, &flp->langs)) {
	    flp->langs = xstrdup("");
	}

	flp->flags = fl->currentFlags;
	flp->specdFlags = fl->currentSpecdFlags;
	flp->verifyFlags = fl->currentVerifyFlags;

	/* Hard links need be counted only once. */
	if (S_ISREG(flp->fl_mode) && flp->fl_nlink > 1) {
	    FileListRec ilp;
	    for (i = 0;  i < fl->fileListRecsUsed; i++) {
		ilp = fl->fileList + i;
		if (!S_ISREG(ilp->fl_mode))
		    continue;
		if (flp->fl_nlink != ilp->fl_nlink)
		    continue;
		if (flp->fl_ino != ilp->fl_ino)
		    continue;
		if (flp->fl_dev != ilp->fl_dev)
		    continue;
		break;
	    }
	} else
	    i = fl->fileListRecsUsed;

	if (!(flp->flags & RPMFILE_EXCLUDE) && S_ISREG(flp->fl_mode) && i >= fl->fileListRecsUsed) 
	    fl->totalFileSize += flp->fl_size;
    }

    fl->fileListRecsUsed++;
    fl->fileCount++;

    return 0;
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
    int rc = RPMRC_FAIL;

    fl->inFtw = 1;  /* Flag to indicate file has buildRoot prefixed */
    fl->isDir = 1;  /* Keep it from following myftw() again         */

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
	    rc = 0;
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

    fl->isDir = 0;
    fl->inFtw = 0;

    return rc;
}

/**
 * Add a pubkey/policy/icon to a binary package.
 * @param pkg
 * @param fl		package file tree walk data
 * @param fileName	path to file, relative is builddir, absolute buildroot.
 * @param tag		tag to add
 * @return		RPMRC_OK on success
 */
static rpmRC processMetadataFile(Package pkg, FileList fl, 
				 const char * fileName, rpmTag tag)
{
    const char * buildDir = "%{_builddir}/%{?buildsubdir}/";
    char * fn = NULL;
    char * apkt = NULL;
    uint8_t * pkt = NULL;
    ssize_t pktlen = 0;
    int absolute = 0;
    int rc = RPMRC_FAIL;
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
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("%s: public key read failed.\n"), fn);
	    goto exit;
	}
	if (xx != PGPARMOR_PUBKEY) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("%s: not an armored public key.\n"), fn);
	    goto exit;
	}
	apkt = pgpArmorWrap(PGPARMOR_PUBKEY, pkt, pktlen);
	break;
    }
    case RPMTAG_POLICIES:
	if ((xx = rpmioSlurp(fn, &pkt, &pktlen)) != 0) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("%s: *.te policy read failed.\n"), fn);
	    goto exit;
	}
	apkt = (char *) pkt;	/* XXX unsigned char */
	pkt = NULL;
	break;
    }

    xx = headerAddOrAppendEntry(pkg->header, tag,
		RPM_STRING_ARRAY_TYPE, &apkt, 1);

    rc = RPMRC_OK;
    if (absolute)
	rc = addFile(fl, fn, NULL);

exit:
    apkt = _free(apkt);
    pkt = _free(pkt);
    fn = _free(fn);
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
    int rc = RPMRC_OK;
    
    doGlob = glob_pattern_p(fileName, quote);

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

    if (doGlob) {
	ARGV_t argv = NULL;
	int argc = 0;
	int i;

	/* XXX for %dev marker in file manifest only */
	if (fl->noGlob) {
	    rpmlog(RPMLOG_ERR, _("Glob not permitted: %s\n"), diskPath);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	rc = rpmGlob(diskPath, &argc, &argv);
	if (rc == 0 && argc >= 1) {
	    for (i = 0; i < argc; i++) {
		rc = addFile(fl, argv[i], NULL);
	    }
	    argvFree(argv);
	} else {
	    rpmlog(RPMLOG_ERR, _("File not found by glob: %s\n"), diskPath);
	    rc = RPMRC_FAIL;
	    goto exit;
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

/**
 */
static rpmRC processPackageFiles(rpmSpec spec, Package pkg,
			       int installSpecialDoc, int test)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    struct FileList_s fl;
    char *s, **fp;
    ARGV_t files = NULL;
    const char *fileName;
    char buf[BUFSIZ];
    struct AttrRec_s arbuf;
    AttrRec specialDocAttrRec = &arbuf;
    char *specialDoc = NULL;

    nullAttrRec(specialDocAttrRec);
    pkg->cpioList = NULL;

    if (pkg->fileFile) {
	char *ffn;
	FILE *fd;

	/* XXX W2DO? urlPath might be useful here. */
	if (*pkg->fileFile == '/') {
	    ffn = rpmGetPath(pkg->fileFile, NULL);
	} else {
	    /* XXX FIXME: add %{buildsubdir} */
	    ffn = rpmGetPath("%{_builddir}/",
		(spec->buildSubdir ? spec->buildSubdir : "") ,
		"/", pkg->fileFile, NULL);
	}
	fd = fopen(ffn, "r");

	if (fd == NULL || ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("Could not open %%files file %s: %m\n"), ffn);
	    return RPMRC_FAIL;
	}
	ffn = _free(ffn);

	while (fgets(buf, sizeof(buf), fd)) {
	    handleComments(buf);
	    if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
		rpmlog(RPMLOG_ERR, _("line: %s\n"), buf);
		fclose(fd);
		return RPMRC_FAIL;
	    }
	    appendStringBuf(pkg->fileList, buf);
	}
	(void) fclose(fd);
    }
    
    /* Init the file list structure */
    memset(&fl, 0, sizeof(fl));

    /* XXX spec->buildRoot == NULL, then xstrdup("") is returned */
    fl.buildRoot = rpmGenPath(spec->rootDir, spec->buildRoot, NULL);

    if (hge(pkg->header, RPMTAG_DEFAULTPREFIX, NULL, (rpm_data_t *)&fl.prefix, NULL))
	fl.prefix = xstrdup(fl.prefix);
    else
	fl.prefix = NULL;

    fl.fileCount = 0;
    fl.totalFileSize = 0;
    fl.processingFailed = 0;

    fl.passedSpecialDoc = 0;
    fl.isSpecialDoc = 0;

    fl.isDir = 0;
    fl.inFtw = 0;
    fl.currentFlags = 0;
    fl.currentVerifyFlags = 0;
    
    fl.noGlob = 0;
    fl.devtype = 0;
    fl.devmajor = 0;
    fl.devminor = 0;

    nullAttrRec(&fl.cur_ar);
    nullAttrRec(&fl.def_ar);
    dupAttrRec(&root_ar, &fl.def_ar);	/* XXX assume %defattr(-,root,root) */

    fl.defVerifyFlags = RPMVERIFY_ALL;
    fl.nLangs = 0;
    fl.currentLangs = NULL;

    fl.currentSpecdFlags = 0;
    fl.defSpecdFlags = 0;

    fl.docDirs = NULL;
    {	char *docs = rpmGetPath("%{?__docdir_path}", NULL);
	argvSplit(&fl.docDirs, docs, ":");
	free(docs);
    }
    
    fl.fileList = NULL;
    fl.fileListRecsAlloced = 0;
    fl.fileListRecsUsed = 0;

    s = getStringBuf(pkg->fileList);
    argvSplit(&files, s, "\n");

    for (fp = files; *fp != NULL; fp++) {
	s = *fp;
	SKIPSPACE(s);
	if (*s == '\0')
	    continue;
	fileName = NULL;
	strcpy(buf, s);
	
	/* Reset for a new line in %files */
	fl.isDir = 0;
	fl.inFtw = 0;
	fl.currentFlags = 0;
	/* turn explicit flags into %def'd ones (gosh this is hacky...) */
	fl.currentSpecdFlags = ((unsigned)fl.defSpecdFlags) >> 8;
	fl.currentVerifyFlags = fl.defVerifyFlags;
	fl.isSpecialDoc = 0;

	fl.noGlob = 0;
 	fl.devtype = 0;
 	fl.devmajor = 0;
 	fl.devminor = 0;

	/* XXX should reset to %deflang value */
	if (fl.currentLangs) {
	    int i;
	    for (i = 0; i < fl.nLangs; i++)
		fl.currentLangs[i] = _free(fl.currentLangs[i]);
	    fl.currentLangs = _free(fl.currentLangs);
	}
  	fl.nLangs = 0;

	dupAttrRec(&fl.def_ar, &fl.cur_ar);

	if (parseForVerify(buf, &fl))
	    continue;
	if (parseForAttr(buf, &fl))
	    continue;
	if (parseForDev(buf, &fl))
	    continue;
	if (parseForConfig(buf, &fl))
	    continue;
	if (parseForLang(buf, &fl))
	    continue;
	if (parseForSimple(spec, pkg, buf, &fl, &fileName))
	    continue;
	if (fileName == NULL)
	    continue;

	if (fl.isSpecialDoc) {
	    /* Save this stuff for last */
	    specialDoc = _free(specialDoc);
	    specialDoc = xstrdup(fileName);
	    dupAttrRec(&fl.cur_ar, specialDocAttrRec);
	} else if (fl.currentFlags & RPMFILE_PUBKEY) {
	    (void) processMetadataFile(pkg, &fl, fileName, RPMTAG_PUBKEYS);
	} else if (fl.currentFlags & RPMFILE_POLICY) {
	    (void) processMetadataFile(pkg, &fl, fileName, RPMTAG_POLICIES);
	} else {
	    (void) processBinaryFile(pkg, &fl, fileName);
	}
    }

    /* Now process special doc, if there is one */
    if (specialDoc) {
	if (installSpecialDoc) {
	    int _missing_doc_files_terminate_build =
		    rpmExpandNumeric("%{?_missing_doc_files_terminate_build}");
	    rpmRC rc;

	    rc = doScript(spec, RPMBUILD_STRINGBUF, "%doc", pkg->specialDoc, test);
	    if (rc != RPMRC_OK && _missing_doc_files_terminate_build)
		fl.processingFailed = 1;
	}

	/* Reset for %doc */
	fl.isDir = 0;
	fl.inFtw = 0;
	fl.currentFlags = 0;
	fl.currentVerifyFlags = fl.defVerifyFlags;

	fl.noGlob = 0;
 	fl.devtype = 0;
 	fl.devmajor = 0;
 	fl.devminor = 0;

	/* XXX should reset to %deflang value */
	if (fl.currentLangs) {
	    int i;
	    for (i = 0; i < fl.nLangs; i++)
		fl.currentLangs[i] = _free(fl.currentLangs[i]);
	    fl.currentLangs = _free(fl.currentLangs);
	}
  	fl.nLangs = 0;

	dupAttrRec(specialDocAttrRec, &fl.cur_ar);
	freeAttrRec(specialDocAttrRec);

	(void) processBinaryFile(pkg, &fl, specialDoc);

	specialDoc = _free(specialDoc);
    }
    
    argvFree(files);

    if (fl.processingFailed)
	goto exit;

    /* Verify that file attributes scope over hardlinks correctly. */
    if (checkHardLinks(&fl))
	(void) rpmlibNeedsFeature(pkg->header,
			"PartialHardlinkSets", "4.0.4-1");

    genCpioListAndHeader(&fl, &pkg->cpioList, pkg->header, 0);

    if (spec->timeCheck)
	timeCheck(spec->timeCheck, pkg->header);
    
exit:
    fl.buildRoot = _free(fl.buildRoot);
    fl.prefix = _free(fl.prefix);

    freeAttrRec(&fl.cur_ar);
    freeAttrRec(&fl.def_ar);

    if (fl.currentLangs) {
	int i;
	for (i = 0; i < fl.nLangs; i++)
	    fl.currentLangs[i] = _free(fl.currentLangs[i]);
	fl.currentLangs = _free(fl.currentLangs);
    }

    fl.fileList = freeFileList(fl.fileList, fl.fileListRecsUsed);
    argvFree(fl.docDirs);
    return fl.processingFailed;
}

void initSourceHeader(rpmSpec spec)
{
    HeaderIterator hi;
    rpmTagType type;
    rpmTag tag;
    rpm_count_t count;
    rpm_data_t ptr;

    spec->sourceHeader = headerNew();
    /* Only specific tags are added to the source package header */
    for (hi = headerInitIterator(spec->packages->header);
	headerNextIterator(hi, &tag, &type, &ptr, &count);
	ptr = headerFreeData(ptr, type))
    {
	switch (tag) {
	case RPMTAG_NAME:
	case RPMTAG_VERSION:
	case RPMTAG_RELEASE:
	case RPMTAG_EPOCH:
	case RPMTAG_SUMMARY:
	case RPMTAG_DESCRIPTION:
	case RPMTAG_PACKAGER:
	case RPMTAG_DISTRIBUTION:
	case RPMTAG_DISTURL:
	case RPMTAG_VENDOR:
	case RPMTAG_LICENSE:
	case RPMTAG_GROUP:
	case RPMTAG_OS:
	case RPMTAG_ARCH:
	case RPMTAG_CHANGELOGTIME:
	case RPMTAG_CHANGELOGNAME:
	case RPMTAG_CHANGELOGTEXT:
	case RPMTAG_URL:
	case HEADER_I18NTABLE:
	    if (ptr)
		(void)headerAddEntry(spec->sourceHeader, tag, type, ptr, count);
	    break;
	default:
	    /* do not copy */
	    break;
	}
    }
    hi = headerFreeIterator(hi);

    /* Add the build restrictions */
    for (hi = headerInitIterator(spec->buildRestrictions);
	headerNextIterator(hi, &tag, &type, &ptr, &count);
	ptr = headerFreeData(ptr, type))
    {
	if (ptr)
	    (void) headerAddEntry(spec->sourceHeader, tag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);

    if (spec->BANames && spec->BACount > 0) {
	(void) headerAddEntry(spec->sourceHeader, RPMTAG_BUILDARCHS,
		       RPM_STRING_ARRAY_TYPE,
		       spec->BANames, (rpm_count_t) spec->BACount);
    }
}

int processSourceFiles(rpmSpec spec)
{
    struct Source *srcPtr;
    StringBuf sourceFiles;
    int x, isSpec = 1;
    struct FileList_s fl;
    char *s, **fp;
    ARGV_t files = NULL;
    Package pkg;
    static char *_srcdefattr;
    static int oneshot;

    if (!oneshot) {
	_srcdefattr = rpmExpand("%{?_srcdefattr}", NULL);
	if (_srcdefattr && !*_srcdefattr)
	    _srcdefattr = _free(_srcdefattr);
	oneshot = 1;
    }
    sourceFiles = newStringBuf();

    /* XXX
     * XXX This is where the source header for noarch packages needs
     * XXX to be initialized.
     */
    if (spec->sourceHeader == NULL)
	initSourceHeader(spec);

    /* Construct the file list and source entries */
    appendLineStringBuf(sourceFiles, spec->specFile);
    if (spec->sourceHeader != NULL)
    for (srcPtr = spec->sources; srcPtr != NULL; srcPtr = srcPtr->next) {
	if (srcPtr->flags & RPMBUILD_ISSOURCE) {
	    (void) headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_SOURCE,
				   RPM_STRING_ARRAY_TYPE, &srcPtr->source, 1);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		(void) headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_NOSOURCE,
				       RPM_INT32_TYPE, &srcPtr->num, 1);
	    }
	}
	if (srcPtr->flags & RPMBUILD_ISPATCH) {
	    (void) headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_PATCH,
				   RPM_STRING_ARRAY_TYPE, &srcPtr->source, 1);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		(void) headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_NOPATCH,
				       RPM_INT32_TYPE, &srcPtr->num, 1);
	    }
	}

      {	char * sfn;
	sfn = rpmGetPath( ((srcPtr->flags & RPMBUILD_ISNO) ? "!" : ""),
		"%{_sourcedir}/", srcPtr->source, NULL);
	appendLineStringBuf(sourceFiles, sfn);
	sfn = _free(sfn);
      }
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (srcPtr = pkg->icon; srcPtr != NULL; srcPtr = srcPtr->next) {
	    char * sfn;
	    sfn = rpmGetPath( ((srcPtr->flags & RPMBUILD_ISNO) ? "!" : ""),
		"%{_sourcedir}/", srcPtr->source, NULL);
	    appendLineStringBuf(sourceFiles, sfn);
	    sfn = _free(sfn);
	}
    }

    spec->sourceCpioList = NULL;

    /* Init the file list structure */
    memset(&fl, 0, sizeof(fl));
    if (_srcdefattr) {
	char *a = xmalloc(strlen(_srcdefattr) + 9 + 1);
	strcpy(a, "%defattr ");
	strcpy(a + 9, _srcdefattr);
	parseForAttr(a, &fl);
	a = _free(a);
    }
    fl.fileList = xcalloc((spec->numSources + 1), sizeof(*fl.fileList));
    fl.processingFailed = 0;
    fl.fileListRecsUsed = 0;
    fl.totalFileSize = 0;
    fl.prefix = NULL;
    fl.buildRoot = NULL;

    s = getStringBuf(sourceFiles);
    argvSplit(&files, s, "\n");

    /* The first source file is the spec file */
    x = 0;
    for (fp = files; *fp != NULL; fp++) {
	const char *diskPath;
	char *tmp;
	FileListRec flp;

	diskPath = *fp;
	SKIPSPACE(diskPath);
	if (! *diskPath)
	    continue;

	flp = &fl.fileList[x];

	flp->flags = isSpec ? RPMFILE_SPECFILE : 0;
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
	}

	if (fl.def_ar.ar_fmodestr) {
	    flp->fl_mode &= S_IFMT;
	    flp->fl_mode |= fl.def_ar.ar_fmode;
	}
	if (fl.def_ar.ar_user) {
	    flp->uname = getUnameS(fl.def_ar.ar_user);
	} else {
	    flp->uname = getUname(flp->fl_uid);
	}
	if (fl.def_ar.ar_group) {
	    flp->gname = getGnameS(fl.def_ar.ar_group);
	} else {
	    flp->gname = getGname(flp->fl_gid);
	}
	flp->langs = xstrdup("");
	
	fl.totalFileSize += flp->fl_size;
	
	if (! (flp->uname && flp->gname)) {
	    rpmlog(RPMLOG_ERR, _("Bad owner/group: %s\n"), diskPath);
	    fl.processingFailed = 1;
	}

	isSpec = 0;
	x++;
    }
    fl.fileListRecsUsed = x;
    argvFree(files);

    if (! fl.processingFailed) {
	if (spec->sourceHeader != NULL)
	    genCpioListAndHeader(&fl, &spec->sourceCpioList,
			spec->sourceHeader, 1);
    }

    sourceFiles = freeStringBuf(sourceFiles);
    fl.fileList = freeFileList(fl.fileList, fl.fileListRecsUsed);
    freeAttrRec(&fl.def_ar);
    return fl.processingFailed;
}

/**
 * Check packaged file list against what's in the build root.
 * @param fileList	packaged file list
 * @return		-1 if skipped, 0 on OK, 1 on error
 */
static int checkFiles(StringBuf fileList)
{
    static char * const av_ckfile[] = { "%{?__check_files}", NULL };
    StringBuf sb_stdout = NULL;
    char * s;
    int rc;
    
    s = rpmExpand(av_ckfile[0], NULL);
    if (!(s && *s)) {
	rc = -1;
	goto exit;
    }
    rc = 0;

    rpmlog(RPMLOG_NOTICE, _("Checking for unpackaged file(s): %s\n"), s);

    rc = rpmfcExec(av_ckfile, fileList, &sb_stdout, 0);
    if (rc < 0)
	goto exit;
    
    if (sb_stdout) {
	int _unpackaged_files_terminate_build =
		rpmExpandNumeric("%{?_unpackaged_files_terminate_build}");
	const char * t;

	t = getStringBuf(sb_stdout);
	if ((*t != '\0') && (*t != '\n')) {
	    rc = (_unpackaged_files_terminate_build) ? 1 : 0;
	    rpmlog((rc ? RPMLOG_ERR : RPMLOG_WARNING),
		_("Installed (but unpackaged) file(s) found:\n%s"), t);
	}
    }
    
exit:
    sb_stdout = freeStringBuf(sb_stdout);
    s = _free(s);
    return rc;
}

int processBinaryFiles(rpmSpec spec, int installSpecialDoc, int test)
{
    Package pkg;
    int res = 0;
    
    check_fileList = newStringBuf();
    
    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	const char *n, *v, *r;
	int rc;

	if (pkg->fileList == NULL)
	    continue;

	(void) headerNVR(pkg->header, &n, &v, &r);
	rpmlog(RPMLOG_NOTICE, _("Processing files: %s-%s-%s\n"), n, v, r);
		   
	if ((rc = processPackageFiles(spec, pkg, installSpecialDoc, test)))
	    res = rc;

	if ((rc = rpmfcGenerateDepends(spec, pkg)))
	    res = rc;
    }

    /* Now we have in fileList list of files from all packages.
     * We pass it to a script which does the work of finding missing
     * and duplicated files.
     */
    
    
    if (checkFiles(check_fileList) > 0) {
	if (res == 0)
	    res = 1;
    }
    
    check_fileList = freeStringBuf(check_fileList);
    
    return res;
}
