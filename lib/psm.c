/** \ingroup rpmts payload
 * \file lib/psm.c
 * Package state machine to handle a package from a transaction set.
 */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmvercmp and others */
#include <rpm/rpmmacro.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTempFile() */
#include <rpm/rpmdb.h>		/* XXX for db_chrootDone */
#include <rpm/rpmlog.h>

#include "rpmio/rpmlua.h"
#include "lib/cpio.h"
#include "lib/fsm.h"		/* XXX CPIO_FOO/FSM_FOO constants */
#include "lib/psm.h"
#include "lib/rpmfi_internal.h" /* XXX replaced/states, fi->hge, fi->te... */
#include "lib/rpmte_internal.h"	/* XXX te->fd */
#include "lib/rpmtsscore.h"
#include "lib/rpmlead.h"		/* writeLead proto */
#include "lib/signature.h"		/* signature constants */
#include "lib/misc.h"		/* XXX rpmMkdirPath, doputenv */

#include "debug.h"

#define	_PSM_DEBUG	0
int _psm_debug = _PSM_DEBUG;
int _psm_threads = 0;

/* Give access to the rpmte global tracking the last instance added
 * to the database.
 */
extern unsigned int myinstall_instance;

/**
 */
struct rpmpsm_s {
    struct rpmsqElem sq;	/*!< Scriptlet/signal queue element. */

    rpmts ts;			/*!< transaction set */
    rpmte te;			/*!< current transaction element */
    rpmfi fi;			/*!< transaction element file info */
    FD_t cfd;			/*!< Payload file handle. */
    FD_t fd;			/*!< Repackage file handle. */
    Header oh;			/*!< Repackage header. */
    rpmdbMatchIterator mi;
    const char * stepName;
    char * rpmio_flags;
    char * failedFile;
    char * pkgURL;		/*!< Repackage URL. */
    const char * pkgfn;		/*!< Repackage file name. */
    int scriptTag;		/*!< Scriptlet data tag. */
    int progTag;		/*!< Scriptlet interpreter tag. */
    int npkgs_installed;	/*!< No. of installed instances. */
    int scriptArg;		/*!< Scriptlet package arg. */
    rpmsenseFlags sense;	/*!< One of RPMSENSE_TRIGGER{PREIN,IN,UN,POSTUN}. */
    int countCorrection;	/*!< 0 if installing, -1 if removing. */
    int chrootDone;		/*!< Was chroot(2) done by pkgStage? */
    int unorderedSuccessor;	/*!< Can the PSM be run asynchronously? */
    rpmCallbackType what;	/*!< Callback type. */
    rpm_off_t amount;		/*!< Callback amount. */
    rpm_off_t total;		/*!< Callback total. */
    rpmRC rc;
    pkgStage goal;
    pkgStage stage;		/*!< Current psm stage. */
    pkgStage nstage;		/*!< Next psm stage. */

    int nrefs;			/*!< Reference count. */
};

int rpmVersionCompare(Header first, Header second)
{
    const char * one, * two;
    int32_t * epochOne, * epochTwo;
    static int32_t zero = 0;
    int rc;

    if (!headerGetEntry(first, RPMTAG_EPOCH, NULL, (rpm_data_t *) &epochOne, NULL))
	epochOne = &zero;
    if (!headerGetEntry(second, RPMTAG_EPOCH, NULL, (rpm_data_t *) &epochTwo, NULL))
	epochTwo = &zero;

	if (*epochOne < *epochTwo)
	    return -1;
	else if (*epochOne > *epochTwo)
	    return 1;

    rc = headerGetEntry(first, RPMTAG_VERSION, NULL, (rpm_data_t *) &one, NULL);
    rc = headerGetEntry(second, RPMTAG_VERSION, NULL, (rpm_data_t *) &two, NULL);

    rc = rpmvercmp(one, two);
    if (rc)
	return rc;

    rc = headerGetEntry(first, RPMTAG_RELEASE, NULL, (rpm_data_t *) &one, NULL);
    rc = headerGetEntry(second, RPMTAG_RELEASE, NULL, (rpm_data_t *) &two, NULL);

    return rpmvercmp(one, two);
}

/**
 * Macros to be defined from per-header tag values.
 * @todo Should other macros be added from header when installing a package?
 */
static struct tagMacro {
const char *	macroname; /*!< Macro name to define. */
    rpm_tag_t	tag;		/*!< Header tag to use for value. */
} tagMacros[] = {
    { "name",		RPMTAG_NAME },
    { "version",	RPMTAG_VERSION },
    { "release",	RPMTAG_RELEASE },
    { "epoch",		RPMTAG_EPOCH },
    { NULL, 0 }
};

/**
 * Define per-header macros.
 * @param fi		transaction element file info
 * @param h		header
 * @return		0 always
 */
static int rpmInstallLoadMacros(rpmfi fi, Header h)
{
    HGE_t hge = (HGE_t) fi->hge;
    struct tagMacro * tagm;
    union {
void * ptr;
const char ** argv;
	const char * str;
	int32_t * i32p;
    } body;
    char numbuf[32];
    rpm_tagtype_t type;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	if (!hge(h, tagm->tag, &type, (rpm_data_t *) &body, NULL))
	    continue;
	switch (type) {
	case RPM_INT32_TYPE:
	    sprintf(numbuf, "%d", *body.i32p);
	    addMacro(NULL, tagm->macroname, NULL, numbuf, -1);
	    break;
	case RPM_STRING_TYPE:
	    addMacro(NULL, tagm->macroname, NULL, body.str, -1);
	    break;
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	case RPM_INT16_TYPE:
	case RPM_BIN_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	default:
	    break;
	}
    }
    return 0;
}

/**
 * Mark files in database shared with this package as "replaced".
 * @param psm		package state machine data
 * @return		0 always
 */
static rpmRC markReplacedFiles(const rpmpsm psm)
{
    const rpmts ts = psm->ts;
    rpmfi fi = psm->fi;
    HGE_t hge = (HGE_t)fi->hge;
    sharedFileInfo replaced = fi->replaced;
    sharedFileInfo sfi;
    rpmdbMatchIterator mi;
    Header h;
    int * offsets;
    unsigned int prev;
    int num, xx;

    if (!(rpmfiFC(fi) > 0 && fi->replaced))
	return RPMRC_OK;

    num = prev = 0;
    for (sfi = replaced; sfi->otherPkg; sfi++) {
	if (prev && prev == sfi->otherPkg)
	    continue;
	prev = sfi->otherPkg;
	num++;
    }
    if (num == 0)
	return RPMRC_OK;

    offsets = alloca(num * sizeof(*offsets));
    offsets[0] = 0;
    num = prev = 0;
    for (sfi = replaced; sfi->otherPkg; sfi++) {
	if (prev && prev == sfi->otherPkg)
	    continue;
	prev = sfi->otherPkg;
	offsets[num++] = sfi->otherPkg;
    }

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
    xx = rpmdbAppendIterator(mi, offsets, num);
    xx = rpmdbSetIteratorRewrite(mi, 1);

    sfi = replaced;
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * secStates;
	int modified;
	rpm_count_t count;

	modified = 0;

	if (!hge(h, RPMTAG_FILESTATES, NULL, (rpm_data_t *)&secStates, &count))
	    continue;
	
	prev = rpmdbGetIteratorOffset(mi);
	num = 0;
	while (sfi->otherPkg && sfi->otherPkg == prev) {
	    assert(sfi->otherFileNum < count);
	    if (secStates[sfi->otherFileNum] != RPMFILE_STATE_REPLACED) {
		secStates[sfi->otherFileNum] = RPMFILE_STATE_REPLACED;
		if (modified == 0) {
		    /* Modified header will be rewritten. */
		    modified = 1;
		    xx = rpmdbSetIteratorModified(mi, modified);
		}
		num++;
	    }
	    sfi++;
	}
    }
    mi = rpmdbFreeIterator(mi);

    return RPMRC_OK;
}

rpmRC rpmInstallSourcePackage(rpmts ts, FD_t fd,
		char ** specFilePtr, char ** cookie)
{
    int scareMem = 1;
    rpmfi fi = NULL;
    char * _sourcedir = NULL;
    char * _specdir = NULL;
    char * specFile = NULL;
    HGE_t hge;
    HFD_t hfd;
    Header h = NULL;
    struct rpmpsm_s psmbuf;
    rpmpsm psm = &psmbuf;
    int isSource;
    rpmRC rpmrc;
    int i;

    memset(psm, 0, sizeof(*psm));
    psm->ts = rpmtsLink(ts, RPMDBG_M("InstallSourcePackage"));

    rpmrc = rpmReadPackageFile(ts, fd, RPMDBG_M("InstallSourcePackage"), &h);
    switch (rpmrc) {
    case RPMRC_NOTTRUSTED:
    case RPMRC_NOKEY:
    case RPMRC_OK:
	break;
    default:
	goto exit;
	break;
    }
    if (h == NULL)
	goto exit;

    rpmrc = RPMRC_OK;

    isSource = headerIsSource(h);

    if (!isSource) {
	rpmlog(RPMLOG_ERR, _("source package expected, binary found\n"));
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    if (rpmtsAddInstallElement(ts, h, NULL, 0, NULL)) {
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    h = headerFree(h);

    if (fi == NULL) {	/* XXX can't happen */
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    fi->te = rpmtsElement(ts, 0);
    if (fi->te == NULL) {	/* XXX can't happen */
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    rpmteSetHeader(fi->te, fi->h);
    fi->te->fd = fdLink(fd, RPMDBG_M("installSourcePackage"));
    hge = fi->hge;
    hfd = fi->hfd;

(void) rpmInstallLoadMacros(fi, fi->h);

    psm->fi = rpmfiLink(fi, RPMDBG_M("rpmInstallLoadMacros"));
    psm->te = fi->te;

    if (cookie) {
	*cookie = NULL;
	if (hge(fi->h, RPMTAG_COOKIE, NULL, (rpm_data_t *) cookie, NULL))
	    *cookie = xstrdup(*cookie);
    }

    /* XXX FIXME: can't do endian neutral MD5 verification yet. */
fi->fmd5s = hfd(fi->fmd5s, RPM_FORCEFREE_TYPE);

    /* XXX FIXME: don't do per-file mapping, force global flags. */
    fi->fmapflags = _free(fi->fmapflags);
    fi->mapflags = CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

    fi->uid = getuid();
    fi->gid = getgid();
    fi->astriplen = 0;
    fi->striplen = 0;

    for (i = 0; i < fi->fc; i++)
	fi->actions[i] = FA_CREATE;

    i = fi->fc;

    if (fi->h != NULL) {	/* XXX can't happen */
	rpmfiBuildFNames(fi->h, RPMTAG_BASENAMES, &fi->apath, NULL);

	if (headerIsEntry(fi->h, RPMTAG_COOKIE))
	    for (i = 0; i < fi->fc; i++)
		if (fi->fflags[i] & RPMFILE_SPECFILE) break;
    }

    if (i == fi->fc) {
	/* Find the spec file by name. */
	for (i = 0; i < fi->fc; i++) {
	    const char * t = fi->apath[i];
	    t += strlen(fi->apath[i]) - 5;
	    if (!strcmp(t, ".spec")) break;
	}
    }

    _sourcedir = rpmGenPath(rpmtsRootDir(ts), "%{_sourcedir}", "");
    rpmrc = rpmMkdirPath(_sourcedir, "sourcedir");
    if (rpmrc) {
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    _specdir = rpmGenPath(rpmtsRootDir(ts), "%{_specdir}", "");
    rpmrc = rpmMkdirPath(_specdir, "specdir");
    if (rpmrc) {
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    /* Build dnl/dil with {_sourcedir, _specdir} as values. */
    if (i < fi->fc) {
	size_t speclen = strlen(_specdir) + 2;
	size_t sourcelen = strlen(_sourcedir) + 2;
	char * t;

	fi->dnl = hfd(fi->dnl, RPM_FORCEFREE_TYPE);

	fi->dc = 2;
	fi->dnl = xmalloc(fi->dc * sizeof(*fi->dnl)
			+ fi->fc * sizeof(*fi->dil)
			+ speclen + sourcelen);
	fi->dil = (unsigned int *)(fi->dnl + fi->dc);
	memset(fi->dil, 0, fi->fc * sizeof(*fi->dil));
	fi->dil[i] = 1;
	fi->dnl[0] = t = (char *)(fi->dil + fi->fc);
	fi->dnl[1] = t = stpcpy( stpcpy(t, _sourcedir), "/") + 1;
	(void) stpcpy( stpcpy(t, _specdir), "/");

	t = xmalloc(speclen + strlen(fi->bnl[i]) + 1);
	(void) stpcpy( stpcpy( stpcpy(t, _specdir), "/"), fi->bnl[i]);
	specFile = t;
    } else {
	rpmlog(RPMLOG_ERR, _("source package contains no .spec file\n"));
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    psm->goal = PSM_PKGINSTALL;

   	/* FIX: psm->fi->dnl should be owned. */
    rpmrc = rpmpsmStage(psm, PSM_PROCESS);

    (void) rpmpsmStage(psm, PSM_FINI);

    if (rpmrc) rpmrc = RPMRC_FAIL;

exit:
    if (specFilePtr && specFile && rpmrc == RPMRC_OK)
	*specFilePtr = specFile;
    else
	specFile = _free(specFile);

    _specdir = _free(_specdir);
    _sourcedir = _free(_sourcedir);

    psm->fi = rpmfiFree(psm->fi);
    psm->te = NULL;

    if (h != NULL) h = headerFree(h);

    if (fi != NULL) {
	rpmteSetHeader(fi->te, NULL);
	if (fi->te->fd != NULL)
	    (void) Fclose(fi->te->fd);
	fi->te->fd = NULL;
	fi->te = NULL;
	fi = rpmfiFree(fi);
    }

    /* XXX nuke the added package(s). */
    rpmtsClean(ts);

    psm->ts = rpmtsFree(psm->ts);

    return rpmrc;
}

static const char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

/**
 * Return scriptlet name from tag.
 * @param tag		scriptlet tag
 * @return		name of scriptlet
 */
static const char * tag2sln(rpm_tag_t tag)
{
    switch (tag) {
    case RPMTAG_PRETRANS:       return "%pretrans";
    case RPMTAG_TRIGGERPREIN:   return "%triggerprein";
    case RPMTAG_PREIN:          return "%pre";
    case RPMTAG_POSTIN:         return "%post";
    case RPMTAG_TRIGGERIN:      return "%triggerin";
    case RPMTAG_TRIGGERUN:      return "%triggerun";
    case RPMTAG_PREUN:          return "%preun";
    case RPMTAG_POSTUN:         return "%postun";
    case RPMTAG_POSTTRANS:      return "%posttrans";
    case RPMTAG_TRIGGERPOSTUN:  return "%triggerpostun";
    case RPMTAG_VERIFYSCRIPT:   return "%verify";
    }
    return "%unknownscript";
}

static rpm_tag_t triggertag(rpmsenseFlags sense) 
{
    rpm_tag_t tag = RPMTAG_NOT_FOUND;
    switch (sense) {
    case RPMSENSE_TRIGGERIN:
	tag = RPMTAG_TRIGGERIN;
	break;
    case RPMSENSE_TRIGGERUN:
	tag = RPMTAG_TRIGGERUN;
	break;
    case RPMSENSE_TRIGGERPOSTUN:
	tag = RPMTAG_TRIGGERPOSTUN;
	break;
    case RPMSENSE_TRIGGERPREIN:
	tag = RPMTAG_TRIGGERPREIN;
	break;
    default:
	break;
    }
    return tag;
}

/**
 * Wait for child process to be reaped.
 * @param psm		package state machine data
 * @return		
 */
static pid_t psmWait(rpmpsm psm)
{
    const rpmts ts = psm->ts;
    rpmtime_t msecs;

    (void) rpmsqWait(&psm->sq);
    msecs = psm->sq.op.usecs/1000;
    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_SCRIPTLETS), &psm->sq.op);

    rpmlog(RPMLOG_DEBUG,
	_("%s: waitpid(%d) rc %d status %x secs %u.%03u\n"),
	psm->stepName, (unsigned)psm->sq.child,
	(unsigned)psm->sq.reaped, psm->sq.status,
	(unsigned)msecs/1000, (unsigned)msecs%1000);

    return psm->sq.reaped;
}

#ifdef WITH_LUA
/**
 * Run internal Lua script.
 */
static rpmRC runLuaScript(rpmpsm psm, Header h, rpm_tag_t stag,
		   unsigned int progArgc, const char **progArgv,
		   const char *script, int arg1, int arg2)
{
    const rpmts ts = psm->ts;
    const char * sln = tag2sln(stag);
    int rootFd = -1;
    const char *n, *v, *r;
    rpmRC rc = RPMRC_OK;
    int i;
    int xx;
    rpmlua lua = NULL; /* Global state. */
    rpmluav var;

    xx = headerNVR(h, &n, &v, &r);

    if (!rpmtsChrootDone(ts)) {
	const char *rootDir = rpmtsRootDir(ts);
	xx = chdir("/");
	rootFd = open(".", O_RDONLY, 0);
	if (rootFd >= 0) {
	    if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
		xx = chroot(rootDir);
	    xx = rpmtsSetChrootDone(ts, 1);
	}
    }

    /* Create arg variable */
    rpmluaPushTable(lua, "arg");
    var = rpmluavNew();
    rpmluavSetListMode(var, 1);
    if (progArgv) {
	for (i = 0; i < progArgc && progArgv[i]; i++) {
	    rpmluavSetValue(var, RPMLUAV_STRING, progArgv[i]);
	    rpmluaSetVar(lua, var);
	}
    }
    if (arg1 >= 0) {
	rpmluavSetValueNum(var, arg1);
	rpmluaSetVar(lua, var);
    }
    if (arg2 >= 0) {
	rpmluavSetValueNum(var, arg2);
	rpmluaSetVar(lua, var);
    }
    var = rpmluavFree(var);
    rpmluaPop(lua);

    {
	char buf[BUFSIZ];
	xx = snprintf(buf, BUFSIZ, "%s(%s-%s-%s)", sln, n, v, r);
	if (rpmluaRunScript(lua, script, buf) == -1) {
	    void * ptr;
	    ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR, stag, 1);
	    rc = RPMRC_FAIL;
	}
    }

    rpmluaDelVar(lua, "arg");

    if (rootFd >= 0) {
	const char *rootDir = rpmtsRootDir(ts);
	xx = fchdir(rootFd);
	xx = close(rootFd);
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
	    xx = chroot(".");
	xx = rpmtsSetChrootDone(ts, 0);
    }

    return rc;
}
#endif

/**
 */
static int ldconfig_done = 0;

static const char * ldconfig_path = "/sbin/ldconfig";

/**
 * Run scriptlet with args.
 *
 * Run a script with an interpreter. If the interpreter is not specified,
 * /bin/sh will be used. If the interpreter is /bin/sh, then the args from
 * the header will be ignored, passing instead arg1 and arg2.
 *
 * @param psm		package state machine data
 * @param h		header
 * @param stag		scriptlet section tag
 * @param progArgc	no. of args from header
 * @param progArgv	args from header, progArgv[0] is the interpreter to use
 * @param script	scriptlet from header
 * @param arg1		no. instances of package installed after scriptlet exec
 *			(-1 is no arg)
 * @param arg2		ditto, but for the target package
 * @return		0 on success
 */
static rpmRC runScript(rpmpsm psm, Header h, rpm_tag_t stag,
		unsigned int progArgc, const char ** progArgv,
		const char * script, int arg1, int arg2)
{
    const rpmts ts = psm->ts;
    rpmfi fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    const char ** argv = NULL;
    int argc = 0;
    const char ** prefixes = NULL;
    rpm_count_t numPrefixes;
    rpm_tagtype_t ipt;
    const char * oldPrefix;
    size_t maxPrefixLength;
    size_t len;
    char * prefixBuf = NULL;
    char * fn = NULL;
    int xx;
    int i;
    int freePrefixes = 0;
    FD_t scriptFd;
    FD_t out;
    rpmRC rc = RPMRC_OK;
    const char *n, *v, *r, *a;
    const char *sln = tag2sln(stag);

    if (progArgv == NULL && script == NULL)
	return rc;

    /* XXX FIXME: except for %verifyscript, rpmteNEVR can be used. */
    xx = headerNVR(h, &n, &v, &r);
    xx = hge(h, RPMTAG_ARCH, NULL, (rpm_data_t *) &a, NULL);

    if (progArgv && strcmp(progArgv[0], "<lua>") == 0) {
#ifdef WITH_LUA
	rpmlog(RPMLOG_DEBUG,
		_("%s: %s(%s-%s-%s.%s) running <lua> scriptlet.\n"),
		psm->stepName, sln, n, v, r, a);
	return runLuaScript(psm, h, stag, progArgc, progArgv,
			    script, arg1, arg2);
#else
	return RPMRC_FAIL;
#endif
    }

    psm->sq.reaper = 1;

    /*
     * If a successor node, and ldconfig was just run, don't bother.
     */
    if (ldconfig_path && progArgv != NULL && psm->unorderedSuccessor) {
 	if (ldconfig_done && !strcmp(progArgv[0], ldconfig_path)) {
	    rpmlog(RPMLOG_DEBUG,
		_("%s: %s(%s-%s-%s.%s) skipping redundant \"%s\".\n"),
		psm->stepName, sln, n, v, r, a,
		progArgv[0]);
	    return rc;
	}
    }

    rpmlog(RPMLOG_DEBUG,
		_("%s: %s(%s-%s-%s.%s) %ssynchronous scriptlet start\n"),
		psm->stepName, sln, n, v, r, a,
		(psm->unorderedSuccessor ? "a" : ""));

    if (!progArgv) {
	argv = alloca(5 * sizeof(*argv));
	argv[0] = "/bin/sh";
	argc = 1;
	ldconfig_done = 0;
    } else {
	argv = alloca((progArgc + 4) * sizeof(*argv));
	memcpy(argv, progArgv, progArgc * sizeof(*argv));
	argc = progArgc;
	ldconfig_done = (ldconfig_path && !strcmp(argv[0], ldconfig_path)
		? 1 : 0);
    }

#if __ia64__
    /* XXX This assumes that all interpreters are elf executables. */
    if ((a != NULL && a[0] == 'i' && a[1] != '\0' && a[2] == '8' && a[3] == '6')
     && strcmp(argv[0], "/sbin/ldconfig"))
    {
	char * fmt = rpmGetPath("%{?_autorelocate_path}", NULL);
	const char * errstr;
	char * newPath;
	char * t;

	newPath = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	fmt = _free(fmt);

	/* XXX On ia64, change leading /emul/ix86 -> /emul/ia32, ick. */
 	if (newPath != NULL && *newPath != '\0'
	 && strlen(newPath) >= (sizeof("/emul/i386")-1)
	 && newPath[0] == '/' && newPath[1] == 'e' && newPath[2] == 'm'
	 && newPath[3] == 'u' && newPath[4] == 'l' && newPath[5] == '/'
	 && newPath[6] == 'i' && newPath[8] == '8' && newPath[9] == '6')
 	{
	    newPath[7] = 'a';
	    newPath[8] = '3';
	    newPath[9] = '2';
	}

	t = alloca(strlen(newPath) + strlen(argv[0]) + 1);
	*t = '\0';
	(void) stpcpy( stpcpy(t, newPath), argv[0]);
	newPath = _free(newPath);
	argv[0] = t;
    }
#endif

    if (hge(h, RPMTAG_INSTPREFIXES, &ipt, (rpm_data_t *) &prefixes, &numPrefixes)) {
	freePrefixes = 1;
    } else if (hge(h, RPMTAG_INSTALLPREFIX, NULL, (rpm_data_t *) &oldPrefix, NULL)) {
	prefixes = &oldPrefix;
	numPrefixes = 1;
    } else {
	numPrefixes = 0;
    }

    maxPrefixLength = 0;
    if (prefixes != NULL)
    for (i = 0; i < numPrefixes; i++) {
	len = strlen(prefixes[i]);
	if (len > maxPrefixLength) maxPrefixLength = len;
    }
    prefixBuf = alloca(maxPrefixLength + 50);

    if (script) {
	const char * rootDir = rpmtsRootDir(ts);
	FD_t fd;

	if (rpmMkTempFile((!rpmtsChrootDone(ts) ? rootDir : "/"), &fn, &fd)) {
	    if (prefixes != NULL && freePrefixes) free(prefixes);
	    return RPMRC_FAIL;
	}

	if (rpmIsDebug() &&
	    (!strcmp(argv[0], "/bin/sh") || !strcmp(argv[0], "/bin/bash")))
	{
	    static const char set_x[] = "set -x\n";
	    xx = Fwrite(set_x, sizeof(set_x[0]), sizeof(set_x)-1, fd);
	}

	if (ldconfig_path && strstr(script, ldconfig_path) != NULL)
	    ldconfig_done = 1;

	xx = Fwrite(script, sizeof(script[0]), strlen(script), fd);
	xx = Fclose(fd);

	{   const char * sn = fn;
	    if (!rpmtsChrootDone(ts) && rootDir != NULL &&
		!(rootDir[0] == '/' && rootDir[1] == '\0'))
	    {
		sn += strlen(rootDir)-1;
	    }
	    argv[argc++] = sn;
	}

	if (arg1 >= 0) {
	    char *av = alloca(20);
	    sprintf(av, "%d", arg1);
	    argv[argc++] = av;
	}
	if (arg2 >= 0) {
	    char *av = alloca(20);
	    sprintf(av, "%d", arg2);
	    argv[argc++] = av;
	}
    }

    argv[argc] = NULL;

    scriptFd = rpmtsScriptFd(ts);
    if (scriptFd != NULL) {
	if (rpmIsVerbose()) {
	    out = fdDup(Fileno(scriptFd));
	} else {
	    out = Fopen("/dev/null", "w.fdio");
	    if (Ferror(out)) {
		out = fdDup(Fileno(scriptFd));
	    }
	}
    } else {
	out = fdDup(STDOUT_FILENO);
    }
    if (out == NULL) return RPMRC_FAIL;	/* XXX can't happen */

    xx = rpmsqFork(&psm->sq);
    if (psm->sq.child == 0) {
	const char * rootDir;
	int pipes[2];
	int flag;
	int fdno;

	pipes[0] = pipes[1] = 0;
	/* make stdin inaccessible */
	xx = pipe(pipes);
	xx = close(pipes[1]);
	xx = dup2(pipes[0], STDIN_FILENO);
	xx = close(pipes[0]);

	/* XXX Force FD_CLOEXEC on 1st 100 inherited fdno's. */
	for (fdno = 3; fdno < 100; fdno++) {
	    flag = fcntl(fdno, F_GETFD);
	    if (flag == -1 || (flag & FD_CLOEXEC))
		continue;
	    xx = fcntl(fdno, F_SETFD, FD_CLOEXEC);
	    /* XXX W2DO? debug msg for inheirited fdno w/o FD_CLOEXEC */
	}

	if (scriptFd != NULL) {
	    int sfdno = Fileno(scriptFd);
	    int ofdno = Fileno(out);
	    if (sfdno != STDERR_FILENO)
		xx = dup2(sfdno, STDERR_FILENO);
	    if (ofdno != STDOUT_FILENO)
		xx = dup2(ofdno, STDOUT_FILENO);
	    /* make sure we don't close stdin/stderr/stdout by mistake! */
	    if (ofdno > STDERR_FILENO && ofdno != sfdno)
		xx = Fclose (out);
	    if (sfdno > STDERR_FILENO && ofdno != sfdno)
		xx = Fclose (scriptFd);
	}

	{   char *ipath = rpmExpand("PATH=%{_install_script_path}", NULL);
	    const char *path = SCRIPT_PATH;

	    if (ipath && ipath[5] != '%')
		path = ipath;

	    xx = doputenv(path);
	    ipath = _free(ipath);
	}

	if (prefixes != NULL)
	for (i = 0; i < numPrefixes; i++) {
	    sprintf(prefixBuf, "RPM_INSTALL_PREFIX%d=%s", i, prefixes[i]);
	    xx = doputenv(prefixBuf);

	    /* backwards compatibility */
	    if (i == 0) {
		sprintf(prefixBuf, "RPM_INSTALL_PREFIX=%s", prefixes[i]);
		xx = doputenv(prefixBuf);
	    }
	}

	rootDir = rpmtsRootDir(ts);
	if (rootDir  != NULL) {	/* XXX can't happen */
	    if (!rpmtsChrootDone(ts) &&
		!(rootDir[0] == '/' && rootDir[1] == '\0'))
	    {
		xx = chroot(rootDir);
	    }
	    xx = chdir("/");
	    rpmlog(RPMLOG_DEBUG, _("%s: %s(%s-%s-%s.%s)\texecv(%s) pid %d\n"),
			psm->stepName, sln, n, v, r, a,
			argv[0], (unsigned)getpid());

	    /* XXX Don't mtrace into children. */
	    unsetenv("MALLOC_CHECK_");

	    /* Permit libselinux to do the scriptlet exec. */
	    if (rpmtsSELinuxEnabled(ts) == 1) {	
		xx = rpm_execcon(0, argv[0], (char ** const) argv, environ);
	    }

	    if (xx == 0) {
	    	xx = execv(argv[0], (char *const *)argv);
	    }
	}
 	_exit(-1);
    }

    if (psm->sq.child == (pid_t)-1) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"), sln, strerror(errno));
	rc = RPMRC_FAIL;
	goto exit;
    }

    (void) psmWait(psm);

  /* XXX filter order dependent multilib "other" arch helper error. */
  if (!(psm->sq.reaped >= 0 && !strcmp(argv[0], "/usr/sbin/glibc_post_upgrade") && WEXITSTATUS(psm->sq.status) == 110)) {
    void *ptr = NULL;
    if (psm->sq.reaped < 0) {
	ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				 stag, WTERMSIG(psm->sq.child));
	rpmlog(RPMLOG_ERR,
		_("%s(%s-%s-%s.%s) scriptlet failed, waitpid(%d) rc %d: %s\n"),
		 sln, n, v, r, a, psm->sq.child, psm->sq.reaped, strerror(errno));
	rc = RPMRC_FAIL;
    } else
    if (!WIFEXITED(psm->sq.status) || WEXITSTATUS(psm->sq.status)) {
      if (WIFSIGNALED(psm->sq.status)) {
	ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				 stag, WTERMSIG(psm->sq.status));
        rpmlog(RPMLOG_ERR,
                 _("%s(%s-%s-%s.%s) scriptlet failed, signal %d\n"),
                 sln, n, v, r, a, WTERMSIG(psm->sq.status));
      } else {
	ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				 stag, WEXITSTATUS(psm->sq.status));
	rpmlog(RPMLOG_ERR,
		_("%s(%s-%s-%s.%s) scriptlet failed, exit status %d\n"),
		sln, n, v, r, a, WEXITSTATUS(psm->sq.status));
      }
	rc = RPMRC_FAIL;
    }
  }

exit:
    if (freePrefixes) prefixes = hfd(prefixes, ipt);

    xx = Fclose(out);	/* XXX dup'd STDOUT_FILENO */

    if (script) {
	if (!rpmIsDebug())
	    xx = unlink(fn);
	fn = _free(fn);
    }

    return rc;
}

/**
 * Retrieve and run scriptlet from header.
 * @param psm		package state machine data
 * @return		rpmRC return code
 */
static rpmRC runInstScript(rpmpsm psm)
{
    rpmfi fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpm_data_t * progArgv;
    rpm_count_t progArgc;
    const char ** argv;
    rpm_tagtype_t ptt, stt;
    char * script;
    rpmRC rc = RPMRC_OK;
    int xx;

    /*
     * headerGetEntry() sets the data pointer to NULL if the entry does
     * not exist.
     */
    xx = hge(fi->h, psm->scriptTag, &stt, (rpm_data_t *) &script, NULL);
    xx = hge(fi->h, psm->progTag, &ptt, (rpm_data_t *) &progArgv, &progArgc);
    if (progArgv == NULL && script == NULL)
	goto exit;

    if (progArgv && ptt == RPM_STRING_TYPE) {
	argv = alloca(sizeof(*argv));
	*argv = (const char *) progArgv;
    } else {
	argv = (const char **) progArgv;
    }

    if (fi->h != NULL)	/* XXX can't happen */
    rc = runScript(psm, fi->h, psm->scriptTag, progArgc, argv,
		script, psm->scriptArg, -1);

exit:
    progArgv = hfd(progArgv, ptt);
    script = hfd(script, stt);
    return rc;
}

/**
 * Execute triggers.
 * @todo Trigger on any provides, not just package NVR.
 * @param psm		package state machine data
 * @param sourceH
 * @param triggeredH
 * @param arg2
 * @param triggersAlreadyRun
 * @return
 */
static rpmRC handleOneTrigger(const rpmpsm psm,
			Header sourceH, Header triggeredH,
			int arg2, unsigned char * triggersAlreadyRun)
{
    int scareMem = 1;
    const rpmts ts = psm->ts;
    rpmfi fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpmds trigger = NULL;
    const char ** triggerScripts;
    const char ** triggerProgs;
    int32_t * triggerIndices;
    const char * sourceName;
    const char * triggerName;
    rpmRC rc = RPMRC_OK;
    int xx;
    int i;

    xx = headerNVR(sourceH, &sourceName, NULL, NULL);
    xx = headerNVR(triggeredH, &triggerName, NULL, NULL);

    trigger = rpmdsInit(rpmdsNew(triggeredH, RPMTAG_TRIGGERNAME, scareMem));
    if (trigger == NULL)
	return rc;

    (void) rpmdsSetNoPromote(trigger, 1);

    while ((i = rpmdsNext(trigger)) >= 0) {
	rpm_tagtype_t tit, tst, tpt;
	const char * Name;
	rpmsenseFlags Flags = rpmdsFlags(trigger);

	if ((Name = rpmdsN(trigger)) == NULL)
	    continue;   /* XXX can't happen */

	if (strcmp(Name, sourceName))
	    continue;
	if (!(Flags & psm->sense))
	    continue;

	/*
	 * XXX Trigger on any provided dependency, not just the package NEVR.
	 */
	if (!rpmdsAnyMatchesDep(sourceH, trigger, 1))
	    continue;

	if (!(	hge(triggeredH, RPMTAG_TRIGGERINDEX, &tit,
		       (rpm_data_t *) &triggerIndices, NULL) &&
		hge(triggeredH, RPMTAG_TRIGGERSCRIPTS, &tst,
		       (rpm_data_t *) &triggerScripts, NULL) &&
		hge(triggeredH, RPMTAG_TRIGGERSCRIPTPROG, &tpt,
		       (rpm_data_t *) &triggerProgs, NULL))
	    )
	    continue;

	{   int arg1;
	    int index;

	    arg1 = rpmdbCountPackages(rpmtsGetRdb(ts), triggerName);
	    if (arg1 < 0) {
		/* XXX W2DO? fails as "execution of script failed" */
		rc = RPMRC_FAIL;
	    } else {
		arg1 += psm->countCorrection;
		index = triggerIndices[i];
		if (triggersAlreadyRun == NULL ||
		    triggersAlreadyRun[index] == 0)
		{
		    rc = runScript(psm, triggeredH, triggertag(psm->sense), 1,
			    triggerProgs + index, triggerScripts[index],
			    arg1, arg2);
		    if (triggersAlreadyRun != NULL)
			triggersAlreadyRun[index] = 1;
		}
	    }
	}

	triggerIndices = hfd(triggerIndices, tit);
	triggerScripts = hfd(triggerScripts, tst);
	triggerProgs = hfd(triggerProgs, tpt);

	/*
	 * Each target/source header pair can only result in a single
	 * script being run.
	 */
	break;
    }

    trigger = rpmdsFree(trigger);

    return rc;
}

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param psm		package state machine data
 * @return		0 on success
 */
static rpmRC runTriggers(rpmpsm psm)
{
    const rpmts ts = psm->ts;
    rpmfi fi = psm->fi;
    int numPackage = -1;
    rpmRC rc = RPMRC_OK;
    const char * N = NULL;

    if (psm->te) 	/* XXX can't happen */
	N = rpmteN(psm->te);
/* XXX: Might need to adjust instance counts four autorollback. */
    if (N) 		/* XXX can't happen */
	numPackage = rpmdbCountPackages(rpmtsGetRdb(ts), N)
				+ psm->countCorrection;
    if (numPackage < 0)
	return RPMRC_NOTFOUND;

    if (fi != NULL && fi->h != NULL)	/* XXX can't happen */
    {	Header triggeredH;
	rpmdbMatchIterator mi;
	int countCorrection = psm->countCorrection;

	psm->countCorrection = 0;
	mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, N, 0);
	while((triggeredH = rpmdbNextIterator(mi)) != NULL)
	    rc |= handleOneTrigger(psm, fi->h, triggeredH, numPackage, NULL);
	mi = rpmdbFreeIterator(mi);
	psm->countCorrection = countCorrection;
    }

    return rc;
}

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @return		0 on success
 */
static rpmRC runImmedTriggers(rpmpsm psm)
{
    const rpmts ts = psm->ts;
    rpmfi fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    const char ** triggerNames;
    rpm_count_t numTriggers, numTriggerIndices;
    rpm_count_t * triggerIndices;
    rpm_tagtype_t tnt, tit;
    unsigned char * triggersRun;
    rpmRC rc = RPMRC_OK;

    if (fi->h == NULL)	return rc;	/* XXX can't happen */

    if (!(	hge(fi->h, RPMTAG_TRIGGERNAME, &tnt,
			(rpm_data_t *) &triggerNames, &numTriggers) &&
		hge(fi->h, RPMTAG_TRIGGERINDEX, &tit,
			(rpm_data_t *) &triggerIndices, &numTriggerIndices))
	)
	return rc;

    triggersRun = alloca(sizeof(*triggersRun) * numTriggerIndices);
    memset(triggersRun, 0, sizeof(*triggersRun) * numTriggerIndices);

    {	Header sourceH = NULL;
	int i;

	for (i = 0; i < numTriggers; i++) {
	    rpmdbMatchIterator mi;

	    if (triggersRun[triggerIndices[i]] != 0) continue;
	
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, triggerNames[i], 0);

	    while((sourceH = rpmdbNextIterator(mi)) != NULL) {
		rc |= handleOneTrigger(psm, sourceH, fi->h,
				rpmdbGetIteratorCount(mi),
				triggersRun);
	    }

	    mi = rpmdbFreeIterator(mi);
	}
    }
    triggerIndices = hfd(triggerIndices, tit);
    triggerNames = hfd(triggerNames, tnt);
    return rc;
}

static const char * pkgStageString(pkgStage a)
{
    switch(a) {
    case PSM_UNKNOWN:		return "unknown";

    case PSM_PKGINSTALL:	return "  install";
    case PSM_PKGERASE:		return "    erase";
    case PSM_PKGCOMMIT:		return "   commit";
    case PSM_PKGSAVE:		return "repackage";

    case PSM_INIT:		return "init";
    case PSM_PRE:		return "pre";
    case PSM_PROCESS:		return "process";
    case PSM_POST:		return "post";
    case PSM_UNDO:		return "undo";
    case PSM_FINI:		return "fini";

    case PSM_CREATE:		return "create";
    case PSM_NOTIFY:		return "notify";
    case PSM_DESTROY:		return "destroy";
    case PSM_COMMIT:		return "commit";

    case PSM_CHROOT_IN:		return "chrootin";
    case PSM_CHROOT_OUT:	return "chrootout";
    case PSM_SCRIPT:		return "script";
    case PSM_TRIGGERS:		return "triggers";
    case PSM_IMMED_TRIGGERS:	return "immedtriggers";

    case PSM_RPMIO_FLAGS:	return "rpmioflags";

    case PSM_RPMDB_LOAD:	return "rpmdbload";
    case PSM_RPMDB_ADD:		return "rpmdbadd";
    case PSM_RPMDB_REMOVE:	return "rpmdbremove";

    default:			return "???";
    }
}

rpmpsm rpmpsmUnlink(rpmpsm psm, const char * msg)
{
    if (psm == NULL) return NULL;
if (_psm_debug && msg != NULL)
fprintf(stderr, "--> psm %p -- %d: %s\n", psm, psm->nrefs, msg);
    psm->nrefs--;
    return NULL;
}

rpmpsm rpmpsmLink(rpmpsm psm, const char * msg)
{
    if (psm == NULL) return NULL;
    psm->nrefs++;

if (_psm_debug && msg != NULL)
fprintf(stderr, "--> psm %p ++ %d %s\n", psm, psm->nrefs, msg);

    return psm;
}

rpmpsm rpmpsmFree(rpmpsm psm)
{
    if (psm == NULL)
	return NULL;

    if (psm->nrefs > 1)
	return rpmpsmUnlink(psm, RPMDBG_M("rpmpsmFree"));

    psm->fi = rpmfiFree(psm->fi);
#ifdef	NOTYET
    psm->te = rpmteFree(psm->te);
#else
    psm->te = NULL;
#endif
    psm->ts = rpmtsFree(psm->ts);

    (void) rpmpsmUnlink(psm, RPMDBG_M("rpmpsmFree"));

    memset(psm, 0, sizeof(*psm));		/* XXX trash and burn */
    psm = _free(psm);

    return NULL;
}

void rpmpsmSetAsync(rpmpsm psm, int async)
{
    assert(psm != NULL);
    psm->unorderedSuccessor = async;
}

rpmRC rpmpsmScriptStage(rpmpsm psm, rpm_tag_t scriptTag, rpm_tag_t progTag)
{
    assert(psm != NULL);
    psm->scriptTag = scriptTag;
    psm->progTag = progTag;
    if (scriptTag == RPMTAG_VERIFYSCRIPT) {
	psm->stepName = "verify";
    }
    return rpmpsmStage(psm, PSM_SCRIPT);
}

rpmfi rpmpsmSetFI(rpmpsm psm, rpmfi fi)
{
    assert(psm != NULL);
    if (psm->fi != NULL) {
	psm->fi = rpmfiFree(psm->fi);
    }
    if (fi != NULL) {
	psm->fi = rpmfiLink(fi, RPMDBG_M("rpmpsmSetFI"));
    }
    return psm->fi;
}

rpmts rpmpsmGetTs(rpmpsm psm)
{
    return (psm ? psm->ts : NULL);
}

rpmpsm rpmpsmNew(rpmts ts, rpmte te, rpmfi fi)
{
    rpmpsm psm = xcalloc(1, sizeof(*psm));

    if (ts)	psm->ts = rpmtsLink(ts, RPMDBG_M("rpmpsmNew"));
#ifdef	NOTYET
    if (te)	psm->te = rpmteLink(te, RPMDBG_M("rpmpsmNew"));
#else
    if (te)	psm->te = te;
#endif
    if (fi)	psm->fi = rpmfiLink(fi, RPMDBG_M("rpmpsmNew"));

    return rpmpsmLink(psm, RPMDBG_M("rpmpsmNew"));
}

static void * rpmpsmThread(void * arg)
{
    rpmpsm psm = arg;
    return ((void *) rpmpsmStage(psm, psm->nstage));
}

static int rpmpsmNext(rpmpsm psm, pkgStage nstage)
{
    psm->nstage = nstage;
    if (_psm_threads)
	return rpmsqJoin( rpmsqThread(rpmpsmThread, psm) );
    return rpmpsmStage(psm, psm->nstage);
}

/**
 * @todo Packages w/o files never get a callback, hence don't get displayed
 * on install with -v.
 */
/* FIX: testing null annotation for fi->h */
rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage)
{
    const rpmts ts = psm->ts;
    uint32_t tscolor = rpmtsColor(ts);
    rpmfi fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpmRC rc = psm->rc;
    int saveerrno;
    int xx;

    switch (stage) {
    case PSM_UNKNOWN:
	break;
    case PSM_INIT:
	rpmlog(RPMLOG_DEBUG, _("%s: %s has %d files, test = %d\n"),
		psm->stepName, rpmteNEVR(psm->te),
		rpmfiFC(fi), (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST));

	/*
	 * When we run scripts, we pass an argument which is the number of
	 * versions of this package that will be installed when we are
	 * finished.
	 */
	psm->npkgs_installed = rpmdbCountPackages(rpmtsGetRdb(ts), rpmteN(psm->te));
	if (psm->npkgs_installed < 0) {
	    rc = RPMRC_FAIL;
	    break;
	}

	/* If we have a score then autorollback is enabled.  If autorollback is
 	 * enabled, and this is an autorollback transaction, then we may need to
	 * adjust the pkgs installed count.
	 *
	 * If all this is true, this adjustment should only be made if the PSM goal
	 * is an install.  No need to make this adjustment on the erase
	 * component of the upgrade, or even more absurd to do this when doing a
	 * PKGSAVE.
	 */
	if (rpmtsGetScore(ts) != NULL &&
	    rpmtsGetType(ts) == RPMTRANS_TYPE_AUTOROLLBACK &&
	    (psm->goal & ~(PSM_PKGSAVE|PSM_PKGERASE))) {
	    /* Get the score, if its not NULL, get the appropriate
 	     * score entry.
	     */
	    rpmtsScore score = rpmtsGetScore(ts);
	    if (score != NULL) {
		/* OK, we got a real score so lets get the appropriate
		 * score entry.
		 */
		rpmtsScoreEntry se;
		se = rpmtsScoreGetEntry(score, rpmteN(psm->te));

		/* IF the header for the install element has been installed,
		 * but the header for the erase element has not been erased,
		 * then decrement the instance count.  This is because in an
		 * autorollback, if the header was added in the initial transaction
		 * then in the case of an upgrade the instance count will be
		 * 2 instead of one when re-installing the old package, and 3 when
		 * erasing the new package.
		 *
		 * Another wrinkle is we only want to make this adjustement
		 * if the thing we are rollback was an upgrade of package.  A pure
		 * install or erase does not need the adjustment
		 */
		if (se && se->installed &&
	 	    !se->erased &&
		    (se->te_types & (TR_ADDED|TR_REMOVED)))
		    psm->npkgs_installed--;
	   }
	}

	if (psm->goal == PSM_PKGINSTALL) {
	    int fc = rpmfiFC(fi);

	    psm->scriptArg = psm->npkgs_installed + 1;

assert(psm->mi == NULL);
	    psm->mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(psm->te), 0);
	    xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_EPOCH, RPMMIRE_STRCMP,
			rpmteE(psm->te));
	    xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_VERSION, RPMMIRE_STRCMP,
			rpmteV(psm->te));
	    xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_RELEASE, RPMMIRE_STRCMP,
			rpmteR(psm->te));
	    if (tscolor) {
		xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_ARCH, RPMMIRE_STRCMP,
			rpmteA(psm->te));
		xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_OS, RPMMIRE_STRCMP,
			rpmteO(psm->te));
	    }

	    while ((psm->oh = rpmdbNextIterator(psm->mi)) != NULL) {
		fi->record = rpmdbGetIteratorOffset(psm->mi);
		psm->oh = NULL;
		break;
	    }
	    psm->mi = rpmdbFreeIterator(psm->mi);
	    rc = RPMRC_OK;

	    /* XXX lazy alloc here may need to be done elsewhere. */
	    if (fi->fstates == NULL && fc > 0) {
		fi->fstates = xmalloc(sizeof(*fi->fstates) * fc);
		memset(fi->fstates, RPMFILE_STATE_NORMAL, fc);
	    }

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;
	    if (fc <= 0)				break;
	
	    /*
	     * Old format relocatable packages need the entire default
	     * prefix stripped to form the cpio list, while all other packages
	     * need the leading / stripped.
	     */
	    {   const char * p;
		xx = hge(fi->h, RPMTAG_DEFAULTPREFIX, NULL, (rpm_data_t *) &p, NULL);
		fi->striplen = (xx ? strlen(p) + 1 : 1);
	    }
	    fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID | (fi->mapflags & CPIO_SBIT_CHECK);
	
	    if (headerIsEntry(fi->h, RPMTAG_ORIGBASENAMES))
		rpmfiBuildFNames(fi->h, RPMTAG_ORIGBASENAMES, &fi->apath, NULL);
	    else
		rpmfiBuildFNames(fi->h, RPMTAG_BASENAMES, &fi->apath, NULL);
	
	    if (fi->fuser == NULL)
		xx = hge(fi->h, RPMTAG_FILEUSERNAME, NULL,
				(rpm_data_t *) &fi->fuser, NULL);
	    if (fi->fgroup == NULL)
		xx = hge(fi->h, RPMTAG_FILEGROUPNAME, NULL,
				(rpm_data_t *) &fi->fgroup, NULL);
	    rc = RPMRC_OK;
	}
	if (psm->goal == PSM_PKGERASE || psm->goal == PSM_PKGSAVE) {
	    psm->scriptArg = psm->npkgs_installed - 1;
	
	    /* Retrieve installed header. */
	    rc = rpmpsmNext(psm, PSM_RPMDB_LOAD);
	    if (rc == RPMRC_OK && psm->te)
		rpmteSetHeader(psm->te, fi->h);
	}
	if (psm->goal == PSM_PKGSAVE) {
	    /* Open output package for writing. */
	    {	char * bfmt = rpmGetPath("%{_repackage_name_fmt}", NULL);
		char * pkgbn =
			headerSprintf(fi->h, bfmt, rpmTagTable, rpmHeaderFormats, NULL);

		bfmt = _free(bfmt);
		psm->pkgURL = rpmGenPath("%{?_repackage_root}",
					 "%{?_repackage_dir}",
					pkgbn);
		pkgbn = _free(pkgbn);
		(void) urlPath(psm->pkgURL, &psm->pkgfn);
		psm->fd = Fopen(psm->pkgfn, "w.ufdio");
		if (psm->fd == NULL || Ferror(psm->fd)) {
		    rc = RPMRC_FAIL;
		    break;
		}
	    }
	}
	break;
    case PSM_PRE:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;

/* XXX insure that trigger index is opened before entering chroot. */
#ifdef	NOTYET
 { static int oneshot = 0;
   dbiIndex dbi;
   if (!oneshot) {
     dbi = dbiOpen(rpmtsGetRdb(ts), RPMTAG_TRIGGERNAME, 0);
     oneshot++;
   }
 }
#endif

	/* Change root directory if requested and not already done. */
	rc = rpmpsmNext(psm, PSM_CHROOT_IN);

	if (psm->goal == PSM_PKGINSTALL) {
	    psm->scriptTag = RPMTAG_PREIN;
	    psm->progTag = RPMTAG_PREINPROG;
	    psm->sense = RPMSENSE_TRIGGERPREIN;
	    psm->countCorrection = 0;   /* XXX is this correct?!? */

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERPREIN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = rpmpsmNext(psm, PSM_TRIGGERS);
		if (rc) break;

		/* Run triggers in this package other package(s) set off. */
		rc = rpmpsmNext(psm, PSM_IMMED_TRIGGERS);
		if (rc) break;
	    }

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPRE)) {
		rc = rpmpsmNext(psm, PSM_SCRIPT);
		if (rc != RPMRC_OK) {
		    rpmlog(RPMLOG_ERR,
			_("%s: %s scriptlet failed (%d), skipping %s\n"),
			psm->stepName, tag2sln(psm->scriptTag), rc,
			rpmteNEVR(psm->te));
		    break;
		}
	    }
	}

	if (psm->goal == PSM_PKGERASE) {
	    psm->scriptTag = RPMTAG_PREUN;
	    psm->progTag = RPMTAG_PREUNPROG;
	    psm->sense = RPMSENSE_TRIGGERUN;
	    psm->countCorrection = -1;

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERUN)) {
		/* Run triggers in this package other package(s) set off. */
		rc = rpmpsmNext(psm, PSM_IMMED_TRIGGERS);
		if (rc) break;

		/* Run triggers in other package(s) this package sets off. */
		rc = rpmpsmNext(psm, PSM_TRIGGERS);
		if (rc) break;
	    }

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPREUN))
		rc = rpmpsmNext(psm, PSM_SCRIPT);
	}
	if (psm->goal == PSM_PKGSAVE) {
	    int noArchiveSize = 0;

	    /* Regenerate original header. */
	    {	void * uh = NULL;
		rpm_tagtype_t uht;
		rpm_count_t uhc;

		if (headerGetEntry(fi->h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)) {
		    psm->oh = headerCopyLoad(uh);
		    uh = hfd(uh, uht);
		} else
		if (headerGetEntry(fi->h, RPMTAG_HEADERIMAGE, &uht, &uh, &uhc))
		{
		    HeaderIterator hi;
		    rpm_tagtype_t type;
		    rpm_tag_t tag;
		    rpm_count_t count;
		    rpm_data_t ptr;
		    Header oh;

		    /* Load the original header from the blob. */
		    oh = headerCopyLoad(uh);

		    /* XXX this is headerCopy w/o headerReload() */
		    psm->oh = headerNew();

		    for (hi = headerInitIterator(oh);
		        headerNextIterator(hi, &tag, &type, &ptr, &count);
		        ptr = headerFreeData(ptr, type))
		    {
			if (tag == RPMTAG_ARCHIVESIZE)
			    noArchiveSize = 1;
		        if (ptr) (void) headerAddEntry(psm->oh, tag, type, ptr, count);
		    }
		    hi = headerFreeIterator(hi);

		    oh = headerFree(oh);
		    uh = hfd(uh, uht);
		} else
		    break;	/* XXX shouldn't ever happen */
	    }

	    /* Retrieve type of payload compression. */
	   	/* FIX: psm->oh may be NULL */
	    rc = rpmpsmNext(psm, PSM_RPMIO_FLAGS);

	    /* Write the lead section into the package. */
	    {
		rpmlead lead = rpmLeadFromHeader(psm->oh);
		rc = rpmLeadWrite(psm->fd, lead);
		lead = rpmLeadFree(lead);
		if (rc != RPMRC_OK) {
		    rpmlog(RPMLOG_ERR, _("Unable to write package: %s\n"),
			 Fstrerror(psm->fd));
		    break;
		}
	    }
		
	    /* Write the signature section into the package. */
	    /* XXX rpm-4.1 and later has archive size in signature header. */
	    {	Header sigh = headerRegenSigHeader(fi->h, noArchiveSize);
		/* Reallocate the signature into one contiguous region. */
		sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
		if (sigh == NULL) {
		    rpmlog(RPMLOG_ERR, _("Unable to reload signature header\n"));
		    rc = RPMRC_FAIL;
		    break;
		}
		rc = rpmWriteSignature(psm->fd, sigh);
		sigh = rpmFreeSignature(sigh);
		if (rc) break;
	    }

	    /* Add remove transaction id to header. */
	    if (psm->oh != NULL)
	    {	int32_t tid = rpmtsGetTid(ts);
		xx = headerAddEntry(psm->oh, RPMTAG_REMOVETID,
			RPM_INT32_TYPE, &tid, 1);
	    }

	    /* Write the metadata section into the package. */
	    rc = headerWrite(psm->fd, psm->oh, HEADER_MAGIC_YES);
	    if (rc) break;
	}
	break;
    case PSM_PROCESS:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;

	if (psm->goal == PSM_PKGINSTALL) {

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;

	    /* XXX Synthesize callbacks for packages with no files. */
	    if (rpmfiFC(fi) <= 0) {
		void * ptr;
		ptr = rpmtsNotify(ts, fi->te, RPMCALLBACK_INST_START, 0, 100);
		ptr = rpmtsNotify(ts, fi->te, RPMCALLBACK_INST_PROGRESS, 100, 100);
		break;
	    }

	    /* Retrieve type of payload compression. */
	    rc = rpmpsmNext(psm, PSM_RPMIO_FLAGS);

	    if (rpmteFd(fi->te) == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }

	   	/* LCL: fi->fd != NULL here. */
	    psm->cfd = Fdopen(fdDup(Fileno(rpmteFd(fi->te))), psm->rpmio_flags);
	    if (psm->cfd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }

	    rc = fsmSetup(fi->fsm, FSM_PKGINSTALL, ts, fi,
			psm->cfd, NULL, &psm->failedFile);
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_UNCOMPRESS),
			fdOp(psm->cfd, FDSTAT_READ));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
			fdOp(psm->cfd, FDSTAT_DIGEST));
	    xx = fsmTeardown(fi->fsm);

	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->cfd);
	    psm->cfd = NULL;
	    errno = saveerrno; /* XXX FIXME: Fclose with libio destroys errno */

	    if (!rc)
		rc = rpmpsmNext(psm, PSM_COMMIT);

	    /* XXX make sure progress is closed out */
	    psm->what = RPMCALLBACK_INST_PROGRESS;
	    psm->amount = (fi->archiveSize ? fi->archiveSize : 100);
	    psm->total = psm->amount;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("unpacking of archive failed%s%s: %s\n"),
			(psm->failedFile != NULL ? _(" on file ") : ""),
			(psm->failedFile != NULL ? psm->failedFile : ""),
			cpioStrerror(rc));
		rc = RPMRC_FAIL;

		/* XXX notify callback on error. */
		psm->what = RPMCALLBACK_UNPACK_ERROR;
		psm->amount = 0;
		psm->total = 0;
		xx = rpmpsmNext(psm, PSM_NOTIFY);

		break;
	    }
	}
	if (psm->goal == PSM_PKGERASE) {
	    int fc = rpmfiFC(fi);

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;
	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_APPLYONLY)	break;

	    /* XXX Synthesize callbacks for packages with no files. */
	    if (rpmfiFC(fi) <= 0) {
		void * ptr;
		ptr = rpmtsNotify(ts, fi->te, RPMCALLBACK_UNINST_START, 0, 100);
		ptr = rpmtsNotify(ts, fi->te, RPMCALLBACK_UNINST_STOP, 0, 100);
		break;
	    }

	    psm->what = RPMCALLBACK_UNINST_START;
	    psm->amount = fc;		/* XXX W2DO? looks wrong. */
	    psm->total = fc;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	    rc = fsmSetup(fi->fsm, FSM_PKGERASE, ts, fi,
			NULL, NULL, &psm->failedFile);
	    xx = fsmTeardown(fi->fsm);

	    psm->what = RPMCALLBACK_UNINST_STOP;
	    psm->amount = 0;		/* XXX W2DO? looks wrong. */
	    psm->total = fc;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	}
	if (psm->goal == PSM_PKGSAVE) {
	    rpmFileAction * actions = fi->actions;
	    rpmFileAction action = fi->action;

	    fi->action = FA_COPYOUT;
	    fi->actions = NULL;

	    if (psm->fd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }
	   	/* FIX: fdDup mey return NULL. */
	    xx = Fflush(psm->fd);
	    psm->cfd = Fdopen(fdDup(Fileno(psm->fd)), psm->rpmio_flags);
	    if (psm->cfd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }

	    rc = fsmSetup(fi->fsm, FSM_PKGBUILD, ts, fi, psm->cfd,
			NULL, &psm->failedFile);
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_COMPRESS),
			fdOp(psm->cfd, FDSTAT_WRITE));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
			fdOp(psm->cfd, FDSTAT_DIGEST));
	    xx = fsmTeardown(fi->fsm);

	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->cfd);
	    psm->cfd = NULL;
	    errno = saveerrno;

	    /* XXX make sure progress is closed out */
	    psm->what = RPMCALLBACK_INST_PROGRESS;
	    psm->amount = (fi->archiveSize ? fi->archiveSize : 100);
	    psm->total = psm->amount;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	    fi->action = action;
	    fi->actions = actions;
	}
	break;
    case PSM_POST:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;

	if (psm->goal == PSM_PKGINSTALL) {
	    int32_t installTime = (int32_t) time(NULL);
	    rpm_count_t fc = rpmfiFC(fi);

	    if (fi->h == NULL) break;	/* XXX can't happen */
	    if (fi->fstates != NULL && fc > 0)
		xx = headerAddEntry(fi->h, RPMTAG_FILESTATES, RPM_CHAR_TYPE,
				fi->fstates, fc);

	    xx = headerAddEntry(fi->h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE,
				&installTime, 1);

	    xx = headerAddEntry(fi->h, RPMTAG_INSTALLCOLOR, RPM_INT32_TYPE,
				&tscolor, 1);

	    /*
	     * If this package has already been installed, remove it from
	     * the database before adding the new one.
	     */
	    if (fi->record && !(rpmtsFlags(ts) & RPMTRANS_FLAG_APPLYONLY)) {
		rc = rpmpsmNext(psm, PSM_RPMDB_REMOVE);
		if (rc) break;
	    }

	    rc = rpmpsmNext(psm, PSM_RPMDB_ADD);
	    if (rc) break;

	    psm->scriptTag = RPMTAG_POSTIN;
	    psm->progTag = RPMTAG_POSTINPROG;
	    psm->sense = RPMSENSE_TRIGGERIN;
	    psm->countCorrection = 0;

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPOST)) {
		rc = rpmpsmNext(psm, PSM_SCRIPT);
		if (rc) break;
	    }
	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERIN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = rpmpsmNext(psm, PSM_TRIGGERS);
		if (rc) break;

		/* Run triggers in this package other package(s) set off. */
		rc = rpmpsmNext(psm, PSM_IMMED_TRIGGERS);
		if (rc) break;
	    }

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_APPLYONLY))
		rc = markReplacedFiles(psm);

	}
	if (psm->goal == PSM_PKGERASE) {

	    psm->scriptTag = RPMTAG_POSTUN;
	    psm->progTag = RPMTAG_POSTUNPROG;
	    psm->sense = RPMSENSE_TRIGGERPOSTUN;
	    psm->countCorrection = -1;

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPOSTUN)) {
		rc = rpmpsmNext(psm, PSM_SCRIPT);
		if (rc) break;
	    }

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERPOSTUN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = rpmpsmNext(psm, PSM_TRIGGERS);
		if (rc) break;
	    }

	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_APPLYONLY))
		rc = rpmpsmNext(psm, PSM_RPMDB_REMOVE);
	}
	if (psm->goal == PSM_PKGSAVE) {
	}

	/* Restore root directory if changed. */
	xx = rpmpsmNext(psm, PSM_CHROOT_OUT);
	break;
    case PSM_UNDO:
	break;
    case PSM_FINI:
	/* Restore root directory if changed. */
	xx = rpmpsmNext(psm, PSM_CHROOT_OUT);

	if (psm->fd != NULL) {
	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->fd);
	    psm->fd = NULL;
	    errno = saveerrno;
	}

	if (rc) {
	    if (psm->failedFile)
		rpmlog(RPMLOG_ERR,
			_("%s failed on file %s: %s\n"),
			psm->stepName, psm->failedFile, cpioStrerror(rc));
	    else
		rpmlog(RPMLOG_ERR, _("%s failed: %s\n"),
			psm->stepName, cpioStrerror(rc));

	    /* XXX notify callback on error. */
	    psm->what = RPMCALLBACK_CPIO_ERROR;
	    psm->amount = 0;
	    psm->total = 0;
	    /* FIX: psm->fd may be NULL. */
	    xx = rpmpsmNext(psm, PSM_NOTIFY);
	}

	if (psm->goal == PSM_PKGERASE || psm->goal == PSM_PKGSAVE) {
	    if (psm->te != NULL) 
		rpmteSetHeader(psm->te, NULL);
	    if (fi->h != NULL)
		fi->h = headerFree(fi->h);
 	}
	psm->oh = headerFree(psm->oh);
	psm->pkgURL = _free(psm->pkgURL);
	psm->rpmio_flags = _free(psm->rpmio_flags);
	psm->failedFile = _free(psm->failedFile);

	fi->fgroup = hfd(fi->fgroup, RPM_FORCEFREE_TYPE);
	fi->fuser = hfd(fi->fuser, RPM_FORCEFREE_TYPE);
	fi->apath = _free(fi->apath);
	fi->fstates = _free(fi->fstates);
	break;

    case PSM_PKGINSTALL:
    case PSM_PKGERASE:
    case PSM_PKGSAVE:
	psm->goal = stage;
	psm->rc = RPMRC_OK;
	psm->stepName = pkgStageString(stage);

	rc = rpmpsmNext(psm, PSM_INIT);
	if (!rc) rc = rpmpsmNext(psm, PSM_PRE);
	if (!rc) rc = rpmpsmNext(psm, PSM_PROCESS);
	if (!rc) rc = rpmpsmNext(psm, PSM_POST);
	xx = rpmpsmNext(psm, PSM_FINI);
	break;
    case PSM_PKGCOMMIT:
	break;

    case PSM_CREATE:
	break;
    case PSM_NOTIFY:
    {	void * ptr;
/* FIX: psm->te may be NULL */
	ptr = rpmtsNotify(ts, psm->te, psm->what, psm->amount, psm->total);
    }	break;
    case PSM_DESTROY:
	break;
    case PSM_COMMIT:
	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_PKGCOMMIT)) break;
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_APPLYONLY) break;

	rc = fsmSetup(fi->fsm, FSM_PKGCOMMIT, ts, fi,
			NULL, NULL, &psm->failedFile);
	xx = fsmTeardown(fi->fsm);
	break;

    case PSM_CHROOT_IN:
    {	const char * rootDir = rpmtsRootDir(ts);
	/* Change root directory if requested and not already done. */
	if (rootDir != NULL && !(rootDir[0] == '/' && rootDir[1] == '\0')
	 && !rpmtsChrootDone(ts) && !psm->chrootDone)
	{
	    static int _pw_loaded = 0;
	    static int _gr_loaded = 0;

	    if (!_pw_loaded) {
		(void)getpwnam("root");
		endpwent();
		_pw_loaded++;
	    }
	    if (!_gr_loaded) {
		(void)getgrnam("root");
		endgrent();
		_gr_loaded++;
	    }

	    xx = chdir("/");
	    if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
		rc = chroot(rootDir);
	    psm->chrootDone = 1;
	    (void) rpmtsSetChrootDone(ts, 1);
	}
    }	break;
    case PSM_CHROOT_OUT:
	/* Restore root directory if changed. */
	if (psm->chrootDone) {
	    const char * rootDir = rpmtsRootDir(ts);
	    const char * currDir = rpmtsCurrDir(ts);
	    if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
		rc = chroot(".");
	    psm->chrootDone = 0;
	    (void) rpmtsSetChrootDone(ts, 0);
	    if (currDir != NULL)	/* XXX can't happen */
		xx = chdir(currDir);
	}
	break;
    case PSM_SCRIPT:	/* Run current package scriptlets. */
	rc = runInstScript(psm);
	break;
    case PSM_TRIGGERS:
	/* Run triggers in other package(s) this package sets off. */
	rc = runTriggers(psm);
	break;
    case PSM_IMMED_TRIGGERS:
	/* Run triggers in this package other package(s) set off. */
	rc = runImmedTriggers(psm);
	break;

    case PSM_RPMIO_FLAGS:
    {	const char * payload_compressor = NULL;
	char * t;

	if (!hge(fi->h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (rpm_data_t *) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	psm->rpmio_flags = t = xmalloc(sizeof("w9.gzdio"));
	*t = '\0';
	t = stpcpy(t, ((psm->goal == PSM_PKGSAVE) ? "w9" : "r"));
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
	rc = RPMRC_OK;
    }	break;

    case PSM_RPMDB_LOAD:
assert(psm->mi == NULL);
	psm->mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));

	fi->h = rpmdbNextIterator(psm->mi);
	if (fi->h != NULL)
	    fi->h = headerLink(fi->h);

	psm->mi = rpmdbFreeIterator(psm->mi);
	rc = (fi->h != NULL ? RPMRC_OK : RPMRC_FAIL);
	break;
    case PSM_RPMDB_ADD:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;
	if (fi->h == NULL)	break;	/* XXX can't happen */
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBADD), 0);
	if (!(rpmtsVSFlags(ts) & RPMVSF_NOHDRCHK))
	    rc = rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), fi->h,
				ts, headerCheck);
	else
	    rc = rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), fi->h,
				NULL, NULL);

	/* Set the database instance so consumers (i.e. rpmtsRun())
	 * can add this to a rollback transaction.
	 */
	rpmteSetDBInstance(psm->te, myinstall_instance);

	/*
	 * If the score exists and this is not a rollback or autorollback
	 * then lets check off installed for this package.
	 */
	if (rpmtsGetScore(ts) != NULL &&
	    rpmtsGetType(ts) != RPMTRANS_TYPE_ROLLBACK &&
	    rpmtsGetType(ts) != RPMTRANS_TYPE_AUTOROLLBACK)
	{
	    /* Get the score, if its not NULL, get the appropriate
 	     * score entry.
	     */
	    rpmtsScore score = rpmtsGetScore(ts);
	    if (score != NULL) {
		rpmtsScoreEntry se;
		/* OK, we got a real score so lets get the appropriate
		 * score entry.
		 */
		rpmlog(RPMLOG_DEBUG,
		    _("Attempting to mark %s as installed in score board(%p).\n"),
		    rpmteN(psm->te), score);
		se = rpmtsScoreGetEntry(score, rpmteN(psm->te));
		if (se != NULL) se->installed = 1;
	    }
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBADD), 0);
	break;
    case PSM_RPMDB_REMOVE:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	rc = rpmdbRemove(rpmtsGetRdb(ts), rpmtsGetTid(ts), fi->record,
				NULL, NULL);

	/*
	 * If the score exists and this is not a rollback or autorollback
	 * then lets check off erased for this package.
	 */
	if (rpmtsGetScore(ts) != NULL &&
	   rpmtsGetType(ts) != RPMTRANS_TYPE_ROLLBACK &&
	   rpmtsGetType(ts) != RPMTRANS_TYPE_AUTOROLLBACK)
	{
	    /* Get the score, if its not NULL, get the appropriate
	     * score entry.
	     */
	    rpmtsScore score = rpmtsGetScore(ts);

	    if (score != NULL) { /* XXX: Can't happen */
		rpmtsScoreEntry se;
		/* OK, we got a real score so lets get the appropriate
		 * score entry.
		 */
		rpmlog(RPMLOG_DEBUG,
		    _("Attempting to mark %s as erased in score board(%p).\n"),
		    rpmteN(psm->te), score);
		se = rpmtsScoreGetEntry(score, rpmteN(psm->te));
		if (se != NULL) se->erased = 1;
	    }
	}

	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	break;

    default:
	break;
   }

   	/* FIX: psm->oh and psm->fi->h may be NULL. */
    return rc;
}
