#include "system.h"

#include "rpmlib.h"

#include "depends.h"
#include "fprint.h"
#include "hash.h"
#include "install.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"

struct fileInfo {
    Header h;
    enum fileInfo_e { ADDED, REMOVED } type;
    enum instActions * actions;
    char ** fl, ** fmd5s, ** flinks;
    uint_32 * fflags;
    uint_16 * fmodes;
    fingerPrint * fps;
    int fc;
};

struct sharedFileInfo {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
};

static rpmProblemSet psCreate(void);
static void psAppend(rpmProblemSet probs, rpmProblemType type, void * key, 
		     Header h, char * str1);
static int archOkay(Header h);
static int osOkay(Header h);
static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs);
static int psTrim(rpmProblemSet filter, rpmProblemSet target);
static int sharedCmp(const void * one, const void * two);
static enum instActions decideFileFate(char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int brokenMd5);
enum fileTypes whatis(short mode);
static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2);

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

/* Return -1 on error, > 0 if newProbs is set, 0 if everything
   happened */
int rpmRunTransactions(rpmTransactionSet ts, rpmNotifyFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags) {
    int i, j, numPackages;
    struct availableList * al = &ts->addedPackages;
    int rc, ourrc = 0;
    int instFlags = 0, rmFlags = 0;
    rpmProblem prob;
    struct availablePackage * alp;
    rpmProblemSet probs;
    dbiIndexSet dbi, * matches;
    Header * hdrs;
    int fileCount;
    int totalFileCount = 0;
    hashTable ht;
    struct fileInfo * flList, * fi;
    struct fileInfo ** recs;
    int numRecs;
    char ** otherMd5s, ** otherLinks;
    Header h;
    char * otherStates;
    struct sharedFileInfo * shared, * sharedList;
    uint_32 * otherFlags;
    uint_16 * otherModes;
    int numShared;
    int pkgNum, otherPkgNum;
    int otherFileNum, fileNum;

    /* FIXME: what if the same package is included in ts twice? */

    /* FIXME: we completely ignore net shared paths here! */

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
	headerGetEntryMinMemory(alp->h, RPMTAG_FILELINKTOS, NULL, 
				(void *) &fi->flinks, NULL);
	headerGetEntryMinMemory(alp->h, RPMTAG_FILEMODES, NULL, 
				(void *) &fi->fmodes, NULL);
	headerGetEntryMinMemory(alp->h, RPMTAG_FILEFLAGS, NULL, 
				(void *) &fi->fflags, NULL);

	fi->h = alp->h;
	fi->type = ADDED;
	fi->actions = malloc(sizeof(*fi->actions) * fi->fc);
        fi->fps = alloca(fi->fc * sizeof(*fi->fps));
	fpLookupList(fi->fl, fi->fps, fi->fc, 1);
	for (i = 0; i < fi->fc; i++) {
	     htAddEntry(ht, fi->fps + i, fi);
	     fi->actions[i] = UNKNOWN;
	}
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

    numPackages = 0;
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (alp->h != flList[numPackages].h) continue;
	fi = flList + numPackages;

	matches = malloc(sizeof(*matches) * fi->fc);
	if (rpmdbFindFpList(ts->db, fi->fps, matches, fi->fc)) return 1;

	numShared = 0;
	for (i = 0; i < fi->fc; i++)
	    numShared += matches[i].count;

	shared = sharedList = malloc(sizeof(*sharedList) * (numShared + 1));
	for (i = 0; i < fi->fc; i++) {
	    for (j = 0; j < matches[i].count; j++) {
		shared->pkgFileNum = i;
		shared->otherPkg = matches[i].recs[j].recOffset;
		shared->otherFileNum = matches[i].recs[j].fileNumber;
		shared++;
	    }
	}
	shared->otherPkg = -1;
	free(matches);

	qsort(sharedList, numShared, sizeof(*shared), sharedCmp);

	i = 0;
	while (i < numShared) {
	    h = rpmdbGetRecord(ts->db, sharedList[i].otherPkg);
	    if (!h) {
		i++;
		continue;
	    }

	    headerGetEntryMinMemory(h, RPMTAG_FILENAMES, NULL,
				    (void **) &otherMd5s, NULL);
	    headerGetEntryMinMemory(h, RPMTAG_FILELINKTOS, NULL,
				    (void **) &otherLinks, NULL);
	    headerGetEntryMinMemory(h, RPMTAG_FILESTATES, NULL,
				    (void **) &otherStates, &j);
	    headerGetEntryMinMemory(h, RPMTAG_FILEMODES, NULL,
				    (void **) &otherModes, &j);
	    headerGetEntryMinMemory(h, RPMTAG_FILEFLAGS, NULL,
				    (void **) &otherFlags, &j);

	    j = i;
	    shared = sharedList + i;
	    while (sharedList[i].otherPkg == shared->otherPkg) {
		otherFileNum = shared->otherFileNum;
		fileNum = shared->pkgFileNum;
		if (otherStates[otherFileNum] == RPMFILE_STATE_NORMAL) {
		    if (filecmp(otherModes[otherFileNum],
				otherMd5s[otherFileNum],
				otherLinks[otherFileNum],
				fi->fmodes[fileNum],
				fi->fmd5s[fileNum],
				fi->flinks[fileNum])) {
			/* FIXME: we need to pass the conflicting header */
			psAppend(probs, RPMPROB_FILE_CONFLICT, alp->key, 
				 alp->h, fi->fl[fileNum]);
		    }

		    if ((otherFlags[otherFileNum] | fi->fflags[fileNum])
			 	& RPMFILE_CONFIG) {
			fi->actions[fileNum] = decideFileFate(fi->fl[fileNum],
				otherModes[otherFileNum], 
				otherMd5s[otherFileNum],
				otherLinks[otherFileNum],
				fi->fmodes[fileNum],
				fi->fmd5s[fileNum],
				fi->flinks[fileNum],
				fi->fflags[fileNum], 
			        !headerIsEntry(h, RPMTAG_RPMVERSION));
		    }
		}

		shared++;
	    }

	    free(otherMd5s);
	    free(otherLinks);
	    headerFree(h);

	    i = shared - sharedList;
	}

	free(sharedList);

	for (i = 0; i < fi->fc; i++) {
	    htGetEntry(ht, &fi->fps[i], (void ***) &recs, &numRecs, NULL);

	    /* We need to figure out the current fate of this file. So,
	       work backwards from this file and look for a final action
	       we can work against. */
	    for (j = 0; recs[j] != fi; j++);

	    otherPkgNum = j - 1;
	    otherFileNum = -1;			/* keep gcc quiet */
	    while (otherPkgNum >= 0) {
		if (recs[otherPkgNum]->type != ADDED) continue;

		/* FIXME: there are more efficient searches in the world... */
		for (otherFileNum = 0; otherFileNum < recs[otherPkgNum]->fc; 
		     otherFileNum++)
		    if (FP_EQUAL(fi->fps[i], 
				 recs[otherPkgNum]->fps[otherFileNum])) 
			break;
		if ((otherFileNum > 0) && 
		    (recs[otherPkgNum]->actions[otherFileNum] == CREATE))
		    break;
		otherPkgNum--;
	    }

	    if (otherPkgNum < 0) {
		/* If it isn't in the database, install it. 
		   FIXME: check for config files here for .rpmorig purporses! */
		if (fi->actions[i] == UNKNOWN)
		    fi->actions[i] = CREATE;
	    } else {
		if (filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			    recs[otherPkgNum]->fmd5s[otherFileNum],
			    recs[otherPkgNum]->flinks[otherFileNum],
			    fi->fmodes[i],
			    fi->fmd5s[i],
			    fi->flinks[i])) {
		    psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, alp->key, 
			     alp->h, fi->fl[i]);
		}

		/* FIXME: is this right??? it locks us into the config
		   file handling choice we already made, which may very
		   well be exactly right. */
		fi->actions[i] = recs[otherPkgNum]->actions[otherFileNum];
		recs[otherPkgNum]->actions[otherFileNum] = SKIP;
	    }
	}

	numPackages++;
    }

    htFree(ht);

    numPackages = 0;
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	if (alp->h != flList[numPackages].h) continue;
	fi = flList + numPackages;
	free(fi->fl);
	free(fi->fmd5s);
	free(fi->flinks);

	numPackages++;
    }

    if (probs->numProblems && (!okProbs || psTrim(okProbs, probs))) {
	*newProbs = probs;
	for (i = 0; i < al->size; i++)
	    headerFree(hdrs[i]);

	numPackages = 0;
	for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	    if (alp->h != flList[numPackages].h) continue;
	    fi = flList + numPackages;
	    free(fi->actions);
	}

	return al->size + ts->numRemovedPackages;
    }

    numPackages = 0;
    for (pkgNum = 0, alp = al->list; pkgNum < al->size; pkgNum++, alp++) {
	fi = flList + numPackages;
	if (installBinaryPackage(ts->root, ts->db, al->list[pkgNum].fd, 
			  	 hdrs[pkgNum], al->list[pkgNum].relocs, 
				 instFlags, notify, notifyData, 
				 fi->actions))
	    ourrc++;
	headerFree(hdrs[pkgNum]);

	if (alp->h != flList[numPackages].h) {
	    free(fi->actions);
	    numPackages++;
	}
    }

    for (i = 0; i < ts->numRemovedPackages; i++) {
	if (removeBinaryPackage(ts->root, ts->db, ts->removedPackages[i], 
				rmFlags))
	    ourrc++;
    }

    if (ourrc) 
    	return -1;
    else
	return 0;
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
    if (str1)
	probs->probs[probs->numProblems].str1 = strdup(str1);
    else
	probs->probs[probs->numProblems].str1 = NULL;
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

static int sharedCmp(const void * one, const void * two) {
    const struct sharedFileInfo * a = one;
    const struct sharedFileInfo * b = two;

    if (a->otherPkg < b->otherPkg)
	return -1;
    else if (a->otherPkg > b->otherPkg)
	return 1;

    return 0;
}

static enum instActions decideFileFate(char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int brokenMd5) {
    char buffer[1024];
    char * dbAttr, * newAttr;
    enum fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int i, rc;

    if (lstat(filespec, &sb)) {
	/* the file doesn't exist on the disk create it unless the new
	   package has marked it as missingok */
	if (newFlags & RPMFILE_MISSINGOK) {
	    rpmMessage(RPMMESS_DEBUG, _("%s skipped due to missingok flag\n"),
			filespec);
	    return SKIP;
	} else
	    return CREATE;
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(dbMode);
    newWhat = whatis(newMode);

    /* RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
       them in older packages as well */
    if (newWhat == XDIR)
	return CREATE;

    if (diskWhat != newWhat) {
	return SAVE;
    } else if (newWhat != dbWhat && diskWhat != dbWhat) {
	return SAVE;
    } else if (dbWhat != newWhat) {
	return CREATE;
    } else if (dbWhat != LINK && dbWhat != REG) {
	return CREATE;
    }

    if (dbWhat == REG) {
	if (brokenMd5)
	    rc = mdfileBroken(filespec, buffer);
	else
	    rc = mdfile(filespec, buffer);

	if (rc) {
	    /* assume the file has been removed, don't freak */
	    return CREATE;
	}
	dbAttr = dbMd5;
	newAttr = newMd5;
    } else /* dbWhat == LINK */ {
	memset(buffer, 0, sizeof(buffer));
	i = readlink(filespec, buffer, sizeof(buffer) - 1);
	if (i == -1) {
	    /* assume the file has been removed, don't freak */
	    return CREATE;
	}
	dbAttr = dbLink;
	newAttr = newLink;
     }

    /* this order matters - we'd prefer to CREATE the file if at all 
       possible in case something else (like the timestamp) has changed */

    if (!strcmp(dbAttr, buffer)) {
	/* this config file has never been modified, so 
	   just replace it */
	return CREATE;
    }

    if (!strcmp(dbAttr, newAttr)) {
	/* this file is the same in all versions of this package */
	return KEEP;
    }

    /* the config file on the disk has been modified, but
       the ones in the two packages are different. It would
       be nice if RPM was smart enough to at least try and
       merge the difference ala CVS, but... */
	    
    return SAVE;
}

enum fileTypes whatis(short mode) {
    enum fileTypes result;

    if (S_ISDIR(mode))
	result = XDIR;
    else if (S_ISCHR(mode))
	result = CDEV;
    else if (S_ISBLK(mode))
	result = BDEV;
    else if (S_ISLNK(mode))
	result = LINK;
    else if (S_ISSOCK(mode))
	result = SOCK;
    else if (S_ISFIFO(mode))
	result = PIPE;
    else
	result = REG;
 
    return result;
}

static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2) {
    enum fileTypes what1, what2;

    what1 = whatis(mode1);
    what2 = whatis(mode2);

    if (what1 != what2) return 1;

    if (what1 == LINK)
	return strcmp(link1, link2);
    else if (what1 == REG)
	return strcmp(md51, md52);

    return 0;
}
