/** \ingroup rpmtrans
 * \file lib/transaction.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "psm.h"
#include "fprint.h"
#include "hash.h"
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

#include "debug.h"

/*@access FD_t@*/		/* XXX compared with NULL */
/*@access Header@*/		/* XXX compared with NULL */
/*@access dbiIndexSet@*/
/*@access rpmdb@*/
/*@access rpmTransactionSet@*/
/*@access TFI_t@*/
/*@access rpmProblemSet@*/
/*@access rpmProblem@*/

struct diskspaceInfo {
    dev_t dev;			/*!< file system device number. */
    signed long bneeded;	/*!< no. of blocks needed. */
    signed long ineeded;	/*!< no. of inodes needed. */
    int bsize;			/*!< file system block size. */
    signed long bavail;		/*!< no. of blocks available. */
    signed long iavail;		/*!< no. of inodes available. */
};

/* Adjust for root only reserved space. On linux e2fs, this is 5%. */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

static void freeFl(rpmTransactionSet ts, TFI_t flList)
{
    TFI_t fi;
    int oc;

    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	freeFi(fi);
    }
}

void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
{
    ts->scriptFd = (fd ? fdLink(fd, "rpmtransSetScriptFd") : NULL);
}

int rpmtransGetKeys(const rpmTransactionSet ts, const void *** ep, int * nep)
{
    int rc = 0;

    if (nep) *nep = ts->orderCount;
    if (ep) {
	const void ** e;
	int oc;

	*ep = e = xmalloc(ts->orderCount * sizeof(*e));
	for (oc = 0; oc < ts->orderCount; oc++, e++) {
	    struct availablePackage * alp;
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
		*e = alp->key;
		break;
	    case TR_REMOVED:
		*e = NULL;
		break;
	    }
	}
    }
    return rc;
}

static rpmProblemSet psCreate(void)
{
    rpmProblemSet probs;

    probs = xmalloc(sizeof(*probs));	/* XXX memory leak */
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

static void psAppend(rpmProblemSet probs, rpmProblemType type,
		const struct availablePackage * alp,
		const char * dn, const char *bn,
		Header altH, unsigned long ulong1)
{
    rpmProblem p;
    char *t;

    if (probs->numProblems == probs->numProblemsAlloced) {
	if (probs->numProblemsAlloced)
	    probs->numProblemsAlloced *= 2;
	else
	    probs->numProblemsAlloced = 2;
	probs->probs = xrealloc(probs->probs,
			probs->numProblemsAlloced * sizeof(*probs->probs));
    }

    p = probs->probs + probs->numProblems++;
    p->type = type;
    p->key = alp->key;
    p->ulong1 = ulong1;
    p->ignoreProblem = 0;

    if (dn || bn) {
	p->str1 =
	    t = xmalloc((dn ? strlen(dn) : 0) + (bn ? strlen(bn) : 0) + 1);
	if (dn) t = stpcpy(t, dn);
	if (bn) t = stpcpy(t, bn);
    } else
	p->str1 = NULL;

    if (alp) {
	p->h = headerLink(alp->h);
	p->pkgNEVR =
	    t = xmalloc(strlen(alp->name) +
			strlen(alp->version) +
			strlen(alp->release) + sizeof("--"));
	t = stpcpy(t, alp->name);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->version);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->release);
    } else {
	p->h = NULL;
	p->pkgNEVR = NULL;
    }

    if (altH) {
	const char *n, *v, *r;
	headerNVR(altH, &n, &v, &r);
	p->altNEVR =
	    t = xmalloc(strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
	t = stpcpy(t, n);
	t = stpcpy(t, "-");
	t = stpcpy(t, v);
	t = stpcpy(t, "-");
	t = stpcpy(t, r);
    } else
	p->altNEVR = NULL;
}

static int archOkay(Header h)
{
    void * pkgArch;
    int type, count;

    /* make sure we're trying to install this on the proper architecture */
    headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);
#ifndef	DYING
    if (type == RPM_INT8_TYPE) {
	int_8 * pkgArchNum;
	int archNum;

	/* old arch handling */
	rpmGetArchInfo(NULL, &archNum);
	pkgArchNum = pkgArch;
	if (archNum != *pkgArchNum) {
	    return 0;
	}
    } else
#endif
    {
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
#ifndef	DYING
    if (type == RPM_INT8_TYPE) {
	/* v1 packages and v2 packages both used improper OS numbers, so just
	   deal with it hope things work */
	return 1;
    } else
#endif
    {
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
	rpmProblem p = probs->probs + i;
	if (p->h)	headerFree(p->h);
	if (p->pkgNEVR)	free((void *)p->pkgNEVR);
	if (p->altNEVR)	free((void *)p->altNEVR);
	if (p->str1) free((void *)p->str1);
    }
    free(probs);
}

static /*@observer@*/ const char *const ftstring (fileTypes ft)
{
    switch (ft) {
    case XDIR:	return "directory";
    case CDEV:	return "char dev";
    case BDEV:	return "block dev";
    case LINK:	return "link";
    case SOCK:	return "sock";
    case PIPE:	return "fifo/pipe";
    case REG:	return "file";
    default:	return "unknown file type";
    }
    /*@notreached@*/
}

static fileTypes whatis(uint_16 mode)
{
    if (S_ISDIR(mode))	return XDIR;
    if (S_ISCHR(mode))	return CDEV;
    if (S_ISBLK(mode))	return BDEV;
    if (S_ISLNK(mode))	return LINK;
    if (S_ISSOCK(mode))	return SOCK;
    if (S_ISFIFO(mode))	return PIPE;
    return REG;
}

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Relocate files in header.
 * @todo multilib file dispositions need to be checked.
 * @param ts		transaction set
 * @param alp		available package
 * @param origH		package header
 * @param actions	file dispositions
 * @return		header with relocated files
 */
static Header relocateFileList(const rpmTransactionSet ts,
		struct availablePackage * alp,
		Header origH, fileAction * actions)
{
    static int _printed = 0;
    rpmProblemSet probs = ts->probs;
    int allowBadRelocate = (ts->ignoreSet & RPMPROB_FILTER_FORCERELOCATE);
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation * relocations = NULL;
    int numRelocations;
    const char ** validRelocations;
    int_32 validType;
    int numValid;
    const char ** baseNames;
    const char ** dirNames;
    int_32 * dirIndexes;
    int_32 * newDirIndexes;
    int_32 fileCount;
    int_32 dirCount;
    uint_32 * fFlags = NULL;
    uint_16 * fModes = NULL;
    char * skipDirList;
    Header h;
    int nrelocated = 0;
    int fileAlloced = 0;
    char * fn = NULL;
    int haveRelocatedFile = 0;
    int len;
    int i, j;

    if (!headerGetEntry(origH, RPMTAG_PREFIXES, &validType,
			(void **) &validRelocations, &numValid))
	numValid = 0;

    numRelocations = 0;
    if (rawRelocations)
	while (rawRelocations[numRelocations].newPath ||
	       rawRelocations[numRelocations].oldPath)
	    numRelocations++;

    /*
     * If no relocations are specified (usually the case), then return the
     * original header. If there are prefixes, however, then INSTPREFIXES
     * should be added, but, since relocateFileList() can be called more
     * than once for the same header, don't bother if already present.
     */
    if (numRelocations == 0) {
	if (numValid) {
	    if (!headerIsEntry(origH, RPMTAG_INSTPREFIXES))
		headerAddEntry(origH, RPMTAG_INSTPREFIXES,
			validType, validRelocations, numValid);
	    validRelocations = headerFreeData(validRelocations, validType);
	}
	/* XXX FIXME multilib file actions need to be checked. */
	return headerLink(origH);
    }

#ifdef DYING
    h = headerCopy(origH);
#else
    h = headerLink(origH);
#endif

    relocations = alloca(sizeof(*relocations) * numRelocations);

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < numRelocations; i++) {
	/* FIXME: default relocations (oldPath == NULL) need to be handled
	   in the UI, not rpmlib */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	relocations[i].oldPath =
	    stripTrailingChar(alloca_strdup(rawRelocations[i].oldPath), '/');

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (rawRelocations[i].newPath) {
	    relocations[i].newPath =
	    	stripTrailingChar(alloca_strdup(rawRelocations[i].newPath), '/');

	    /* Verify that the relocation's old path is in the header. */
	    for (j = 0; j < numValid; j++)
		if (!strcmp(validRelocations[j], relocations[i].oldPath)) break;
	    /* XXX actions check prevents problem from being appended twice. */
	    if (j == numValid && !allowBadRelocate && actions)
		psAppend(probs, RPMPROB_BADRELOCATE, alp,
			 relocations[i].oldPath, NULL, NULL, 0);
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

    if (!_printed) {
	_printed = 1;
	rpmMessage(RPMMESS_DEBUG, _("========== relocations\n"));
	for (i = 0; i < numRelocations; i++) {
	    if (relocations[i].newPath == NULL)
		rpmMessage(RPMMESS_DEBUG, _("%5d exclude  %s\n"),
			i, relocations[i].oldPath);
	    else
		rpmMessage(RPMMESS_DEBUG, _("%5d relocate %s -> %s\n"),
			i, relocations[i].oldPath, relocations[i].newPath);
	}
    }

    /* Add relocation values to the header */
    if (numValid) {
	const char ** actualRelocations;
	int numActual;

	actualRelocations = xmalloc(sizeof(*actualRelocations) * numValid);
	numActual = 0;
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (strcmp(validRelocations[i], relocations[j].oldPath))
		    continue;
		/* On install, a relocate to NULL means skip the path. */
		if (relocations[j].newPath) {
		    actualRelocations[numActual] = relocations[j].newPath;
		    numActual++;
		}
		break;
	    }
	    if (j == numRelocations) {
		actualRelocations[numActual] = validRelocations[i];
		numActual++;
	    }
	}

	if (numActual)
	    headerAddEntry(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numActual);

	free((void *)actualRelocations);
	validRelocations = headerFreeData(validRelocations, validType);
    }

    headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL,
				(const void **) &baseNames, &fileCount);
    headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL,
				(const void **) &dirIndexes, NULL);
    headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL,
				(const void **) &dirNames, &dirCount);
    headerGetEntryMinMemory(h, RPMTAG_FILEFLAGS, NULL,
				(const void **) &fFlags, NULL);
    headerGetEntryMinMemory(h, RPMTAG_FILEMODES, NULL,
				(const void **) &fModes, NULL);

    skipDirList = alloca(dirCount * sizeof(*skipDirList));
    memset(skipDirList, 0, dirCount * sizeof(*skipDirList));

    newDirIndexes = alloca(sizeof(*newDirIndexes) * fileCount);
    memcpy(newDirIndexes, dirIndexes, sizeof(*newDirIndexes) * fileCount);
    dirIndexes = newDirIndexes;

    /*
     * For all relocations, we go through sorted file/relocation lists 
     * backwards so that /usr/local relocations take precedence over /usr 
     * ones.
     */

    /* Relocate individual paths. */

    for (i = fileCount - 1; i >= 0; i--) {
	char * te;
	int fslen;

	/*
	 * If only adding libraries of different arch into an already
	 * installed package, skip all other files.
	 */
	if (alp->multiLib && !isFileMULTILIB((fFlags[i]))) {
	    if (actions) {
		actions[i] = FA_SKIPMULTILIB;
		rpmMessage(RPMMESS_DEBUG, _("excluding multilib path %s%s\n"), 
			dirNames[dirIndexes[i]], baseNames[i]);
	    }
	    continue;
	}

	len = strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}
	te = stpcpy( stpcpy(fn, dirNames[dirIndexes[i]]), baseNames[i]);
	fslen = (te - fn);

	/*
	 * See if this file needs relocating.
	 */
	/*
	 * XXX FIXME: Would a bsearch of the (already sorted) 
	 * relocation list be a good idea?
	 */
	for (j = numRelocations - 1; j >= 0; j--) {
	    len = strlen(relocations[j].oldPath);
	    if (fslen < len)
		continue;
	    if (strncmp(relocations[j].oldPath, fn, len))
		continue;
	    break;
	}
	if (j < 0) continue;

	/* On install, a relocate to NULL means skip the path. */
	if (relocations[j].newPath == NULL) {
	    fileTypes ft = whatis(fModes[i]);
	    int k;
	    if (ft == XDIR) {
		/* Start with the parent, looking for directory to exclude. */
		for (k = dirIndexes[i]; k < dirCount; k++) {
		    len = strlen(dirNames[k]) - 1;
		    while (len > 0 && dirNames[k][len-1] == '/') len--;
		    if (len == fslen && !strncmp(dirNames[k], fn, len))
			break;
		}
		if (k >= dirCount)
		    continue;
		skipDirList[k] = 1;
	    }
	    if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s %s\n"),
			ftstring(ft), fn);
	    }
	    continue;
	}

	if (actions)
	    rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
		    fn, relocations[j].newPath);
	nrelocated++;

	len = strlen(relocations[j].newPath);
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}
	strcpy(fn, relocations[j].newPath);

	{   char * s = strrchr(fn, '/');
	    *s++ = '\0';

	    /* fn is the new dirName, and s is the new baseName */
	    if (strcmp(baseNames[i], s))
		baseNames[i] = alloca_strdup(s);
	}

	/* Does this directory already exist in the directory list? */
	for (j = 0; j < dirCount; j++)
	    if (!strcmp(fn, dirNames[j])) break;
	
	if (j < dirCount) {
	    dirIndexes[i] = j;
	    continue;
	}

	/* Creating new paths is a pita */
	if (!haveRelocatedFile) {
	    const char ** newDirList;
	    int k;

	    haveRelocatedFile = 1;
	    newDirList = xmalloc(sizeof(*newDirList) * (dirCount + 1));
	    for (k = 0; k < dirCount; k++)
		newDirList[k] = alloca_strdup(dirNames[k]);
	    dirNames = headerFreeData(dirNames, RPM_STRING_ARRAY_TYPE);
	    dirNames = newDirList;
	} else {
	    dirNames = xrealloc(dirNames, 
			       sizeof(*dirNames) * (dirCount + 1));
	}

	dirNames[dirCount] = alloca_strdup(fn);
	dirIndexes[i] = dirCount;
	dirCount++;
    }

    /* Finish off by relocating directories. */
    for (i = dirCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {
	    int oplen;

	    oplen = strlen(relocations[j].oldPath);
	    if (strncmp(relocations[j].oldPath, dirNames[i], oplen))
		continue;

	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (!(dirNames[i][oplen] == '/'))
		continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		const char *s = relocations[j].newPath;
		char *t = alloca(strlen(s) + strlen(dirNames[i]) - oplen + 1);

		(void) stpcpy( stpcpy(t, s) , dirNames[i] + oplen);
		if (actions)
		    rpmMessage(RPMMESS_DEBUG,
			_("relocating directory %s to %s\n"), dirNames[i], t);
		dirNames[i] = t;
		nrelocated++;
	    } else {
		if (actions && !skipDirList[i]) {
		    rpmMessage(RPMMESS_DEBUG, _("excluding directory %s\n"), 
			dirNames[dirIndexes[i]]);
		    actions[i] = FA_SKIPNSTATE;
		}
	    }
	    break;
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (nrelocated) {
	int c;
	void * p;
	int t;

	p = NULL;
	headerGetEntry(h, RPMTAG_BASENAMES, &t, &p, &c);
	headerAddEntry(h, RPMTAG_ORIGBASENAMES, t, p, c);
	p = headerFreeData(p, t);

	p = NULL;
	headerGetEntry(h, RPMTAG_DIRNAMES, &t, &p, &c);
	headerAddEntry(h, RPMTAG_ORIGDIRNAMES, t, p, c);
	p = headerFreeData(p, t);

	p = NULL;
	headerGetEntry(h, RPMTAG_DIRINDEXES, &t, &p, &c);
	headerAddEntry(h, RPMTAG_ORIGDIRINDEXES, t, p, c);
	p = headerFreeData(p, t);

	headerModifyEntry(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			  baseNames, fileCount);
	headerModifyEntry(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			  dirNames, dirCount);
	headerModifyEntry(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			  dirIndexes, fileCount);
    }

    baseNames = headerFreeData(baseNames, RPM_STRING_ARRAY_TYPE);
    dirNames = headerFreeData(dirNames, RPM_STRING_ARRAY_TYPE);
    if (fn) free(fn);

    return h;
}

/*
 * As the problem sets are generated in an order solely dependent
 * on the ordering of the packages in the transaction, and that
 * ordering can't be changed, the problem sets must be parallel to
 * one another. Additionally, the filter set must be a subset of the
 * target set, given the operations available on transaction set.
 * This is good, as it lets us perform this trim in linear time, rather
 * then logarithmic or quadratic.
 */
static int psTrim(rpmProblemSet filter, rpmProblemSet target)
{
    rpmProblem f = filter->probs;
    rpmProblem t = target->probs;
    int gotProblems = 0;

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

static fileAction decideFileFate(const char * dirName,
			const char * baseName, short dbMode,
			const char * dbMd5, const char * dbLink, short newMode,
			const char * newMd5, const char * newLink, int newFlags,
			int brokenMd5, rpmtransFlags transFlags)
{
    char buffer[1024];
    const char * dbAttr, * newAttr;
    fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int i, rc;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;
    char * filespec = alloca(strlen(dirName) + strlen(baseName) + 1);

    (void) stpcpy( stpcpy(filespec, dirName), baseName);

    if (lstat(filespec, &sb)) {
	/*
	 * The file doesn't exist on the disk. Create it unless the new
	 * package has marked it as missingok, or allfiles is requested.
	 */
	if (!(transFlags & RPMTRANS_FLAG_ALLFILES) &&
	   (newFlags & RPMFILE_MISSINGOK)) {
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
	/* this config file has never been modified, so just replace it */
	return FA_CREATE;
    }

    if (!strcmp(dbAttr, newAttr)) {
	/* this file is the same in all versions of this package */
	return FA_SKIP;
    }

    /*
     * The config file on the disk has been modified, but
     * the ones in the two packages are different. It would
     * be nice if RPM was smart enough to at least try and
     * merge the difference ala CVS, but...
     */
    return save;
}

static int filecmp(short mode1, const char * md51, const char * link1,
	           short mode2, const char * md52, const char * link2)
{
    fileTypes what1 = whatis(mode1);
    fileTypes what2 = whatis(mode2);

    if (what1 != what2) return 1;

    if (what1 == LINK)
	return strcmp(link1, link2);
    else if (what1 == REG)
	return strcmp(md51, md52);

    return 0;
}

static int handleInstInstalledFiles(TFI_t fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount, int reportConflicts,
				    rpmProblemSet probs,
				    rpmtransFlags transFlags)
{
    HGE_t hge = fi->hge;
    HFD_t hfd = fi->hfd;
    int oltype, omtype;
    Header h;
    int i;
    const char ** otherMd5s;
    const char ** otherLinks;
    const char * otherStates;
    uint_32 * otherFlags;
    uint_32 * otherSizes;
    uint_16 * otherModes;
    int numReplaced = 0;

    rpmdbMatchIterator mi;

    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, &shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	rpmdbFreeIterator(mi);
	return 1;
    }

    hge(h, RPMTAG_FILEMD5S, &omtype, (void **) &otherMd5s, NULL);
    hge(h, RPMTAG_FILELINKTOS, &oltype, (void **) &otherLinks, NULL);
    hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);
    hge(h, RPMTAG_FILEMODES, NULL, (void **) &otherModes, NULL);
    hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &otherFlags, NULL);
    hge(h, RPMTAG_FILESIZES, NULL, (void **) &otherSizes, NULL);

    fi->replaced = xmalloc(sizeof(*fi->replaced) * sharedCount);

    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	/* XXX another tedious segfault, assume file state normal. */
	if (otherStates && otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	if (XFA_SKIPPING(fi->actions[fileNum]))
	    continue;

	if (filecmp(otherModes[otherFileNum],
			otherMd5s[otherFileNum],
			otherLinks[otherFileNum],
			fi->fmodes[fileNum],
			fi->fmd5s[fileNum],
			fi->flinks[fileNum])) {
	    if (reportConflicts)
		psAppend(probs, RPMPROB_FILE_CONFLICT, fi->ap,
			fi->dnl[fi->dil[fileNum]], fi->bnl[fileNum], h, 0);
	    if (!(otherFlags[otherFileNum] | fi->fflags[fileNum])
			& RPMFILE_CONFIG) {
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
	    }
	}

	if ((otherFlags[otherFileNum] | fi->fflags[fileNum]) & RPMFILE_CONFIG) {
	    fi->actions[fileNum] = decideFileFate(
			fi->dnl[fi->dil[fileNum]],
			fi->bnl[fileNum],
			otherModes[otherFileNum],
			otherMd5s[otherFileNum],
			otherLinks[otherFileNum],
			fi->fmodes[fileNum],
			fi->fmd5s[fileNum],
			fi->flinks[fileNum],
			fi->fflags[fileNum],
			!headerIsEntry(h, RPMTAG_RPMVERSION),
			transFlags);
	}

	fi->replacedSizes[fileNum] = otherSizes[otherFileNum];
    }

    otherMd5s = hfd(otherMd5s, omtype);
    otherLinks = hfd(otherLinks, oltype);
    rpmdbFreeIterator(mi);

    fi->replaced = xrealloc(fi->replaced,	/* XXX memory leak */
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}

static int handleRmvdInstalledFiles(TFI_t fi, rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount)
{
    HGE_t hge = fi->hge;
    Header h;
    const char * otherStates;
    int i;
   
    rpmdbMatchIterator mi;

    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	rpmdbFreeIterator(mi);
	return 1;
    }

    hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);

    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	fi->actions[fileNum] = FA_SKIP;
    }

    rpmdbFreeIterator(mi);

    return 0;
}

/**
 * Update disk space needs on each partition for this package.
 */
static void handleOverlappedFiles(TFI_t fi, hashTable ht,
			   rpmProblemSet probs, struct diskspaceInfo * dsl)
{
    int i, j;
    struct diskspaceInfo * ds = NULL;
    uint_32 fixupSize = 0;
    char * filespec = NULL;
    int fileSpecAlloced = 0;
  
    for (i = 0; i < fi->fc; i++) {
	int otherPkgNum, otherFileNum;
	const TFI_t * recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	j = strlen(fi->dnl[fi->dil[i]]) + strlen(fi->bnl[i]) + 1;
	if (j > fileSpecAlloced) {
	    fileSpecAlloced = j * 2;
	    filespec = xrealloc(filespec, fileSpecAlloced);
	}

	(void) stpcpy( stpcpy( filespec, fi->dnl[fi->dil[i]]), fi->bnl[i]);

	if (dsl) {
	    ds = dsl;
	    while (ds->bsize && ds->dev != fi->fps[i].entry->dev) ds++;
	    if (!ds->bsize) ds = NULL;
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
	for (j = 0; j < numRecs && recs[j] != fi; j++)
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

		/* If the addresses are the same, so are the values. */
		if ((fi->fps + i) == (recs[otherPkgNum]->fps + otherFileNum))
		    break;

		/* Otherwise, compare fingerprints by value. */
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
		if ((fi->fflags[i] & RPMFILE_CONFIG) && 
			!lstat(filespec, &sb)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (fi->fflags[i] & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
		break;
	    }

	    /* Mark added overlapped non-identical files as a conflict. */
	    if (probs && filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			recs[otherPkgNum]->fmd5s[otherFileNum],
			recs[otherPkgNum]->flinks[otherFileNum],
			fi->fmodes[i],
			fi->fmd5s[i],
			fi->flinks[i])) {
		psAppend(probs, RPMPROB_NEW_FILE_CONFLICT, fi->ap,
				filespec, NULL, recs[otherPkgNum]->ap->h, 0);
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = recs[otherPkgNum]->fsizes[otherFileNum];

	    if ((fi->fflags[i] & RPMFILE_CONFIG) && !lstat(filespec, &sb)) {
		/* Here is an overlapped  pre-existing config file. */
		fi->actions[i] = (fi->fflags[i] & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_SKIP;
	    } else {
		fi->actions[i] = FA_CREATE;
	    }
	    break;
	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
		/* Here is an overlapped added file we don't want to nuke. */
		if (recs[otherPkgNum]->actions[otherFileNum] != FA_ERASE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		recs[otherPkgNum]->actions[otherFileNum] = FA_SKIP;
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		break;
	    if (fi->fstates[i] != RPMFILE_STATE_NORMAL)
		break;
	    if (!(S_ISREG(fi->fmodes[i]) && (fi->fflags[i] & RPMFILE_CONFIG))) {
		fi->actions[i] = FA_ERASE;
		break;
	    }
		
	    /* Here is a pre-existing modified config file that needs saving. */
	    {	char mdsum[50];
		if (!mdfile(filespec, mdsum) && strcmp(fi->fmd5s[i], mdsum)) {
		    fi->actions[i] = FA_BACKUP;
		    break;
		}
	    }
	    fi->actions[i] = FA_ERASE;
	    break;
	}

	if (ds) {
	    uint_32 s = BLOCK_ROUND(fi->fsizes[i], ds->bsize);

	    switch (fi->actions[i]) {
	      case FA_BACKUP:
	      case FA_SAVE:
	      case FA_ALTNAME:
		ds->ineeded++;
		ds->bneeded += s;
		break;

	    /*
	     * FIXME: If a two packages share a file (same md5sum), and
	     * that file is being replaced on disk, will ds->bneeded get
	     * decremented twice? Quite probably!
	     */
	      case FA_CREATE:
		ds->ineeded++;
		ds->bneeded += s;
		ds->bneeded -= BLOCK_ROUND(fi->replacedSizes[i], ds->bsize);
		break;

	      case FA_ERASE:
		ds->ineeded--;
		ds->bneeded -= s;
		break;

	      default:
		break;
	    }

	    ds->bneeded -= BLOCK_ROUND(fixupSize, ds->bsize);
	}
    }
    if (filespec) free(filespec);
}

static int ensureOlder(struct availablePackage * alp, Header old,
		rpmProblemSet probs)
{
    int result, rc = 0;

    if (old == NULL) return 1;

    result = rpmVersionCompare(old, alp->h);
    if (result <= 0)
	rc = 0;
    else if (result > 0) {
	rc = 1;
	psAppend(probs, RPMPROB_OLDPACKAGE, alp, NULL, NULL, old, 0);
    }

    return rc;
}

static void skipFiles(const rpmTransactionSet ts, TFI_t fi)
{
    int noDocs = (ts->transFlags & RPMTRANS_FLAG_NODOCS);
    char ** netsharedPaths = NULL;
    const char ** languages;
    const char * s;
    int i;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	const char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	if (tmpPath && *tmpPath != '%')
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	free((void *)tmpPath);
    }


    s = rpmExpand("%{_install_langs}", NULL);
    if (!(s && *s != '%')) {
	if (s) free((void *)s);
	s = NULL;
    }
    if (s) {
	languages = (const char **) splitString(s, strlen(s), ':');
	free((void *)s);
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
	    const char * dir = fi->dnl[fi->dil[i]];

	    len = strlen(*nsp);
	    if (strncmp(dir, *nsp, len))
		continue;

	    /* Only directories or complete file paths can be net shared */
	    if (!(dir[len] == '/' || dir[len] == '\0'))
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
	if (fi->flangs && languages && *fi->flangs[i]) {
	    const char **lang, *l, *le;
	    for (lang = languages; *lang; lang++) {
		if (!strcmp(*lang, "all"))
		    break;
		for (l = fi->flangs[i]; *l; l = le) {
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
    if (fi->flangs) {
	free(fi->flangs); fi->flangs = NULL;
    }
    if (languages) freeSplitString((char **)languages);
}

#define	NOTIFY(_ts, _al)	if ((_ts)->notify) (void) (_ts)->notify _al

int rpmRunTransactions(	rpmTransactionSet ts,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			rpmProblemSet okProbs, rpmProblemSet * newProbs,
			rpmtransFlags transFlags, rpmprobFilterFlags ignoreSet)
{
    int i, j;
    int rc, ourrc = 0;
    struct availablePackage * alp;
    Header * hdrs;
    int totalFileCount = 0;
    hashTable ht;
    TFI_t flList, fi;
    struct diskspaceInfo * dip;
    struct sharedFileInfo * shared, * sharedList;
    int numShared;
    int flEntries;
    int nexti;
    int lastFailed;
    int oc;
    fingerPrintCache fpc;

    /* FIXME: what if the same package is included in ts twice? */

    ts->transFlags = transFlags;

    /* XXX MULTILIB is broken, as packages can and do execute /sbin/ldconfig. */
    if (ts->transFlags & (RPMTRANS_FLAG_JUSTDB | RPMTRANS_FLAG_MULTILIB))
	ts->transFlags |= (RPMTRANS_FLAG_NOSCRIPTS|RPMTRANS_FLAG_NOTRIGGERS);

    ts->notify = notify;
    ts->notifyData = notifyData;
    ts->probs = *newProbs = psCreate();
    ts->ignoreSet = ignoreSet;
    if (ts->currDir)
	free((void *)ts->currDir);
    ts->currDir = currentDirectory();
    ts->chrootDone = 0;
    ts->id = time(NULL);

    /* Get available space on mounted file systems. */
    if (!(ts->ignoreSet & RPMPROB_FILTER_DISKSPACE) &&
		!rpmGetFilesystemList(&ts->filesystems, &ts->filesystemCount)) {
	struct stat sb;

	if (ts->di)
	    free((void *)ts->di);
	dip = ts->di = xcalloc(sizeof(*ts->di), ts->filesystemCount + 1);

	for (i = 0; (i < ts->filesystemCount) && dip; i++) {
#if STATFS_IN_SYS_STATVFS
	    struct statvfs sfb;
	    memset(&sfb, 0, sizeof(sfb));
	    if (statvfs(ts->filesystems[i], &sfb))
#else
	    struct statfs sfb;
#  if STAT_STATFS4
/* this platform has the 4-argument version of the statfs call.  The last two
 * should be the size of struct statfs and 0, respectively.  The 0 is the
 * filesystem type, and is always 0 when statfs is called on a mounted
 * filesystem, as we're doing.
 */
	    memset(&sfb, 0, sizeof(sfb));
	    if (statfs(ts->filesystems[i], &sfb, sizeof(sfb), 0))
#  else
	    memset(&sfb, 0, sizeof(sfb));
	    if (statfs(ts->filesystems[i], &sfb))
#  endif
#endif
	    {
		dip = NULL;
	    } else {
		ts->di[i].bsize = sfb.f_bsize;
		ts->di[i].bneeded = 0;
		ts->di[i].ineeded = 0;
#ifdef STATFS_HAS_F_BAVAIL
		ts->di[i].bavail = sfb.f_bavail;
#else
/* FIXME: the statfs struct doesn't have a member to tell how many blocks are
 * available for non-superusers.  f_blocks - f_bfree is probably too big, but
 * it's about all we can do.
 */
		ts->di[i].bavail = sfb.f_blocks - sfb.f_bfree;
#endif
		/* XXX Avoid FAT and other file systems that have not inodes. */
		ts->di[i].iavail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
				? sfb.f_ffree : -1;

		stat(ts->filesystems[i], &sb);
		ts->di[i].dev = sb.st_dev;
	    }
	}

	if (dip) ts->di[i].bsize = 0;
    }

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
    for (alp = ts->addedPackages.list;
	(alp - ts->addedPackages.list) < ts->addedPackages.size;
	alp++)
    {
	if (!archOkay(alp->h) && !(ts->ignoreSet & RPMPROB_FILTER_IGNOREARCH))
	    psAppend(ts->probs, RPMPROB_BADARCH, alp, NULL, NULL, NULL, 0);

	if (!osOkay(alp->h) && !(ts->ignoreSet & RPMPROB_FILTER_IGNOREOS))
	    psAppend(ts->probs, RPMPROB_BADOS, alp, NULL, NULL, NULL, 0);

	if (!(ts->ignoreSet & RPMPROB_FILTER_OLDPACKAGE)) {
	    rpmdbMatchIterator mi;
	    Header oldH;
	    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, alp->name, 0);
	    while ((oldH = rpmdbNextIterator(mi)) != NULL)
		ensureOlder(alp, oldH, ts->probs);
	    rpmdbFreeIterator(mi);
	}

	/* XXX multilib should not display "already installed" problems */
	if (!(ts->ignoreSet & RPMPROB_FILTER_REPLACEPKG) && !alp->multiLib) {
	    rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, alp->name, 0);
	    rpmdbSetIteratorVersion(mi, alp->version);
	    rpmdbSetIteratorRelease(mi, alp->release);
	    while (rpmdbNextIterator(mi) != NULL) {
		psAppend(ts->probs, RPMPROB_PKG_INSTALLED, alp,
			NULL, NULL, NULL, 0);
		break;
	    }
	    rpmdbFreeIterator(mi);
	}

	totalFileCount += alp->filesCount;

    }

    /* FIXME: it seems a bit silly to read in all of these headers twice */
    /* The ordering doesn't matter here */
    if (ts->numRemovedPackages > 0) {
	rpmdbMatchIterator mi;
	Header h;
	int fileCount;

	mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES, NULL, 0);
	rpmdbAppendIterator(mi, ts->removedPackages, ts->numRemovedPackages);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (headerGetEntry(h, RPMTAG_BASENAMES, NULL, NULL, &fileCount))
		totalFileCount += fileCount;
	}
	rpmdbFreeIterator(mi);
    }

    /* ===============================================
     * Initialize file list:
     */
    flEntries = ts->addedPackages.size + ts->numRemovedPackages;
    flList = alloca(sizeof(*flList) * (flEntries));

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	const char **preTrans;
	int preTransCount;

	memset(fi, 0, sizeof(*fi));
	fi->magic = TFIMAGIC;
	preTrans = NULL;
	preTransCount = 0;

	fi->type = ts->order[oc].type;
	switch (ts->order[oc].type) {
	case TR_ADDED:
	    i = ts->order[oc].u.addedIndex;
	    alp = ts->addedPackages.list + i;
	    fi->ap = alp;
	    fi->record = 0;
	    loadFi(alp->h, fi);
	    if (fi->fc == 0) {
		hdrs[i] = headerLink(fi->h);
		continue;
	    }

	    /* Allocate file actions (and initialize to FA_UNKNOWN) */
	    fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));
	    hdrs[i] = relocateFileList(ts, alp, fi->h, fi->actions);

	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    skipFiles(ts, fi);
	    break;
	case TR_REMOVED:
	    fi->ap = alp = NULL;
	    fi->record = ts->order[oc].u.removed.dboffset;
	    {	rpmdbMatchIterator mi;

		mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));
		if ((fi->h = rpmdbNextIterator(mi)) != NULL)
		    fi->h = headerLink(fi->h);
		rpmdbFreeIterator(mi);
	    }
	    if (fi->h == NULL) {
		/* ACK! */
		continue;
	    }
	    /* XXX header arg unused. */
	    loadFi(fi->h, fi);
	    break;
	}

	if (fi->fc)
	    fi->fps = xmalloc(sizeof(*fi->fps) * fi->fc);
    }

    /* Open all database indices before installing. */
    rpmdbOpenAll(ts->rpmdb);

    if (!ts->chrootDone) {
	chdir("/");
	/*@-unrecog@*/ chroot(ts->rootDir); /*@=unrecog@*/
	ts->chrootDone = 1;
    }

    ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    for (fi = flList; (fi - flList) < flEntries; fi++) {
	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fi->fc, fi->fps);
	for (i = 0; i < fi->fc; i++) {
	    if (XFA_SKIPPING(fi->actions[i]))
		continue;
	    htAddEntry(ht, fi->fps + i, fi);
	}
    }

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, flEntries,
	NULL, ts->notifyData));

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    for (fi = flList; (fi - flList) < flEntries; fi++) {
	dbiIndexSet * matches;
	int knownBad;

	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, (fi - flList), flEntries,
	       NULL, ts->notifyData));

	if (fi->fc == 0) continue;

	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(sizeof(*matches), fi->fc);
	if (rpmdbFindFpList(ts->rpmdb, fi->fps, matches, fi->fc))
	    return 1;

	numShared = 0;
	for (i = 0; i < fi->fc; i++)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = xmalloc(sizeof(*sharedList) * (numShared + 1));
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
	    if (matches[i]) {
		dbiFreeIndexSet(matches[i]);
		matches[i] = NULL;
	    }
	}
	numShared = shared - sharedList;
	shared->otherPkg = -1;
	free((void *)matches);

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
		handleInstInstalledFiles(fi, ts->rpmdb, shared, nexti - i,
		!(beingRemoved || (ts->ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)),
			 ts->probs, ts->transFlags);
		break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    handleRmvdInstalledFiles(fi, ts->rpmdb, shared, nexti - i);
		break;
	    }
	}

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
	handleOverlappedFiles(fi, ht,
	       ((ts->ignoreSet & RPMPROB_FILTER_REPLACENEWFILES)
		    ? NULL : ts->probs), ts->di);

	/* Check added package has sufficient space on each partition used. */
	switch (fi->type) {
	case TR_ADDED:
	    if (!(ts->di && fi->fc))
		break;
	    for (i = 0; i < ts->filesystemCount; i++) {

		dip = ts->di + i;

		/* XXX Avoid FAT and other file systems that have not inodes. */
		if (dip->iavail <= 0)
		    continue;

		if (adj_fs_blocks(dip->bneeded) > dip->bavail)
		    psAppend(ts->probs, RPMPROB_DISKSPACE, fi->ap,
				ts->filesystems[i], NULL, NULL,
	 	   (adj_fs_blocks(dip->bneeded) - dip->bavail) * dip->bsize);

		if (adj_fs_blocks(dip->ineeded) > dip->iavail)
		    psAppend(ts->probs, RPMPROB_DISKNODES, fi->ap,
				ts->filesystems[i], NULL, NULL,
	 	    (adj_fs_blocks(dip->ineeded) - dip->iavail));
	    }
	    break;
	case TR_REMOVED:
	    break;
	}
    }

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, flEntries,
	NULL, ts->notifyData));

    if (ts->chrootDone) {
	/*@-unrecog@*/ chroot("."); /*@-unrecog@*/ 
	ts->chrootDone = 0;
	chdir(ts->currDir);
    }

    /* ===============================================
     * Free unused memory as soon as possible.
     */

    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	if (fi->fc == 0)
	    continue;
	if (fi->fps) {
	    free(fi->fps); fi->fps = NULL;
	}
    }

    fpCacheFree(fpc);
    htFree(ht);

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
    if ((ts->transFlags & RPMTRANS_FLAG_BUILD_PROBS) ||
           (ts->probs->numProblems && (!okProbs || psTrim(okProbs, ts->probs)))) {
	*newProbs = ts->probs;

	for (alp = ts->addedPackages.list, fi = flList;
	        (alp - ts->addedPackages.list) < ts->addedPackages.size;
		alp++, fi++) {
	    headerFree(hdrs[alp - ts->addedPackages.list]);
	}

	freeFl(ts, flList);
	return ts->orderCount;
    }

    /* ===============================================
     * Save removed files before erasing.
     */
    if (ts->transFlags & (RPMTRANS_FLAG_DIRSTASH | RPMTRANS_FLAG_REPACKAGE)) {
	for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		break;
	    case TR_REMOVED:
		if (ts->transFlags & RPMTRANS_FLAG_REPACKAGE)
		    repackage(ts, fi);
		break;
	    }
	}
    }

    /* ===============================================
     * Install and remove packages.
     */

    lastFailed = -2;	/* erased packages have -1 */
    for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++) {
	int gotfd;

	gotfd = 0;
	switch (ts->order[oc].type) {
	case TR_ADDED:
	    i = ts->order[oc].u.addedIndex;
	    alp = ts->addedPackages.list + i;

	    if (alp->fd == NULL) {
		alp->fd = ts->notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
			    alp->key, ts->notifyData);
		if (alp->fd) {
		    Header h;
		    rpmRC rpmrc;

		    headerFree(hdrs[i]);
		    hdrs[i] = NULL;
		    rpmrc = rpmReadPackageHeader(alp->fd, &h, NULL, NULL, NULL);
		    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE)) {
			(void)ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0, alp->key, ts->notifyData);
			alp->fd = NULL;
			ourrc++;
		    } else {
			hdrs[i] = relocateFileList(ts, alp, h, NULL);
			headerFree(h);
		    }
		    if (alp->fd) gotfd = 1;
		}
	    }

	    if (alp->fd) {
		Header hsave = NULL;

		if (fi->h) {
		    hsave = headerLink(fi->h);
		    headerFree(fi->h);
		}
		fi->h = headerLink(hdrs[i]);
		if (alp->multiLib)
		    ts->transFlags |= RPMTRANS_FLAG_MULTILIB;

if (fi->ap == NULL) fi->ap = alp;	/* XXX WTFO? */
		if (installBinaryPackage(ts, fi)) {
		    ourrc++;
		    lastFailed = i;
		}
		headerFree(fi->h);
		fi->h = NULL;
		if (hsave) {
		    fi->h = headerLink(hsave);
		    headerFree(hsave);
		}
	    } else {
		ourrc++;
		lastFailed = i;
	    }

	    headerFree(hdrs[i]);

	    if (gotfd) {
		(void)ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			alp->key, ts->notifyData);
		alp->fd = NULL;
	    }
	    break;
	case TR_REMOVED:
	    /* If install failed, then we shouldn't erase. */
	    if (ts->order[oc].u.removed.dependsOnIndex == lastFailed)
		break;

	    if (removeBinaryPackage(ts, fi))
		ourrc++;

	    break;
	}
	(void) rpmdbSync(ts->rpmdb);
    }

    freeFl(ts, flList);

    if (ourrc)
    	return -1;
    else
	return 0;
}
