#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

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
#if STATFS_IN_SYS_STATVFS
# include <sys/statvfs.h>
#else
# if STATFS_IN_SYS_VFS
#  include <sys/vfs.h>
# else
#  if STATFS_IN_SYS_MOUNT
#   include <sys/mount.h>
#  else
#   if STATFS_IN_SYS_STATFS
#    include <sys/statfs.h>
#   endif
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

/* Adjust for root only reserved space. On linux e2fs, this is 5%. */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

#define	XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED)

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

	switch (fi->type) {
	case TR_REMOVED:
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
	    break;
	case TR_ADDED:
	    break;
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

void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
{
    ts->scriptFd = fd;
}

static rpmProblemSet psCreate(void)
{
    rpmProblemSet probs;

    probs = malloc(sizeof(*probs));
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

static void psAppend(rpmProblemSet probs, rpmProblemType type,
		     const void * key, Header h, const char * str1,
		     Header altH, unsigned long ulong1)
{
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

static int archOkay(Header h)
{
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

static int osOkay(Header h)
{
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

void rpmProblemSetFree(rpmProblemSet probs)
{
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
			       int allowBadRelocate)
{
    int numValid, numRelocations;
    int i, j;
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation * relocations = NULL;
    const char ** validRelocations;
    const char ** names;
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

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < numRelocations; i++) {
	/* FIXME: default relocations (oldPath == NULL) need to be handled
	   in the UI, not rpmlib */

	{   const char *s = rawRelocations[i].oldPath;
	    char *t = alloca(strlen(s) + 1);
	    strcpy(t, s);
	    stripTrailingSlashes(t);
	    relocations[i].oldPath = t;
	}

	if (rawRelocations[i].newPath) {
	    const char *s = rawRelocations[i].newPath;
	    char *t = alloca(strlen(s) + 1);
	    strcpy(t, s);
	    stripTrailingSlashes(t);
	    relocations[i].newPath = t;

	    /* Verify that the relocation's old path is in the header. */
	    for (j = 0; j < numValid; j++)
		if (!strcmp(validRelocations[j], relocations[i].oldPath)) break;
	    if (j == numValid && !allowBadRelocate)
		psAppend(probs, RPMPROB_BADRELOCATE, alp->key, alp->h,
			 relocations[i].oldPath, NULL,0 );
	} else {
	    relocations[i].newPath = NULL;
	}
    }

    /* stupid bubble sort, but it's probably faster here */
    for (i = 0; i < numRelocations; i++) {
	int madeSwap;
	madeSwap = 0;
	for (j = 1; j < numRelocations; j++) {
	    rpmRelocation tmpReloc;
	    if (strcmp(relocations[j - 1].oldPath, relocations[j].oldPath) <= 0)
		continue;
	    tmpReloc = relocations[j - 1];
	    relocations[j - 1] = relocations[j];
	    relocations[j] = tmpReloc;
	    madeSwap = 1;
	}
	if (!madeSwap) break;
    }

    /* Add relocation values to the header */
    if (numValid) {
	const char ** actualRelocations;
	actualRelocations = malloc(sizeof(*actualRelocations) * numValid);
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (strcmp(validRelocations[i], relocations[j].oldPath))
		    continue;
		actualRelocations[i] = relocations[j].newPath;
		break;
	    }
	    if (j == numRelocations)
		actualRelocations[i] = validRelocations[i];
	}

	headerAddEntry(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numValid);

	xfree(actualRelocations);
	xfree(validRelocations);
    }

    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &names, &fileCount);

    /* Relocate all file paths.
     * Go through sorted file and relocation lists backwards so that /usr/local
     * relocations take precedence over /usr ones.
     */
    for (i = fileCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {
	    int len;
	    len = strlen(relocations[j].oldPath);
	    if (strncmp(relocations[j].oldPath, names[i], len))
		continue;

	    /* Only directories or complete file paths can be relocated */
	    if (!(names[i][len] == '/' || names[i][len] == '\0'))
		continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		const char *s = relocations[j].newPath;
		char *t = alloca(strlen(s) + strlen(names[i]) - len + 1);
		strcpy(t, s);
		strcat(t, names[i] + len);
		rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
			names[i], t);
		names[i] = t;
		relocated = 1;
	    } else /* On install, a relocate to NULL means skip the file */
	    if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s\n"), names[i]);
	    }
	    break;
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (relocated) {
	const char ** origNames;
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &origNames, NULL);
	headerAddEntry(h, RPMTAG_ORIGFILENAMES, RPM_STRING_ARRAY_TYPE,
			  origNames, fileCount);
	xfree(origNames);
	headerModifyEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE,
			  names, fileCount);
    }

    xfree(names);

    return h;
}

static int psTrim(rpmProblemSet filter, rpmProblemSet target)
{
    /* As the problem sets are generated in an order solely dependent
       on the ordering of the packages in the transaction, and that
       ordering can't be changed, the problem sets must be parallel to
       one another. Additionally, the filter set must be a subset of the
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

static int sharedCmp(const void * one, const void * two)
{
    const struct sharedFileInfo * a = one;
    const struct sharedFileInfo * b = two;

    if (a->otherPkg < b->otherPkg)
	return -1;
    else if (a->otherPkg > b->otherPkg)
	return 1;

    return 0;
}

static enum fileTypes whatis(short mode)
{
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

static enum fileActions decideFileFate(const char * filespec, short dbMode,
			const char * dbMd5, const char * dbLink, short newMode,
			const char * newMd5, const char * newLink, int newFlags,
				int brokenMd5)
{
    char buffer[1024];
    const char * dbAttr, * newAttr;
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
	} else {
	    return FA_CREATE;
	}
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(dbMode);
    newWhat = whatis(newMode);

    /* RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
       them in older packages as well */
    if (newWhat == XDIR) {
	return FA_CREATE;
    }

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

static int filecmp(short mode1, const char * md51, const char * link1,
	           short mode2, const char * md52, const char * link2)
{
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
				    rpmProblemSet probs)
{
    Header h;
    int i;
    const char ** otherMd5s;
    const char ** otherLinks;
    const char * otherStates;
    uint_32 * otherFlags;
    uint_32 * otherSizes;
    uint_16 * otherModes;
    int numReplaced = 0;

    if ((h = rpmdbGetRecord(db, shared->otherPkg)) == NULL)
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
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

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
			& RPMFILE_CONFIG) {
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
	    }
	}

	if ((otherFlags[otherFileNum] | fi->fflags[fileNum]) & RPMFILE_CONFIG) {
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
			            int sharedCount)
{
    Header h;
    const char * otherStates;
    int i;
   
    if ((h = rpmdbGetRecord(db, shared->otherPkg)) == NULL)
	return 1;

    headerGetEntryMinMemory(h, RPMTAG_FILESTATES, NULL,
			    (void **) &otherStates, NULL);

    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	fi->actions[fileNum] = FA_SKIP;
    }

    headerFree(h);

    return 0;
}

static void handleOverlappedFiles(struct fileInfo * fi, hashTable ht,
			   rpmProblemSet probs, struct diskspaceInfo * dsl)
{
    int i, j;
    struct diskspaceInfo * ds = NULL;
    uint_32 fixupSize = 0;
  
    for (i = 0; i < fi->fc; i++) {
	int otherPkgNum, otherFileNum;
	const struct fileInfo ** recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	if (dsl) {
	    ds = dsl;
	    while (ds->block && ds->dev != fi->fps[i].dev) ds++;
	    if (!ds->block) ds = NULL;
	    fixupSize = 0;
	}

	/*
	 * Retrieve all records that apply to this file. Note that the
	 * file info records were built in the same order as the packages
	 * will be installed and removed so the records for an overlapped
	 * files will be sorted in exactly the same order.
	 */
	htGetEntry(ht, &fi->fps[i], (const void ***) &recs, &numRecs, NULL);

	/*
	 * If this package is being added, look only at other packages
	 * being added -- removed packages dance to a different tune.
	 * If both this and the other package are being added, overlapped
	 * files must be identical (or marked as a conflict). The
	 * disposition of already installed config files leads to
	 * a small amount of extra complexity.
	 *
	 * If this package is being removed, then there are two cases that
	 * need to be worried about:
	 * If the other package is being added, then skip any overlapped files
	 * so that this package removal doesn't nuke the overlapped files
	 * that were just installed.
	 * If both this and the other package are being removed, then each
	 * file removal from preceding packages needs to be skipped so that
	 * the file removal occurs only on the last occurence of an overlapped
	 * file in the transaction set.
	 *
	 */

	/* Locate this overlapped file in the set of added/removed packages. */
	for (j = 0; recs[j] != fi; j++)
	    ;

	/* Find what the previous disposition of this file was. */
	otherFileNum = -1;			/* keep gcc quiet */
	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    /* Added packages need only look at other added packages. */
	    if (fi->type == TR_ADDED && recs[otherPkgNum]->type != TR_ADDED)
		continue;

	    /* TESTME: there are more efficient searches in the world... */
	    for (otherFileNum = 0; otherFileNum < recs[otherPkgNum]->fc;
		 otherFileNum++) {
		if (FP_EQUAL(fi->fps[i], recs[otherPkgNum]->fps[otherFileNum]))
			break;
	    }
	    /* XXX is this test still necessary? */
	    if (recs[otherPkgNum]->actions[otherFileNum] != FA_UNKNOWN)
		break;
	}

	switch (fi->type) {
	struct stat sb;
	case TR_ADDED:
	    if (otherPkgNum < 0) {
		/* XXX is this test still necessary? */
		if (fi->actions[i] != FA_UNKNOWN)
		    break;
		if ((fi->fflags[i] & RPMFILE_CONFIG) && !lstat(fi->fl[i], &sb)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (fi->fflags[i] & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
		break;
	    }

	    /* Mark added overlapped nnon-identical files as a conflict. */
	    if (probs && filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			recs[otherPkgNum]->fmd5s[otherFileNum],
			recs[otherPkgNum]->flinks[otherFileNum],
			fi->fmodes[i],
			fi->fmd5s[i],
			fi->flinks[i])) {
		psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, fi->ap->key,
			 fi->ap->h, fi->fl[i], recs[otherPkgNum]->ap->h, 0);
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = recs[otherPkgNum]->fsizes[otherFileNum];

	    if ((fi->fflags[i] & RPMFILE_CONFIG) && !lstat(fi->fl[i], &sb)) {
		/* Here is an overlapped  pre-existing config file. */
		fi->actions[i] = (fi->fflags[i] & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_SKIP;
	    } else {
		fi->actions[i] = FA_CREATE;
	    }
	    break;
	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
#if XXX_ERASED_PACKAGES_LAST
		fi->actions[i] = FA_SKIP;
		break;
#else
		/* Here is an overlapped added file we don't want to nuke */
		if (recs[otherPkgNum]->actions[otherFileNum] != FA_REMOVE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		recs[otherPkgNum]->actions[otherFileNum] = FA_SKIP;
#endif
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		break;
	    if (fi->fstates[i] != RPMFILE_STATE_NORMAL)
		break;
	    if (!(S_ISREG(fi->fmodes[i]) && (fi->fflags[i] & RPMFILE_CONFIG))) {
		fi->actions[i] = FA_REMOVE;
		break;
	    }
		
	    /* Here is a pre-existing modified config file that needs saving. */
	    {	char mdsum[50];
		if (!mdfile(fi->fl[i], mdsum) && strcmp(fi->fmd5s[i], mdsum)) {
		    fi->actions[i] = FA_BACKUP;
		    break;
		}
	    }
	    fi->actions[i] = FA_REMOVE;
	    break;
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
		       const void * key)
{
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

static void skipFiles(struct fileInfo * fi, int noDocs)
{
    int i;
    char ** netsharedPaths = NULL;
    const char ** fileLangs;
    const char ** languages;
    const char *s;

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

    s = rpmExpand("%{_install_langs}", NULL);
    if (!(s && *s != '%')) {
	if (s) xfree(s);
	s = NULL;
    }
    if (s) {
	languages = (const char **) splitString(s, strlen(s), ':');
	xfree(s);
    /* XXX LINGUAS/LANG is used by the installer so leave alone for now */
    } else if ((s = getenv("LINGUAS")) || (s = getenv("LANG")) || (s = "en")) {
	languages = (const char **) splitString(s, strlen(s), ':');
    } else
	languages = NULL;

    for (i = 0; i < fi->fc; i++) {
	char **nsp;

	/* Don't bother with skipped files */
	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	/*
	 * Skip net shared paths.
	 * Net shared paths are not relative to the current root (though
	 * they do need to take package relocations into account).
	 */
	for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
	    int len;
	    len = strlen(*nsp);
	    if (strncmp(fi->fl[i], *nsp, len))
		continue;

	    /* Only directories or complete file paths can be net shared */
	    if (!(fi->fl[i][len] == '/' || fi->fl[i][len] == '\0'))
		continue;
	    break;
	}

	if (nsp && *nsp) {
	    fi->actions[i] = FA_SKIPNETSHARED;
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	if (fileLangs && languages && *fileLangs[i]) {
	    const char **lang, *l, *le;
	    for (lang = languages; *lang; lang++) {
		if (!strcmp(*lang, "all"))
		    break;
		for (l = fileLangs[i]; *l; l = le) {
		    for (le = l; *le && *le != '|'; le++)
			;
		    if ((le-l) > 0 && !strncmp(*lang, l, (le-l)))
			break;
		    if (*le == '|') le++;	/* skip over | */
		}
		if (*l)	break;
	    }
	    if (*lang == NULL) {
		fi->actions[i] = FA_SKIPNSTATE;
		continue;
	    }
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (fi->fflags[i] & RPMFILE_DOC))
	    fi->actions[i] = FA_SKIPNSTATE;
    }

    if (netsharedPaths) freeSplitString(netsharedPaths);
    if (fileLangs) free(fileLangs);
    if (languages) freeSplitString((char **)languages);
}

#define	NOTIFY(_x)	if (notify) notify _x

/* Return -1 on error, > 0 if newProbs is set, 0 if everything happened */

int rpmRunTransactions(rpmTransactionSet ts, rpmCallbackFunction notify,
		       void * notifyData, rpmProblemSet okProbs,
		       rpmProblemSet * newProbs, int flags, int ignoreSet)
{
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
    int nexti;
    int lastFailed;
    const char * currDir;
    FD_t fd;
    const char ** filesystems;
    int filesystemCount;
    struct diskspaceInfo * di = NULL;
    int oc;

    /* FIXME: what if the same package is included in ts twice? */

    /* Get available space on mounted file systems */
    if (!(ignoreSet & RPMPROB_FILTER_DISKSPACE) &&
		!rpmGetFilesystemList(&filesystems, &filesystemCount)) {
	struct stat sb;

	di = alloca(sizeof(*di) * (filesystemCount + 1));

	for (i = 0; (i < filesystemCount) && di; i++) {
#if STATFS_IN_SYS_STATVFS
	    struct statvfs sfb;
	    if (statvfs(filesystems[i], &sfb))
#else
	    struct statfs sfb;
#  if STAT_STATFS4
/* this platform has the 4-argument version of the statfs call.  The last two
 * should be the size of struct statfs and 0, respectively.  The 0 is the
 * filesystem type, and is always 0 when statfs is called on a mounted
 * filesystem, as we're doing.
 */
	    if (statfs(filesystems[i], &sfb, sizeof(sfb), 0))
#  else
	    if (statfs(filesystems[i], &sfb))
#  endif
#endif
	    {
		di = NULL;
	    } else {
		di[i].block = sfb.f_bsize;
		di[i].needed = 0;
#ifdef STATFS_HAS_F_BAVAIL
		di[i].avail = sfb.f_bavail;
#else
/* FIXME: the statfs struct doesn't have a member to tell how many blocks are
 * available for non-superusers.  f_blocks - f_bfree is probably too big, but
 * it's about all we can do.
 */
		di[i].avail = sfb.f_blocks - sfb.f_bfree;
#endif


		stat(filesystems[i], &sb);
		di[i].dev = sb.st_dev;
	    }
	}

	if (di) di[i].block = 0;
    }

    probs = psCreate();
    *newProbs = probs;
    hdrs = alloca(sizeof(*hdrs) * ts->addedPackages.size);

    /* ===============================================
     * For packages being installed:
     * - verify package arch/os.
     * - verify package epoch:version-release is newer.
     * - count files.
     * For packages being removed:
     * - count files.
     */
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
		for (i = 0; i < dbiIndexSetCount(dbi); i++)
		    ensureOlder(ts->db, alp->h, dbiIndexRecordOffset(dbi, i),
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
	    headerFree(h);
	}
    }

    /* ===============================================
     * Initialize file list:
     */
    flEntries = ts->addedPackages.size + ts->numRemovedPackages;
    flList = alloca(sizeof(*flList) * (flEntries));

    ht = htCreate(totalFileCount * 2, 0, fpHashFunction, fpEqual);

    /* FIXME?: we'd be better off assembling one very large file list and
       calling fpLookupList only once. I'm not sure that the speedup is
       worth the trouble though. */
    for (fi = flList, oc = 0; oc < ts->orderCount; fi++, oc++) {
	memset(fi, 0, sizeof(*fi));

	switch (ts->order[oc].type) {
	case TR_ADDED:
	    i = ts->order[oc].u.addedIndex;
	    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;

	    if (!headerGetEntryMinMemory(alp->h, RPMTAG_FILENAMES, NULL,
					 NULL, &fi->fc)) {
		fi->h = headerLink(alp->h);
		hdrs[i] = headerLink(fi->h);
		continue;
	    }

	    /* Allocate file actions (and initialize to RPMFILE_STATE_NORMAL) */
	    fi->actions = calloc(sizeof(*fi->actions), fi->fc);
	    hdrs[i] = relocateFileList(alp, probs, alp->h, fi->actions,
			          ignoreSet & RPMPROB_FILTER_FORCERELOCATE);
	    fi->h = headerLink(hdrs[i]);
	    fi->type = TR_ADDED;
	    fi->ap = alp;
	    break;
	case TR_REMOVED:
	    fi->record = ts->order[oc].u.removed.dboffset;
	    fi->h = rpmdbGetRecord(ts->db, fi->record);
	    if (fi->h == NULL) {
		/* ACK! */
		continue;
	    }
	    fi->type = TR_REMOVED;
	    break;
	}

	if (!headerGetEntry(fi->h, RPMTAG_FILENAMES, NULL,
				     (void **) &fi->fl, &fi->fc)) {
	    /* This catches removed packages w/ no file lists */
	    fi->fc = 0;
	    continue;
	}

	/* actions is initialized earlier for added packages */
	if (fi->actions == NULL)
	    fi->actions = calloc(sizeof(*fi->actions), fi->fc);

	headerGetEntry(fi->h, RPMTAG_FILEMODES, NULL,
				(void **) &fi->fmodes, NULL);
	headerGetEntry(fi->h, RPMTAG_FILEFLAGS, NULL,
				(void **) &fi->fflags, NULL);
	headerGetEntry(fi->h, RPMTAG_FILESIZES, NULL,
				(void **) &fi->fsizes, NULL);
	headerGetEntry(fi->h, RPMTAG_FILESTATES, NULL,
				(void **) &fi->fstates, NULL);

	switch (ts->order[oc].type) {
	case TR_REMOVED:
	    headerGetEntry(fi->h, RPMTAG_FILEMD5S, NULL,
				    (void **) &fi->fmd5s, NULL);
	    headerGetEntry(fi->h, RPMTAG_FILELINKTOS, NULL,
				    (void **) &fi->flinks, NULL);
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
	    break;
	case TR_ADDED:
	    headerGetEntryMinMemory(fi->h, RPMTAG_FILEMD5S, NULL,
				    (void **) &fi->fmd5s, NULL);
	    headerGetEntryMinMemory(fi->h, RPMTAG_FILELINKTOS, NULL,
				    (void **) &fi->flinks, NULL);

	    /* 0 makes for noops */
	    fi->replacedSizes = calloc(fi->fc, sizeof(*fi->replacedSizes));

	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    skipFiles(fi, flags & RPMTRANS_FLAG_NODOCS);
	    break;
	}

        fi->fps = malloc(fi->fc * sizeof(*fi->fps));
    }

    /* There are too many returns to plug this leak. Use alloca instead. */
    {  const char *s = currentDirectory();
       char *t = alloca(strlen(s) + 1);
       strcpy(t, s);
       currDir = t;
       xfree(s);
    }

    chdir("/");
    chroot(ts->root);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    for (fi = flList; (fi - flList) < flEntries; fi++) {
	fpLookupList(fi->fl, fi->fps, fi->fc, 1);
	for (i = 0; i < fi->fc; i++) {
	    if (XFA_SKIPPING(fi->actions[i]))
		continue;
	    htAddEntry(ht, fi->fps + i, fi);
	}
    }

    NOTIFY((NULL, RPMCALLBACK_TRANS_START, 6, flEntries, NULL, notifyData));

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    for (fi = flList; (fi - flList) < flEntries; fi++) {
	int knownBad;

	NOTIFY((NULL, RPMCALLBACK_TRANS_PROGRESS, (fi - flList), flEntries,
	       NULL, notifyData));

	/* Extract file info for all files in this package from the database. */
	matches = malloc(sizeof(*matches) * fi->fc);
	if (rpmdbFindFpList(ts->db, fi->fps, matches, fi->fc)) return 1;

	numShared = 0;
	for (i = 0; i < fi->fc; i++)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = malloc(sizeof(*sharedList) * (numShared + 1));
	for (i = 0; i < fi->fc; i++) {
	    /*
	     * Take care not to mark files as replaced in packages that will
	     * have been removed before we will get here.
	     */
	    for (j = 0; j < dbiIndexSetCount(matches[i]); j++) {
		int k, ro;
		ro = dbiIndexRecordOffset(matches[i], j);
		knownBad = 0;
		for (k = 0; ro != knownBad && k < ts->orderCount; k++) {
		    switch (ts->order[k].type) {
		    case TR_REMOVED:
			if (ts->order[k].u.removed.dboffset == ro)
			    knownBad = ro;
			break;
		    case TR_ADDED:
			break;
		    }
		}

		shared->pkgFileNum = i;
		shared->otherPkg = dbiIndexRecordOffset(matches[i], j);
		shared->otherFileNum = dbiIndexRecordFileNumber(matches[i], j);
		shared->isRemoved = (knownBad == ro);
		shared++;
	    }
	    dbiFreeIndexRecord(matches[i]);
	}
	numShared = shared - sharedList;
	shared->otherPkg = -1;
	free(matches);

	/* Sort file info by other package index (otherPkg) */
	qsort(sharedList, numShared, sizeof(*shared), sharedCmp);

	/* For all files from this package that are in the database ... */
	for (i = 0; i < numShared; i = nexti) {
	    int beingRemoved;

	    shared = sharedList + i;

	    /* Find the end of the files in the other package. */
	    for (nexti = i + 1; nexti < numShared; nexti++) {
		if (sharedList[nexti].otherPkg != shared->otherPkg)
		    break;
	    }

	    /* Is this file from a package being removed? */
	    beingRemoved = 0;
	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] != shared->otherPkg)
		    continue;
		beingRemoved = 1;
		break;
	    }

	    /* Determine the fate of each file. */
	    switch (fi->type) {
	    case TR_ADDED:
		handleInstInstalledFiles(fi, ts->db, shared, nexti - i,
		!(beingRemoved || (ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)),
			 probs);
		break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    handleRmvdInstalledFiles(fi, ts->db, shared, nexti - i);
		break;
	    }
	}

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
	handleOverlappedFiles(fi, ht,
	       (ignoreSet & RPMPROB_FILTER_REPLACENEWFILES) ? NULL : probs, di);

	/* Check added package has sufficient space on each partition used. */
	switch (fi->type) {
	case TR_ADDED:
	    if (!(di && fi->fc))
		break;
	    for (i = 0; i < filesystemCount; i++) {
		if (adj_fs_blocks(di[i].needed) <= di[i].avail)
		    continue;
		psAppend(probs, RPMPROB_DISKSPACE, fi->ap->key, fi->ap->h,
			filesystems[i], NULL,
	 	    (adj_fs_blocks(di[i].needed) - di[i].avail) * di[i].block);
	    }
	    break;
	case TR_REMOVED:
	    break;
	}
    }

    NOTIFY((NULL, RPMCALLBACK_TRANS_STOP, 6, flEntries, NULL, notifyData));

    chroot(".");
    chdir(currDir);

    /* ===============================================
     * Free unused memory as soon as possible.
     */
    htFree(ht);

    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	if (fi->fc == 0)
	    continue;
	free(fi->fl); fi->fl = NULL;
	switch (fi->type) {
	case TR_ADDED:
	    free(fi->fmd5s); fi->fmd5s = NULL;
	    free(fi->flinks); fi->flinks = NULL;
	    free(fi->fps); fi->fps = NULL;
	    break;
	case TR_REMOVED:
	    break;
	}
    }

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
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

    /* ===============================================
     * Install and remove packages.
     */
    lastFailed = -2;
    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	switch (ts->order[oc].type) {
	case TR_ADDED:
	    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
	    i = ts->order[oc].u.addedIndex;

	    if ((fd = alp->fd) == 0) {
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
	    break;
	case TR_REMOVED:
	    if (ts->order[oc].u.removed.dependsOnIndex == lastFailed)
		break;
	    if (removeBinaryPackage(ts->root, ts->db, fi->record,
				    flags, fi->actions, ts->scriptFd))
		ourrc++;
	    break;
	}
    }

    freeFl(ts, flList);

    if (ourrc)
    	return -1;
    else
	return 0;
}
