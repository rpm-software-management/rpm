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
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmdb.h>		/* XXX for db_chrootDone */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/argv.h>

#include "rpmio/rpmlua.h"
#include "lib/cpio.h"
#include "lib/fsm.h"		/* XXX CPIO_FOO/FSM_FOO constants */
#include "lib/psm.h"
#include "lib/rpmfi_internal.h" /* XXX replaced/states, fi->te... */
#include "lib/rpmte_internal.h"	/* XXX te->fd */
#include "lib/rpmlead.h"		/* writeLead proto */
#include "lib/signature.h"		/* signature constants */
#include "lib/misc.h"		/* XXX rpmMkdirPath */

#include "debug.h"

#define	_PSM_DEBUG	0
int _psm_debug = _PSM_DEBUG;
int _psm_threads = 0;

/**
 */
struct rpmpsm_s {
    struct rpmsqElem sq;	/*!< Scriptlet/signal queue element. */

    rpmts ts;			/*!< transaction set */
    rpmte te;			/*!< current transaction element */
    rpmfi fi;			/*!< transaction element file info */
    FD_t cfd;			/*!< Payload file handle. */
    rpmdbMatchIterator mi;
    const char * stepName;
    const char * rpmio_flags;
    char * failedFile;
    int scriptTag;		/*!< Scriptlet data tag. */
    int progTag;		/*!< Scriptlet interpreter tag. */
    int npkgs_installed;	/*!< No. of installed instances. */
    int scriptArg;		/*!< Scriptlet package arg. */
    rpmsenseFlags sense;	/*!< One of RPMSENSE_TRIGGER{PREIN,IN,UN,POSTUN}. */
    int countCorrection;	/*!< 0 if installing, -1 if removing. */
    int chrootDone;		/*!< Was chroot(2) done by pkgStage? */
    int unorderedSuccessor;	/*!< Can the PSM be run asynchronously? */
    rpmCallbackType what;	/*!< Callback type. */
    rpm_loff_t amount;		/*!< Callback amount. */
    rpm_loff_t total;		/*!< Callback total. */
    rpmRC rc;
    pkgStage goal;
    pkgStage stage;		/*!< Current psm stage. */
    pkgStage nstage;		/*!< Next psm stage. */

    int nrefs;			/*!< Reference count. */
};

int rpmVersionCompare(Header first, Header second)
{
    struct rpmtd_s one, two;
    static uint32_t zero = 0;
    uint32_t *epochOne = &zero, *epochTwo = &zero;
    int rc;

    if (headerGet(first, RPMTAG_EPOCH, &one, HEADERGET_MINMEM))
	epochOne = rpmtdGetUint32(&one);
    if (headerGet(second, RPMTAG_EPOCH, &two, HEADERGET_MINMEM))
	epochTwo = rpmtdGetUint32(&two);

    if (*epochOne < *epochTwo)
	return -1;
    else if (*epochOne > *epochTwo)
	return 1;

    headerGet(first, RPMTAG_VERSION, &one, HEADERGET_MINMEM);
    headerGet(second, RPMTAG_VERSION, &two, HEADERGET_MINMEM);

    rc = rpmvercmp(rpmtdGetString(&one), rpmtdGetString(&two));
    if (rc)
	return rc;

    headerGet(first, RPMTAG_RELEASE, &one, HEADERGET_MINMEM);
    headerGet(second, RPMTAG_RELEASE, &two, HEADERGET_MINMEM);

    return rpmvercmp(rpmtdGetString(&one), rpmtdGetString(&two));
}

/**
 * Macros to be defined from per-header tag values.
 * @todo Should other macros be added from header when installing a package?
 */
static struct tagMacro {
    const char *macroname; 	/*!< Macro name to define. */
    rpmTag tag;			/*!< Header tag to use for value. */
} const tagMacros[] = {
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
    const struct tagMacro * tagm;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	struct rpmtd_s td;
	char *body;
	if (!headerGet(h, tagm->tag, &td, HEADERGET_DEFAULT))
	    continue;

	switch (rpmtdType(&td)) {
	case RPM_INT32_TYPE: /* fallthrough */
	case RPM_STRING_TYPE:
	    body = rpmtdFormat(&td, RPMTD_FORMAT_STRING, NULL);
	    addMacro(NULL, tagm->macroname, NULL, body, -1);
	    free(body);
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
	rpmtdFreeData(&td);
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

    offsets = xmalloc(num * sizeof(*offsets));
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
	int modified;
	struct rpmtd_s secStates;
	modified = 0;

	if (!headerGet(h, RPMTAG_FILESTATES, &secStates, HEADERGET_MINMEM))
	    continue;
	
	prev = rpmdbGetIteratorOffset(mi);
	num = 0;
	while (sfi->otherPkg && sfi->otherPkg == prev) {
	    int ix = rpmtdSetIndex(&secStates, sfi->otherFileNum);
	    assert(ix != -1);

	    char *state = rpmtdGetChar(&secStates);
	    if (state && *state != RPMFILE_STATE_REPLACED) {
		*state = RPMFILE_STATE_REPLACED;
		if (modified == 0) {
		    /* Modified header will be rewritten. */
		    modified = 1;
		    xx = rpmdbSetIteratorModified(mi, modified);
		}
		num++;
	    }
	    sfi++;
	}
	rpmtdFreeData(&secStates);
    }
    mi = rpmdbFreeIterator(mi);
    free(offsets);

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

(void) rpmInstallLoadMacros(fi, fi->h);

    psm->fi = rpmfiLink(fi, RPMDBG_M("rpmInstallLoadMacros"));
    psm->te = fi->te;

    if (cookie) {
	struct rpmtd_s ctd;
	*cookie = NULL;
	if (headerGet(fi->h, RPMTAG_COOKIE, &ctd, HEADERGET_MINMEM)) {
	    *cookie = xstrdup(rpmtdGetString(&ctd));
	    rpmtdFreeData(&ctd);
	}
    }

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
	struct rpmtd_s bnames;
	headerGet(fi->h, RPMTAG_FILENAMES, &bnames, HEADERGET_EXT);
	fi->apath = bnames.data; /* XXX Ick */

	if (headerIsEntry(fi->h, RPMTAG_COOKIE))
	    for (i = 0; i < fi->fc; i++)
		if (fi->fflags[i] & RPMFILE_SPECFILE) break;
    }

    if (i == fi->fc) {
	/* Find the spec file by name. */
	for (i = 0; i < fi->fc; i++) {
	    if (rpmFileHasSuffix(fi->apath[i], ".spec"))
		break;
	}
    }

    _sourcedir = rpmGenPath(rpmtsRootDir(ts), "%{_sourcedir}", "");
    rpmrc = rpmMkdirPath(_sourcedir, "_sourcedir");
    if (rpmrc) {
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    _specdir = rpmGenPath(rpmtsRootDir(ts), "%{_specdir}", "");
    rpmrc = rpmMkdirPath(_specdir, "_specdir");
    if (rpmrc) {
	rpmrc = RPMRC_FAIL;
	goto exit;
    }

    /* Build dnl/dil with {_sourcedir, _specdir} as values. */
    if (i < fi->fc) {
	size_t speclen = strlen(_specdir) + 2;
	size_t sourcelen = strlen(_sourcedir) + 2;
	char * t;

	fi->dnl = _free(fi->dnl);

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

static const char * const SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

/**
 * Return scriptlet name from tag.
 * @param tag		scriptlet tag
 * @return		name of scriptlet
 */
static const char * tag2sln(rpmTag tag)
{
    switch ((rpm_tag_t) tag) {
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
    default: break;
    }
    return "%unknownscript";
}

static rpmTag triggertag(rpmsenseFlags sense) 
{
    rpmTag tag = RPMTAG_NOT_FOUND;
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
	"%s: waitpid(%d) rc %d status %x secs %u.%03u\n",
	psm->stepName, (unsigned)psm->sq.child,
	(unsigned)psm->sq.reaped, psm->sq.status,
	(unsigned)msecs/1000, (unsigned)msecs%1000);

    return psm->sq.reaped;
}

/**
 * Run internal Lua script.
 */
static rpmRC runLuaScript(rpmpsm psm, Header h, rpmTag stag, ARGV_t argv,
		   const char *script, int arg1, int arg2)
{
#ifdef WITH_LUA
    const rpmts ts = psm->ts;
    char *nevra, *sname = NULL;
    int rootFd = -1;
    rpmRC rc = RPMRC_OK;
    int xx;
    rpmlua lua = NULL; /* Global state. */
    rpmluav var;

    nevra = headerGetNEVRA(h, NULL);
    rasprintf(&sname, "%s(%s)", tag2sln(stag), nevra);
    free(nevra);

    rpmlog(RPMLOG_DEBUG, "%s: %s running <lua> scriptlet.\n",
	   psm->stepName, sname);
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
    if (argv) {
	char **p;
	for (p = argv; *p; p++) {
	    rpmluavSetValue(var, RPMLUAV_STRING, *p);
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
	if (rpmluaRunScript(lua, script, sname) == -1) {
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
    free(sname);

    return rc;
#else
    return RPMRC_FAIL;
#endif
}

/**
 */
static int ldconfig_done = 0;

static const char * ldconfig_path = "/sbin/ldconfig";

static void doScriptExec(rpmts ts, ARGV_const_t argv, rpmtd prefixes,
			FD_t scriptFd, FD_t out)
{
    const char * rootDir;
    int pipes[2];
    int flag;
    int fdno;
    int xx;

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

    {   char *ipath = rpmExpand("%{_install_script_path}", NULL);
	const char *path = SCRIPT_PATH;

	if (ipath && ipath[5] != '%')
	    path = ipath;

	xx = setenv("PATH", path, 1);
	ipath = _free(ipath);
    }

    if (rpmtdCount(prefixes) > 0) {
	const char *pfx;
	/* backwards compatibility */
	if ((pfx = rpmtdGetString(prefixes))) {
	    xx = setenv("RPM_INSTALL_PREFIX", pfx, 1);
	}

	while ((pfx = rpmtdNextString(prefixes))) {
	    char *name = NULL;
	    rasprintf(&name, "RPM_INSTALL_PREFIX%d", rpmtdGetIndex(prefixes));
	    xx = setenv(name, pfx, 1);
	    free(name);
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

	/* XXX Don't mtrace into children. */
	unsetenv("MALLOC_CHECK_");

	/* Permit libselinux to do the scriptlet exec. */
	if (rpmtsSELinuxEnabled(ts) == 1) {	
	    xx = rpm_execcon(0, argv[0], argv, environ);
	}

	if (xx == 0) {
	    xx = execv(argv[0], argv);
	}
    }
    _exit(-1);
}

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
 * @param argvp		ARGV_t pointer to args from header, *argvp[0] is the 
 * 			interpreter to use. Pointer as we might need to 
 * 			modify via argvAdd()
 * @param script	scriptlet from header
 * @param arg1		no. instances of package installed after scriptlet exec
 *			(-1 is no arg)
 * @param arg2		ditto, but for the target package
 * @return		0 on success
 */
static rpmRC runScript(rpmpsm psm, Header h, rpmTag stag, ARGV_t * argvp,
		const char * script, int arg1, int arg2)
{
    const rpmts ts = psm->ts;
    char * fn = NULL;
    int xx;
    FD_t scriptFd;
    FD_t out = NULL;
    rpmRC rc = RPMRC_OK;
    char *nevra, *sname = NULL; 
    struct rpmtd_s prefixes;

    assert(argvp != NULL);
    if (*argvp == NULL && script == NULL)
	return rc;

    if (*argvp && strcmp(*argvp[0], "<lua>") == 0) {
	return runLuaScript(psm, h, stag, *argvp, script, arg1, arg2);
    }

    /* XXX FIXME: except for %verifyscript, rpmteNEVR could be used. */
    nevra = headerGetNEVRA(h, NULL);
    rasprintf(&sname, "%s(%s)", tag2sln(stag), nevra);
    free(nevra);

    psm->sq.reaper = 1;

    /*
     * If a successor node, and ldconfig was just run, don't bother.
     */
    if (ldconfig_path && *argvp != NULL && psm->unorderedSuccessor) {
 	if (ldconfig_done && !strcmp(*argvp[0], ldconfig_path)) {
	    rpmlog(RPMLOG_DEBUG, "%s: %s skipping redundant \"%s\".\n",
		   psm->stepName, sname, *argvp[0]);
	    free(sname);
	    return rc;
	}
    }

    rpmlog(RPMLOG_DEBUG, "%s: %s %ssynchronous scriptlet start\n",
	   psm->stepName, sname, (psm->unorderedSuccessor ? "a" : ""));

    if (argvCount(*argvp) == 0) {
	argvAdd(argvp, "/bin/sh");
	ldconfig_done = 0;
    } else {
	ldconfig_done = (ldconfig_path && !strcmp(*argvp[0], ldconfig_path)
		? 1 : 0);
    }


    /* Try new style prefixes first, then old. Otherwise there are none.. */
    if (!headerGet(h, RPMTAG_INSTPREFIXES, &prefixes, HEADERGET_DEFAULT)) {
	headerGet(h, RPMTAG_INSTALLPREFIX, &prefixes, HEADERGET_DEFAULT);
    }

    if (script) {
	const char * rootDir = rpmtsRootDir(ts);
	FD_t fd;

	fd = rpmMkTempFile((!rpmtsChrootDone(ts) ? rootDir : "/"), &fn);
	if (fd == NULL || Ferror(fd)) {
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	if (rpmIsDebug() &&
	    (!strcmp(*argvp[0], "/bin/sh") || !strcmp(*argvp[0], "/bin/bash")))
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
	    argvAdd(argvp, sn);
	}

	if (arg1 >= 0) {
	    argvAddNum(argvp, arg1);
	}
	if (arg2 >= 0) {
	    argvAddNum(argvp, arg2);
	}
    }

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
    if (out == NULL) { /* XXX can't happen */
	rc = RPMRC_FAIL;	
	goto exit;
    }

    xx = rpmsqFork(&psm->sq);
    if (psm->sq.child == 0) {
	rpmlog(RPMLOG_DEBUG, "%s: %s\texecv(%s) pid %d\n",
	       psm->stepName, sname, *argvp[0], (unsigned)getpid());
	doScriptExec(ts, *argvp, &prefixes, scriptFd, out);
    }

    if (psm->sq.child == (pid_t)-1) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"), sname, strerror(errno));
	rc = RPMRC_FAIL;
	goto exit;
    }

    (void) psmWait(psm);

    if (psm->sq.reaped < 0) {
	(void) rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				 stag, WTERMSIG(psm->sq.child));
	rpmlog(RPMLOG_ERR, _("%s scriptlet failed, waitpid(%d) rc %d: %s\n"),
		 sname, psm->sq.child, psm->sq.reaped, strerror(errno));
	rc = RPMRC_FAIL;
    } else if (!WIFEXITED(psm->sq.status) || WEXITSTATUS(psm->sq.status)) {
      	if (WIFSIGNALED(psm->sq.status)) {
	    (void)rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				stag, WTERMSIG(psm->sq.status));
	    rpmlog(RPMLOG_ERR, _("%s scriptlet failed, signal %d\n"),
                   sname, WTERMSIG(psm->sq.status));
	} else {
	    (void) rpmtsNotify(ts, psm->te, RPMCALLBACK_SCRIPT_ERROR,
				 stag, WEXITSTATUS(psm->sq.status));
	    rpmlog(RPMLOG_ERR, _("%s scriptlet failed, exit status %d\n"),
		   sname, WEXITSTATUS(psm->sq.status));
	}
	rc = RPMRC_FAIL;
    }

exit:
    rpmtdFreeData(&prefixes);

    if (out)
	xx = Fclose(out);	/* XXX dup'd STDOUT_FILENO */

    if (script) {
	if (!rpmIsDebug())
	    xx = unlink(fn);
	fn = _free(fn);
    }
    free(sname);

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
    rpmRC rc = RPMRC_OK;
    ARGV_t argv;
    struct rpmtd_s script, prog;
    const char *str;

    if (fi->h == NULL)	/* XXX can't happen */
	return RPMRC_FAIL;

    headerGet(fi->h, psm->scriptTag, &script, HEADERGET_DEFAULT);
    headerGet(fi->h, psm->progTag, &prog, HEADERGET_DEFAULT);
    if (rpmtdCount(&script) == 0 && rpmtdCount(&prog) == 0)
	goto exit;

    argv = argvNew();
    while ((str = rpmtdNextString(&prog))) {
	argvAdd(&argv, str);
    }

    rc = runScript(psm, fi->h, psm->scriptTag, &argv,
		   rpmtdGetString(&script), psm->scriptArg, -1);
    argvFree(argv);

exit:
    rpmtdFreeData(&script);
    rpmtdFreeData(&prog);
    return rc;
}

/**
 * Execute triggers.
 * @todo Trigger on any provides, not just package NVR.
 * @param psm		package state machine data
 * @param sourceH	header of trigger source
 * @param trigH		header of triggered package
 * @param arg2
 * @param triggersAlreadyRun
 * @return
 */
static rpmRC handleOneTrigger(const rpmpsm psm,
			Header sourceH, Header trigH,
			int arg2, unsigned char * triggersAlreadyRun)
{
    int scareMem = 1;
    const rpmts ts = psm->ts;
    rpmds trigger = NULL;
    const char * sourceName;
    const char * triggerName;
    rpmRC rc = RPMRC_OK;
    int xx;
    int i;

    xx = headerNVR(sourceH, &sourceName, NULL, NULL);
    xx = headerNVR(trigH, &triggerName, NULL, NULL);

    trigger = rpmdsInit(rpmdsNew(trigH, RPMTAG_TRIGGERNAME, scareMem));
    if (trigger == NULL)
	return rc;

    (void) rpmdsSetNoPromote(trigger, 1);

    while ((i = rpmdsNext(trigger)) >= 0) {
	const char * Name;
	rpmsenseFlags Flags = rpmdsFlags(trigger);
	struct rpmtd_s tscripts, tprogs, tindexes;
	headerGetFlags hgflags = HEADERGET_MINMEM;

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

	if (!(headerGet(trigH, RPMTAG_TRIGGERINDEX, &tindexes, hgflags) &&
	      headerGet(trigH, RPMTAG_TRIGGERSCRIPTS, &tscripts, hgflags) &&
	      headerGet(trigH, RPMTAG_TRIGGERSCRIPTPROG, &tprogs, hgflags))) {
	    continue;
	}

	{   int arg1;
	    int index;
	    const char ** triggerScripts = tscripts.data;
	    const char ** triggerProgs = tprogs.data;
	    uint32_t * triggerIndices = tindexes.data;

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
		    ARGV_t argv = argvNew();
		    argvAdd(&argv, *(triggerProgs + index));
		    rc = runScript(psm, trigH, triggertag(psm->sense), 
			    &argv, triggerScripts[index],
			    arg1, arg2);
		    if (triggersAlreadyRun != NULL)
			triggersAlreadyRun[index] = 1;
		    argvFree(argv);
		}
	    }
	}

	rpmtdFreeData(&tindexes);
	rpmtdFreeData(&tscripts);
	rpmtdFreeData(&tprogs);

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
    unsigned char * triggersRun;
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s tnames, tindexes;

    if (fi->h == NULL)	return rc;	/* XXX can't happen */

    if (!(headerGet(fi->h, RPMTAG_TRIGGERNAME, &tnames, HEADERGET_MINMEM) &&
	  headerGet(fi->h, RPMTAG_TRIGGERINDEX, &tindexes, HEADERGET_MINMEM))) {
	return rc;
    }

    triggersRun = xcalloc(rpmtdCount(&tindexes), sizeof(*triggersRun));
    {	Header sourceH = NULL;
	const char *trigName;
    	rpm_count_t *triggerIndices = tindexes.data;

	while ((trigName = rpmtdNextString(&tnames))) {
	    rpmdbMatchIterator mi;
	    int i = rpmtdGetIndex(&tnames);

	    if (triggersRun[triggerIndices[i]] != 0) continue;
	
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, trigName, 0);

	    while((sourceH = rpmdbNextIterator(mi)) != NULL) {
		rc |= handleOneTrigger(psm, sourceH, fi->h,
				rpmdbGetIteratorCount(mi),
				triggersRun);
	    }

	    mi = rpmdbFreeIterator(mi);
	}
    }
    rpmtdFreeData(&tnames);
    rpmtdFreeData(&tindexes);
    free(triggersRun);
    return rc;
}

static const char * pkgStageString(pkgStage a)
{
    switch(a) {
    case PSM_UNKNOWN:		return "unknown";

    case PSM_PKGINSTALL:	return "  install";
    case PSM_PKGERASE:		return "    erase";
    case PSM_PKGCOMMIT:		return "   commit";

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

rpmRC rpmpsmScriptStage(rpmpsm psm, rpmTag scriptTag, rpmTag progTag)
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

/* FIX: testing null annotation for fi->h */
rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage)
{
    const rpmts ts = psm->ts;
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmfi fi = psm->fi;
    headerGetFlags hgflags = fi->h ? HEADERGET_MINMEM : HEADERGET_ALLOC;
    rpmRC rc = psm->rc;
    int saveerrno;
    int xx;

    switch (stage) {
    case PSM_UNKNOWN:
	break;
    case PSM_INIT:
	rpmlog(RPMLOG_DEBUG, "%s: %s has %d files, test = %d\n",
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

	if (psm->goal == PSM_PKGINSTALL) {
	    Header oh;
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

	    while ((oh = rpmdbNextIterator(psm->mi)) != NULL) {
		fi->record = rpmdbGetIteratorOffset(psm->mi);
		oh = NULL;
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
	    {   const char * p = NULL;
		struct rpmtd_s dprefix;
		if (headerGet(fi->h, RPMTAG_DEFAULTPREFIX, &dprefix, 
							HEADERGET_MINMEM)) {
		    p = rpmtdGetString(&dprefix);
		}
		fi->striplen = p ? strlen(p) + 1 : 1;
	    }
	    fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID | (fi->mapflags & CPIO_SBIT_CHECK);
	
	    {	struct rpmtd_s filenames;
		rpmTag ftag = RPMTAG_FILENAMES;
		if (headerIsEntry(fi->h, RPMTAG_ORIGBASENAMES)) {
		    ftag = RPMTAG_ORIGFILENAMES;
		}
		headerGet(fi->h, ftag, &filenames, HEADERGET_EXT);
		fi->apath = filenames.data; /* Ick.. */
	    }
	
	    /* XXX AFAICT these are "can't happen" cases... */
	    if (fi->fuser == NULL) {
		struct rpmtd_s fuser;
		headerGet(fi->h, RPMTAG_FILEUSERNAME, &fuser, hgflags);
		fi->fuser = fuser.data;
	    }
	    if (fi->fgroup == NULL) {
		struct rpmtd_s fgroup;
		headerGet(fi->h, RPMTAG_FILEGROUPNAME, &fgroup, hgflags);
		fi->fgroup = fgroup.data;
	    }
	    rc = RPMRC_OK;
	}
	if (psm->goal == PSM_PKGERASE) {
	    psm->scriptArg = psm->npkgs_installed - 1;
	
	    /* Retrieve installed header. */
	    rc = rpmpsmNext(psm, PSM_RPMDB_LOAD);
	    if (rc == RPMRC_OK && psm->te)
		rpmteSetHeader(psm->te, fi->h);
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
	break;
    case PSM_POST:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;

	if (psm->goal == PSM_PKGINSTALL) {
	    rpm_time_t installTime = (rpm_time_t) time(NULL);
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

	/* Restore root directory if changed. */
	xx = rpmpsmNext(psm, PSM_CHROOT_OUT);
	break;
    case PSM_UNDO:
	break;
    case PSM_FINI:
	/* Restore root directory if changed. */
	xx = rpmpsmNext(psm, PSM_CHROOT_OUT);

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
	    xx = rpmpsmNext(psm, PSM_NOTIFY);
	}

	if (psm->goal == PSM_PKGERASE) {
	    if (psm->te != NULL) 
		rpmteSetHeader(psm->te, NULL);
	    if (fi->h != NULL)
		fi->h = headerFree(fi->h);
 	}
	psm->failedFile = _free(psm->failedFile);

	fi->fgroup = _free(fi->fgroup);
	fi->fuser = _free(fi->fuser);
	fi->apath = _free(fi->apath);
	fi->fstates = _free(fi->fstates);
	break;

    case PSM_PKGINSTALL:
    case PSM_PKGERASE:
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
	struct rpmtd_s pc;

	headerGet(fi->h, RPMTAG_PAYLOADCOMPRESSOR, &pc, HEADERGET_DEFAULT);
	payload_compressor = rpmtdGetString(&pc);
	if (!payload_compressor)
	    payload_compressor = "gzip";
	if (!strcmp(payload_compressor, "gzip"))
	    psm->rpmio_flags = "r.gzdio";
	if (!strcmp(payload_compressor, "bzip2"))
	    psm->rpmio_flags = "r.bzdio";
	if (!strcmp(payload_compressor, "lzma"))
	    psm->rpmio_flags = "r.lzdio";
	rpmtdFreeData(&pc);

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

	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBADD), 0);
	break;
    case PSM_RPMDB_REMOVE:
	if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)	break;
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	rc = rpmdbRemove(rpmtsGetRdb(ts), rpmtsGetTid(ts), fi->record,
				NULL, NULL);
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	break;

    default:
	break;
   }

   	/* FIX: psm->oh and psm->fi->h may be NULL. */
    return rc;
}
