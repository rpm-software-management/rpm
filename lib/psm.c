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

#include "lib/fsm.h"		/* XXX CPIO_FOO/FSM_FOO constants */
#include "lib/rpmchroot.h"
#include "lib/rpmfi_internal.h" /* XXX replaced/states... */
#include "lib/rpmte_internal.h"	/* XXX internal apis */
#include "lib/rpmdb_internal.h" /* rpmdbAdd/Remove */
#include "lib/rpmts_internal.h" /* rpmtsPlugins() etc */
#include "lib/rpmds_internal.h" /* rpmdsFilterTi() */
#include "lib/rpmscript.h"
#include "lib/misc.h"
#include "lib/rpmtriggers.h"

#include "lib/rpmplugins.h"

#include "debug.h"

struct rpmpsm_s {
    rpmts ts;			/*!< transaction set */
    rpmte te;			/*!< current transaction element */
    rpmfiles files;		/*!< transaction element file info */
    int scriptArg;		/*!< Scriptlet package arg. */
    int countCorrection;	/*!< 0 if installing, -1 if removing. */
    rpmCallbackType what;	/*!< Callback type. */
    rpm_loff_t amount;		/*!< Callback amount. */
    rpm_loff_t total;		/*!< Callback total. */

    int nrefs;			/*!< Reference count. */
};

static rpmpsm rpmpsmNew(rpmts ts, rpmte te, pkgGoal goal);
static rpmRC rpmpsmUnpack(rpmpsm psm);
static rpmpsm rpmpsmFree(rpmpsm psm);
static const char * pkgGoalString(pkgGoal goal);

/**
 * Adjust file states in database for files shared with this package:
 * currently either "replaced" or "wrong color".
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
    unsigned int * offsets;
    unsigned int prev;
    unsigned int num;

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
    rpmdbAppendIterator(mi, offsets, num);
    rpmdbSetIteratorRewrite(mi, 1);

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
	    if (state && *state != sfi->rstate) {
		*state = sfi->rstate;
		if (modified == 0) {
		    /* Modified header will be rewritten. */
		    modified = 1;
		    rpmdbSetIteratorModified(mi, modified);
		}
		num++;
	    }
	    sfi=rpmfsNextReplaced(fs, sfi);
	}
	rpmtdFreeData(&secStates);
    }
    rpmdbFreeIterator(mi);
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
	if (rpmdsFlags(req) & RPMSENSE_MISSINGOK)
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
    Header h = NULL;
    rpmpsm psm = NULL;
    rpmte te = NULL;
    rpmRC rpmrc;
    int specix = -1;

    rpmrc = rpmReadPackageFile(ts, fd, NULL, &h);
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

    specix = headerFindSpec(h);

    if (specix < 0) {
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

    {
	/* set all files to be installed */
	rpmfs fs = rpmteGetFileStates(te);
	int fc = rpmfsFC(fs);
	for (int i = 0; i < fc; i++)
	    rpmfsSetAction(fs, i, FA_CREATE);
    }

    psm = rpmpsmNew(ts, te, PKG_INSTALL);

    if (rpmpsmUnpack(psm) == RPMRC_OK)
	rpmrc = RPMRC_OK;

    rpmpsmFree(psm);

exit:
    if (rpmrc == RPMRC_OK && specix >= 0) {
	if (cookie)
	    *cookie = headerGetAsString(h, RPMTAG_COOKIE);
	if (specFilePtr) {
	    rpmfiles files = rpmteFiles(te);
	    *specFilePtr = rpmfilesFN(files, specix);
	    rpmfilesFree(files);
	}
    }

    /* XXX nuke the added package(s). */
    headerFree(h);
    rpmtsEmpty(ts);

    return rpmrc;
}

static rpmRC runInstScript(rpmpsm psm, rpmTagVal scriptTag)
{
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s pfx;
    Header h = rpmteHeader(psm->te);
    rpmScript script = rpmScriptFromTag(h, scriptTag);

    if (script) {
	headerGet(h, RPMTAG_INSTPREFIXES, &pfx, HEADERGET_ALLOC|HEADERGET_ARGV);
	rc = runScript(psm->ts, psm->te, h, pfx.data, script, psm->scriptArg, -1);
	rpmtdFreeData(&pfx);
    }

    rpmScriptFree(script);
    headerFree(h);

    return rc;
}

/**
 * Execute triggers.
 * @todo Trigger on any provides, not just package NVR.
 * @param ts		transaction set
 * @param te		transaction element
 * @param sense		trigger type
 * @param sourceH	header of trigger source
 * @param trigH		header of triggered package
 * @param arg2
 * @param triggersAlreadyRun
 * @return
 */
static rpmRC handleOneTrigger(rpmts ts, rpmte te, rpmsenseFlags sense,
			Header sourceH, Header trigH, int countCorrection,
			int arg2, unsigned char * triggersAlreadyRun)
{
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
	uint32_t tix;

	if (!(rpmdsFlags(trigger) & sense))
	    continue;

 	if (!rstreq(rpmdsN(trigger), sourceName))
	    continue;

	/* XXX Trigger on any provided dependency, not just the package NEVR */
	if (!rpmdsAnyMatchesDep(sourceH, trigger, 1))
	    continue;

	tix = rpmdsTi(trigger);
	if (triggersAlreadyRun == NULL || triggersAlreadyRun[tix] == 0) {
	    int arg1 = rpmdbCountPackages(rpmtsGetRdb(ts), triggerName);

	    if (arg1 < 0) {
		/* XXX W2DO? fails as "execution of script failed" */
		rc = RPMRC_FAIL;
	    } else {
		rpmScript script = rpmScriptFromTriggerTag(trigH,
			     triggertag(sense), RPMSCRIPT_NORMALTRIGGER, tix);
		arg1 += countCorrection;
		rc = runScript(ts, te, trigH, pfx.data, script, arg1, arg2);
		if (triggersAlreadyRun != NULL)
		    triggersAlreadyRun[tix] = 1;

		rpmScriptFree(script);
	    }
	}

	/*
	 * Each target/source header pair can only result in a single
	 * script being run.
	 */
	break;
    }

    rpmtdFreeData(&pfx);
    rpmdsFree(trigger);

    return rc;
}

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param psm		package state machine data
 * @param sense		trigger type
 * @return		0 on success
 */
static rpmRC runTriggers(rpmpsm psm, rpmsenseFlags sense)
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

	mi = rpmtsInitIterator(ts, RPMDBI_TRIGGERNAME, N, 0);
	while ((triggeredH = rpmdbNextIterator(mi)) != NULL) {
	    nerrors += handleOneTrigger(ts, NULL, sense, h, triggeredH,
					0, numPackage, NULL);
	}
	rpmdbFreeIterator(mi);
	headerFree(h);
    }

    return (nerrors == 0) ? RPMRC_OK : RPMRC_FAIL;
}

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @param sense		trigger type
 * @return		0 on success
 */
static rpmRC runImmedTriggers(rpmpsm psm, rpmsenseFlags sense)
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

	    while ((sourceH = rpmdbNextIterator(mi)) != NULL) {
		nerrors += handleOneTrigger(psm->ts, psm->te,
				sense, sourceH, h,
				psm->countCorrection,
				rpmdbGetIteratorCount(mi),
				triggersRun);
	    }

	    rpmdbFreeIterator(mi);
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
    if (psm) {
	rpmfilesFree(psm->files);
	rpmtsFree(psm->ts),
	/* XXX rpmte not refcounted yet */
	memset(psm, 0, sizeof(*psm)); /* XXX trash and burn */
    	free(psm);
    }
    return NULL;
}

static rpmpsm rpmpsmNew(rpmts ts, rpmte te, pkgGoal goal)
{
    rpmpsm psm = xcalloc(1, sizeof(*psm));
    psm->ts = rpmtsLink(ts);
    psm->files = rpmteFiles(te);
    psm->te = te; /* XXX rpmte not refcounted yet */
    if (!rpmteIsSource(te)) {
	/*
	 * When we run scripts, we pass an argument which is the number of
	 * versions of this package that will be installed when we are
	 * finished.
	 */
	int npkgs_installed = rpmdbCountPackages(rpmtsGetRdb(ts), rpmteN(te));
	switch (goal) {
	case PKG_INSTALL:
	case PKG_PRETRANS:
	    psm->scriptArg = npkgs_installed + 1;
	    psm->countCorrection = 0;
	    break;
	case PKG_ERASE:
	    psm->scriptArg = npkgs_installed - 1;
	    psm->countCorrection = -1;
	    break;
	case PKG_VERIFY:
	case PKG_POSTTRANS:
	    psm->scriptArg = npkgs_installed;
	    psm->countCorrection = 0;
	    break;
	default:
	    break;
	}
    }

    if (goal == PKG_INSTALL) {
	Header h = rpmteHeader(te);
	psm->total = headerGetNumber(h, RPMTAG_LONGARCHIVESIZE);
	headerFree(h);
    } else if (goal == PKG_ERASE) {
	psm->total = rpmfilesFC(psm->files);
    }
    /* Fake up something for packages with no files */
    if (psm->total == 0)
	psm->total = 100;

    if (goal == PKG_INSTALL || goal == PKG_ERASE) {
	rpmlog(RPMLOG_DEBUG, "%s: %s has %d files\n", pkgGoalString(goal),
	    rpmteNEVRA(psm->te), rpmfilesFC(psm->files));
    }

    return psm;
}

void rpmpsmNotify(rpmpsm psm, int what, rpm_loff_t amount)
{
    if (psm) {
	int changed = 0;
	if (amount > psm->total)
	    amount = psm->total;
	if (amount > psm->amount) {
	    psm->amount = amount;
	    changed = 1;
	}
	if (what && what != psm->what) {
	    psm->what = what;
	    changed = 1;
	}
	if (changed) {
	   rpmtsNotify(psm->ts, psm->te, psm->what, psm->amount, psm->total);
	}
    }
}

/*
 * --replacepkgs hack: find the header instance we're replacing and
 * mark it as the db instance of the install element. In PSM_POST,
 * if an install element already has a db instance, it's removed
 * before proceeding with the adding the new header to the db.
 */
static void markReplacedInstance(rpmts ts, rpmte te)
{
    rpmdbMatchIterator mi = rpmtsPrunedIterator(ts, RPMDBI_NAME, rpmteN(te), 1);
    rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP, rpmteE(te));
    rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP, rpmteV(te));
    rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP, rpmteR(te));
    /* XXX shouldn't we also do this on colorless transactions? */
    if (rpmtsColor(ts)) {
	rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP, rpmteA(te));
	rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP, rpmteO(te));
    }

    while (rpmdbNextIterator(mi) != NULL) {
	rpmteSetDBInstance(te, rpmdbGetIteratorOffset(mi));
	break;
    }
    rpmdbFreeIterator(mi);
}

static rpmRC dbAdd(rpmts ts, rpmte te)
{
    Header h = rpmteHeader(te);
    rpm_time_t installTime = (rpm_time_t) time(NULL);
    rpmfs fs = rpmteGetFileStates(te);
    rpm_count_t fc = rpmfsFC(fs);
    rpm_fstate_t * fileStates = rpmfsGetStates(fs);
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_tid_t tid = rpmtsGetTid(ts);
    rpmRC rc;

    if (fileStates != NULL && fc > 0) {
	headerPutChar(h, RPMTAG_FILESTATES, fileStates, fc);
    }

    headerPutUint32(h, RPMTAG_INSTALLTID, &tid, 1);
    headerPutUint32(h, RPMTAG_INSTALLTIME, &installTime, 1);
    headerPutUint32(h, RPMTAG_INSTALLCOLOR, &tscolor, 1);

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBADD), 0);
    rc = (rpmdbAdd(rpmtsGetRdb(ts), h) == 0) ? RPMRC_OK : RPMRC_FAIL;
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBADD), 0);

    if (rc == RPMRC_OK) {
	rpmteSetDBInstance(te, headerGetInstance(h));
	packageHashAddEntry(ts->members->installedPackages,
			    headerGetInstance(h), te);
    }
    headerFree(h);
    return rc;
}

static rpmRC dbRemove(rpmts ts, rpmte te)
{
    rpmRC rc;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);
    rc = (rpmdbRemove(rpmtsGetRdb(ts), rpmteDBInstance(te)) == 0) ?
						RPMRC_OK : RPMRC_FAIL;
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DBREMOVE), 0);

    if (rc == RPMRC_OK)
	rpmteSetDBInstance(te, 0);
    return rc;
}

static rpmRC rpmpsmUnpack(rpmpsm psm)
{
    char *failedFile = NULL;
    int fsmrc = 0;
    int saved_errno = 0;
    rpmRC rc = RPMRC_OK;

    rpmpsmNotify(psm, RPMCALLBACK_INST_START, 0);
    /* make sure first progress call gets made */
    rpmpsmNotify(psm, RPMCALLBACK_INST_PROGRESS, 0);

    if (!(rpmtsFlags(psm->ts) & RPMTRANS_FLAG_JUSTDB)) {
	if (rpmfilesFC(psm->files) > 0) {
	    fsmrc = rpmPackageFilesInstall(psm->ts, psm->te, psm->files,
				   psm, &failedFile);
	    saved_errno = errno;
	}
    }

    /* XXX make sure progress reaches 100% */
    rpmpsmNotify(psm, RPMCALLBACK_INST_PROGRESS, psm->total);
    rpmpsmNotify(psm, RPMCALLBACK_INST_STOP, psm->total);

    if (fsmrc) {
	char *emsg;
	errno = saved_errno;
	emsg = rpmfileStrerror(fsmrc);
	rpmlog(RPMLOG_ERR,
		_("unpacking of archive failed%s%s: %s\n"),
		(failedFile != NULL ? _(" on file ") : ""),
		(failedFile != NULL ? failedFile : ""),
		emsg);
	free(emsg);
	rc = RPMRC_FAIL;

	/* XXX notify callback on error. */
	rpmtsNotify(psm->ts, psm->te, RPMCALLBACK_UNPACK_ERROR, 0, 0);
    }
    free(failedFile);
    return rc;
}

static rpmRC rpmpsmRemove(rpmpsm psm)
{
    char *failedFile = NULL;
    int fsmrc = 0;

    rpmpsmNotify(psm, RPMCALLBACK_UNINST_START, 0);
    /* make sure first progress call gets made */
    rpmpsmNotify(psm, RPMCALLBACK_UNINST_PROGRESS, 0);

    /* XXX should't we log errors from here? */
    if (!(rpmtsFlags(psm->ts) & RPMTRANS_FLAG_JUSTDB)) {
	if (rpmfilesFC(psm->files) > 0) {
	    fsmrc = rpmPackageFilesRemove(psm->ts, psm->te, psm->files,
					  psm, &failedFile);
	}
    }
    /* XXX make sure progress reaches 100% */
    rpmpsmNotify(psm, RPMCALLBACK_UNINST_PROGRESS, psm->total);
    rpmpsmNotify(psm, RPMCALLBACK_UNINST_STOP, psm->total);

    free(failedFile);
    return (fsmrc == 0) ? RPMRC_OK : RPMRC_FAIL;
}

static rpmRC rpmPackageInstall(rpmts ts, rpmpsm psm)
{
    rpmRC rc = RPMRC_OK;
    int once = 1;

    rpmswEnter(rpmtsOp(psm->ts, RPMTS_OP_INSTALL), 0);
    while (once--) {
	/* HACK: replacepkgs abuses te instance to remove old header */
	if (rpmtsFilterFlags(psm->ts) & RPMPROB_FILTER_REPLACEPKG)
	    markReplacedInstance(ts, psm->te);


	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERPREIN)) {
	    /* Run triggers in other package(s) this package sets off. */
	    rc = runTriggers(psm, RPMSENSE_TRIGGERPREIN);
	    if (rc) break;

	    /* Run triggers in this package other package(s) set off. */
	    rc = runImmedTriggers(psm, RPMSENSE_TRIGGERPREIN);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPRE)) {
	    rc = runInstScript(psm, RPMTAG_PREIN);
	    if (rc) break;
	}

	rc = rpmpsmUnpack(psm);
	if (rc) break;

	/*
	 * If this package has already been installed, remove it from
	 * the database before adding the new one.
	 */
	if (rpmteDBInstance(psm->te)) {
	    rc = dbRemove(ts, psm->te);
	    if (rc) break;
	}

	rc = dbAdd(ts, psm->te);
	if (rc) break;

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERIN)) {
	    /* Run upper file triggers i. e. with higher priorities */
	    /* Run file triggers in other package(s) this package sets off. */
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERIN,
				RPMSCRIPT_FILETRIGGER, 1);
	    if (rc) break;

	    /* Run file triggers in this package other package(s) set off. */
	    rc = runImmedFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERIN,
				    RPMSCRIPT_FILETRIGGER, 1);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPOST)) {
	    rc = runInstScript(psm, RPMTAG_POSTIN);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERIN)) {
	    /* Run triggers in other package(s) this package sets off. */
	    rc = runTriggers(psm, RPMSENSE_TRIGGERIN);
	    if (rc) break;

	    /* Run triggers in this package other package(s) set off. */
	    rc = runImmedTriggers(psm, RPMSENSE_TRIGGERIN);
	    if (rc) break;

	    /* Run lower file triggers i. e. with lower priorities */
	    /* Run file triggers in other package(s) this package sets off. */
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERIN,
				RPMSCRIPT_FILETRIGGER, 2);
	    if (rc) break;

	    /* Run file triggers in this package other package(s) set off. */
	    rc = runImmedFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERIN,
				    RPMSCRIPT_FILETRIGGER, 2);
	    if (rc) break;
	}

	rc = markReplacedFiles(psm);
    }

    rpmswExit(rpmtsOp(psm->ts, RPMTS_OP_INSTALL), 0);

    return rc;
}

static rpmRC rpmPackageErase(rpmts ts, rpmpsm psm)
{
    rpmRC rc = RPMRC_OK;
    int once = 1;

    rpmswEnter(rpmtsOp(psm->ts, RPMTS_OP_ERASE), 0);
    while (once--) {

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERUN)) {
	    /* Run file triggers in this package other package(s) set off. */
	    rc = runImmedFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERUN,
				    RPMSCRIPT_FILETRIGGER, 1);
	    if (rc) break;

	    /* Run file triggers in other package(s) this package sets off. */
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERUN,
				RPMSCRIPT_FILETRIGGER, 1);
	    if (rc) break;

	    /* Run triggers in this package other package(s) set off. */
	    rc = runImmedTriggers(psm, RPMSENSE_TRIGGERUN);
	    if (rc) break;

	    /* Run triggers in other package(s) this package sets off. */
	    rc = runTriggers(psm, RPMSENSE_TRIGGERUN);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPREUN)) {
	    rc = runInstScript(psm, RPMTAG_PREUN);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERUN)) {
	    /* Run file triggers in this package other package(s) set off. */
	    rc = runImmedFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERUN,
				    RPMSCRIPT_FILETRIGGER, 2);
	    if (rc) break;

	    /* Run file triggers in other package(s) this package sets off. */
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERUN,
				RPMSCRIPT_FILETRIGGER, 2);
	    if (rc) break;
	}

	rc = rpmpsmRemove(psm);
	if (rc) break;

	/* Run file triggers in other package(s) this package sets off. */
	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERPOSTUN)) {
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERPOSTUN,
				RPMSCRIPT_FILETRIGGER, 1);
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOPOSTUN)) {
	    rc = runInstScript(psm, RPMTAG_POSTUN);
	    if (rc) break;
	}

	if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERPOSTUN)) {
	    /* Run triggers in other package(s) this package sets off. */
	    rc = runTriggers(psm, RPMSENSE_TRIGGERPOSTUN);
	    if (rc) break;

	    /* Run file triggers in other package(s) this package sets off. */
	    rc = runFileTriggers(psm->ts, psm->te, RPMSENSE_TRIGGERPOSTUN,
				RPMSCRIPT_FILETRIGGER, 2);
	}
	if (rc) break;

	if (!(rpmtsFlags(ts) & (RPMTRANS_FLAG_NOPOSTTRANS|RPMTRANS_FLAG_NOTRIGGERPOSTUN))) {
	    /* Prepare post transaction uninstall triggers */
	    rpmtriggersPrepPostUnTransFileTrigs(psm->ts, psm->te);
	}

	rc = dbRemove(ts, psm->te);
    }

    rpmswExit(rpmtsOp(psm->ts, RPMTS_OP_ERASE), 0);

    return rc;
}

static const char * pkgGoalString(pkgGoal goal)
{
    switch (goal) {
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

    psm = rpmpsmNew(ts, te, goal);
    if (rpmChrootIn() == 0) {
	/* Run pre transaction element hook for all plugins */
	rc = rpmpluginsCallPsmPre(rpmtsPlugins(ts), te);

	if (!rc) {
	    switch (goal) {
	    case PKG_INSTALL:
		rc = rpmPackageInstall(ts, psm);
		break;
	    case PKG_ERASE:
		rc = rpmPackageErase(ts, psm);
		break;
	    case PKG_PRETRANS:
	    case PKG_POSTTRANS:
	    case PKG_VERIFY:
		rc = runInstScript(psm, goal);
		break;
	    case PKG_TRANSFILETRIGGERIN:
		rc = runImmedFileTriggers(ts, te, RPMSENSE_TRIGGERIN,
					    RPMSCRIPT_TRANSFILETRIGGER, 0);
		break;
	    case PKG_TRANSFILETRIGGERUN:
		rc = runImmedFileTriggers(ts, te, RPMSENSE_TRIGGERUN,
					    RPMSCRIPT_TRANSFILETRIGGER, 0);
		break;
	    default:
		break;
	    }
	}
	/* Run post transaction element hook for all plugins */
	rpmpluginsCallPsmPost(rpmtsPlugins(ts), te, rc);

	/* XXX an error here would require a full abort */
	(void) rpmChrootOut();
    }
    rpmpsmFree(psm);
    return rc;
}
