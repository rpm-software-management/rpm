#include "system.h"

#include "rpmlib.h"

#include "depends.h"
#include "fprint.h"
#include "hash.h"
#include "install.h"
#include "lookup.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"

struct fileInfo {
  /* for all packages */
    enum fileInfo_e { ADDED, REMOVED } type;
    enum fileActions * actions;
    fingerPrint * fps;
    uint_32 * fflags;
    char ** fl, ** fmd5s;
    uint_16 * fmodes;
    Header h;
    int fc;
  /* these are for ADDED packages */
    char ** flinks;
    struct availablePackage * ap;
    struct sharedFileInfo * replaced;
  /* for REMOVED packages */
    unsigned int record;
    char * fstates;
};

static rpmProblemSet psCreate(void);
static void psAppend(rpmProblemSet probs, rpmProblemType type, 
		     const void * key, Header h, char * str1, 
		     Header altHeader);
static int archOkay(Header h);
static int osOkay(Header h);
static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs, Header h, 
			       enum fileActions * actions);
static int psTrim(rpmProblemSet filter, rpmProblemSet target);
static int sharedCmp(const void * one, const void * two);
static enum fileActions decideFileFate(char * filespec, short dbMode, 
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
			   rpmProblemSet probs);

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

/* Return -1 on error, > 0 if newProbs is set, 0 if everything
   happened */
int rpmRunTransactions(rpmTransactionSet ts, rpmCallbackFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags) {
    int i, j;
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
    struct sharedFileInfo * shared, * sharedList;
    int numShared;
    int flEntries;
    int last;
    int beingRemoved;
    char * currDir, * chptr;
    FD_t fd;

    /* FIXME: what if the same package is included in ts twice? */

    /* FIXME: we completely ignore net shared paths here! */

    if (flags & RPMTRANS_FLAG_TEST) {
	instFlags |= RPMINSTALL_TEST; 
	rmFlags |= RPMUNINSTALL_TEST;
    }

    probs = psCreate();
    *newProbs = probs;
    hdrs = alloca(sizeof(*hdrs) * al->size);

    for (alp = al->list; (alp - al->list) < al->size; alp++) {
	if (!archOkay(alp->h))
	    psAppend(probs, RPMPROB_BADARCH, alp->key, alp->h, NULL, NULL);

	if (!osOkay(alp->h)) {
	    psAppend(probs, RPMPROB_BADOS, alp->key, alp->h, NULL, NULL);
	}

	rc = findMatches(ts->db, alp->name, alp->version, alp->release, &dbi);
	if (rc == 2) {
	    return -1;
	} else if (!rc) {
	    prob.key = alp->key;
	    psAppend(probs, RPMPROB_PKG_INSTALLED, alp->key, alp->h, NULL, 
		     NULL);
	    dbiFreeIndexRecord(dbi);
	}

	if (headerGetEntry(alp->h, RPMTAG_FILENAMES, NULL, NULL, &fileCount))
	    totalFileCount += fileCount;
    }

    /* FIXME: it seems a bit silly to read in all of these headers twice */
    for (i = 0; i < ts->numRemovedPackages; i++, fi++) {
	Header h;

	if ((h = rpmdbGetRecord(ts->db, ts->removedPackages[i]))) {
	    if (headerGetEntry(h, RPMTAG_FILENAMES, NULL, NULL, 
			       &fileCount))
		totalFileCount += fileCount;
	}
    }

    flEntries = al->size + ts->numRemovedPackages;
    flList = alloca(sizeof(*flList) * (flEntries));

    ht = htCreate(totalFileCount * 2, 0, fpHashFunction, fpEqual);

    /* FIXME?: we'd be better off assembling one very large file list and
       calling fpLookupList only once. I'm not sure that the speedup is
       worth the trouble though. */
    for (fi = flList, alp = al->list; (alp - al->list) < al->size; 
		fi++, alp++) {
	if (!headerGetEntryMinMemory(alp->h, RPMTAG_FILENAMES, NULL, 
				     (void *) NULL, &fi->fc)) {
	    fi->fc = 0;
	    fi->h = alp->h;
	    continue;
	}

	fi->actions = calloc(sizeof(*fi->actions), fi->fc);
	fi->h = hdrs[alp - al->list] = relocateFileList(alp, probs, alp->h, 
						         fi->actions);

	headerGetEntryMinMemory(fi->h, RPMTAG_FILENAMES, NULL, 
				     (void *) &fi->fl, &fi->fc);

	headerGetEntryMinMemory(fi->h, RPMTAG_FILEMD5S, NULL, 
				(void *) &fi->fmd5s, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILELINKTOS, NULL, 
				(void *) &fi->flinks, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILEMODES, NULL, 
				(void *) &fi->fmodes, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILEFLAGS, NULL, 
				(void *) &fi->fflags, NULL);

	fi->type = ADDED;
        fi->fps = alloca(fi->fc * sizeof(*fi->fps));
	fi->ap = alp;
	fi->replaced = NULL;
    }

    for (i = 0; i < ts->numRemovedPackages; i++, fi++) {
	fi->type = REMOVED;
	fi->record = ts->removedPackages[i];
	fi->h = rpmdbGetRecord(ts->db, fi->record);
	if (!fi->h) {
	    /* ACK! */
	    continue;
	}
	if (!headerGetEntryMinMemory(fi->h, RPMTAG_FILENAMES, NULL, 
				     (void *) &fi->fl, &fi->fc)) {
	    fi->fc = 0;
	    continue;
	}
	headerGetEntryMinMemory(fi->h, RPMTAG_FILEFLAGS, NULL, 
				(void *) &fi->fflags, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILEMD5S, NULL, 
				(void *) &fi->fmd5s, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILEMODES, NULL, 
				(void *) &fi->fmodes, NULL);
	headerGetEntryMinMemory(fi->h, RPMTAG_FILESTATES, NULL, 
				(void *) &fi->fstates, NULL);

	fi->actions = calloc(sizeof(*fi->actions), fi->fc);
        fi->fps = alloca(fi->fc * sizeof(*fi->fps));
    }

    for (fi = flList; (fi - flList) < flEntries; fi++) {
	fpLookupList(fi->fl, fi->fps, fi->fc, 1);
	for (i = 0; i < fi->fc; i++) {
	    htAddEntry(ht, fi->fps + i, fi);
	}
    }

    chptr = currentDirectory();
    currDir = alloca(strlen(chptr) + 1);
    strcpy(currDir, chptr);
    free(chptr);
    chdir("/");
    chroot(ts->root);

    for (fi = flList; (fi - flList) < flEntries; fi++) {
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
	    last = i + 1;
	    j = sharedList[i].otherPkg;
	    while ((sharedList[last].otherPkg == j) && (last < numShared))
		last++;
	    last--;

	    for (j = 0; j < ts->numRemovedPackages; j++)
		if (ts->removedPackages[j] == sharedList[i].otherPkg)
		    break;
	    beingRemoved = (j < ts->numRemovedPackages);

	    if (fi->type == ADDED)
		handleInstInstalledFiles(fi, ts->db, sharedList + i, 
					 last - i + 1, !beingRemoved, probs);
	    else if (fi->type == REMOVED && !beingRemoved)
		handleRmvdInstalledFiles(fi, ts->db, sharedList + i, 
					 last - i + 1);

	    i = last + 1;
	}

	free(sharedList);

	handleOverlappedFiles(fi, ht, probs);
    }

    chroot(".");
    chdir(currDir);

    htFree(ht);

    for (alp = al->list, fi = flList; (alp - al->list) < al->size; 
		alp++, fi++) {
	if (fi->fc) {
	    free(fi->fl);
	    if (fi->type == ADDED) {
		free(fi->fmd5s);
		free(fi->flinks);
	    }
	}
    }

    if ((flags & RPMTRANS_FLAG_BUILD_PROBS) || 
           (probs->numProblems && (!okProbs || psTrim(okProbs, probs)))) {
	*newProbs = probs;
	for (i = 0; i < al->size; i++)
	    headerFree(hdrs[i]);

	for (alp = al->list, fi = flList; (alp - al->list) < al->size; 
			alp++, fi++) {
	    if (fi->fc)
		free(fi->actions);
	}

	return al->size + ts->numRemovedPackages;
    }

    for (alp = al->list, fi = flList; (alp - al->list) < al->size; 
			alp++, fi++) {
	if (alp->fd) {
	    fd = alp->fd;
	} else {
	    fd = notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0, 
			alp->key, notifyData);
	    if (fd) {
		Header h;

		headerFree(hdrs[alp - al->list]);
		rc = rpmReadPackageHeader(fd, &h, NULL, NULL, NULL);
		if (rc) {
		    notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0, 
				alp->key, notifyData);
		    ourrc++;
		    fd = NULL;
		} else {
		    hdrs[alp - al->list] = 
			relocateFileList(alp, probs, h, NULL);
		    headerFree(h);
		}
	    }
	}

	if (fd) {
	    if (installBinaryPackage(ts->root, ts->db, fd, 
				     hdrs[alp - al->list], instFlags, notify, 
				     notifyData, alp->key, fi->actions, 
				     fi->replaced))
		ourrc++;
	} else {
	    ourrc++;
	}

	headerFree(hdrs[alp - al->list]);

	if (fi->fc)
	    free(fi->actions);

	if (!alp->fd && fd)
	    notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0, alp->key, 
		   notifyData);
    }

    /* fi is left at the first package which is to be removed */
    for (i = 0; i < ts->numRemovedPackages; i++, fi++) {
	if (removeBinaryPackage(ts->root, ts->db, ts->removedPackages[i], 
				rmFlags, fi->actions))
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

static void psAppend(rpmProblemSet probs, rpmProblemType type, 
		     const void * key, Header h, char * str1, Header altH) {
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
    }
    free(probs);
}

static Header relocateFileList(struct availablePackage * alp, 
			       rpmProblemSet probs, Header origH,
			       enum fileActions * actions) {
    int numValid, numRelocations;
    int i, j, madeSwap, rc;
    rpmRelocation * nextReloc, * relocations = NULL;
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation tmpReloc;
    char ** validRelocations, ** actualRelocations;
    char ** names;
    char ** origNames;
    int len, newLen;
    char * newName;
    int_32 fileCount;
    Header h;
    int relocated = 0;

    if (!rawRelocations) return headerLink(origH);
    h = headerCopy(origH);

    if (!headerGetEntry(h, RPMTAG_PREFIXES, NULL,
			(void **) &validRelocations, &numValid))
	numValid = 0;

    for (i = 0; rawRelocations[i].newPath || rawRelocations[i].oldPath; i++) ;
    numRelocations = i;
    relocations = alloca(sizeof(*relocations) * numRelocations);

    /* FIXME? this code assumes the validRelocations array won't
       have trailing /'s in it */
    /* FIXME: all of this needs to be tested with an old format
       relocateable package */

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
	    if (j == numValid)
		psAppend(probs, RPMPROB_BADRELOCATE, alp->key, alp->h, 
			 relocations[i].oldPath, NULL);
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
    nextReloc = relocations + numRelocations - 1;
    len = strlen(nextReloc->oldPath);
    newLen = nextReloc->newPath ? strlen(nextReloc->newPath) : 0;
    for (i = fileCount - 1; i >= 0 && nextReloc; i--) {
	do {
	    rc = (!strncmp(nextReloc->oldPath, names[i], len) && 
		    (names[i][len] == '/'));
	    if (!rc) {
		if (nextReloc == relocations) {
		    nextReloc = 0;
		} else {
		    nextReloc--;
		    len = strlen(nextReloc->oldPath);
		    newLen = nextReloc->newPath ? 
				strlen(nextReloc->newPath) : 0;
		}
	    }
	} while (!rc && nextReloc);

	if (rc) {
	    if (nextReloc->newPath) {
		newName = alloca(newLen + strlen(names[i]) + 1);
		strcpy(newName, nextReloc->newPath);
		strcat(newName, names[i] + len);
		rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
			   names[i], newName);
		names[i] = newName;
		relocated = 1;
	    } else if (actions) {
		actions[i] = SKIPNSTATE;
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

static enum fileActions decideFileFate(char * filespec, short dbMode, 
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
	return SKIP;
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

static int handleInstInstalledFiles(struct fileInfo * fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount, int reportConflicts,
				    rpmProblemSet probs) {
    Header h;
    int i;
    char ** otherMd5s, ** otherLinks;
    char * otherStates;
    uint_32 * otherFlags;
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
			     fi->ap->h, fi->fl[fileNum], h);
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
	    fi->actions[fileNum] = SKIP;
    }

    return 0;
}

void handleOverlappedFiles(struct fileInfo * fi, hashTable ht,
			     rpmProblemSet probs) {
    int i, j;
    struct fileInfo ** recs;
    int numRecs;
    int otherPkgNum, otherFileNum;
    struct stat sb;
    char mdsum[50];
    int rc;
   
    for (i = 0; i < fi->fc; i++) {
	htGetEntry(ht, &fi->fps[i], (void ***) &recs, &numRecs, NULL);

	/* We need to figure out the current fate of this file. So,
	   work backwards from this file and look for a final action
	   we can work against. */
	for (j = 0; recs[j] != fi; j++);

	otherPkgNum = j - 1;
	otherFileNum = -1;			/* keep gcc quiet */
	while (otherPkgNum >= 0) {
	    if (recs[otherPkgNum]->type == ADDED) {
		/* TESTME: there are more efficient searches in the world... */
		for (otherFileNum = 0; otherFileNum < recs[otherPkgNum]->fc; 
		     otherFileNum++)
		    if (FP_EQUAL(fi->fps[i], 
				 recs[otherPkgNum]->fps[otherFileNum])) 
			break;
		if ((otherFileNum >= 0) && 
		    (recs[otherPkgNum]->actions[otherFileNum] == CREATE))
		    break;
	    }
	    otherPkgNum--;
	}

	if (fi->type == ADDED && otherPkgNum < 0) {
	    /* If it isn't in the database, install it. 
	       FIXME: check for config files here for .rpmorig purporses! */
	    if (fi->actions[i] == UNKNOWN) {
		if ((fi->fflags[i] & RPMFILE_CONFIG) && 
			    !lstat(fi->fl[i], &sb))
		    fi->actions[i] = BACKUP;
		else
		    fi->actions[i] = CREATE;
	    }
	} else if (fi->type == ADDED) {
	    if (filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			recs[otherPkgNum]->fmd5s[otherFileNum],
			recs[otherPkgNum]->flinks[otherFileNum],
			fi->fmodes[i],
			fi->fmd5s[i],
			fi->flinks[i])) {
		psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, fi->ap->key, 
			 fi->ap->h, fi->fl[i], recs[otherPkgNum]->ap->h);
	    }

	    /* FIXME: is this right??? it locks us into the config
	       file handling choice we already made, which may very
	       well be exactly right. */
	    fi->actions[i] = recs[otherPkgNum]->actions[otherFileNum];
	    recs[otherPkgNum]->actions[otherFileNum] = SKIP;
	} else if (fi->type == REMOVED && otherPkgNum >= 0) {
	    fi->actions[i] = SKIP;
	} else if (fi->type == REMOVED) {
	    if (fi->actions[i] != SKIP && fi->actions[i] != SKIPNSTATE &&
			fi->fstates[i] == RPMFILE_STATE_NORMAL ) {
		if (S_ISREG(fi->fmodes[i]) && 
			    (fi->fflags[i] & RPMFILE_CONFIG)) {
		    rc = mdfile(fi->fl[i], mdsum);
		    if (!rc && strcmp(fi->fmd5s[i], mdsum)) {
			fi->actions[i] = BACKUP;
		    } else {
			/* FIXME: config files may need to be saved */
			fi->actions[i] = REMOVE;
		    }
		} else {
		    fi->actions[i] = REMOVE;
		}
	    }
	}
    }
}
