#include "system.h"

#include "rpmlib.h"
#include "rpmmacro.h"	/* XXX for rpmGetPath */

#include "depends.h"
#include "fprint.h"
#include "hash.h"
#include "install.h"
#include "lookup.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_VFS
# include <sys/vfs.h>
#else
# if STATFS_IN_SYS_MOUNT
#  include <sys/mount.h>
# else
#  if STATFS_IN_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif

struct fileInfo {
  /* for all packages */
    enum rpmTransactionType type;
    enum fileActions * actions;
    fingerPrint * fps;
    uint_32 * fflags, * fsizes;
    const char ** fl;
    char ** fmd5s;
    uint_16 * fmodes;
    Header h;
    int fc;
    char * fstates;
  /* these are for TR_ADDED packages */
    char ** flinks;
    struct availablePackage * ap;
    struct sharedFileInfo * replaced;
    uint_32 * replacedSizes;
  /* for TR_REMOVED packages */
    unsigned int record;
};

struct diskspaceInfo {
    dev_t dev;
    signed long needed;		/* in blocks */
    int block;
    signed long avail;
};

/* argon thought a shift optimization here was a waste of time...  he's 
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

static rpmProblemSet psCreate(void);
static void psAppend(rpmProblemSet probs, rpmProblemType type, 
		     const void * key, Header h, const char * str1, 
		     Header altHeader, unsigned long ulong1);
static int archOkay(Header h);
static int osOkay(Header h);
static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs, Header h, 
			       enum fileActions * actions,
			       int allowBadRelocate);
static int psTrim(rpmProblemSet filter, rpmProblemSet target);
static int sharedCmp(const void * one, const void * two);
static enum fileActions decideFileFate(const char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int brokenMd5);
enum fileTypes whatis(short mode);
static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2);
static int handleInstInstalledFiles(struct fileInfo * fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount, int reportConflicts,
				    rpmProblemSet probs);
static int handleRmvdInstalledFiles(struct fileInfo * fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount);
void handleOverlappedFiles(struct fileInfo * fi, hashTable ht,
			   rpmProblemSet probs, struct diskspaceInfo * dsl);
static int ensureOlder(rpmdb db, Header new, int dbOffset, rpmProblemSet probs,
		       const void * key);
static void skipFiles(struct fileInfo * fi, int noDocs);

static void freeFi(struct fileInfo *fi)
{
	if (fi->h) {
	    headerFree(fi->h); fi->h = NULL;
	}
	if (fi->actions) {
	    free(fi->actions); fi->actions = NULL;
	}
	if (fi->replacedSizes) {
	    free(fi->replacedSizes); fi->replacedSizes = NULL;
	}
	if (fi->replaced) {
	    free(fi->replaced); fi->replaced = NULL;
	}
	if (fi->fl) {
	    free(fi->fl); fi->fl = NULL;
	}
	if (fi->flinks) {
	    free(fi->flinks); fi->flinks = NULL;
	}
	if (fi->fmd5s) {
	    free(fi->fmd5s); fi->fmd5s = NULL;
	}

	if (fi->type == TR_REMOVED) {
	    if (fi->fsizes) {
		free(fi->fsizes); fi->fsizes = NULL;
	    }
	    if (fi->fflags) {
		free(fi->fflags); fi->fflags = NULL;
	    }
	    if (fi->fmodes) {
		free(fi->fmodes); fi->fmodes = NULL;
	    }
	    if (fi->fstates) {
		free(fi->fstates); fi->fstates = NULL;
	    }
	}
}

static void freeFl(rpmTransactionSet ts, struct fileInfo *flList)
{
    struct fileInfo *fi;
    int oc;

    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	freeFi(fi);
    }
}

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

#define	NOTIFY(_x)	if (notify) notify _x

/* Return -1 on error, > 0 if newProbs is set, 0 if everything
   happened */
int rpmRunTransactions(rpmTransactionSet ts, rpmCallbackFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags, int ignoreSet) {
    int i, j;
    int rc, ourrc = 0;
    struct availablePackage * alp;
    rpmProblemSet probs;
    dbiIndexSet dbi, * matches;
    Header * hdrs;
    int fileCount;
    int totalFileCount = 0;
    hashTable ht;
    struct fileInfo * flList, * fi;
    struct sharedFileInfo * shared, * sharedList;
    int numShared;
    int flEntries;
    int last;
    int lastFailed;
    int beingRemoved;
    char * currDir, * chptr;
    FD_t fd;
    const char ** filesystems;
    int filesystemCount;
    struct diskspaceInfo * di = NULL;
    int oc;

    /* FIXME: what if the same package is included in ts twice? */

    if (!(ignoreSet & RPMPROB_FILTER_DISKSPACE) &&
		!rpmGetFilesystemList(&filesystems, &filesystemCount)) {
	struct statfs sfb;
	struct stat sb;

	di = alloca(sizeof(*di) * (filesystemCount + 1));

	for (i = 0; (i < filesystemCount) && di; i++) {
	    if (statfs(filesystems[i], &sfb)) {
		di = NULL;
	    } else {
		di[i].block = sfb.f_bsize;
		di[i].needed = 0;
		di[i].avail = sfb.f_bavail;

		stat(filesystems[i], &sb);
		di[i].dev = sb.st_dev;
	    }
	}

	if (di) di[i].block = 0;
    }

    probs = psCreate();
    *newProbs = probs;
    hdrs = alloca(sizeof(*hdrs) * ts->addedPackages.size);

    /* The ordering doesn't matter here */
    for (alp = ts->addedPackages.list; (alp - ts->addedPackages.list) < 
	    ts->addedPackages.size; alp++) {
	if (!archOkay(alp->h) && !(ignoreSet & RPMPROB_FILTER_IGNOREARCH))
	    psAppend(probs, RPMPROB_BADARCH, alp->key, alp->h, NULL, NULL, 0);

	if (!osOkay(alp->h) && !(ignoreSet & RPMPROB_FILTER_IGNOREOS)) {
	    psAppend(probs, RPMPROB_BADOS, alp->key, alp->h, NULL, NULL, 0);
	}

	if (!(ignoreSet & RPMPROB_FILTER_OLDPACKAGE)) {
	    rc = rpmdbFindPackage(ts->db, alp->name, &dbi);
	    if (rc == 2) {
		return -1;
	    } else if (!rc) {
		for (i = 0; i < dbi.count; i++) 
		    ensureOlder(ts->db, alp->h, dbi.recs[i].recOffset, 
				      probs, alp->key);

		dbiFreeIndexRecord(dbi);
	    }
	}

	rc = findMatches(ts->db, alp->name, alp->version, alp->release, &dbi);
	if (rc == 2) {
	    return -1;
	} else if (!rc) {
	    if (!(ignoreSet & RPMPROB_FILTER_REPLACEPKG))
		psAppend(probs, RPMPROB_PKG_INSTALLED, alp->key, alp->h, NULL, 
			 NULL, 0);
	    dbiFreeIndexRecord(dbi);
	}

	if (headerGetEntry(alp->h, RPMTAG_FILENAMES, NULL, NULL, &fileCount))
	    totalFileCount += fileCount;
    }

    /* FIXME: it seems a bit silly to read in all of these headers twice */
    /* The ordering doesn't matter here */
    for (i = 0; i < ts->numRemovedPackages; i++) {
	Header h;

	if ((h = rpmdbGetRecord(ts->db, ts->removedPackages[i]))) {
	    if (headerGetEntry(h, RPMTAG_FILENAMES, NULL, NULL, 
			       &fileCount))
		totalFileCount += fileCount;
	    headerFree(h);	/* XXX ==> LEAK */
	}
    }

    flEntries = ts->addedPackages.size + ts->numRemovedPackages;
    flList = alloca(sizeof(*flList) * (flEntries));

    ht = htCreate(totalFileCount * 2, 0, fpHashFunction, fpEqual);

    /* FIXME?: we'd be better off assembling one very large file list and
       calling fpLookupList only once. I'm not sure that the speedup is
       worth the trouble though. */
    for (fi = flList, oc = 0; oc < ts->orderCount; fi++, oc++) {
	memset(fi, 0, sizeof(*fi));

	if (ts->order[oc].type == TR_ADDED) {
	    i = ts->order[oc].u.addedIndex;
	    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;

	    if (!headerGetEntryMinMemory(alp->h, RPMTAG_FILENAMES, NULL, 
					 (void *) NULL, &fi->fc)) {
		fi->h = headerLink(alp->h);
		hdrs[i] = headerLink(fi->h);
		continue;
	    }

	    fi->actions = calloc(sizeof(*fi->actions), fi->fc);
	    hdrs[i] = relocateFileList(alp, probs, alp->h, fi->actions, 
			          ignoreSet & RPMPROB_FILTER_FORCERELOCATE);
	    fi->h = headerLink(hdrs[i]);
	    fi->type = TR_ADDED;
	    fi->ap = alp;
	} else {
	    fi->record = ts->order[oc].u.removed.dboffset;
	    fi->h = rpmdbGetRecord(ts->db, fi->record);
	    if (!fi->h) {
		/* ACK! */
		continue;
	    }
	    fi->type = TR_REMOVED;
	}

	if (!headerGetEntry(fi->h, RPMTAG_FILENAMES, NULL, 
				     (void *) &fi->fl, &fi->fc)) {
	    /* This catches removed packages w/ no file lists */
	    fi->fc = 0;
	    continue;
	}

	/* actions is initialized earlier for added packages */
	if (!fi->actions) 
	    fi->actions = calloc(sizeof(*fi->actions), fi->fc);

	headerGetEntry(fi->h, RPMTAG_FILEMODES, NULL, 
				(void *) &fi->fmodes, NULL);
	headerGetEntry(fi->h, RPMTAG_FILEFLAGS, NULL, 
				(void *) &fi->fflags, NULL);
	headerGetEntry(fi->h, RPMTAG_FILESIZES, NULL, 
				(void *) &fi->fsizes, NULL);
	headerGetEntry(fi->h, RPMTAG_FILESTATES, NULL, 
				(void *) &fi->fstates, NULL);

	if (ts->order[oc].type == TR_REMOVED) {
	    headerGetEntry(fi->h, RPMTAG_FILEMD5S, NULL, 
				    (void *) &fi->fmd5s, NULL);
	    headerGetEntry(fi->h, RPMTAG_FILELINKTOS, NULL, 
				    (void *) &fi->flinks, NULL);
	    fi->fsizes = memcpy(malloc(fi->fc * sizeof(*fi->fsizes)),
				fi->fsizes, fi->fc * sizeof(*fi->fsizes));
	    fi->fflags = memcpy(malloc(fi->fc * sizeof(*fi->fflags)),
				fi->fflags, fi->fc * sizeof(*fi->fflags));
	    fi->fmodes = memcpy(malloc(fi->fc * sizeof(*fi->fmodes)),
				fi->fmodes, fi->fc * sizeof(*fi->fmodes));
	    fi->fstates = memcpy(malloc(fi->fc * sizeof(*fi->fstates)),
				fi->fstates, fi->fc * sizeof(*fi->fstates));
	    headerFree(fi->h);
	    fi->h = NULL;
	} else {
	    /* ADDED package */

	    headerGetEntryMinMemory(fi->h, RPMTAG_FILEMD5S, NULL, 
				    (void *) &fi->fmd5s, NULL);
	    headerGetEntryMinMemory(fi->h, RPMTAG_FILELINKTOS, NULL, 
				    (void *) &fi->flinks, NULL);

	    /* 0 makes for noops */
	    fi->replacedSizes = calloc(fi->fc, sizeof(*fi->replacedSizes));
	    skipFiles(fi, flags & RPMTRANS_FLAG_NODOCS);
	}

        fi->fps = malloc(fi->fc * sizeof(*fi->fps));
    }

    chptr = currentDirectory();
    currDir = alloca(strlen(chptr) + 1);
    strcpy(currDir, chptr);
    free(chptr);
    chdir("/");
    chroot(ts->root);

    for (fi = flList; (fi - flList) < flEntries; fi++) {
	fpLookupList(fi->fl, fi->fps, fi->fc, 1);
	for (i = 0; i < fi->fc; i++) {
	    if (fi->actions[i] != FA_SKIP && fi->actions[i] != FA_SKIPNSTATE)
	        htAddEntry(ht, fi->fps + i, fi);
	}
    }

    NOTIFY((NULL, RPMCALLBACK_TRANS_START, 6, flEntries, NULL, notifyData));

    for (fi = flList; (fi - flList) < flEntries; fi++) {
	NOTIFY((NULL, RPMCALLBACK_TRANS_PROGRESS, (fi - flList), flEntries,
	       NULL, notifyData));

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
	    dbiFreeIndexRecord(matches[i]);
	}
	shared->otherPkg = -1;
	free(matches);

	qsort(sharedList, numShared, sizeof(*shared), sharedCmp);

	i = 0;
	while (i < numShared) {
	    last = i + 1;
	    j = sharedList[i].otherPkg;
	    while ((sharedList[last].otherPkg == j) && (last < numShared))
		last++;
	    last--;

	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] == sharedList[i].otherPkg)
		    break;
	    }
	    beingRemoved = (j < ts->numRemovedPackages);

	    if (fi->type == TR_ADDED)
		handleInstInstalledFiles(fi, ts->db, sharedList + i, 
			 last - i + 1, 
			 !(beingRemoved || 
				(ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)), 
			 probs);
	    else if (fi->type == TR_REMOVED && !beingRemoved)
		handleRmvdInstalledFiles(fi, ts->db, sharedList + i, 
					 last - i + 1);

	    i = last + 1;
	}

	free(sharedList);

	handleOverlappedFiles(fi, ht, 
	       (ignoreSet & RPMPROB_FILTER_REPLACENEWFILES) ? NULL : probs, di);

	if (di && fi->type == TR_ADDED && fi->fc) {
	    for (i = 0; i < filesystemCount; i++) {
		if (((di[i].needed * 20) / 19)> di[i].avail) {
		    psAppend(probs, RPMPROB_DISKSPACE, fi->ap->key, fi->ap->h,
			     filesystems[i], NULL, 
			     (di[i].needed - di[i].avail) * di[i].block);
		}
	    }
	}
    }

    NOTIFY((NULL, RPMCALLBACK_TRANS_STOP, 6, flEntries, NULL, notifyData));

    chroot(".");
    chdir(currDir);

    htFree(ht);

    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	if (fi->fc) {
	    free(fi->fl); fi->fl = NULL;
	    if (fi->type == TR_ADDED) {
		free(fi->fmd5s); fi->fmd5s = NULL;
		free(fi->flinks); fi->flinks = NULL;
		free(fi->fps); fi->fps = NULL;
	    }
	}
    }

    if ((flags & RPMTRANS_FLAG_BUILD_PROBS) || 
           (probs->numProblems && (!okProbs || psTrim(okProbs, probs)))) {
	*newProbs = probs;

	for (alp = ts->addedPackages.list, fi = flList; 
	        (alp - ts->addedPackages.list) < ts->addedPackages.size; 
		alp++, fi++) {
	    headerFree(hdrs[alp - ts->addedPackages.list]);
	}

	freeFl(ts, flList);
	return ts->orderCount;
    }

    lastFailed = -2;
    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	if (ts->order[oc].type == TR_ADDED) {
	    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
	    i = ts->order[oc].u.addedIndex;

	    if (alp->fd) {
		fd = alp->fd;
	    } else {
		fd = notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0, 
			    alp->key, notifyData);
		if (fd) {
		    Header h;

		    headerFree(hdrs[i]);
		    rc = rpmReadPackageHeader(fd, &h, NULL, NULL, NULL);
		    if (rc) {
			notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0, 
				    alp->key, notifyData);
			ourrc++;
			fd = NULL;
		    } else {
			hdrs[i] = relocateFileList(alp, probs, h, NULL, 1);
			headerFree(h);
		    }
		}
	    }

	    if (fd) {
		if (installBinaryPackage(ts->root, ts->db, fd, 
					 hdrs[i], flags, notify, 
					 notifyData, alp->key, fi->actions, 
					 fi->fc ? fi->replaced : NULL,
					 ts->scriptFd)) {
		    ourrc++;
		    lastFailed = i;
		}
	    } else {
		ourrc++;
		lastFailed = i;
	    }

	    headerFree(hdrs[i]);

	    if (!alp->fd && fd)
		notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0, alp->key, 
		   notifyData);
	} else if (ts->order[oc].u.removed.dependsOnIndex != lastFailed) {
	    if (removeBinaryPackage(ts->root, ts->db, fi->record,
				    flags, fi->actions, ts->scriptFd))

		ourrc++;
	}
    }

    freeFl(ts, flList);

    if (ourrc) 
    	return -1;
    else
	return 0;
}

void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd) {
    ts->scriptFd = fd;
}

static rpmProblemSet psCreate(void) {
    rpmProblemSet probs;

    probs = malloc(sizeof(*probs));
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

static void psAppend(rpmProblemSet probs, rpmProblemType type, 
		     const void * key, Header h, const char * str1, 
		     Header altH, unsigned long ulong1) {
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
    probs->probs[probs->numProblems].ulong1 = ulong1;
    if (str1)
	probs->probs[probs->numProblems].str1 = strdup(str1);
    else
	probs->probs[probs->numProblems].str1 = NULL;

    if (altH)
	probs->probs[probs->numProblems].altH = headerLink(altH);
    else
	probs->probs[probs->numProblems].altH = NULL;

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
	if (probs->probs[i].altH) headerFree(probs->probs[i].altH);
    }
    free(probs);
}

static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs, Header origH,
			       enum fileActions * actions,
			       int allowBadRelocate) {
    int numValid, numRelocations;
    int i, j, madeSwap, rc;
    rpmRelocation * nextReloc, * relocations = NULL;
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation tmpReloc;
    char ** validRelocations, ** actualRelocations;
    char ** names;
    char ** origNames;
    int len = 0;
    char * newName;
    int_32 fileCount;
    Header h;
    int relocated = 0;

    if (!headerGetEntry(origH, RPMTAG_PREFIXES, NULL,
			(void **) &validRelocations, &numValid))
	numValid = 0;

    if (!rawRelocations && !numValid) return headerLink(origH);

    h = headerCopy(origH);

    if (rawRelocations) {
	for (i = 0; rawRelocations[i].newPath || rawRelocations[i].oldPath; 
		i++) ;
	numRelocations = i;
    } else {
	numRelocations = 0;
    }
    relocations = alloca(sizeof(*relocations) * numRelocations);

    for (i = 0; i < numRelocations; i++) {
	/* FIXME: default relocations (oldPath == NULL) need to be handled
	   in the UI, not rpmlib */

	relocations[i].oldPath = 
	    alloca(strlen(rawRelocations[i].oldPath) + 1);
	strcpy(relocations[i].oldPath, rawRelocations[i].oldPath);
	stripTrailingSlashes(relocations[i].oldPath);

	if (rawRelocations[i].newPath) {
	    relocations[i].newPath = 
		alloca(strlen(rawRelocations[i].newPath) + 1);
	    strcpy(relocations[i].newPath, rawRelocations[i].newPath);
	    stripTrailingSlashes(relocations[i].newPath);
	} else {
	    relocations[i].newPath = NULL;
	}

	if (relocations[i].newPath) {
	    for (j = 0; j < numValid; j++) 
		if (!strcmp(validRelocations[j],
			    relocations[i].oldPath)) break;
	    if (j == numValid && !allowBadRelocate) 
		psAppend(probs, RPMPROB_BADRELOCATE, alp->key, alp->h, 
			 relocations[i].oldPath, NULL,0 );
	}
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

    /* go through things backwards so that /usr/local relocations take
       precedence over /usr ones */
    for (i = fileCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {
	    nextReloc = relocations + j;
	    len = strlen(relocations[j].oldPath);
	    rc = (!strncmp(relocations[j].oldPath, names[i], len) && 
		    ((names[i][len] == '/') || !names[i][len]));
	    if (rc) break;
	}

	if (j >= 0) {
	    nextReloc = relocations + j;
	    if (nextReloc->newPath) {
		newName = alloca(strlen(nextReloc->newPath) + 
				 strlen(names[i]) + 1);
		strcpy(newName, nextReloc->newPath);
		strcat(newName, names[i] + len);
		rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
			   names[i], newName);
		names[i] = newName;
		relocated = 1;
	    } else if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s\n"), names[i]);
	    }
	} 
    }

    if (relocated) {
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &origNames, NULL);
	headerAddEntry(h, RPMTAG_ORIGFILENAMES, RPM_STRING_ARRAY_TYPE, 
			  origNames, fileCount);
	free(origNames);
	headerModifyEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, 
			  names, fileCount);
    }

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

static enum fileActions decideFileFate(const char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int brokenMd5) {
    char buffer[1024];
    char * dbAttr, * newAttr;
    enum fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int i, rc;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;

    if (lstat(filespec, &sb)) {
	/* the file doesn't exist on the disk create it unless the new
	   package has marked it as missingok */
	if (newFlags & RPMFILE_MISSINGOK) {
	    rpmMessage(RPMMESS_DEBUG, _("%s skipped due to missingok flag\n"),
			filespec);
	    return FA_SKIP;
	} else
	    return FA_CREATE;
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(dbMode);
    newWhat = whatis(newMode);

    /* RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
       them in older packages as well */
    if (newWhat == XDIR)
	return FA_CREATE;

    if (diskWhat != newWhat) {
	return save;
    } else if (newWhat != dbWhat && diskWhat != dbWhat) {
	return save;
    } else if (dbWhat != newWhat) {
	return FA_CREATE;
    } else if (dbWhat != LINK && dbWhat != REG) {
	return FA_CREATE;
    }

    if (dbWhat == REG) {
	if (brokenMd5)
	    rc = mdfileBroken(filespec, buffer);
	else
	    rc = mdfile(filespec, buffer);

	if (rc) {
	    /* assume the file has been removed, don't freak */
	    return FA_CREATE;
	}
	dbAttr = dbMd5;
	newAttr = newMd5;
    } else /* dbWhat == LINK */ {
	memset(buffer, 0, sizeof(buffer));
	i = readlink(filespec, buffer, sizeof(buffer) - 1);
	if (i == -1) {
	    /* assume the file has been removed, don't freak */
	    return FA_CREATE;
	}
	dbAttr = dbLink;
	newAttr = newLink;
     }

    /* this order matters - we'd prefer to CREATE the file if at all 
       possible in case something else (like the timestamp) has changed */

    if (!strcmp(dbAttr, buffer)) {
	/* this config file has never been modified, so 
	   just replace it */
	return FA_CREATE;
    }

    if (!strcmp(dbAttr, newAttr)) {
	/* this file is the same in all versions of this package */
	return FA_SKIP;
    }

    /* the config file on the disk has been modified, but
       the ones in the two packages are different. It would
       be nice if RPM was smart enough to at least try and
       merge the difference ala CVS, but... */
	    
    return save;
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

static int handleInstInstalledFiles(struct fileInfo * fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount, int reportConflicts,
				    rpmProblemSet probs) {
    Header h;
    int i;
    char ** otherMd5s, ** otherLinks;
    char * otherStates;
    uint_32 * otherFlags, * otherSizes;
    uint_16 * otherModes;
    int otherFileNum;
    int fileNum;
    int numReplaced = 0;

    if (!(h = rpmdbGetRecord(db, shared->otherPkg)))
	return 1;

    headerGetEntryMinMemory(h, RPMTAG_FILEMD5S, NULL,
			    (void **) &otherMd5s, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILELINKTOS, NULL,
			    (void **) &otherLinks, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILESTATES, NULL,
			    (void **) &otherStates, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILEMODES, NULL,
			    (void **) &otherModes, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILEFLAGS, NULL,
			    (void **) &otherFlags, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILESIZES, NULL,
			    (void **) &otherSizes, NULL);

    fi->replaced = malloc(sizeof(*fi->replaced) * sharedCount);

    for (i = 0; i < sharedCount; i++, shared++) {
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;
	if (otherStates[otherFileNum] == RPMFILE_STATE_NORMAL) {
	    if (filecmp(otherModes[otherFileNum],
			otherMd5s[otherFileNum],
			otherLinks[otherFileNum],
			fi->fmodes[fileNum],
			fi->fmd5s[fileNum],
			fi->flinks[fileNum])) {
		if (reportConflicts)
		    psAppend(probs, RPMPROB_FILE_CONFLICT, fi->ap->key, 
			     fi->ap->h, fi->fl[fileNum], h,0 );
		if (!(otherFlags[otherFileNum] | fi->fflags[fileNum])
			    & RPMFILE_CONFIG)
		    fi->replaced[numReplaced++] = *shared;
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

	    fi->replacedSizes[fileNum] = otherSizes[otherFileNum];
	}
    }

    free(otherMd5s);
    free(otherLinks);
    headerFree(h);

    fi->replaced = realloc(fi->replaced,
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}

static int handleRmvdInstalledFiles(struct fileInfo * fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount) {
    Header h;
    int otherFileNum;
    int fileNum;
    char * otherStates;
    int i;
    
    if (!(h = rpmdbGetRecord(db, shared->otherPkg)))
	return 1;

    headerGetEntryMinMemory(h, RPMTAG_FILESTATES, NULL,
			    (void **) &otherStates, NULL);

    for (i = 0; i < sharedCount; i++, shared++) {
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[fileNum] == RPMFILE_STATE_NORMAL)
	    fi->actions[fileNum] = FA_SKIP;
    }

    headerFree(h);

    return 0;
}

void handleOverlappedFiles(struct fileInfo * fi, hashTable ht,
			   rpmProblemSet probs, struct diskspaceInfo * dsl) {
    int i, j;
    struct fileInfo ** recs;
    int numRecs;
    int otherPkgNum, otherFileNum;
    struct stat sb;
    char mdsum[50];
    int rc;
    struct diskspaceInfo * ds = NULL;
    uint_32 fixupSize = 0;
   
    for (i = 0; i < fi->fc; i++) {
	if (fi->actions[i] == FA_SKIP || fi->actions[i] == FA_SKIPNSTATE)
	    continue;

	if (dsl) {
	    ds = dsl;
	    while (ds->block && ds->dev != fi->fps[i].dev) ds++;
	    if (!ds->block) ds = NULL;
	    fixupSize = 0;
	} 

	htGetEntry(ht, &fi->fps[i], (void ***) &recs, &numRecs, NULL);

	/* We need to figure out the current fate of this file. So,
	   work backwards from this file and look for a final action
	   we can work against. */
	for (j = 0; recs[j] != fi; j++);

	otherPkgNum = j - 1;
	otherFileNum = -1;			/* keep gcc quiet */
	while (otherPkgNum >= 0) {
	    if (recs[otherPkgNum]->type == TR_ADDED) {
		/* TESTME: there are more efficient searches in the world... */
		for (otherFileNum = 0; otherFileNum < recs[otherPkgNum]->fc; 
		     otherFileNum++)
		    if (FP_EQUAL(fi->fps[i], 
				 recs[otherPkgNum]->fps[otherFileNum])) 
			break;
		if ((otherFileNum >= 0) && 
		    (recs[otherPkgNum]->actions[otherFileNum] != FA_UNKNOWN))
		    break;
	    }
	    otherPkgNum--;
	}

	if (fi->type == TR_ADDED && otherPkgNum < 0) {
	    if (fi->actions[i] == FA_UNKNOWN) {
		if ((fi->fflags[i] & RPMFILE_CONFIG) && 
			    !lstat(fi->fl[i], &sb))
		    fi->actions[i] = FA_BACKUP;
		else
		    fi->actions[i] = FA_CREATE;
	    }
	} else if (fi->type == TR_ADDED) {
	    if (probs && filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			recs[otherPkgNum]->fmd5s[otherFileNum],
			recs[otherPkgNum]->flinks[otherFileNum],
			fi->fmodes[i],
			fi->fmd5s[i],
			fi->flinks[i])) {
		psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, fi->ap->key, 
			 fi->ap->h, fi->fl[i], recs[otherPkgNum]->ap->h, 0);
	    }

	    fixupSize = recs[otherPkgNum]->fsizes[otherFileNum];

	    /* FIXME: is this right??? it locks us into the config
	       file handling choice we already made, which may very
	       well be exactly right. What about noreplace files?? */
	    fi->actions[i] = FA_CREATE;
	} else if (fi->type == TR_REMOVED && otherPkgNum >= 0) {
	    fi->actions[i] = FA_SKIP;
	} else if (fi->type == TR_REMOVED) {
	    if (fi->actions[i] != FA_SKIP && fi->actions[i] != FA_SKIPNSTATE &&
			fi->fstates[i] == RPMFILE_STATE_NORMAL ) {
		if (S_ISREG(fi->fmodes[i]) && 
			    (fi->fflags[i] & RPMFILE_CONFIG)) {
		    rc = mdfile(fi->fl[i], mdsum);
		    if (!rc && strcmp(fi->fmd5s[i], mdsum)) {
			fi->actions[i] = FA_BACKUP;
		    } else {
			/* FIXME: config files may need to be saved */
			fi->actions[i] = FA_REMOVE;
		    }
		} else {
		    fi->actions[i] = FA_REMOVE;
		}
	    }
	}

	if (ds) {
	    uint_32 s = BLOCK_ROUND(fi->fsizes[i], ds->block);

	    switch (fi->actions[i]) {
	      case FA_BACKUP:
	      case FA_SAVE:
	      case FA_ALTNAME:
		ds->needed += s;
		break;

	      /* FIXME: If a two packages share a file (same md5sum), and 
		 that file is being replaced on disk, will ds->needed get
		 decremented twice? Quite probably! */
	      case FA_CREATE:
		ds->needed += s;
		ds->needed -= BLOCK_ROUND(fi->replacedSizes[i], ds->block);
		break;

	      case FA_REMOVE:
		ds->needed -= s;
		break;

	      default:
		break;
	    }

	    ds->needed -= BLOCK_ROUND(fixupSize, ds->block);
	}
    }
}

static int ensureOlder(rpmdb db, Header new, int dbOffset, rpmProblemSet probs,
		       const void * key) {
    Header old;
    int result, rc = 0;

    old = rpmdbGetRecord(db, dbOffset);
    if (old == NULL) return 1;

    result = rpmVersionCompare(old, new);
    if (result <= 0)
	rc = 0;
    else if (result > 0) {
	rc = 1;
	psAppend(probs, RPMPROB_OLDPACKAGE, key, new, NULL, old, 0);
    }

    headerFree(old);

    return rc;
}

static void skipFiles(struct fileInfo * fi, int noDocs) {
    int i, j;
    char ** netsharedPaths = NULL, ** nsp;
    char ** fileLangs, ** languages, ** lang;
    char * oneLang[2] = { NULL, NULL };
    int freeLanguages = 0;
    char * chptr;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	const char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	if (tmpPath && *tmpPath != '%')
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	xfree(tmpPath);
    }

    if (!headerGetEntry(fi->h, RPMTAG_FILELANGS, NULL, (void **) &fileLangs, 
			NULL))
	fileLangs = NULL;

    if ((chptr = getenv("LINGUAS"))) {
	languages = splitString(chptr, strlen(chptr), ':');
	freeLanguages = 1;
    } else if ((oneLang[0] = getenv("LANG"))) {
	languages = oneLang;
    } else {
	oneLang[0] = "en";
	languages = oneLang;
    }

    for (i = 0; i < fi->fc; i++) {
	if (fi->actions[i] == FA_SKIP || fi->actions[i] == FA_SKIPNSTATE)
	    continue;

	/* netsharedPaths are not relative to the current root (though 
	   they do need to take package relocations into account) */
	for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
	    j = strlen(*nsp);
	    if (!strncmp(fi->fl[i], *nsp, j) &&
		(fi->fl[i][j] == '\0' ||
		 fi->fl[i][j] == '/'))
		break;
	}

	if (nsp && *nsp) {
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}

	if (fileLangs && languages && *fileLangs[i]) {
	    for (lang = languages; *lang; lang++)
		if (!strcmp(*lang, fileLangs[i])) break;
	    if (!*lang) {
		fi->actions[i] = FA_SKIPNSTATE;
		continue;
	    }
	}

	if (noDocs && (fi->fflags[i] & RPMFILE_DOC))
	    fi->actions[i] = FA_SKIPNSTATE;
    }

    if (netsharedPaths) freeSplitString(netsharedPaths);
    if (fileLangs) free(fileLangs);
    if (freeLanguages) freeSplitString(languages);
}
