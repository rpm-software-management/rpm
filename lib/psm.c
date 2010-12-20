/** \ingroup rpmts payload
 * \file lib/psm.c
 * Package state machine to handle a package from a transaction set.
 */

#include "system.h"

#include <errno.h>

#include <rpm/rpmlib.h>		/* rpmvercmp and others */
#include <rpm/rpmmacro.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/argv.h>

#include "lib/cpio.h"
#include "lib/fsm.h"		/* XXX CPIO_FOO/FSM_FOO constants */
#include "lib/rpmchroot.h"
#include "lib/rpmfi_internal.h" /* XXX replaced/states... */
#include "lib/rpmte_internal.h"	/* XXX internal apis */
#include "lib/rpmdb_internal.h" /* rpmdbAdd/Remove */
#include "lib/rpmscript.h"

#include "debug.h"

typedef enum pkgStage_e {
    PSM_UNKNOWN		=  0,
    PSM_INIT		=  1,
    PSM_PRE		=  2,
    PSM_PROCESS		=  3,
    PSM_POST		=  4,
    PSM_UNDO		=  5,
    PSM_FINI		=  6,

    PSM_CREATE		= 17,
    PSM_NOTIFY		= 22,
    PSM_DESTROY		= 23,

    PSM_SCRIPT		= 53,
    PSM_TRIGGERS	= 54,
    PSM_IMMED_TRIGGERS	= 55,

    PSM_RPMDB_ADD	= 98,
    PSM_RPMDB_REMOVE	= 99

} pkgStage;

typedef struct rpmpsm_s {
    rpmts ts;			/*!< transaction set */
    rpmte te;			/*!< current transaction element */
    rpmfi fi;			/*!< transaction element file info */
    const char * goalName;
    char * failedFile;
    rpmTagVal scriptTag;	/*!< Scriptlet data tag. */
    int npkgs_installed;	/*!< No. of installed instances. */
    int scriptArg;		/*!< Scriptlet package arg. */
    rpmsenseFlags sense;	/*!< One of RPMSENSE_TRIGGER{PREIN,IN,UN,POSTUN}. */
    int countCorrection;	/*!< 0 if installing, -1 if removing. */
    rpmCallbackType what;	/*!< Callback type. */
    rpm_loff_t amount;		/*!< Callback amount. */
    rpm_loff_t total;		/*!< Callback total. */
    pkgGoal goal;
    pkgStage stage;		/*!< Current psm stage. */
    pkgStage nstage;		/*!< Next psm stage. */

    int nrefs;			/*!< Reference count. */
} * rpmpsm;

static rpmpsm rpmpsmNew(rpmts ts, rpmte te);
static rpmpsm rpmpsmFree(rpmpsm psm);
static rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage);

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
 * @param h		header
 * @return		0 always
 */
static void rpmInstallLoadMacros(Header h)
{
    const struct tagMacro * tagm;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	struct rpmtd_s td;
	char *body;
	if (!headerGet(h, tagm->tag, &td, HEADERGET_DEFAULT))
	    continue;

	switch (rpmtdType(&td)) {
	default:
	    body = rpmtdFormat(&td, RPMTD_FORMAT_STRING, NULL);
	    addMacro(NULL, tagm->macroname, NULL, body, -1);
	    free(body);
	    break;
	case RPM_NULL_TYPE:
	    break;
	}
	rpmtdFreeData(&td);
    }
}

/**
 * Mark files in database shared with this package as "replaced".
 * @param psm		package state machine data
 * @return		0 always
 */
static rpmRC markReplacedFiles(const rpmpsm psm)
{
    const rpmts ts = psm->ts;
    rpmfs fs = rpmteGetFileStates(psm->te);
    sharedFileInfo replaced = rpmfsGetReplaced(fs);
    sharedFileInfo sfi;
    rpmdbMatchIterator mi;
    Header h;
    int * offsets;
    unsigned int prev;
    int num, xx;

    if (!replaced)
	return RPMRC_OK;

    num = prev = 0;
    for (sfi = replaced; sfi; sfi=rpmfsNextReplaced(fs, sfi)) {
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
    for (sfi = replaced; sfi; sfi=rpmfsNextReplaced(fs, sfi)) {
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
	while (sfi && sfi->otherPkg == prev) {
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
	    sfi=rpmfsNextReplaced(fs, sfi);
	}
	rpmtdFreeData(&secStates);
    }
    mi = rpmdbFreeIterator(mi);
    free(offsets);

    return RPMRC_OK;
}

static int rpmlibDeps(Header h)
{
    rpmds req = rpmdsInit(rpmdsNew(h, RPMTAG_REQUIRENAME, 0));
    rpmds rpmlib = NULL;
    rpmdsRpmlib(&rpmlib, NULL);
    int rc = 1;
    char *nvr = NULL;
    while (rpmdsNext(req) >= 0) {
	if (!(rpmdsFlags(req) & RPMSENSE_RPMLIB))
	    continue;
	if (rpmdsSearch(rpmlib, req) < 0) {
	    if (!nvr) {
		nvr = headerGetAsString(h, RPMTAG_NEVRA);
		rpmlog(RPMLOG_ERR, _("Missing rpmlib features for %s:\n"), nvr);
	    }
	    rpmlog(RPMLOG_ERR, "\t%s\n", rpmdsDNEVR(req)+2);
	    rc = 0;
	}
    }
    rpmdsFree(req);
    rpmdsFree(rpmlib);
    free(nvr);
    return rc;
}

rpmRC rpmInstallSourcePackage(rpmts ts, FD_t fd,
		char ** specFilePtr, char ** cookie)
{
    rpmfi fi = NULL;
    char * specFile = NULL;
    const char *rootdir = rpmtsRootDir(ts);
    Header h = NULL;
    rpmpsm psm = NULL;
    rpmte te = NULL;
    rpmRC rpmrc;
    int specix = -1;
    struct rpmtd_s filenames;

    rpmtdReset(&filenames);
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

    rpmrc = RPMRC_FAIL; /* assume failure */

    if (!headerIsSource(h)) {
	rpmlog(RPMLOG_ERR, _("source package expected, binary found\n"));
	goto exit;
    }

    /* src.rpm install can require specific rpmlib features, check them */
    if (!rpmlibDeps(h))
	goto exit;

    if (headerGet(h, RPMTAG_BASENAMES, &filenames, HEADERGET_ALLOC)) {
	struct rpmtd_s td;
	const char *str;
	const char *_cookie = headerGetString(h, RPMTAG_COOKIE);
	if (cookie && _cookie) *cookie = xstrdup(_cookie);
	
	/* Try to find spec by file flags */
	if (_cookie && headerGet(h, RPMTAG_FILEFLAGS, &td, HEADERGET_MINMEM)) {
	    rpmfileAttrs *flags;
	    while (specix < 0 && (flags = rpmtdNextUint32(&td))) {
		if (*flags & RPMFILE_SPECFILE)
		    specix = rpmtdGetIndex(&td);
	    }
	}
	/* Still no spec? Look by filename. */
	while (specix < 0 && (str = rpmtdNextString(&filenames))) {
	    if (rpmFileHasSuffix(str, ".spec")) 
		specix = rpmtdGetIndex(&filenames);
	}
    }

    if (rootdir && rstreq(rootdir, "/"))
	rootdir = NULL;

    /* Macros need to be added before trying to create directories */
    rpmInstallLoadMacros(h);

    if (specix >= 0) {
	const char *bn;

	headerDel(h, RPMTAG_BASENAMES);
	headerDel(h, RPMTAG_DIRNAMES);
	headerDel(h, RPMTAG_DIRINDEXES);

	rpmtdInit(&filenames);
	for (int i = 0; (bn = rpmtdNextString(&filenames)); i++) {
	    int spec = (i == specix);
	    char *fn = rpmGenPath(rpmtsRootDir(ts),
				  spec ? "%{_specdir}" : "%{_sourcedir}", bn);
	    headerPutString(h, RPMTAG_OLDFILENAMES, fn);
	    if (spec) specFile = xstrdup(fn);
	    free(fn);
	}
	headerConvert(h, HEADERCONV_COMPRESSFILELIST);
    } else {
	rpmlog(RPMLOG_ERR, _("source package contains no .spec file\n"));
	goto exit;
    };

    if (rpmtsAddInstallElement(ts, h, NULL, 0, NULL)) {
	goto exit;
    }

    te = rpmtsElement(ts, 0);
    if (te == NULL) {	/* XXX can't happen */
	goto exit;
    }
    rpmteSetFd(te, fd);

    rpmteSetHeader(te, h);
    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, RPMFI_KEEPHEADER);
    h = headerFree(h);

    if (fi == NULL) {	/* XXX can't happen */
	goto exit;
    }
    fi->apath = filenames.data; /* Ick */
    rpmteSetFI(te, fi);
    fi = rpmfiFree(fi);

    if (rpmMkdirs(rpmtsRootDir(ts), "%{_topdir}:%{_sourcedir}:%{_specdir}")) {
	goto exit;
    }

    {
	/* set all files to be installed */
	rpmfs fs = rpmteGetFileStates(te);
	int i;
	unsigned int fc = rpmfiFC(fi);
	for (i=0; i<fc; i++) rpmfsSetAction(fs, i, FA_CREATE);
    }

    psm = rpmpsmNew(ts, te);
    psm->goal = PKG_INSTALL;

   	/* FIX: psm->fi->dnl should be owned. */
    if (rpmpsmStage(psm, PSM_PROCESS) == RPMRC_OK)
	rpmrc = RPMRC_OK;

    (void) rpmpsmStage(psm, PSM_FINI);
    psm = rpmpsmFree(psm);

exit:
    if (specFilePtr && specFile && rpmrc == RPMRC_OK)
	*specFilePtr = specFile;
    else
	specFile = _free(specFile);

    if (h != NULL) h = headerFree(h);
    if (fi != NULL) fi = rpmfiFree(fi);

    /* XXX nuke the added package(s). */
    rpmtsClean(ts);

    return rpmrc;
}

static rpmTagVal triggertag(rpmsenseFlags sense) 
{
    rpmTagVal tag = RPMTAG_NOT_FOUND;
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
 * Run a scriptlet with args.
 *
 * Run a script with an interpreter. If the interpreter is not specified,
 * /bin/sh will be used. If the interpreter is /bin/sh, then the args from
 * the header will be ignored, passing instead arg1 and arg2.
 *
 * @param psm		package state machine data
 * @param prefixes	install prefixes
 * @param script	scriptlet from header
 * @param arg1		no. instances of package installed after scriptlet exec
 *			(-1 is no arg)
 * @param arg2		ditto, but for the target package
 * @return		0 on success
 */
static rpmRC runScript(rpmpsm psm, ARGV_const_t prefixes, 
		       rpmScript script, int arg1, int arg2)
{
    rpmRC rc = RPMRC_OK;
    int warn_only = (script->tag != RPMTAG_PREIN &&
		     script->tag != RPMTAG_PREUN &&
		     script->tag != RPMTAG_VERIFYSCRIPT);
    int selinux = !(rpmtsFlags(psm->ts) & RPMTRANS_FLAG_NOCONTEXTS);

    rpmswEnter(rpmtsOp(psm->ts, RPMTS_OP_SCRIPTLETS), 0);
    rc = rpmScriptRun(script, arg1, arg2, rpmtsScriptFd(psm->ts),
		      prefixes, warn_only, selinux);
    rpmswExit(rpmtsOp(psm->ts, RPMTS_OP_SCRIPTLETS), 0);

    /* 
     * Notify callback for all errors. "total" abused for warning/error,
     * rc only reflects whether the condition prevented install/erase 
     * (which is only happens with %prein and %preun scriptlets) or not.
     */
    if (rc != RPMRC_OK) {
	if (warn_only) {
	    rc = RPMRC_OK;
	}
	rpmtsNotify(psm->ts, psm->te, RPMCALLBACK_SCRIPT_ERROR, script->tag, rc);
    }

    return rc;
}

static rpmRC runInstScript(rpmpsm psm)
{
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s pfx;
    Header h = rpmteHeader(psm->te);
    rpmScript script = rpmScriptFromTag(h, psm->scriptTag);

    if (script) {
	headerGet(h, RPMTAG_INSTPREFIXES, &pfx, HEADERGET_ALLOC|HEADERGET_ARGV);
	rc = runScript(psm, pfx.data, script, psm->scriptArg, -1);
	rpmtdFreeData(&pfx);
    }

    rpmScriptFree(script);
    headerFree(h);

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
    const rpmts ts = psm->ts;
    rpmds trigger = rpmdsInit(rpmdsNew(trigH, RPMTAG_TRIGGERNAME, 0));
    struct rpmtd_s pfx;
    const char * sourceName = headerGetString(sourceH, RPMTAG_NAME);
    const char * triggerName = headerGetString(trigH, RPMTAG_NAME);
    rpmRC rc = RPMRC_OK;
    int i;

    if (trigger == NULL)
	return rc;

    headerGet(trigH, RPMTAG_INSTPREFIXES, &pfx, HEADERGET_ALLOC|HEADERGET_ARGV);
    (void) rpmdsSetNoPromote(trigger, 1);

    while ((i = rpmdsNext(trigger)) >= 0) {
	struct rpmtd_s tscripts, tprogs, tindexes, tflags;
	headerGetFlags hgflags = HEADERGET_MINMEM;

	if (!(rpmdsFlags(trigger) & psm->sense))
	    continue;

 	if (!rstreq(rpmdsN(trigger), sourceName))
	    continue;

	/* XXX Trigger on any provided dependency, not just the package NEVR */
	if (!rpmdsAnyMatchesDep(sourceH, trigger, 1))
	    continue;

	/* XXX FIXME: this leaks memory if scripts or progs retrieve fails */
	if (!(headerGet(trigH, RPMTAG_TRIGGERINDEX, &tindexes, hgflags) &&
	      headerGet(trigH, RPMTAG_TRIGGERSCRIPTS, &tscripts, hgflags) &&
	      headerGet(trigH, RPMTAG_TRIGGERSCRIPTPROG, &tprogs, hgflags))) {
	    continue;
	} else {
	    int arg1 = rpmdbCountPackages(rpmtsGetRdb(ts), triggerName);
	    char ** triggerScripts = tscripts.data;
	    char ** triggerProgs = tprogs.data;
	    uint32_t * triggerIndices = tindexes.data;
	    uint32_t * triggerFlags = NULL;
	    uint32_t ix = triggerIndices[i];

	    headerGet(trigH, RPMTAG_TRIGGERSCRIPTFLAGS, &tflags, hgflags);
	    triggerFlags = tflags.data;

	    if (arg1 < 0) {
		/* XXX W2DO? fails as "execution of script failed" */
		rc = RPMRC_FAIL;
	    } else {
		arg1 += psm->countCorrection;

		if (triggersAlreadyRun == NULL || triggersAlreadyRun[ix] == 0) {
		    /* XXX TODO add rpmScript API to handle this, ugh */
		    char *macro = NULL;
		    char *qformat = NULL;
		    char *args[2] = { triggerProgs[ix], NULL };
		    struct rpmScript_s script = {
			.tag = triggertag(psm->sense),
			.body = triggerScripts[ix],
			.flags = triggerFlags ? triggerFlags[ix] : 0,
			.args = args
		    };

		    if (script.body && (script.flags & RPMSCRIPT_EXPAND)) {
			macro = rpmExpand(script.body, NULL);
			script.body = macro;
		    }
		    if (script.body && (script.flags & RPMSCRIPT_QFORMAT)) {
			qformat = headerFormat(trigH, script.body, NULL);
			script.body = qformat;
		    }

		    rc = runScript(psm, pfx.data, &script, arg1, arg2);
		    if (triggersAlreadyRun != NULL)
			triggersAlreadyRun[ix] = 1;
		    free(macro);
		    free(qformat);
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

    rpmtdFreeData(&pfx);
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
    int numPackage = -1;
    const char * N = NULL;
    int nerrors = 0;

    if (psm->te) 	/* XXX can't happen */
	N = rpmteN(psm->te);
    if (N) 		/* XXX can't happen */
	numPackage = rpmdbCountPackages(rpmtsGetRdb(ts), N)
				+ psm->countCorrection;
    if (numPackage < 0)
	return RPMRC_NOTFOUND;

    {	Header triggeredH;
	Header h = rpmteHeader(psm->te);
	rpmdbMatchIterator mi;
	int countCorrection = psm->countCorrection;

	psm->countCorrection = 0;
	mi = rpmtsInitIterator(ts, RPMDBI_TRIGGERNAME, N, 0);
	while((triggeredH = rpmdbNextIterator(mi)) != NULL)
	    nerrors += handleOneTrigger(psm, h, triggeredH, numPackage, NULL);
	mi = rpmdbFreeIterator(mi);
	psm->countCorrection = countCorrection;
	headerFree(h);
    }

    return (nerrors == 0) ? RPMRC_OK : RPMRC_FAIL;
}

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @return		0 on success
 */
static rpmRC runImmedTriggers(rpmpsm psm)
{
    const rpmts ts = psm->ts;
    unsigned char * triggersRun;
    struct rpmtd_s tnames, tindexes;
    Header h = rpmteHeader(psm->te);
    int nerrors = 0;

    if (!(headerGet(h, RPMTAG_TRIGGERNAME, &tnames, HEADERGET_MINMEM) &&
	  headerGet(h, RPMTAG_TRIGGERINDEX, &tindexes, HEADERGET_MINMEM))) {
	goto exit;
    }

    triggersRun = xcalloc(rpmtdCount(&tindexes), sizeof(*triggersRun));
    {	Header sourceH = NULL;
	const char *trigName;
    	rpm_count_t *triggerIndices = tindexes.data;

	while ((trigName = rpmtdNextString(&tnames))) {
	    rpmdbMatchIterator mi;
	    int i = rpmtdGetIndex(&tnames);

	    if (triggersRun[triggerIndices[i]] != 0) continue;
	
	    mi = rpmtsInitIterator(ts, RPMDBI_NAME, trigName, 0);

	    while((sourceH = rpmdbNextIterator(mi)) != NULL) {
		nerrors += handleOneTrigger(psm, sourceH, h,
				rpmdbGetIteratorCount(mi),
				triggersRun);
	    }

	    mi = rpmdbFreeIterator(mi);
	}
    }
    rpmtdFreeData(&tnames);
    rpmtdFreeData(&tindexes);
    free(triggersRun);

exit:
    headerFree(h);
    return (nerrors == 0) ? RPMRC_OK : RPMRC_FAIL;
}

static rpmpsm rpmpsmFree(rpmpsm psm)
{
    if (psm == NULL)
	return NULL;

    psm->fi = rpmfiFree(psm->fi);
#ifdef	NOTYET
    psm->te = rpmteFree(psm->te);
#else
    psm->te = NULL;
#endif
    psm->ts = rpmtsFree(psm->ts);

    memset(psm, 0, sizeof(*psm));		/* XXX trash and burn */
    psm = _free(psm);

    return NULL;
}

static rpmpsm rpmpsmNew(rpmts ts, rpmte te)
{
    rpmpsm psm = xcalloc(1, sizeof(*psm));

    if (ts)	psm->ts = rpmtsLink(ts);
    if (te) {
#ifdef	NOTYET
	psm->te = rpmteLink(te, RPMDBG_M("rpmpsmNew")); 
#else
	psm->te = te;
#endif
    	psm->fi = rpmfiLink(rpmteFI(te));
    }

    return psm;
}

static rpmRC rpmpsmNext(rpmpsm psm, pkgStage nstage)
{
    psm->nstage = nstage;
    return rpmpsmStage(psm, psm->nstage);
}

static rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage)
{
    const rpmts ts = psm->ts;
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmfi fi = psm->fi;
    rpmRC rc = RPMRC_OK;
    int saveerrno;
    int xx;

    switch (stage) {
    case PSM_UNKNOWN:
	break;
    case PSM_INIT:
	rpmlog(RPMLOG_DEBUG, "%s: %s has %d files\n",
		psm->goalName, rpmteNEVR(psm->te), rpmfiFC(fi));

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

	if (psm->goal == PKG_INSTALL) {
	    rpmdbMatchIterator mi;
	    Header oh;

	    psm->scriptArg = psm->npkgs_installed + 1;

	    mi = rpmtsInitIterator(ts, RPMDBI_NAME, rpmteN(psm->te), 0);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP,
			rpmteE(psm->te));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP,
			rpmteV(psm->te));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP,
			rpmteR(psm->te));
	    if (tscolor) {
		xx = rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP,
			rpmteA(psm->te));
		xx = rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP,
			rpmteO(psm->te));
	    }

	    while ((oh = rpmdbNextIterator(mi)) != NULL) {
		rpmteSetDBInstance(psm->te, rpmdbGetIteratorOffset(mi));
		oh = NULL;
		break;
	    }
	    mi = rpmdbFreeIterator(mi);
	    rc = RPMRC_OK;

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;
	
	    if (rpmfiFC(fi) > 0) {
		struct rpmtd_s filenames;
		rpmTag ftag = RPMTAG_FILENAMES;
		Header h = rpmteHeader(psm->te);
	
		if (headerIsEntry(h, RPMTAG_ORIGBASENAMES)) {
		    ftag = RPMTAG_ORIGFILENAMES;
		}
		headerGet(h, ftag, &filenames, HEADERGET_EXT);
		fi->apath = filenames.data; /* Ick.. */
		headerFree(h);
	    }
	}
	if (psm->goal == PKG_ERASE) {
	    psm->scriptArg = psm->npkgs_installed - 1;
	}
	break;
    case PSM_PRE:
	if (psm->goal == PKG_INSTALL) {
	    psm->scriptTag = RPMTAG_PREIN;
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
		if (rc) break;
	    }
	}

	if (psm->goal == PKG_ERASE) {
	    psm->scriptTag = RPMTAG_PREUN;
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
	if (psm->goal == PKG_INSTALL) {
	    FD_t payload = NULL;

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;

	    /* XXX Synthesize callbacks for packages with no files. */
	    if (rpmfiFC(fi) <= 0) {
		void * ptr;
		ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_INST_START, 0, 100);
		ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_INST_PROGRESS, 100, 100);
		break;
	    }

	    payload = rpmtePayload(psm->te);
	    if (payload == NULL) {
		rc = RPMRC_FAIL;
		break;
	    }

	    rc = fsmSetup(rpmfiFSM(fi), FSM_PKGINSTALL, ts, psm->te, fi,
			payload, NULL, &psm->failedFile);
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_UNCOMPRESS),
			fdOp(payload, FDSTAT_READ));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
			fdOp(payload, FDSTAT_DIGEST));
	    xx = fsmTeardown(rpmfiFSM(fi));

	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(payload);
	    errno = saveerrno; /* XXX FIXME: Fclose with libio destroys errno */

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
	if (psm->goal == PKG_ERASE) {
	    int fc = rpmfiFC(fi);

	    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)	break;

	    /* XXX Synthesize callbacks for packages with no files. */
	    if (rpmfiFC(fi) <= 0) {
		void * ptr;
		ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_UNINST_START, 0, 100);
		ptr = rpmtsNotify(ts, psm->te, RPMCALLBACK_UNINST_STOP, 0, 100);
		break;
	    }

	    psm->what = RPMCALLBACK_UNINST_START;
	    psm->amount = fc;		/* XXX W2DO? looks wrong. */
	    psm->total = fc;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	    rc = fsmSetup(rpmfiFSM(fi), FSM_PKGERASE, ts, psm->te, fi,
			NULL, NULL, &psm->failedFile);
	    xx = fsmTeardown(rpmfiFSM(fi));

	    psm->what = RPMCALLBACK_UNINST_STOP;
	    psm->amount = 0;		/* XXX W2DO? looks wrong. */
	    psm->total = fc;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);

	}
	break;
    case PSM_POST:
	if (psm->goal == PKG_INSTALL) {
	    rpm_time_t installTime = (rpm_time_t) time(NULL);
	    rpmfs fs = rpmteGetFileStates(psm->te);
	    rpm_count_t fc = rpmfsFC(fs);
	    rpm_fstate_t * fileStates = rpmfsGetStates(fs);
	    Header h = rpmteHeader(psm->te);

	    if (fileStates != NULL && fc > 0) {
		headerPutChar(h, RPMTAG_FILESTATES, fileStates, fc);
	    }

	    headerPutUint32(h, RPMTAG_INSTALLTIME, &installTime, 1);
	    headerPutUint32(h, RPMTAG_INSTALLCOLOR, &tscolor, 1);
	    headerFree(h);

	    /*
	     * If this package has already been installed, remove it from
	     * the database before adding the new one.
	     */
	    if (rpmteDBInstance(psm->te)) {
		rc = rpmpsmNext(psm, PSM_RPMDB_REMOVE);
		if (rc) break;
	    }

	    rc = rpmpsmNext(psm, PSM_RPMDB_ADD);
	    if (rc) break;

	    psm->scriptTag = RPMTAG_POSTIN;
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

	    rc = markReplacedFiles(psm);

	}
	if (psm->goal == PKG_ERASE) {

	    psm->scriptTag = RPMTAG_POSTUN;
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

	    rc = rpmpsmNext(psm, PSM_RPMDB_REMOVE);
	}
	break;
    case PSM_UNDO:
	break;
    case PSM_FINI:
	if (rc) {
	    if (psm->failedFile)
		rpmlog(RPMLOG_ERR,
			_("%s failed on file %s: %s\n"),
			psm->goalName, psm->failedFile, cpioStrerror(rc));
	    else
		rpmlog(RPMLOG_ERR, _("%s failed: %s\n"),
			psm->goalName, cpioStrerror(rc));

	    /* XXX notify callback on error. */
	    psm->what = RPMCALLBACK_CPIO_ERROR;
	    psm->amount = 0;
	    psm->total = 0;
	    xx = rpmpsmNext(psm, PSM_NOTIFY);
	}

	psm->failedFile = _free(psm->failedFile);

	fi->apath = _free(fi->apath);
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

    case PSM_RPMDB_ADD: {
	Header h = rpmteHeader(psm->te);

	if (!headerIsEntry(h, RPMTAG_INSTALLTID)) {
	    rpm_tid_t tid = rpmtsGetTid(ts);
	    if (tid != 0 && tid != (rpm_tid_t)-1)
		headerPutUint32(h, RPMTAG_INSTALLTID, &tid, 1);
	}
	
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBADD), 0);
	rc = (rpmdbAdd(rpmtsGetRdb(ts), h) == 0) ? RPMRC_OK : RPMRC_FAIL;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBADD), 0);

	if (rc == RPMRC_OK)
	    rpmteSetDBInstance(psm->te, headerGetInstance(h));
	headerFree(h);
    }   break;

    case PSM_RPMDB_REMOVE:
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	rc = (rpmdbRemove(rpmtsGetRdb(ts), rpmteDBInstance(psm->te)) == 0) ?
						    RPMRC_OK : RPMRC_FAIL;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
	if (rc == RPMRC_OK)
	    rpmteSetDBInstance(psm->te, 0);
	break;

    default:
	break;
   }

    return rc;
}

static const char * pkgGoalString(pkgGoal goal)
{
    switch(goal) {
    case PKG_INSTALL:	return "  install";
    case PKG_ERASE:	return "    erase";
    case PKG_VERIFY:	return "   verify";
    case PKG_PRETRANS:	return " pretrans";
    case PKG_POSTTRANS:	return "posttrans";
    default:		return "unknown";
    }
}

rpmRC rpmpsmRun(rpmts ts, rpmte te, pkgGoal goal)
{
    rpmpsm psm = NULL;
    rpmRC rc = RPMRC_FAIL;

    /* Psm can't fail in test mode, just return early */
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)
	return RPMRC_OK;

    psm = rpmpsmNew(ts, te);
    if (rpmChrootIn() == 0) {
	rpmtsOpX op;
	psm->goal = goal;
	psm->goalName = pkgGoalString(goal);

	switch (goal) {
	case PKG_INSTALL:
	case PKG_ERASE:
	    op = (goal == PKG_INSTALL) ? RPMTS_OP_INSTALL : RPMTS_OP_ERASE;
	    rpmswEnter(rpmtsOp(psm->ts, op), 0);

	    rc = rpmpsmNext(psm, PSM_INIT);
	    if (!rc) rc = rpmpsmNext(psm, PSM_PRE);
	    if (!rc) rc = rpmpsmNext(psm, PSM_PROCESS);
	    if (!rc) rc = rpmpsmNext(psm, PSM_POST);
	    (void) rpmpsmNext(psm, PSM_FINI);

	    rpmswExit(rpmtsOp(psm->ts, op), 0);
	    break;
	case PKG_PRETRANS:
	case PKG_POSTTRANS:
	case PKG_VERIFY:
	    psm->scriptTag = goal;
	    rc = rpmpsmStage(psm, PSM_SCRIPT);
	    break;
	default:
	    break;
	}
	/* XXX an error here would require a full abort */
	(void) rpmChrootOut();
    }
    rpmpsmFree(psm);
    return rc;
}
