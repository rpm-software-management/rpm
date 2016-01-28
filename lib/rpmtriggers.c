#include "system.h"

#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <stdlib.h>

#include "lib/rpmtriggers.h"
#include "lib/rpmts_internal.h"
#include "lib/rpmdb_internal.h"
#include "lib/rpmds_internal.h"
#include "lib/rpmfi_internal.h"
#include "lib/rpmchroot.h"

#define TRIGGER_PRIORITY_BOUND 10000

rpmtriggers rpmtriggersCreate(unsigned int hint)
{
    rpmtriggers triggers = xmalloc(sizeof(struct rpmtriggers_s));
    triggers->count = 0;
    triggers->alloced = hint;
    triggers->triggerInfo = xmalloc(sizeof(struct triggerInfo_s) *
				    triggers->alloced);
    return triggers;
}

rpmtriggers rpmtriggersFree(rpmtriggers triggers)
{
    _free(triggers->triggerInfo);
    _free(triggers);

    return NULL;
}

static void rpmtriggersAdd(rpmtriggers trigs, unsigned int hdrNum,
			    unsigned int tix, unsigned int priority)
{
    if (trigs->count == trigs->alloced) {
	trigs->alloced <<= 1;
	trigs->triggerInfo = xrealloc(trigs->triggerInfo,
				sizeof(struct triggerInfo_s) * trigs->alloced);
    }

    trigs->triggerInfo[trigs->count].hdrNum = hdrNum;
    trigs->triggerInfo[trigs->count].tix = tix;
    trigs->triggerInfo[trigs->count].priority = priority;
    trigs->count++;
}

static int trigCmp(const void *a, const void *b)
{
    const struct triggerInfo_s *trigA = a, *trigB = b;

    if (trigA->priority < trigB->priority)
	return 1;

    if (trigA->priority > trigB->priority)
	return -1;

    if (trigA->hdrNum < trigB->hdrNum)
	return -1;

    if (trigA->hdrNum > trigB->hdrNum)
	return 1;

    if (trigA->tix < trigB->tix)
	return -1;

    if (trigA->tix > trigB->tix)
	return 1;

    return 0;
}

static void rpmtriggersSortAndUniq(rpmtriggers trigs)
{
    unsigned int from;
    unsigned int to = 0;
    unsigned int count = trigs->count;

    if (count > 1)
	qsort(trigs->triggerInfo, count, sizeof(struct triggerInfo_s), trigCmp);

    for (from = 0; from < count; from++) {
	if (from > 0 &&
	    !trigCmp((const void *) &trigs->triggerInfo[from - 1],
		    (const void *) &trigs->triggerInfo[from])) {

	    trigs->count--;
	    continue;
	}
	if (from != to)
	    trigs->triggerInfo[to] = trigs->triggerInfo[from];
	to++;
    }
}

void rpmtriggersPrepPostUnTransFileTrigs(rpmts ts, rpmte te)
{
    rpmdbMatchIterator mi;
    rpmdbIndexIterator ii;
    Header trigH;
    const void *key;
    size_t keylen;
    rpmfiles files;
    rpmds rpmdsTriggers;
    rpmds rpmdsTrigger;
    int tix = 0;

    ii = rpmdbIndexIteratorInit(rpmtsGetRdb(ts), RPMDBI_TRANSFILETRIGGERNAME);
    mi = rpmdbNewIterator(rpmtsGetRdb(ts), RPMDBI_PACKAGES);
    files = rpmteFiles(te);

    /* Iterate over file triggers in rpmdb */
    while ((rpmdbIndexIteratorNext(ii, &key, &keylen)) == 0) {
	/* Check if file trigger matches any file in this te */
	rpmfi fi = rpmfilesFindPrefix(files, key);
	if (rpmfiFC(fi) > 0) {
	    /* If yes then store it */
	    rpmdbAppendIterator(mi, rpmdbIndexIteratorPkgOffsets(ii),
				rpmdbIndexIteratorNumPkgs(ii));
	}
	rpmfiFree(fi);
    }
    rpmdbIndexIteratorFree(ii);

    if (rpmdbGetIteratorCount(mi)) {
	/* Filter triggers and save only trans postun triggers into ts */
	while((trigH = rpmdbNextIterator(mi)) != NULL) {
	    rpmdsTriggers = rpmdsNew(trigH, RPMTAG_TRANSFILETRIGGERNAME, 0);
	    while ((rpmdsTrigger = rpmdsFilterTi(rpmdsTriggers, tix))) {
		if ((rpmdsNext(rpmdsTrigger) >= 0) &&
		    (rpmdsFlags(rpmdsTrigger) & RPMSENSE_TRIGGERPOSTUN)) {
		    struct rpmtd_s priorities;

		    headerGet(trigH, RPMTAG_TRANSFILETRIGGERPRIORITIES,
				&priorities, HEADERGET_MINMEM);
		    rpmtdSetIndex(&priorities, tix);
		    rpmtriggersAdd(ts->trigs2run, rpmdbGetIteratorOffset(mi),
				    tix, *rpmtdGetUint32(&priorities));
		}
		rpmdsFree(rpmdsTrigger);
		tix++;
	    }
	    rpmdsFree(rpmdsTriggers);
	}
    }
    rpmdbFreeIterator(mi);
}

int runPostUnTransFileTrigs(rpmts ts)
{
    int i;
    Header trigH;
    struct rpmtd_s installPrefixes;
    rpmScript script;
    rpmtriggers trigs = ts->trigs2run;
    int nerrors = 0;

    if (rpmChrootIn() != 0)
	return -1;

    rpmtriggersSortAndUniq(trigs);
    /* Iterate over stored triggers */
    for (i = 0; i < trigs->count; i++) {
	/* Get header containing trigger script */
	trigH = rpmdbGetHeaderAt(rpmtsGetRdb(ts),
				trigs->triggerInfo[i].hdrNum);

	/* Maybe package whith this trigger is already uninstalled */
	if (trigH == NULL)
	    continue;

	/* Prepare and run script */
	script = rpmScriptFromTriggerTag(trigH, RPMSENSE_TRIGGERPOSTUN,
		RPMSCRIPT_TRANSFILETRIGGER, trigs->triggerInfo[i].tix);

	headerGet(trigH, RPMTAG_INSTPREFIXES, &installPrefixes,
		HEADERGET_ALLOC|HEADERGET_ARGV);

	nerrors += runScript(ts, NULL, installPrefixes.data, script, 0, 0);
	rpmtdFreeData(&installPrefixes);
	rpmScriptFree(script);
	headerFree(trigH);
    }

    rpmChrootOut();

    return nerrors;
}

/*
 * Get files from next package from match iterator. If files are
 * available in memory then don't read them from rpmdb.
 */
static rpmfiles rpmtsNextFiles(rpmts ts, rpmdbMatchIterator mi)
{
    Header h;
    rpmte *te;
    rpmfiles files = NULL;
    rpmstrPool pool = ts->members->pool;
    int ix;
    unsigned int offset;

    ix = rpmdbGetIteratorIndex(mi);
    if (ix < rpmdbGetIteratorCount(mi)) {
	offset = rpmdbGetIteratorOffsetFor(mi, ix);
	if (packageHashGetEntry(ts->members->removedPackages, offset,
				&te, NULL, NULL)) {
	    /* Files are available in memory */
	    files  = rpmteFiles(te[0]);
	}

	if (packageHashGetEntry(ts->members->installedPackages, offset,
				&te, NULL, NULL)) {
	    /* Files are available in memory */
	    files  = rpmteFiles(te[0]);
	}
    }

    if (files) {
	rpmdbSetIteratorIndex(mi, ix + 1);
    } else {
	/* Files are not available in memory. Read them from rpmdb */
	h = rpmdbNextIterator(mi);
	if (h) {
	    files = rpmfilesNew(pool, h, RPMTAG_BASENAMES,
				RPMFI_FLAGS_ONLY_FILENAMES);
	}
    }

    return files;
}


typedef struct matchFilesIter_s {
    rpmts ts;
    rpmds rpmdsTrigger;
    rpmfiles files;
    rpmfi fi;
    const char *pfx;
    rpmdbMatchIterator pi;
    packageHash tranPkgs;
} *matchFilesIter;

static matchFilesIter matchFilesIterator(rpmds trigger, rpmfiles files)
{
    matchFilesIter mfi = xcalloc(1, sizeof(*mfi));
    rpmdsInit(trigger);
    mfi->rpmdsTrigger = trigger;
    mfi->files = rpmfilesLink(files);
    return mfi;
}

static matchFilesIter matchDBFilesIterator(rpmds trigger, rpmts ts,
					    int inTransaction)
{
    matchFilesIter mfi = xcalloc(1, sizeof(*mfi));
    rpmsenseFlags sense;

    rpmdsSetIx(trigger, 0);
    sense = rpmdsFlags(trigger);
    rpmdsInit(trigger);

    mfi->rpmdsTrigger = trigger;
    mfi->ts = ts;

    /* If inTransaction is set then filter out packages that aren't in transaction */
    if (inTransaction) {
	if (sense & RPMSENSE_TRIGGERIN)
	    mfi->tranPkgs = ts->members->installedPackages;
	else
	    mfi->tranPkgs = ts->members->removedPackages;
    }
    return mfi;
}

static const char *matchFilesNext(matchFilesIter mfi)
{
    const char *matchFile = NULL;
    int fx = 0;

    /* Decide if we iterate over given files (mfi->files) */
    if (!mfi->ts)
	do {
	    /* Get next file from mfi->fi */
	    rpmfiNext(mfi->fi);
	    matchFile = rpmfiFN(mfi->fi);
	    if (strlen(matchFile))
		break;
	    matchFile = NULL;

	    /* If we are done with current mfi->fi, create mfi->fi for next prefix */
	    fx = rpmdsNext(mfi->rpmdsTrigger);
	    mfi->pfx = rpmdsN(mfi->rpmdsTrigger);
	    rpmfiFree(mfi->fi);
	    mfi->fi = rpmfilesFindPrefix(mfi->files, mfi->pfx);

	} while (fx >= 0);
    /* or we iterate over files in rpmdb */
    else
	do {
	    rpmfiNext(mfi->fi);
	    matchFile = rpmfiFN(mfi->fi);
	    if (strlen(matchFile))
		break;
	    matchFile = NULL;

	    /* If we are done with current mfi->fi, create mfi->fi for next package */
	    rpmfilesFree(mfi->files);
	    rpmfiFree(mfi->fi);
	    mfi->files = rpmtsNextFiles(mfi->ts, mfi->pi);
	    mfi->fi = rpmfilesFindPrefix(mfi->files, mfi->pfx);
	    if (mfi->files)
		continue;

	    /* If we are done with all packages, go through packages with new prefix */
	    fx = rpmdsNext(mfi->rpmdsTrigger);
	    mfi->pfx = rpmdsN(mfi->rpmdsTrigger);
	    rpmdbFreeIterator(mfi->pi);
	    mfi->pi = rpmdbInitPrefixIterator(rpmtsGetRdb(mfi->ts),
						RPMDBI_DIRNAMES, mfi->pfx, 0);

	    rpmdbFilterIterator(mfi->pi, mfi->tranPkgs, 0);

	} while (fx >= 0);


    return matchFile;
}

static int matchFilesEmpty(matchFilesIter mfi)
{
    const char *matchFile;

    /* Try to get the first file */
    matchFile = matchFilesNext(mfi);

    /* Rewind back this file */
    rpmfiInit(mfi->fi, 0);

    if (matchFile)
	/* We have at least one file so iterator is not empty */
	return 0;
    else
	/* No file in iterator */
	return 1;
}

static matchFilesIter matchFilesIteratorFree(matchFilesIter mfi)
{
    rpmfiFree(mfi->fi);
    rpmfilesFree(mfi->files);
    rpmdbFreeIterator(mfi->pi);
    free(mfi);
    return NULL;
}

/*
 * Run all file triggers in header h
 * @param searchMode	    0 match trigger prefixes against files in te
 *			    1 match trigger prefixes against files in whole ts
 *			    2 match trigger prefixes against files in whole
 *			      rpmdb
 */
static int runHandleTriggersInPkg(rpmts ts, rpmte te, Header h,
				rpmsenseFlags sense, rpmscriptTriggerModes tm,
				int searchMode, int ti)
{
    int nerrors = 0;
    rpmds rpmdsTriggers, rpmdsTrigger;
    rpmfiles files = NULL;
    matchFilesIter mfi = NULL;
    rpmScript script;
    struct rpmtd_s installPrefixes;
    char *(*inputFunc)(void *);

    rpmdsTriggers = rpmdsNew(h, triggerDsTag(tm), 0);
    rpmdsTrigger = rpmdsFilterTi(rpmdsTriggers, ti);
    /*
     * Now rpmdsTrigger contains all dependencies belonging to one trigger
     * with trigger index tix. Have a look at the first one to check flags.
     */
    if ((rpmdsNext(rpmdsTrigger) >= 0) &&
	(rpmdsFlags(rpmdsTrigger) & sense)) {

	switch (searchMode) {
	    case 0:
		/* Create iterator over files in te that this trigger matches */
		files = rpmteFiles(te);
		mfi = matchFilesIterator(rpmdsTrigger, files);
		break;
	    case 1:
		/* Create iterator over files in ts that this trigger matches */
		mfi = matchDBFilesIterator(rpmdsTrigger, ts, 1);
		break;
	    case 2:
		/* Create iterator over files in whole rpmd that this trigger matches */
		mfi = matchDBFilesIterator(rpmdsTrigger, ts, 0);
		break;
	}

	/* If this trigger matches any file then run trigger script */
	if (!matchFilesEmpty(mfi)) {
	    script = rpmScriptFromTriggerTag(h, triggertag(sense), tm, ti);

	    headerGet(h, RPMTAG_INSTPREFIXES, &installPrefixes,
		    HEADERGET_ALLOC|HEADERGET_ARGV);


	    /*
	     * As input function set function to get next file from
	     * matching file iterator. As parameter for this function
	     * set matching file iterator. Input function will be called
	     * during execution of trigger script in order to get data
	     * that will be passed as stdin to trigger script. To get
	     * these data from lua script function rpm.input() can be used.
	     */
	    inputFunc = (char *(*)(void *)) matchFilesNext;
	    rpmScriptSetNextFileFunc(script, inputFunc, mfi);

	    nerrors += runScript(ts, te, installPrefixes.data,
				script, 0, 0);
	    rpmtdFreeData(&installPrefixes);
	    rpmScriptFree(script);
	}
	rpmfilesFree(files);
	matchFilesIteratorFree(mfi);
    }
    rpmdsFree(rpmdsTrigger);
    rpmdsFree(rpmdsTriggers);

    return nerrors;
}

/* Return true if any file in package (te) starts with pfx */
static int matchFilesInPkg(rpmts ts, rpmte te, const char *pfx,
			    rpmsenseFlags sense)
{
    int rc;
    rpmfiles files = rpmteFiles(te);
    rpmfi fi = rpmfilesFindPrefix(files, pfx);

    rc = (fi != NULL);
    rpmfilesFree(files);
    rpmfiFree(fi);
    return rc;
}

/* Return true if any added/removed file in ts starts with pfx */
static int matchFilesInTran(rpmts ts, rpmte te, const char *pfx,
			    rpmsenseFlags sense)
{
    int rc = 1;
    rpmdbMatchIterator pi;

    /* Get all files from rpmdb starting with pfx */
    pi = rpmdbInitPrefixIterator(rpmtsGetRdb(ts), RPMDBI_DIRNAMES, pfx, 0);

    if (sense & RPMSENSE_TRIGGERIN)
	/* Leave in pi only files installed in ts */
	rpmdbFilterIterator(pi, ts->members->installedPackages, 0);
    else
	/* Leave in pi only files removed in ts */
	rpmdbFilterIterator(pi, ts->members->removedPackages, 0);

    rc = rpmdbGetIteratorCount(pi);
    rpmdbFreeIterator(pi);

    return rc;
}

rpmRC runFileTriggers(rpmts ts, rpmte te, rpmsenseFlags sense,
			rpmscriptTriggerModes tm, int priorityClass)
{
    int nerrors = 0, i;
    rpmdbIndexIterator ii;
    const void *key;
    char *pfx;
    size_t keylen;
    Header trigH;
    int (*matchFunc)(rpmts, rpmte, const char*, rpmsenseFlags sense);
    rpmTagVal priorityTag;
    rpmtriggers triggers = rpmtriggersCreate(10);

    /* Decide if we match triggers against files in te or in whole ts */
    if (tm == RPMSCRIPT_FILETRIGGER) {
	matchFunc = matchFilesInPkg;
	priorityTag = RPMTAG_FILETRIGGERPRIORITIES;
    } else {
	matchFunc = matchFilesInTran;
	priorityTag = RPMTAG_TRANSFILETRIGGERPRIORITIES;
    }

    ii = rpmdbIndexIteratorInit(rpmtsGetRdb(ts), triggerDsTag(tm));

    /* Loop over all file triggers in rpmdb */
    while ((rpmdbIndexIteratorNext(ii, &key, &keylen)) == 0) {
	pfx = xmalloc(keylen + 1);
	memcpy(pfx, key, keylen);
	pfx[keylen] = '\0';

	/* Check if file trigger is fired by any file in ts/te */
	if (matchFunc(ts, te, pfx, sense)) {
	    for (i = 0; i < rpmdbIndexIteratorNumPkgs(ii); i++) {
		struct rpmtd_s priorities;
		unsigned int priority;
		unsigned int offset = rpmdbIndexIteratorPkgOffset(ii, i);
		unsigned int tix = rpmdbIndexIteratorTagNum(ii, i);

		/*
		 * Don't handle transaction triggers installed in current
		 * transaction to avoid executing the same script two times.
		 * These triggers are handled in runImmedFileTriggers().
		 */
		if (tm == RPMSCRIPT_TRANSFILETRIGGER &&
		    (packageHashHasEntry(ts->members->removedPackages, offset) ||
		    packageHashHasEntry(ts->members->installedPackages, offset)))
		    continue;

		/* Get priority of trigger from header */
		trigH = rpmdbGetHeaderAt(rpmtsGetRdb(ts), offset);
		headerGet(trigH, priorityTag, &priorities, HEADERGET_MINMEM);
		rpmtdSetIndex(&priorities, tix);
		priority = *rpmtdGetUint32(&priorities);
		headerFree(trigH);

		/* Store file trigger in array */
		rpmtriggersAdd(triggers, offset, tix, priority);
	    }
	}
	free(pfx);
    }
    rpmdbIndexIteratorFree(ii);

    /* Sort triggers by priority, offset, trigger index */
    rpmtriggersSortAndUniq(triggers);

    if (rpmChrootIn() != 0) {
	rpmtriggersFree(triggers);
	return RPMRC_FAIL;
    }

    /* Handle stored triggers */
    for (i = 0; i < triggers->count; i++) {
	if (priorityClass == 1) {
	    if (triggers->triggerInfo[i].priority < TRIGGER_PRIORITY_BOUND)
		continue;
	} else if (priorityClass == 2) {
	    if (triggers->triggerInfo[i].priority >= TRIGGER_PRIORITY_BOUND)
		continue;
	}

	trigH = rpmdbGetHeaderAt(rpmtsGetRdb(ts), triggers->triggerInfo[i].hdrNum);
	if (tm == RPMSCRIPT_FILETRIGGER)
	    nerrors += runHandleTriggersInPkg(ts, te, trigH, sense, tm, 0,
						triggers->triggerInfo[i].tix);
	else
	    nerrors += runHandleTriggersInPkg(ts, te, trigH, sense, tm, 1,
						triggers->triggerInfo[i].tix);
	headerFree(trigH);
    }
    rpmtriggersFree(triggers);
    /* XXX an error here would require a full abort */
    (void) rpmChrootOut();

    return (nerrors == 0) ? RPMRC_OK : RPMRC_FAIL;
}

rpmRC runImmedFileTriggers(rpmts ts, rpmte te, rpmsenseFlags sense,
			    rpmscriptTriggerModes tm, int priorityClass)
{
    int nerrors = 0;
    int triggersCount, i;
    Header trigH = rpmteHeader(te);
    struct rpmtd_s priorities;
    rpmTagVal priorityTag;
    rpmtriggers triggers;

    if (tm == RPMSCRIPT_FILETRIGGER) {
	priorityTag = RPMTAG_FILETRIGGERPRIORITIES;
    } else {
	priorityTag = RPMTAG_TRANSFILETRIGGERPRIORITIES;
    }
    headerGet(trigH, priorityTag, &priorities, HEADERGET_MINMEM);

    triggersCount = rpmtdCount(&priorities);
    triggers = rpmtriggersCreate(triggersCount);

    for (i = 0; i < triggersCount; i++) {
	rpmtdSetIndex(&priorities, i);
	/* Offset is not important, all triggers are from the same package */
	rpmtriggersAdd(triggers, 0, i, *rpmtdGetUint32(&priorities));
    }

    /* Sort triggers by priority, offset, trigger index */
    rpmtriggersSortAndUniq(triggers);

    for (i = 0; i < triggersCount; i++) {
	if (priorityClass == 1) {
	    if (triggers->triggerInfo[i].priority < TRIGGER_PRIORITY_BOUND)
		continue;
	} else if (priorityClass == 2) {
	    if (triggers->triggerInfo[i].priority >= TRIGGER_PRIORITY_BOUND)
		continue;
	}

	nerrors += runHandleTriggersInPkg(ts, te, trigH, sense, tm, 2,
					    triggers->triggerInfo[i].tix);
    }
    rpmtriggersFree(triggers);
    headerFree(trigH);

    return (nerrors == 0) ? RPMRC_OK : RPMRC_FAIL;
}
