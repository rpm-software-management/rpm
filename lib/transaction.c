/** \ingroup rpmtrans
 * \file lib/transaction.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "psm.h"
#include "fprint.h"
#include "rpmhash.h"
#include "misc.h" /* XXX stripTrailingChar, splitString, currentDirectory */
#include "rpmdb.h"

/*@-redecl -exportheadervar@*/
extern const char * chroot_prefix;
/*@=redecl =exportheadervar@*/

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
/*@access PSM_t@*/
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

static /*@null@*/ void * freeFl(rpmTransactionSet ts,
		/*@only@*/ /*@null@*/ TFI_t flList)
	/*@*/
{
    if (flList) {
	TFI_t fi;
	int oc;

	/*@-usereleased@*/
	for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++)
	    freeFi(fi);
	flList = _free(flList);
	/*@=usereleased@*/
    }
    return NULL;
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
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		if (ts->addedPackages.list) {
		    struct availablePackage * alp;
		    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
		    *e = alp->key;
		    break;
		}
		/*@fallthrough@*/
	    default:
	    case TR_REMOVED:
		/*@-mods@*/	/* FIX: double indirection. */
		*e = NULL;
		/*@=mods@*/
		break;
	    }
	}
    }
    return rc;
}

static rpmProblemSet psCreate(void)
	/*@*/
{
    rpmProblemSet probs;

    probs = xcalloc(1, sizeof(*probs));	/* XXX memory leak */
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

static void psAppend(rpmProblemSet probs, rpmProblemType type,
		const struct availablePackage * alp,
		const char * dn, const char *bn,
		Header altH, unsigned long ulong1)
	/*@modifies *probs, alp @*/
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

    p = probs->probs + probs->numProblems;
    probs->numProblems++;
    memset(p, 0, sizeof(*p));
    p->type = type;
    /*@-assignexpose@*/
    p->key = alp->key;
    /*@=assignexpose@*/
    p->ulong1 = ulong1;
    p->ignoreProblem = 0;
    p->str1 = NULL;
    p->h = NULL;
    p->pkgNEVR = NULL;
    p->altNEVR = NULL;

    if (dn || bn) {
	p->str1 =
	    t = xcalloc(1, (dn ? strlen(dn) : 0) + (bn ? strlen(bn) : 0) + 1);
	if (dn) t = stpcpy(t, dn);
	if (bn) t = stpcpy(t, bn);
    }

    if (alp) {
	p->h = headerLink(alp->h);
	p->pkgNEVR =
	    t = xcalloc(1, strlen(alp->name) +
			   strlen(alp->version) +
			   strlen(alp->release) + sizeof("--"));
	t = stpcpy(t, alp->name);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->version);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->release);
    }

    if (altH) {
	const char * n, * v, * r;
	(void) headerNVR(altH, &n, &v, &r);
	p->altNEVR =
	    t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
	t = stpcpy(t, n);
	t = stpcpy(t, "-");
	t = stpcpy(t, v);
	t = stpcpy(t, "-");
	t = stpcpy(t, r);
    }
}

static int archOkay(Header h)
	/*@*/
{
    void * pkgArch;
    int type, count;

    /* make sure we're trying to install this on the proper architecture */
    (void) headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);
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
	/*@*/
{
    void * pkgOs;
    int type, count;

    /* make sure we're trying to install this on the proper os */
    (void) headerGetEntry(h, RPMTAG_OS, &type, (void **) &pkgOs, &count);
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
	p->h = headerFree(p->h);
	p->pkgNEVR = _free(p->pkgNEVR);
	p->altNEVR = _free(p->altNEVR);
	p->str1 = _free(p->str1);
    }
    free(probs);
}

static /*@observer@*/ const char *const ftstring (fileTypes ft)
	/*@*/
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
	/*@*/
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
 * @param fi		transaction element file info
 * @param alp		available package
 * @param origH		package header
 * @param actions	file dispositions
 * @return		header with relocated files
 */
static Header relocateFileList(const rpmTransactionSet ts, TFI_t fi,
		struct availablePackage * alp,
		Header origH, fileAction * actions)
	/*@modifies ts, fi, alp, origH, actions @*/
{
    HGE_t hge = fi->hge;
    HAE_t hae = fi->hae;
    HME_t hme = fi->hme;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    static int _printed = 0;
    rpmProblemSet probs = ts->probs;
    int allowBadRelocate = (ts->ignoreSet & RPMPROB_FILTER_FORCERELOCATE);
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation * relocations = NULL;
    int numRelocations;
    const char ** validRelocations;
    rpmTagType validType;
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
    int reldel = 0;
    int len;
    int i, j;

    if (!hge(origH, RPMTAG_PREFIXES, &validType,
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
    if (rawRelocations == NULL || numRelocations == 0) {
	if (numValid) {
	    if (!headerIsEntry(origH, RPMTAG_INSTPREFIXES))
		(void) hae(origH, RPMTAG_INSTPREFIXES,
			validType, validRelocations, numValid);
	    validRelocations = hfd(validRelocations, validType);
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
	char * t;

	/*
	 * Default relocations (oldPath == NULL) are handled in the UI,
	 * not rpmlib.
	 */
	if (rawRelocations[i].oldPath == NULL) continue; /* XXX can't happen */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	t = alloca_strdup(rawRelocations[i].oldPath);
	relocations[i].oldPath = (t[0] == '/' && t[1] == '\0')
	    ? t
	    : stripTrailingChar(t, '/');

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (rawRelocations[i].newPath) {
	    int del;

	    t = alloca_strdup(rawRelocations[i].newPath);
	    relocations[i].newPath = (t[0] == '/' && t[1] == '\0')
		? t
		: stripTrailingChar(t, '/');

	    /*@-nullpass@*/	/* FIX:  relocations[i].oldPath == NULL */
	    /* Verify that the relocation's old path is in the header. */
	    for (j = 0; j < numValid; j++)
		if (!strcmp(validRelocations[j], relocations[i].oldPath))
		    /*@innerbreak@*/ break;
	    /* XXX actions check prevents problem from being appended twice. */
	    if (j == numValid && !allowBadRelocate && actions)
		psAppend(probs, RPMPROB_BADRELOCATE, alp,
			 relocations[i].oldPath, NULL, NULL, 0);
	    del =
		strlen(relocations[i].newPath) - strlen(relocations[i].oldPath);
	    /*@=nullpass@*/

	    if (del > reldel)
		reldel = del;
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
	    if (relocations[j - 1].oldPath == NULL || /* XXX can't happen */
		relocations[j    ].oldPath == NULL || /* XXX can't happen */
	strcmp(relocations[j - 1].oldPath, relocations[j].oldPath) <= 0)
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
	    if (relocations[i].oldPath == NULL) continue; /* XXX can't happen */
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

	actualRelocations = xmalloc(numValid * sizeof(*actualRelocations));
	numActual = 0;
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (relocations[j].oldPath == NULL || /* XXX can't happen */
		    strcmp(validRelocations[i], relocations[j].oldPath))
		    continue;
		/* On install, a relocate to NULL means skip the path. */
		if (relocations[j].newPath) {
		    actualRelocations[numActual] = relocations[j].newPath;
		    numActual++;
		}
		/*@innerbreak@*/ break;
	    }
	    if (j == numRelocations) {
		actualRelocations[numActual] = validRelocations[i];
		numActual++;
	    }
	}

	if (numActual)
	    (void) hae(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numActual);

	actualRelocations = _free(actualRelocations);
	validRelocations = hfd(validRelocations, validType);
    }

    (void) hge(h, RPMTAG_BASENAMES, NULL, (void **) &baseNames, &fileCount);
    (void) hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
    (void) hge(h, RPMTAG_DIRNAMES, NULL, (void **) &dirNames, &dirCount);
    (void) hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fFlags, NULL);
    (void) hge(h, RPMTAG_FILEMODES, NULL, (void **) &fModes, NULL);

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
	fileTypes ft;
	int fnlen;

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

	len = reldel +
		strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}
	*fn = '\0';
	fnlen = stpcpy( stpcpy(fn, dirNames[dirIndexes[i]]), baseNames[i]) - fn;

	/*
	 * See if this file path needs relocating.
	 */
	/*
	 * XXX FIXME: Would a bsearch of the (already sorted) 
	 * relocation list be a good idea?
	 */
	for (j = numRelocations - 1; j >= 0; j--) {
	    if (relocations[j].oldPath == NULL) continue; /* XXX can't happen */
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (fnlen < len)
		continue;
	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (!(fn[len] == '/' || fnlen == len))
		continue;

	    if (strncmp(relocations[j].oldPath, fn, len))
		continue;
	    /*@innerbreak@*/ break;
	}
	if (j < 0) continue;

	ft = whatis(fModes[i]);

	/* On install, a relocate to NULL means skip the path. */
	if (relocations[j].newPath == NULL) {
	    if (ft == XDIR) {
		/* Start with the parent, looking for directory to exclude. */
		for (j = dirIndexes[i]; j < dirCount; j++) {
		    len = strlen(dirNames[j]) - 1;
		    while (len > 0 && dirNames[j][len-1] == '/') len--;
		    if (fnlen != len)
			continue;
		    if (strncmp(fn, dirNames[j], fnlen))
			continue;
		    /*@innerbreak@*/ break;
		}
		if (j < dirCount)
		    skipDirList[j] = 1;
	    }
	    if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s %s\n"),
			ftstring(ft), fn);
	    }
	    continue;
	}

	/* Relocation on full paths only, please. */
	if (fnlen != len) continue;

	if (actions)
	    rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
		    fn, relocations[j].newPath);
	nrelocated++;

	strcpy(fn, relocations[j].newPath);
	{   char * te = strrchr(fn, '/');
	    if (te) {
		if (te > fn) te++;	/* root is special */
		fnlen = te - fn;
	    } else
		te = fn + strlen(fn);
	    /*@-nullpass -nullderef@*/	/* LCL: te != NULL here. */
	    if (strcmp(baseNames[i], te)) /* basename changed too? */
		baseNames[i] = alloca_strdup(te);
	    *te = '\0';			/* terminate new directory name */
	    /*@=nullpass =nullderef@*/
	}

	/* Does this directory already exist in the directory list? */
	for (j = 0; j < dirCount; j++) {
	    if (fnlen != strlen(dirNames[j]))
		continue;
	    if (strncmp(fn, dirNames[j], fnlen))
		continue;
	    /*@innerbreak@*/ break;
	}
	
	if (j < dirCount) {
	    dirIndexes[i] = j;
	    continue;
	}

	/* Creating new paths is a pita */
	if (!haveRelocatedFile) {
	    const char ** newDirList;

	    haveRelocatedFile = 1;
	    newDirList = xmalloc((dirCount + 1) * sizeof(*newDirList));
	    for (j = 0; j < dirCount; j++)
		newDirList[j] = alloca_strdup(dirNames[j]);
	    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
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

	    if (relocations[j].oldPath == NULL) continue; /* XXX can't happen */
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (len && strncmp(relocations[j].oldPath, dirNames[i], len))
		continue;

	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (dirNames[i][len] != '/')
		continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		const char * s = relocations[j].newPath;
		char * t = alloca(strlen(s) + strlen(dirNames[i]) - len + 1);

		(void) stpcpy( stpcpy(t, s) , dirNames[i] + len);
		if (actions)
		    rpmMessage(RPMMESS_DEBUG,
			_("relocating directory %s to %s\n"), dirNames[i], t);
		dirNames[i] = t;
		nrelocated++;
	    }
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (nrelocated) {
	int c;
	void * p;
	rpmTagType t;

	p = NULL;
	(void) hge(h, RPMTAG_BASENAMES, &t, &p, &c);
	(void) hae(h, RPMTAG_ORIGBASENAMES, t, p, c);
	p = hfd(p, t);

	p = NULL;
	(void) hge(h, RPMTAG_DIRNAMES, &t, &p, &c);
	(void) hae(h, RPMTAG_ORIGDIRNAMES, t, p, c);
	p = hfd(p, t);

	p = NULL;
	(void) hge(h, RPMTAG_DIRINDEXES, &t, &p, &c);
	(void) hae(h, RPMTAG_ORIGDIRINDEXES, t, p, c);
	p = hfd(p, t);

	(void) hme(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			  baseNames, fileCount);
	fi->bnl = hfd(fi->bnl, RPM_STRING_ARRAY_TYPE);
	(void) hge(h, RPMTAG_BASENAMES, NULL, (void **) &fi->bnl, &fi->fc);

	(void) hme(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			  dirNames, dirCount);
	fi->dnl = hfd(fi->dnl, RPM_STRING_ARRAY_TYPE);
	(void) hge(h, RPMTAG_DIRNAMES, NULL, (void **) &fi->dnl, &fi->dc);

	(void) hme(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			  dirIndexes, fileCount);
	(void) hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &fi->dil, NULL);
    }

    baseNames = hfd(baseNames, RPM_STRING_ARRAY_TYPE);
    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
    fn = _free(fn);

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
	/*@modifies target @*/
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
	    /*@-nullpass@*/	/* LCL: looks good to me */
	    if (f->h == t->h && f->type == t->type && t->key == f->key &&
		     XSTRCMP(f->str1, t->str1))
		/*@innerbreak@*/ break;
	    /*@=nullpass@*/
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
	/*@*/
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
	/*@*/
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
	/*@*/
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

static int handleInstInstalledFiles(TFI_t fi, /*@null@*/ rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount, int reportConflicts,
				    rpmProblemSet probs,
				    rpmtransFlags transFlags)
	/*@modifies fi, db, probs @*/
{
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpmTagType oltype, omtype;
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
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    (void) hge(h, RPMTAG_FILEMD5S, &omtype, (void **) &otherMd5s, NULL);
    (void) hge(h, RPMTAG_FILELINKTOS, &oltype, (void **) &otherLinks, NULL);
    (void) hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);
    (void) hge(h, RPMTAG_FILEMODES, NULL, (void **) &otherModes, NULL);
    (void) hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &otherFlags, NULL);
    (void) hge(h, RPMTAG_FILESIZES, NULL, (void **) &otherSizes, NULL);

    fi->replaced = xmalloc(sharedCount * sizeof(*fi->replaced));

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
		/*@-assignexpose@*/
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
		/*@=assignexpose@*/
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
    mi = rpmdbFreeIterator(mi);

    fi->replaced = xrealloc(fi->replaced,	/* XXX memory leak */
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}

static int handleRmvdInstalledFiles(TFI_t fi, /*@null@*/ rpmdb db,
			            struct sharedFileInfo * shared,
			            int sharedCount)
	/*@modifies fi, db @*/
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
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    (void) hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);

    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	fi->actions[fileNum] = FA_SKIP;
    }

    mi = rpmdbFreeIterator(mi);

    return 0;
}

/**
 * Update disk space needs on each partition for this package.
 */
static void handleOverlappedFiles(TFI_t fi, hashTable ht,
			   rpmProblemSet probs, struct diskspaceInfo * dsl)
	/*@modifies fi, probs, dsl @*/
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
	(void) htGetEntry(ht, &fi->fps[i], (const void ***) &recs, &numRecs, NULL);

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
	    {};

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
		    /*@innerbreak@*/ break;

		/* Otherwise, compare fingerprints by value. */
		/*@-nullpass@*/	/* LCL: looks good to me */
		if (FP_EQUAL(fi->fps[i], recs[otherPkgNum]->fps[otherFileNum]))
		    /*@innerbreak@*/ break;
		/*@=nullpass@*/

	    }
	    /* XXX is this test still necessary? */
	    if (recs[otherPkgNum]->actions[otherFileNum] != FA_UNKNOWN)
		/*@innerbreak@*/ break;
	}

	switch (fi->type) {
	case TR_ADDED:
	  { struct stat sb;
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
	  } break;
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
	    if (fi->fstates && fi->fstates[i] != RPMFILE_STATE_NORMAL)
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
	     * FIXME: If two packages share a file (same md5sum), and
	     * that file is being replaced on disk, will ds->bneeded get
	     * decremented twice? Quite probably!
	     */
	      case FA_CREATE:
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
	/*@modifies alp, probs @*/
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
	/*@modifies fi @*/
{
    int noDocs = (ts->transFlags & RPMTRANS_FLAG_NODOCS);
    char ** netsharedPaths = NULL;
    const char ** languages;
    const char * dn, * bn;
    int dnlen, bnlen, ix;
    const char * s;
    int * drc;
    char * dff;
    int i, j;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	const char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	if (tmpPath && *tmpPath != '%')
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	tmpPath = _free(tmpPath);
    }

    s = rpmExpand("%{_install_langs}", NULL);
    if (!(s && *s != '%'))
	s = _free(s);
    if (s) {
	languages = (const char **) splitString(s, strlen(s), ':');
	s = _free(s);
    } else
	languages = NULL;

    /* Compute directory refcount, skip directory if now empty. */
    drc = alloca(fi->dc * sizeof(*drc));
    memset(drc, 0, fi->dc * sizeof(*drc));
    dff = alloca(fi->dc * sizeof(*dff));
    memset(dff, 0, fi->dc * sizeof(*dff));

    for (i = 0; i < fi->fc; i++) {
	char **nsp;

	bn = fi->bnl[i];
	bnlen = strlen(bn);
	ix = fi->dil[i];
	dn = fi->dnl[ix];
	dnlen = strlen(dn);

	drc[ix]++;

	/* Don't bother with skipped files */
	if (XFA_SKIPPING(fi->actions[i])) {
	    drc[ix]--;
	    continue;
	}

	/*
	 * Skip net shared paths.
	 * Net shared paths are not relative to the current root (though
	 * they do need to take package relocations into account).
	 */
	for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
	    int len;

	    len = strlen(*nsp);
	    if (dnlen >= len) {
		if (strncmp(dn, *nsp, len)) continue;
		/* Only directories or complete file paths can be net shared */
		if (!(dn[len] == '/' || dn[len] == '\0')) continue;
	    } else {
		if (len < (dnlen + bnlen)) continue;
		if (strncmp(dn, *nsp, dnlen)) continue;
		if (strncmp(bn, (*nsp) + dnlen, bnlen)) continue;
		len = dnlen + bnlen;
		/* Only directories or complete file paths can be net shared */
		if (!((*nsp)[len] == '/' || (*nsp)[len] == '\0')) continue;
	    }

	    /*@innerbreak@*/ break;
	}

	if (nsp && *nsp) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNETSHARED;
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	if (fi->flangs && languages && *fi->flangs[i]) {
	    const char **lang, *l, *le;
	    for (lang = languages; *lang != '\0'; lang++) {
		if (!strcmp(*lang, "all"))
		    /*@innerbreak@*/ break;
		for (l = fi->flangs[i]; *l != '\0'; l = le) {
		    for (le = l; *le != '\0' && *le != '|'; le++)
			{};
		    if ((le-l) > 0 && !strncmp(*lang, l, (le-l)))
			/*@innerbreak@*/ break;
		    if (*le == '|') le++;	/* skip over | */
		}
		if (*l != '\0')
		    /*@innerbreak@*/ break;
	    }
	    if (*lang == NULL) {
		drc[ix]--;	dff[ix] = 1;
		fi->actions[i] = FA_SKIPNSTATE;
		continue;
	    }
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (fi->fflags[i] & RPMFILE_DOC)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}
    }

    /* Skip (now empty) directories that had skipped files. */
    for (j = 0; j < fi->dc; j++) {

	if (drc[j]) continue;	/* dir still has files. */
	if (!dff[j]) continue;	/* dir was not emptied here. */
	
	/* Find parent directory and basename. */
	dn = fi->dnl[j];	dnlen = strlen(dn) - 1;
	bn = dn + dnlen;	bnlen = 0;
	while (bn > dn && bn[-1] != '/') {
		bnlen++;
		dnlen--;
		bn--;
	}

	/* If explicitly included in the package, skip the directory. */
	for (i = 0; i < fi->fc; i++) {
	    const char * dir;

	    if (XFA_SKIPPING(fi->actions[i]))
		continue;
	    if (whatis(fi->fmodes[i]) != XDIR)
		continue;
	    dir = fi->dnl[fi->dil[i]];
	    if (strlen(dir) != dnlen)
		continue;
	    if (strncmp(dir, dn, dnlen))
		continue;
	    if (strlen(fi->bnl[i]) != bnlen)
		continue;
	    if (strncmp(fi->bnl[i], bn, bnlen))
		continue;
	    rpmMessage(RPMMESS_DEBUG, _("excluding directory %s\n"), dn);
	    fi->actions[i] = FA_SKIPNSTATE;
	    /*@innerbreak@*/ break;
	}
    }

    if (netsharedPaths) freeSplitString(netsharedPaths);
#ifdef	DYING	/* XXX freeFi will deal with this later. */
    fi->flangs = _free(fi->flangs);
#endif
    if (languages) freeSplitString((char **)languages);
}

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct tsIterator_s {
/*@kept@*/ rpmTransactionSet ts;	/*!< transaction set. */
    int reverse;			/*!< reversed traversal? */
    int ocsave;				/*!< last returned iterator index. */
    int oc;				/*!< iterator index. */
};

/**
 * Return transaction element order count.
 * @param a		transaction element iterator
 * @return		element order count
 */
static int tsGetOc(void * a)
	/*@*/
{
    struct tsIterator_s * iter = a;
    int oc = iter->ocsave;
    return oc;
}

/**
 * Return transaction element available package pointer.
 * @param a		transaction element iterator
 * @return		available package pointer
 */
static /*@dependent@*/ struct availablePackage * tsGetAlp(void * a)
	/*@*/
{
    struct tsIterator_s * iter = a;
    struct availablePackage * alp = NULL;
    int oc = iter->ocsave;

    if (oc != -1) {
	rpmTransactionSet ts = iter->ts;
	TFI_t fi = ts->flList + oc;
	if (ts->addedPackages.list && fi->type == TR_ADDED)
	    alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
    }
    return alp;
}

/**
 * Destroy transaction element iterator.
 * @param a		transaction element iterator
 * @return		NULL always
 */
static /*@null@*/ void * tsFreeIterator(/*@only@*//*@null@*/ const void * a)
	/*@modifies a @*/
{
    return _free(a);
}

/**
 * Create transaction element iterator.
 * @param a		transaction set
 * @return		transaction element iterator
 */
static void * tsInitIterator(/*@kept@*/ const void * a)
	/*@*/
{
    rpmTransactionSet ts = (void *)a;
    struct tsIterator_s * iter = NULL;

    iter = xcalloc(1, sizeof(*iter));
    iter->ts = ts;
    iter->reverse = ((ts->transFlags & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    iter->oc = (iter->reverse ? (ts->orderCount - 1) : 0);
    iter->ocsave = iter->oc;
    return iter;
}

/**
 * Return next transaction element's file info.
 * @param a		file info iterator
 * @return		next index, -1 on termination
 */
static /*@dependent@*/ TFI_t tsNextIterator(void * a)
	/*@*/
{
    struct tsIterator_s * iter = a;
    rpmTransactionSet ts = iter->ts;
    TFI_t fi = NULL;
    int oc = -1;

    if (iter->reverse) {
	if (iter->oc >= 0)		oc = iter->oc--;
    } else {
    	if (iter->oc < ts->orderCount)	oc = iter->oc++;
    }
    iter->ocsave = oc;
    if (oc != -1)
	fi = ts->flList + oc;
    return fi;
}

#define	NOTIFY(_ts, _al)	if ((_ts)->notify) (void) (_ts)->notify _al

int rpmRunTransactions(	rpmTransactionSet ts,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			rpmProblemSet okProbs, rpmProblemSet * newProbs,
			rpmtransFlags transFlags, rpmprobFilterFlags ignoreSet)
{
    int i, j;
    int ourrc = 0;
    struct availablePackage * alp;
#ifdef	DYING
    Header * hdrs;
#endif
    int totalFileCount = 0;
    hashTable ht;
    TFI_t fi;
    struct diskspaceInfo * dip;
    struct sharedFileInfo * shared, * sharedList;
    int numShared;
    int nexti;
    int lastFailed;
    int oc;
    fingerPrintCache fpc;
    struct psm_s psmbuf;
    PSM_t psm = &psmbuf;
    void * tsi;

    /* FIXME: what if the same package is included in ts twice? */

    ts->transFlags = transFlags;
    if (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS)
	ts->transFlags |= (_noTransScripts | _noTransTriggers);
    if (ts->transFlags & RPMTRANS_FLAG_NOTRIGGERS)
	ts->transFlags |= _noTransTriggers;

    /* XXX MULTILIB is broken, as packages can and do execute /sbin/ldconfig. */
    if (ts->transFlags & (RPMTRANS_FLAG_JUSTDB | RPMTRANS_FLAG_MULTILIB))
	ts->transFlags |= (_noTransScripts | _noTransTriggers);

    ts->notify = notify;
    ts->notifyData = notifyData;
    /*@-assignexpose@*/
    ts->probs = *newProbs = psCreate();
    /*@=assignexpose@*/
    ts->ignoreSet = ignoreSet;
    ts->currDir = _free(ts->currDir);
    ts->currDir = currentDirectory();
    ts->chrootDone = 0;
    if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
    ts->id = (int_32) time(NULL);

    memset(psm, 0, sizeof(*psm));
    /*@-assignexpose@*/
    psm->ts = ts;
    /*@=assignexpose@*/

    /* Get available space on mounted file systems. */
    if (!(ts->ignoreSet & RPMPROB_FILTER_DISKSPACE) &&
		!rpmGetFilesystemList(&ts->filesystems, &ts->filesystemCount)) {
	struct stat sb;

	ts->di = _free(ts->di);
	dip = ts->di = xcalloc((ts->filesystemCount + 1), sizeof(*ts->di));

	for (i = 0; (i < ts->filesystemCount) && dip; i++) {
#if STATFS_IN_SYS_STATVFS
	    struct statvfs sfb;
	    memset(&sfb, 0, sizeof(sfb));
	    if (statvfs(ts->filesystems[i], &sfb))
#else
	    struct statfs sfb;
#  if STAT_STATFS4
/* This platform has the 4-argument version of the statfs call.  The last two
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

		(void) stat(ts->filesystems[i], &sb);
		ts->di[i].dev = sb.st_dev;
	    }
	}

	if (dip) ts->di[i].bsize = 0;
    }

#ifdef	DYING
    hdrs = alloca(sizeof(*hdrs) * ts->addedPackages.size);
#endif

    /* ===============================================
     * For packages being installed:
     * - verify package arch/os.
     * - verify package epoch:version-release is newer.
     * - count files.
     * For packages being removed:
     * - count files.
     */
    /* The ordering doesn't matter here */
    if (ts->addedPackages.list != NULL)
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
		(void) ensureOlder(alp, oldH, ts->probs);
	    mi = rpmdbFreeIterator(mi);
	}

	/* XXX multilib should not display "already installed" problems */
	if (!(ts->ignoreSet & RPMPROB_FILTER_REPLACEPKG) && !alp->multiLib) {
	    rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, alp->name, 0);
	    (void) rpmdbSetIteratorRE(mi, RPMTAG_VERSION,
			RPMMIRE_DEFAULT, alp->version);
	    (void) rpmdbSetIteratorRE(mi, RPMTAG_RELEASE,
			RPMMIRE_DEFAULT, alp->release);

	    while (rpmdbNextIterator(mi) != NULL) {
		psAppend(ts->probs, RPMPROB_PKG_INSTALLED, alp,
			NULL, NULL, NULL, 0);
		/*@innerbreak@*/ break;
	    }
	    mi = rpmdbFreeIterator(mi);
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
	(void) rpmdbAppendIterator(mi, ts->removedPackages, ts->numRemovedPackages);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (headerGetEntry(h, RPMTAG_BASENAMES, NULL, NULL, &fileCount))
		totalFileCount += fileCount;
	}
	mi = rpmdbFreeIterator(mi);
    }

    /* ===============================================
     * Initialize file list:
     */
    ts->flEntries = ts->addedPackages.size + ts->numRemovedPackages;
    ts->flList = xcalloc(ts->flEntries, sizeof(*ts->flList));

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
    tsi = tsInitIterator(ts);
    while ((fi = tsNextIterator(tsi)) != NULL) {
	oc = tsGetOc(tsi);
	fi->magic = TFIMAGIC;

	/* XXX watchout: fi->type must be set for tsGetAlp() to "work" */
	fi->type = ts->order[oc].type;
	switch (fi->type) {
	case TR_ADDED:
	    i = ts->order[oc].u.addedIndex;
	    /* XXX watchout: fi->type must be set for tsGetAlp() to "work" */
	    fi->ap = tsGetAlp(tsi);
	    fi->record = 0;
	    loadFi(fi->ap->h, fi);
	    if (fi->fc == 0) {
#ifdef	DYING
		hdrs[i] = headerLink(fi->h);
#endif
		continue;
	    }

#ifdef	DYING
	    /* Allocate file actions (and initialize to FA_UNKNOWN) */
	    fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));
	    hdrs[i] = relocateFileList(ts, fi, fi->ap, fi->h, fi->actions);
#else
	    {   Header foo = relocateFileList(ts, fi, fi->ap, fi->h, fi->actions);
		foo = headerFree(foo);
	    }
#endif

	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    skipFiles(ts, fi);
	    break;
	case TR_REMOVED:
	    fi->ap = NULL;
	    fi->record = ts->order[oc].u.removed.dboffset;
	    {	rpmdbMatchIterator mi;

		mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));
		if ((fi->h = rpmdbNextIterator(mi)) != NULL)
		    fi->h = headerLink(fi->h);
		mi = rpmdbFreeIterator(mi);
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
	    fi->fps = xmalloc(fi->fc * sizeof(*fi->fps));
    }
    tsi = tsFreeIterator(tsi);

#ifdef	DYING
    /* Open all database indices before installing. */
    (void) rpmdbOpenAll(ts->rpmdb);
#endif

    if (!ts->chrootDone) {
	(void) chdir("/");
	/*@-unrecog -superuser @*/
	(void) chroot(ts->rootDir);
	/*@=unrecog =superuser @*/
	ts->chrootDone = 1;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 1;
	/*@-onlytrans@*/
	chroot_prefix = ts->rootDir;
	/*@=onlytrans@*/
    }

    ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    tsi = tsInitIterator(ts);
    while ((fi = tsNextIterator(tsi)) != NULL) {
	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fi->fc, fi->fps);
	for (i = 0; i < fi->fc; i++) {
	    if (XFA_SKIPPING(fi->actions[i]))
		continue;
	    /*@-dependenttrans@*/
	    htAddEntry(ht, fi->fps + i, fi);
	    /*@=dependenttrans@*/
	}
    }
    tsi = tsFreeIterator(tsi);

    /*@-moduncon@*/
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, ts->flEntries,
	NULL, ts->notifyData));
    /*@=moduncon@*/

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    tsi = tsInitIterator(ts);
    while ((fi = tsNextIterator(tsi)) != NULL) {
	dbiIndexSet * matches;
	int knownBad;

	/*@-moduncon@*/
	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, (fi - ts->flList),
			ts->flEntries, NULL, ts->notifyData));
	/*@=moduncon@*/

	if (fi->fc == 0) continue;

	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(fi->fc, sizeof(*matches));
	if (rpmdbFindFpList(ts->rpmdb, fi->fps, matches, fi->fc))
	    return 1;	/* XXX WTFO? */

	numShared = 0;
	for (i = 0; i < fi->fc; i++)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = xcalloc((numShared + 1), sizeof(*sharedList));
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
	    matches[i] = dbiFreeIndexSet(matches[i]);
	}
	numShared = shared - sharedList;
	shared->otherPkg = -1;
	matches = _free(matches);

	/* Sort file info by other package index (otherPkg) */
	qsort(sharedList, numShared, sizeof(*shared), sharedCmp);

	/* For all files from this package that are in the database ... */
	for (i = 0; i < numShared; i = nexti) {
	    int beingRemoved;

	    shared = sharedList + i;

	    /* Find the end of the files in the other package. */
	    for (nexti = i + 1; nexti < numShared; nexti++) {
		if (sharedList[nexti].otherPkg != shared->otherPkg)
		    /*@innerbreak@*/ break;
	    }

	    /* Is this file from a package being removed? */
	    beingRemoved = 0;
	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] != shared->otherPkg)
		    continue;
		beingRemoved = 1;
		/*@innerbreak@*/ break;
	    }

	    /* Determine the fate of each file. */
	    switch (fi->type) {
	    case TR_ADDED:
		(void) handleInstInstalledFiles(fi, ts->rpmdb, shared, nexti - i,
		!(beingRemoved || (ts->ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)),
			 ts->probs, ts->transFlags);
		break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    (void) handleRmvdInstalledFiles(fi, ts->rpmdb, shared, nexti - i);
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
    tsi = tsFreeIterator(tsi);

    if (ts->chrootDone) {
	/*@-unrecog -superuser @*/
	(void) chroot(".");
	/*@=unrecog =superuser @*/
	ts->chrootDone = 0;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
	chroot_prefix = NULL;
	(void) chdir(ts->currDir);
    }

    /*@-moduncon@*/
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, ts->flEntries,
	NULL, ts->notifyData));
    /*@=moduncon@*/

    /* ===============================================
     * Free unused memory as soon as possible.
     */

    tsi = tsInitIterator(ts);
    while ((fi = tsNextIterator(tsi)) != NULL) {
	psm->fi = fi;
	if (fi->fc == 0)
	    continue;
	fi->fps = _free(fi->fps);
    }
    tsi = tsFreeIterator(tsi);

    fpCacheFree(fpc);
    htFree(ht);

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
    if ((ts->transFlags & RPMTRANS_FLAG_BUILD_PROBS) ||
           (ts->probs->numProblems && (!okProbs || psTrim(okProbs, ts->probs))))
    {
	*newProbs = ts->probs;

#ifdef	DYING
	for (alp = ts->addedPackages.list, fi = ts->flList;
		(alp - ts->addedPackages.list) < ts->addedPackages.size;
		alp++, fi++)
	{
	    hdrs[alp - ts->addedPackages.list] =
		headerFree(hdrs[alp - ts->addedPackages.list]);
	}
#endif

	ts->flList = freeFl(ts, ts->flList);
	ts->flEntries = 0;
	/*@-nullstate@*/
	return ts->orderCount;
	/*@=nullstate@*/
    }

    /* ===============================================
     * Save removed files before erasing.
     */
    if (ts->transFlags & (RPMTRANS_FLAG_DIRSTASH | RPMTRANS_FLAG_REPACKAGE)) {
	tsi = tsInitIterator(ts);
	while ((fi = tsNextIterator(tsi)) != NULL) {
	    psm->fi = fi;
	    switch (fi->type) {
	    case TR_ADDED:
		break;
	    case TR_REMOVED:
		if (ts->transFlags & RPMTRANS_FLAG_REPACKAGE)
		    (void) psmStage(psm, PSM_PKGSAVE);
		break;
	    }
	}
	tsi = tsFreeIterator(tsi);
    }

    /* ===============================================
     * Install and remove packages.
     */

    lastFailed = -2;	/* erased packages have -1 */
    tsi = tsInitIterator(ts);
    while ((fi = tsNextIterator(tsi)) != NULL) {
	Header h;
	int gotfd;

	gotfd = 0;
	psm->fi = fi;
	switch (fi->type)
	{
	case TR_ADDED:
	    alp = tsGetAlp(tsi);
assert(alp == fi->ap);
	    i = alp - ts->addedPackages.list;

	    h = headerLink(fi->h);
	    if (alp->fd == NULL) {
		alp->fd = ts->notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
			    alp->key, ts->notifyData);
		if (alp->fd) {
		    rpmRC rpmrc;

#ifdef	DYING
		    hdrs[i] = headerFree(hdrs[i]);
#else
		    h = headerFree(h);
#endif
		    /*@-mustmod@*/	/* LCL: segfault */
		    rpmrc = rpmReadPackageHeader(alp->fd, &h, NULL, NULL, NULL);
		    /*@=mustmod@*/
		    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE)) {
			(void)ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0, alp->key, ts->notifyData);
			alp->fd = NULL;
			ourrc++;
		    } else {
#ifdef	DYING
			hdrs[i] = relocateFileList(ts, fi, alp, h, NULL);
			h = headerFree(h);
#else
			Header foo = relocateFileList(ts, fi, alp, h, NULL);
			h = headerFree(h);
			h = headerLink(foo);
			foo = headerFree(foo);
#endif
		    }
		    if (alp->fd) gotfd = 1;
		}
	    }

	    if (alp->fd) {
		Header hsave = NULL;

		if (fi->h) {
		    hsave = headerLink(fi->h);
		    fi->h = headerFree(fi->h);
		}
#ifdef	DYING
		fi->h = headerLink(hdrs[i]);
#else
		fi->h = headerLink(h);
#endif
		if (alp->multiLib)
		    ts->transFlags |= RPMTRANS_FLAG_MULTILIB;

assert(alp == fi->ap);
		if (psmStage(psm, PSM_PKGINSTALL)) {
		    ourrc++;
		    lastFailed = i;
		}
		fi->h = headerFree(fi->h);
		if (hsave) {
		    fi->h = headerLink(hsave);
		    hsave = headerFree(hsave);
		}
	    } else {
		ourrc++;
		lastFailed = i;
	    }

#ifdef	DYING
	    hdrs[i] = headerFree(hdrs[i]);
#else
	    h = headerFree(h);
#endif

	    if (gotfd) {
		(void)ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			alp->key, ts->notifyData);
		alp->fd = NULL;
	    }
	    break;
	case TR_REMOVED:
	    oc = tsGetOc(tsi);
	    /* If install failed, then we shouldn't erase. */
	    if (ts->order[oc].u.removed.dependsOnIndex == lastFailed)
		break;

	    if (psmStage(psm, PSM_PKGERASE))
		ourrc++;

	    break;
	}
	(void) rpmdbSync(ts->rpmdb);
    }
    tsi = tsFreeIterator(tsi);

    ts->flList = freeFl(ts, ts->flList);
    ts->flEntries = 0;

    /*@-nullstate@*/
    if (ourrc)
    	return -1;
    else
	return 0;
    /*@=nullstate@*/
}
