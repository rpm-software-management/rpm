#include "system.h"

#include "rpmlib.h"

#include "depends.h"
#include "fprint.h"
#include "hash.h"
#include "install.h"
#include "misc.h"

struct fileFate {
    enum fileFate_e { CREATE, REMOVE } action;
    Header winner;			/* only set for CREATE actions */
};

struct fileInfo {
    Header h;
    enum fileInfo_e { ADDED, REMOVED } action;
    char ** fl, ** fmd5s;
    fingerPrint * fps;
    int fc;
};

static rpmProblemSet psCreate(void);
static void psAppend(rpmProblemSet probs, rpmProblemType type, void * key, 
		     Header h, char * str1);
static int archOkay(Header h);
static int osOkay(Header h);
static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs);
static int psTrim(rpmProblemSet filter, rpmProblemSet target);

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

/* Return -1 on error, > 0 if newProbs is set, 0 if everything
   happened */
int rpmRunTransactions(rpmTransactionSet ts, rpmNotifyFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags) {
    int i, j, numPackages, pkgNum, fileNum;
    struct availableList * al = &ts->addedPackages;
    int rc, ourrc = 0;
    int instFlags = 0, rmFlags = 0;
    rpmProblem prob;
    struct availablePackage * alp;
    rpmProblemSet probs;
    dbiIndexSet dbi;
    Header * hdrs;
    int fileCount;
    int totalFileCount = 0;
    hashTable ht, fates;
    struct fileInfo * flList, * fi, * winner;
    int winnerFileNum;
    struct fileFate * fateList;
    int numFates;
    struct fileInfo ** recs;
    int numRecs;
    char ** othermd5s;
    Header h;
    int32_t * otherstates;

    /* FIXME: what if the same package is included in ts twice? */

    if (flags & RPMTRANS_FLAG_TEST) {
	instFlags |= RPMINSTALL_TEST; 
	rmFlags |= RPMUNINSTALL_TEST;
    }

    probs = psCreate();
    *newProbs = probs;
    hdrs = alloca(sizeof(*hdrs) * al->size);

    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (!archOkay(alp->h))
	    psAppend(probs, RPMPROB_BADARCH, alp->key, alp->h, NULL);

	if (!osOkay(alp->h)) {
	    psAppend(probs, RPMPROB_BADOS, alp->key, alp->h, NULL);
	}

	rc = findMatches(ts->db, alp->name, alp->version, alp->release, &dbi);
	if (rc == 2) {
	    return -1;
	} else if (!rc) {
	    prob.key = alp->key;
	    psAppend(probs, RPMPROB_PKG_INSTALLED, alp->key, alp->h, NULL);
	    dbiFreeIndexRecord(dbi);
	}

	hdrs[pkgNum] = relocateFileList(alp, probs);

	if (headerGetEntry(alp->h, RPMTAG_FILENAMES, NULL, NULL, &fileCount))
	    totalFileCount += fileCount;
    }

    flList = alloca(sizeof(*flList) * (al->size + ts->numRemovedPackages));

    ht = htCreate(totalFileCount * 2, 0, fpHashFunction, fpEqual);
    fi = flList;

    /* FIXME: we'd be better off assembling one very large file list and
       calling fpLookupList only once. I'm not sure that the speedup is
       worth the trouble though. */
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++, fi++) {
	if (!headerGetEntryMinMemory(alp->h, RPMTAG_FILENAMES, NULL, 
				     (void *) &fi->fl, &fi->fc))
	    continue;
	headerGetEntryMinMemory(alp->h, RPMTAG_FILEMD5S, NULL, 
				(void *) &fi->fmd5s, NULL);

	fi->h = alp->h;
	fi->action = ADDED;
        fi->fps = alloca(fi->fc * sizeof(*fi->fps));
	fpLookupList(fi->fl, fi->fps, fi->fc, 1);
	for (i = 0; i < fi->fc; i++)
	     htAddEntry(ht, fi->fps + i, fi);
    }

/* FIXME 
    for (i = 0; i < ts->numRemovedPackages; i++) {
	flList[numPackages]->h = alp->h;
	flList[numPackages]->info = info;
	aggregateFileList(flList + numPackages, ht);
	numPackages++;
    }
*/


#if 0
    /* We build this list to reduce database i/o, while still allowing
       error messages to come out in a sensible order. */
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (!ts->db || rpmdbFindByFile(ts->db, fi->fl[i], &matches))
	    continue;

	for (j = 0; j < matches.count; j++) {
	    
	}
    }
#endif

    fateList = alloca(totalFileCount * sizeof(*fateList));
    fates = htCreate(totalFileCount * 2, sizeof(fingerPrint), fpHashFunction, 
		     fpEqual);
    numFates = 0;
    numPackages = 0;
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (alp->h != flList[numPackages].h) continue;
	fi = flList + numPackages;

	for (i = 0; i < fi->fc; i++) {
	    if (htHasEntry(fates, &fi->fps[i])) continue;

	    htGetEntry(ht, &fi->fps[i], (void ***) &recs, &numRecs, NULL);
	    /* note that &recs[0] must equal fi! */
	    for (j = numRecs - 1; j > 0 && (recs[j]->action != ADDED); 
		 winner--);
	    winner = recs[j];

	    for (winnerFileNum = 0; winnerFileNum < winner->fc; 
			winnerFileNum++)
	    	if (FP_EQUAL(fi->fps[i], recs[j]->fps[winnerFileNum])) break;

	    for (j = 0; recs[j] != winner; j++) {
		/* FIXME: there are more efficient searches in the world... */
		for (fileNum = 0; fileNum < recs[j]->fc; fileNum++)
		    if (FP_EQUAL(fi->fps[i], recs[j]->fps[fileNum])) break;

		if (strcmp(winner->fmd5s[winnerFileNum], 
			   recs[j]->fmd5s[fileNum])) {
		    /* FIXME: we need to pass the conflicting header */
		    psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, alp->key, 
			     alp->h, fi->fl[i]);
		}
	    }

	    fateList[numFates].action = CREATE;
	    fateList[numFates].winner = alp->h;
	    htAddEntry(fates, fi->fps + i, fateList + numFates);
	    numFates++;

	    /* this is horrible inefficient :-( */
	    if (ts->db && !rpmdbFindByFile(ts->db, fi->fl[i], &dbi)) {
		for (j = 0; j < dbi.count; j++) {
		    h = rpmdbGetRecord(ts->db, dbi.recs[j].recOffset);
		    /* this shouldn't happen */
		    if (!h) continue;

		    headerGetEntryMinMemory(h, RPMTAG_FILENAMES, NULL,
					    (void **) &othermd5s, NULL);
		    headerGetEntryMinMemory(h, RPMTAG_FILESTATES, NULL,
					    (void **) &otherstates, NULL);

		    if ((otherstates[dbi.recs[j].fileNumber] == 
				RPMFILE_STATE_NORMAL) &&
			strcmp(winner->fmd5s[winnerFileNum], 
			       othermd5s[dbi.recs[j].fileNumber])) {	
			/* FIXME: we need to pass the conflicting header */
			psAppend(probs, RPMPROB_FILE_CONFLICT, alp->key, 
				 alp->h, fi->fl[i]);
		    }

		    free(othermd5s);
		}
	    }
	}

	numPackages++;
    }

    htFree(ht);

    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (alp->h != flList[numPackages].h) continue;
	fi = flList + numPackages;
	free(fi->fl);
	free(fi->fmd5s);
    }

    if (probs->numProblems && (!okProbs || psTrim(okProbs, probs))) {
	htFree(fates);
	*newProbs = probs;
	for (i = 0; i < al->size; i++)
	    headerFree(hdrs[i]);
	return al->size + ts->numRemovedPackages;
    }

    htFree(fates);

    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (installBinaryPackage(ts->root, ts->db, al->list[pkgNum].fd, 
			  	 hdrs[pkgNum], al->list[pkgNum].relocs, 
				 instFlags, notify, notifyData))
	    ourrc++;
	headerFree(hdrs[pkgNum]);
    }

    for (i = 0; i < ts->numRemovedPackages; i++) {
	if (removeBinaryPackage(ts->root, ts->db, ts->removedPackages[i], 
				rmFlags))
	    ourrc++;
    }

    return ourrc;
}

static rpmProblemSet psCreate(void) {
    rpmProblemSet probs;

    probs = malloc(sizeof(*probs));
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

static void psAppend(rpmProblemSet probs, rpmProblemType type, void * key, 
		     Header h, char * str1) {
    if (probs->numProblems == probs->numProblemsAlloced) {
	if (probs->numProblemsAlloced)
	    probs->numProblemsAlloced *= 2;
	else
	    probs->numProblemsAlloced = 2;
	probs->probs = realloc(probs->probs, 
			probs->numProblemsAlloced * sizeof(*probs->probs));
    }

    probs->probs[probs->numProblems].type = type;
    probs->probs[probs->numProblems].key = key;
    probs->probs[probs->numProblems].h = headerLink(h);
    probs->probs[probs->numProblems].str1 = strdup(str1);
    probs->probs[probs->numProblems++].ignoreProblem = 0;
}

static int archOkay(Header h) {
    int_8 * pkgArchNum;
    void * pkgArch;
    int type, count, archNum;

    /* make sure we're trying to install this on the proper architecture */
    headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);
    if (type == RPM_INT8_TYPE) {
	/* old arch handling */
	rpmGetArchInfo(NULL, &archNum);
	pkgArchNum = pkgArch;
	if (archNum != *pkgArchNum) {
	    return 0;
	}
    } else {
	/* new arch handling */
	if (!rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch)) {
	    return 0;
	}
    }

    return 1;
}

static int osOkay(Header h) {
    void * pkgOs;
    int type, count;

    /* make sure we're trying to install this on the proper os */
    headerGetEntry(h, RPMTAG_OS, &type, (void **) &pkgOs, &count);
    if (type == RPM_INT8_TYPE) {
	/* v1 packages and v2 packages both used improper OS numbers, so just
	   deal with it hope things work */
	return 1;
    } else {
	/* new os handling */
	if (!rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs)) {
	    return 0;
	}
    }

    return 1;
}

void rpmProblemSetFree(rpmProblemSet probs) {
    int i;

    for (i = 0; i < probs->numProblems; i++) {
	headerFree(probs->probs[i].h);
	if (probs->probs[i].str1) free(probs->probs[i].str1);
    }
    free(probs);
}

static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs) {
    int numValid, numRelocations;
    int i, j, madeSwap, rc;
    rpmRelocation * nextReloc, * relocations = NULL;
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation tmpReloc;
    char ** validRelocations, ** actualRelocations;
    char ** names;
    int len, newLen;
    char * newName;
    int_32 fileCount;
    Header h;

    if (!rawRelocations) return headerLink(alp->h);
    h = headerCopy(alp->h);

    if (!headerGetEntry(h, RPMTAG_PREFIXES, NULL,
			(void **) &validRelocations, &numValid))
	numValid = 0;

    for (i = 0; rawRelocations[i].newPath; i++) ;
    numRelocations = i;
    relocations = alloca(sizeof(*relocations) * numRelocations);

    /* FIXME? this code assumes the validRelocations array won't
       have trailing /'s in it */
    /* FIXME: all of this needs to be tested with an old format
       relocateable package */

    for (i = 0; i < numRelocations; i++) {
	/* FIXME: default relocations (oldPath == NULL) need to be handled
	   int the UI, not rpmlib */

	relocations[i].oldPath = 
	    alloca(strlen(rawRelocations[i].oldPath) + 1);
	strcpy(relocations[i].oldPath, rawRelocations[i].oldPath);
	stripTrailingSlashes(relocations[i].oldPath);

	relocations[i].newPath = 
	    alloca(strlen(rawRelocations[i].newPath) + 1);
	strcpy(relocations[i].newPath, rawRelocations[i].newPath);
	stripTrailingSlashes(relocations[i].newPath);

	for (j = 0; j < numValid; j++) 
	    if (!strcmp(validRelocations[j],
			relocations[i].oldPath)) break;
	if (j == numValid)
	    psAppend(probs, RPMPROB_BADRELOCATE, alp->key, alp->h, 
		     relocations[i].oldPath);
    }

    /* stupid bubble sort, but it's probably faster here */
    for (i = 0; i < numRelocations; i++) {
	madeSwap = 0;
	for (j = 1; j < numRelocations; j++) {
	    if (strcmp(relocations[j - 1].oldPath, 
		       relocations[j].oldPath) > 0) {
		tmpReloc = relocations[j - 1];
		relocations[j - 1] = relocations[j];
		relocations[j] = tmpReloc;
		madeSwap = 1;
	    }
	}
	if (!madeSwap) break;
    }

    if (numValid) {
	actualRelocations = malloc(sizeof(*actualRelocations) * numValid);
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (!strcmp(validRelocations[i], relocations[j].oldPath)) {
		    actualRelocations[i] = relocations[j].newPath;
		    break;
		}
	    }

	    if (j == numRelocations)
		actualRelocations[i] = validRelocations[i];
	}

	headerAddEntry(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numValid);

	free(actualRelocations);
	free(validRelocations);
    }

    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &names, 
		   &fileCount);
    headerAddEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, names,
		   fileCount);

    /* go through things backwards so that /usr/local relocations take
       precedence over /usr ones */
    nextReloc = relocations + numRelocations - 1;
    len = strlen(nextReloc->oldPath);
    newLen = strlen(nextReloc->newPath);
    for (i = fileCount - 1; i >= 0 && nextReloc; i--) {
	do {
	    rc = strncmp(nextReloc->oldPath, names[i], len);
	    if (rc > 0) {
		if (nextReloc == relocations) {
		    nextReloc = 0;
		} else {
		    nextReloc--;
		    len = strlen(nextReloc->oldPath);
		    newLen = strlen(nextReloc->newPath);
		}
	    }
	} while (rc > 0 && nextReloc);

	if (!rc) {
	    newName = alloca(newLen + strlen(names[i]) + 1);
	    strcpy(newName, nextReloc->newPath);
	    strcat(newName, names[i] + len);
	    rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
		       names[i], newName);
	    names[i] = newName;
	} 
    }

    headerModifyEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, 
		      names, fileCount);

    free(names);

    return h;
}

static int psTrim(rpmProblemSet filter, rpmProblemSet target) {
    /* As the problem sets are generated in an order solely dependent
       on the ordering of the packages in the transaction, and that
       ordering can't be changed, the problem sets must be parallel to
       on another. Additionally, the filter set must be a subset of the
       target set, given the operations available on transaction set.
       This is good, as it lets us perform this trim in linear time, rather
       then logarithmic or quadratic. */
    rpmProblem * f, * t;
    int gotProblems = 0;

    f = filter->probs;
    t = target->probs;

    while ((f - filter->probs) < filter->numProblems) {
	if (!f->ignoreProblem) {
	    f++;
	    continue;
	}
	while ((t - target->probs) < target->numProblems) {
	    if (f->h == t->h && f->type == t->type && t->key == f->key &&
		     XSTRCMP(f->str1, t->str1))
		break;
	    t++;
	    gotProblems = 1;
	}

	if ((t - target->probs) == target->numProblems) {
	    /* this can't happen ;-) lets be sane if it doesn though */
	    break;
	}

	t->ignoreProblem = f->ignoreProblem;
	t++, f++;
    }

    if ((t - target->probs) < target->numProblems)
	gotProblems = 1;

    return gotProblems;
}
