/** \ingroup rpmtrans
 * \file lib/transaction.c
 */

#include "system.h"

#include <rpmmacro.h>	/* XXX for rpmExpand */

#define _NEED_TEITERATOR	1
#include "psm.h"

#include "rpmdb.h"
#include "fprint.h"
#include "legacy.h"	/* XXX domd5 */
#include "misc.h" /* XXX stripTrailingChar, splitString, currentDirectory */

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_STATVFS
/*@-incondefs@*/
# include <sys/statvfs.h>
#if defined(__LCLINT__)
/*@-declundef -exportheader -protoparammatch @*/ /* LCL: missing annotation */
extern int statvfs (const char * file, /*@out@*/ struct statvfs * buf)
	/*@globals fileSystem @*/
	/*@modifies *buf, fileSystem @*/;
/*@=declundef =exportheader =protoparammatch @*/
/*@=incondefs@*/
#endif
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

/*@access FD_t @*/		/* XXX compared with NULL */
/*@access Header @*/		/* XXX compared with NULL */
/*@access rpmProblemSet @*/	/* XXX need rpmProblemSetOK() */
/*@access dbiIndexSet @*/
/*@access rpmdb @*/

/*@access PSM_t @*/

/*@access alKey @*/
/*@access fnpyKey @*/

/*@access TFI_t @*/

/*@access teIterator @*/
/*@access rpmTransactionSet @*/

/**
 */
struct diskspaceInfo {
    dev_t dev;			/*!< File system device number. */
    signed long bneeded;	/*!< No. of blocks needed. */
    signed long ineeded;	/*!< No. of inodes needed. */
    int bsize;			/*!< File system block size. */
    signed long bavail;		/*!< No. of blocks available. */
    signed long iavail;		/*!< No. of inodes available. */
};

/**
 * Adjust for root only reserved space. On linux e2fs, this is 5%.
 */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
{
    ts->scriptFd = (fd ? fdLink(fd, "rpmtransSetScriptFd") : NULL);
}

int rpmtransGetKeys(const rpmTransactionSet ts, fnpyKey ** ep, int * nep)
{
    int rc = 0;

    if (nep) *nep = ts->orderCount;
    if (ep) {
	teIterator pi;	transactionElement p;
	fnpyKey * e;

	*ep = e = xmalloc(ts->orderCount * sizeof(*e));
	pi = teInitIterator(ts);
	while ((p = teNextIterator(pi)) != NULL) {
	    switch (teGetType(p)) {
	    case TR_ADDED:
		/*@-dependenttrans@*/
		*e = teGetKey(p);
		/*@=dependenttrans@*/
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
	    default:
		*e = NULL;
		/*@switchbreak@*/ break;
	    }
	    e++;
	}
	pi = teFreeIterator(pi);
    }
    return rc;
}

/**
 */
static int archOkay(/*@null@*/ const char * pkgArch)
	/*@*/
{
    if (pkgArch == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch) ? 1 : 0);
}

/**
 */
static int osOkay(/*@null@*/ const char * pkgOs)
	/*@*/
{
    if (pkgOs == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs) ? 1 : 0);
}

/**
 */
static int sharedCmp(const void * one, const void * two)
	/*@*/
{
    sharedFileInfo a = (sharedFileInfo) one;
    sharedFileInfo b = (sharedFileInfo) two;

    if (a->otherPkg < b->otherPkg)
	return -1;
    else if (a->otherPkg > b->otherPkg)
	return 1;

    return 0;
}

/**
 */
static fileAction decideFileFate(const rpmTransactionSet ts,
		const TFI_t ofi, TFI_t nfi)
	/*@globals fileSystem @*/
	/*@modifies nfi, fileSystem @*/
{
    const char * fn = tfiGetFN(nfi);
    int newFlags = tfiGetFFlags(nfi);
    char buffer[1024];
    fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;

    if (lstat(fn, &sb)) {
	/*
	 * The file doesn't exist on the disk. Create it unless the new
	 * package has marked it as missingok, or allfiles is requested.
	 */
	if (!(ts->transFlags & RPMTRANS_FLAG_ALLFILES)
	 && (newFlags & RPMFILE_MISSINGOK))
	{
	    rpmMessage(RPMMESS_DEBUG, _("%s skipped due to missingok flag\n"),
			fn);
	    return FA_SKIP;
	} else {
	    return FA_CREATE;
	}
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(ofi->fmodes[ofi->i]);
    newWhat = whatis(nfi->fmodes[nfi->i]);

    /*
     * RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
     * them in older packages as well.
     */
    if (newWhat == XDIR)
	return FA_CREATE;

    if (diskWhat != newWhat)
	return save;
    else if (newWhat != dbWhat && diskWhat != dbWhat)
	return save;
    else if (dbWhat != newWhat)
	return FA_CREATE;
    else if (dbWhat != LINK && dbWhat != REG)
	return FA_CREATE;

    /*
     * This order matters - we'd prefer to CREATE the file if at all
     * possible in case something else (like the timestamp) has changed.
     */
    if (dbWhat == REG) {
	if (ofi->md5s != NULL && nfi->md5s != NULL) {
	    const unsigned char * omd5 = ofi->md5s + (16 * ofi->i);
	    const unsigned char * nmd5 = nfi->md5s + (16 * nfi->i);
	    if (domd5(fn, buffer, 0))
		return FA_CREATE;	/* assume file has been removed */
	    if (!memcmp(omd5, buffer, 16))
		return FA_CREATE;	/* unmodified config file, replace. */
	    if (!memcmp(omd5, nmd5, 16))
		return FA_SKIP;		/* identical file, don't bother. */
	} else {
	    const char * omd5 = ofi->fmd5s[ofi->i];
	    const char * nmd5 = nfi->fmd5s[nfi->i];
	    if (domd5(fn, buffer, 1))
		return FA_CREATE;	/* assume file has been removed */
	    if (!strcmp(omd5, buffer))
		return FA_CREATE;	/* unmodified config file, replace. */
	    if (!strcmp(omd5, nmd5))
		return FA_SKIP;		/* identical file, don't bother. */
	}
    } else /* dbWhat == LINK */ {
	memset(buffer, 0, sizeof(buffer));
	if (readlink(fn, buffer, sizeof(buffer) - 1) == -1)
	    return FA_CREATE;	/* assume file has been removed */
	if (!strcmp(ofi->flinks[ofi->i], buffer))
	    return FA_CREATE;	/* unmodified config file, replace. */
	if (!strcmp(ofi->flinks[ofi->i], nfi->flinks[nfi->i]))
	    return FA_SKIP;	/* identical file, don't bother. */
     }

    /*
     * The config file on the disk has been modified, but
     * the ones in the two packages are different. It would
     * be nice if RPM was smart enough to at least try and
     * merge the difference ala CVS, but...
     */
    return save;
}

/**
 */
static int filecmp(TFI_t afi, TFI_t bfi)
	/*@*/
{
    fileTypes awhat = whatis(afi->fmodes[afi->i]);
    fileTypes bwhat = whatis(bfi->fmodes[bfi->i]);

    if (awhat != bwhat) return 1;

    if (awhat == LINK) {
	const char * alink = afi->flinks[afi->i];
	const char * blink = bfi->flinks[bfi->i];
	return strcmp(alink, blink);
    } else if (awhat == REG) {
	if (afi->md5s != NULL && bfi->md5s != NULL) {
	    const unsigned char * amd5 = afi->md5s + (16 * afi->i);
	    const unsigned char * bmd5 = bfi->md5s + (16 * bfi->i);
	    return memcmp(amd5, bmd5, 16);
	} else {
	    const char * amd5 = afi->fmd5s[afi->i];
	    const char * bmd5 = bfi->fmd5s[bfi->i];
	    return strcmp(amd5, bmd5);
	}
    }

    return 0;
}

/**
 */
/* XXX only ts->{probs,rpmdb} modified */
static int handleInstInstalledFiles(const rpmTransactionSet ts,
		transactionElement p, TFI_t fi,
		sharedFileInfo shared,
		int sharedCount, int reportConflicts)
	/*@globals fileSystem @*/
	/*@modifies ts, fi, fileSystem @*/
{
    const char * altNEVR = NULL;
    TFI_t otherFi = NULL;
    int numReplaced = 0;
    int i;

    {	rpmdbMatchIterator mi;
	Header h;
	int scareMem = 0;

	mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    altNEVR = hGetNEVR(h, NULL);
	    otherFi = fiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);
	    break;
	}
	mi = rpmdbFreeIterator(mi);
    }

    if (otherFi == NULL)
	return 1;

    fi->replaced = xcalloc(sharedCount, sizeof(*fi->replaced));

    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;

	otherFileNum = shared->otherFileNum;
	(void) tfiSetFX(otherFi, otherFileNum);

	fileNum = shared->pkgFileNum;
	(void) tfiSetFX(fi, fileNum);

#ifdef	DYING
	/* XXX another tedious segfault, assume file state normal. */
	if (otherStates && otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;
#endif

	if (XFA_SKIPPING(fi->actions[fileNum]))
	    continue;

	if (filecmp(otherFi, fi)) {
	    if (reportConflicts) {
		rpmProblemSetAppend(ts->probs, RPMPROB_FILE_CONFLICT,
			teGetNEVR(p), teGetKey(p),
			tfiGetDN(fi), tfiGetBN(fi),
			altNEVR,
			0);
	    }
	    if (!(tfiGetFFlags(otherFi) | tfiGetFFlags(fi)) & RPMFILE_CONFIG) {
		/*@-assignexpose@*/ /* FIX: p->replaced, not fi */
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
		/*@=assignexpose@*/
	    }
	}

	if ((tfiGetFFlags(otherFi) | tfiGetFFlags(fi)) & RPMFILE_CONFIG) {
	    fileAction action;
	    action = decideFileFate(ts, otherFi, fi);
	    fi->actions[fileNum] = action;
	}
	fi->replacedSizes[fileNum] = otherFi->fsizes[otherFi->i];

    }

    altNEVR = _free(altNEVR);
    otherFi = fiFree(otherFi, 1);

    fi->replaced = xrealloc(fi->replaced,	/* XXX memory leak */
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}

/**
 */
/* XXX only ts->rpmdb modified */
static int handleRmvdInstalledFiles(const rpmTransactionSet ts, TFI_t fi,
		sharedFileInfo shared, int sharedCount)
	/*@globals fileSystem @*/
	/*@modifies ts, fi, fileSystem @*/
{
    HGE_t hge = fi->hge;
    Header h;
    const char * otherStates;
    int i, xx;
   
    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    xx = hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);

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

#define	ISROOT(_d)	(((_d)[0] == '/' && (_d)[1] == '\0') ? "" : (_d))

/*@unchecked@*/
static int _fps_debug = 0;

static int fpsCompare (const void * one, const void * two)
{
    const struct fingerPrint_s * a = (const struct fingerPrint_s *)one;
    const struct fingerPrint_s * b = (const struct fingerPrint_s *)two;
    int adnlen = strlen(a->entry->dirName);
    int asnlen = (a->subDir ? strlen(a->subDir) : 0);
    int abnlen = strlen(a->baseName);
    int bdnlen = strlen(b->entry->dirName);
    int bsnlen = (b->subDir ? strlen(b->subDir) : 0);
    int bbnlen = strlen(b->baseName);
    char * afn, * bfn, * t;
    int rc = 0;

    if (adnlen == 1 && asnlen != 0) adnlen = 0;
    if (bdnlen == 1 && bsnlen != 0) bdnlen = 0;

    afn = t = alloca(adnlen+asnlen+abnlen+2);
    if (adnlen) t = stpcpy(t, a->entry->dirName);
    *t++ = '/';
    if (a->subDir && asnlen) t = stpcpy(t, a->subDir);
    if (abnlen) t = stpcpy(t, a->baseName);
    if (afn[0] == '/' && afn[1] == '/') afn++;

    bfn = t = alloca(bdnlen+bsnlen+bbnlen+2);
    if (bdnlen) t = stpcpy(t, b->entry->dirName);
    *t++ = '/';
    if (b->subDir && bsnlen) t = stpcpy(t, b->subDir);
    if (bbnlen) t = stpcpy(t, b->baseName);
    if (bfn[0] == '/' && bfn[1] == '/') bfn++;

    rc = strcmp(afn, bfn);
/*@-modfilesys@*/
if (_fps_debug)
fprintf(stderr, "\trc(%d) = strcmp(\"%s\", \"%s\")\n", rc, afn, bfn);
/*@=modfilesys@*/

/*@-modfilesys@*/
if (_fps_debug)
fprintf(stderr, "\t%s/%s%s\trc %d\n",
ISROOT(b->entry->dirName),
(b->subDir ? b->subDir : ""),
b->baseName,
rc
);
/*@=modfilesys@*/

    return rc;
}

/*@unchecked@*/
static int _linear_fps_search = 0;

static int findFps(const struct fingerPrint_s * fiFps,
		const struct fingerPrint_s * otherFps,
		int otherFc)
	/*@*/
{
    int otherFileNum;

/*@-modfilesys@*/
if (_fps_debug)
fprintf(stderr, "==> %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);
/*@=modfilesys@*/

  if (_linear_fps_search) {

linear:
    for (otherFileNum = 0; otherFileNum < otherFc; otherFileNum++, otherFps++) {

/*@-modfilesys@*/
if (_fps_debug)
fprintf(stderr, "\t%4d %s/%s%s\n", otherFileNum,
ISROOT(otherFps->entry->dirName),
(otherFps->subDir ? otherFps->subDir : ""),
otherFps->baseName);
/*@=modfilesys@*/

	/* If the addresses are the same, so are the values. */
	if (fiFps == otherFps)
	    break;

	/* Otherwise, compare fingerprints by value. */
	/*@-nullpass@*/	/* LCL: looks good to me */
	if (FP_EQUAL((*fiFps), (*otherFps)))
	    break;
	/*@=nullpass@*/
    }

if (otherFileNum == otherFc) {
/*@-modfilesys@*/
if (_fps_debug)
fprintf(stderr, "*** NULL %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);
/*@=modfilesys@*/
}

    return otherFileNum;

  } else {

    const struct fingerPrint_s * bingoFps;

    bingoFps = bsearch(fiFps, otherFps, otherFc, sizeof(*otherFps), fpsCompare);
    if (bingoFps == NULL) {
/*@-modfilesys@*/
fprintf(stderr, "*** NULL %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);
/*@=modfilesys@*/
	goto linear;
    }

    /* If the addresses are the same, so are the values. */
    /*@-nullpass@*/	/* LCL: looks good to me */
    if (!(fiFps == bingoFps || FP_EQUAL((*fiFps), (*bingoFps)))) {
/*@-modfilesys@*/
fprintf(stderr, "***  BAD %s/%s%s\n",
ISROOT(bingoFps->entry->dirName),
(bingoFps->subDir ? bingoFps->subDir : ""),
bingoFps->baseName);
/*@=modfilesys@*/
	goto linear;
    }

    otherFileNum = (bingoFps != NULL ? (bingoFps - otherFps) : 0);

  }

    return otherFileNum;
}

/**
 * Update disk space needs on each partition for this package.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(const rpmTransactionSet ts,
		const transactionElement p, TFI_t fi)
	/*@globals fileSystem @*/
	/*@modifies ts, fi, fileSystem @*/
{
    struct diskspaceInfo * ds = NULL;
    uint_32 fixupSize = 0;
    const char * fn;
    int i, j;
  
    fi = tfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = tfiNext(fi)) >= 0) {
	struct fingerPrint_s * fiFps;
	int otherPkgNum, otherFileNum;
	TFI_t otherFi;
	const TFI_t * recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	fn = tfiGetFN(fi);
	fiFps = fi->fps + i;

	if (ts->di) {
	    ds = ts->di;
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
	(void) htGetEntry(ts->ht, fiFps,
			(const void ***) &recs, &numRecs, NULL);

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
	otherFi = NULL;
	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    struct fingerPrint_s * otherFps;
	    int otherFc;

	    otherFi = recs[otherPkgNum];

	    /* Added packages need only look at other added packages. */
	    if (teGetType(p) == TR_ADDED && teGetType(otherFi->te) != TR_ADDED)
		/*@innercontinue@*/ continue;

	    otherFps = otherFi->fps;
	    otherFc = tfiGetFC(otherFi);

	    otherFileNum = findFps(fiFps, otherFps, otherFc);
	    (void) tfiSetFX(otherFi, otherFileNum);

	    /* XXX is this test still necessary? */
	    if (otherFi->actions[otherFileNum] != FA_UNKNOWN)
		/*@innerbreak@*/ break;
	}

	switch (teGetType(p)) {
	case TR_ADDED:
	  { struct stat sb;
	    if (otherPkgNum < 0) {
		/* XXX is this test still necessary? */
		if (fi->actions[i] != FA_UNKNOWN)
		    /*@switchbreak@*/ break;
		if ((tfiGetFFlags(fi) & RPMFILE_CONFIG) && 
			!lstat(fn, &sb)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (tfiGetFFlags(fi) & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
		/*@switchbreak@*/ break;
	    }

assert(otherFi != NULL);
	    /* Mark added overlapped non-identical files as a conflict. */
	    if ((ts->ignoreSet & RPMPROB_FILTER_REPLACENEWFILES)
	     && filecmp(otherFi, fi))
	    {
		rpmProblemSetAppend(ts->probs, RPMPROB_NEW_FILE_CONFLICT,
			teGetNEVR(p), teGetKey(p),
			fn, NULL,
			teGetNEVR(otherFi->te),
			0);
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = otherFi->fsizes[otherFileNum];

	    if ((tfiGetFFlags(fi) & RPMFILE_CONFIG) && !lstat(fn, &sb)) {
		/* Here is an overlapped  pre-existing config file. */
		fi->actions[i] = (tfiGetFFlags(fi) & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_SKIP;
	    } else {
		fi->actions[i] = FA_CREATE;
	    }
	  } /*@switchbreak@*/ break;

	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
assert(otherFi != NULL);
		/* Here is an overlapped added file we don't want to nuke. */
		if (otherFi->actions[otherFileNum] != FA_ERASE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    /*@switchbreak@*/ break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		otherFi->actions[otherFileNum] = FA_SKIP;
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@switchbreak@*/ break;
	    if (fi->fstates && fi->fstates[i] != RPMFILE_STATE_NORMAL)
		/*@switchbreak@*/ break;
	    if (!(S_ISREG(fi->fmodes[i]) && (tfiGetFFlags(fi) & RPMFILE_CONFIG))) {
		fi->actions[i] = FA_ERASE;
		/*@switchbreak@*/ break;
	    }
		
	    /* Here is a pre-existing modified config file that needs saving. */
	    {	char md5sum[50];
		if (fi->md5s != NULL) {
		    const unsigned char * fmd5 = fi->md5s + (16 * i);
		    if (!domd5(fn, md5sum, 0) && memcmp(fmd5, md5sum, 16)) {
			fi->actions[i] = FA_BACKUP;
			/*@switchbreak@*/ break;
		    }
		} else {
		    const char * fmd5 = fi->fmd5s[i];
		    if (!domd5(fn, md5sum, 1) && strcmp(fmd5, md5sum)) {
			fi->actions[i] = FA_BACKUP;
			/*@switchbreak@*/ break;
		    }
		}
	    }
	    fi->actions[i] = FA_ERASE;
	    /*@switchbreak@*/ break;
	}

	if (ds) {
	    uint_32 s = BLOCK_ROUND(fi->fsizes[i], ds->bsize);

	    switch (fi->actions[i]) {
	      case FA_BACKUP:
	      case FA_SAVE:
	      case FA_ALTNAME:
		ds->ineeded++;
		ds->bneeded += s;
		/*@switchbreak@*/ break;

	    /*
	     * FIXME: If two packages share a file (same md5sum), and
	     * that file is being replaced on disk, will ds->bneeded get
	     * decremented twice? Quite probably!
	     */
	      case FA_CREATE:
		ds->bneeded += s;
		ds->bneeded -= BLOCK_ROUND(fi->replacedSizes[i], ds->bsize);
		/*@switchbreak@*/ break;

	      case FA_ERASE:
		ds->ineeded--;
		ds->bneeded -= s;
		/*@switchbreak@*/ break;

	      default:
		/*@switchbreak@*/ break;
	    }

	    ds->bneeded -= BLOCK_ROUND(fixupSize, ds->bsize);
	}
    }
}

/**
 * Ensure that current package is newer than installed package.
 * @param ts		transaction set
 * @param p		current transaction element
 * @param h		installed header
 * @return		0 if not newer, 1 if okay
 */
static int ensureOlder(rpmTransactionSet ts,
		const transactionElement p, const Header h)
	/*@modifies ts @*/
{
    int_32 reqFlags = (RPMSENSE_LESS | RPMSENSE_EQUAL);
    const char * reqEVR;
    rpmDepSet req;
    char * t;
    int nb;
    int rc;

    if (p == NULL || h == NULL)
	return 1;

    nb = strlen(teGetNEVR(p)) + (teGetE(p) != NULL ? strlen(teGetE(p)) : 0) + 1;
    t = alloca(nb);
    *t = '\0';
    reqEVR = t;
    if (teGetE(p) != NULL)	t = stpcpy( stpcpy(t, teGetE(p)), ":");
    if (teGetV(p) != NULL)	t = stpcpy(t, teGetV(p));
    *t++ = '-';
    if (teGetR(p) != NULL)	t = stpcpy(t, teGetR(p));
    
    req = dsSingle(RPMTAG_REQUIRENAME, teGetN(p), reqEVR, reqFlags);
    rc = headerMatchesDepFlags(h, req);
    req = dsFree(req);

    if (rc == 0) {
	const char * altNEVR = hGetNEVR(h, NULL);
	rpmProblemSetAppend(ts->probs, RPMPROB_OLDPACKAGE,
		teGetNEVR(p), teGetKey(p),
		NULL, NULL,
		altNEVR,
		0);
	altNEVR = _free(altNEVR);
	rc = 1;
    } else
	rc = 0;

    return rc;
}

/**
 */
/*@-mustmod@*/ /* FIX: fi->actions is modified. */
static void skipFiles(const rpmTransactionSet ts, TFI_t fi)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies fi, rpmGlobalMacroContext @*/
{
    int noDocs = (ts->transFlags & RPMTRANS_FLAG_NODOCS);
    char ** netsharedPaths = NULL;
    const char ** languages;
    const char * dn, * bn;
    int dnlen, bnlen, ix;
    const char * s;
    int * drc;
    char * dff;
    int dc;
    int i, j;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	const char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	/*@-branchstate@*/
	if (tmpPath && *tmpPath != '%')
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	/*@=branchstate@*/
	tmpPath = _free(tmpPath);
    }

    s = rpmExpand("%{_install_langs}", NULL);
    /*@-branchstate@*/
    if (!(s && *s != '%'))
	s = _free(s);
    if (s) {
	languages = (const char **) splitString(s, strlen(s), ':');
	s = _free(s);
    } else
	languages = NULL;
    /*@=branchstate@*/

    /* Compute directory refcount, skip directory if now empty. */
    dc = tfiGetDC(fi);
    drc = alloca(dc * sizeof(*drc));
    memset(drc, 0, dc * sizeof(*drc));
    dff = alloca(dc * sizeof(*dff));
    memset(dff, 0, dc * sizeof(*dff));

    fi = tfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = tfiNext(fi)) >= 0)
    {
	char **nsp;

	bn = tfiGetBN(fi);
	bnlen = strlen(bn);
	ix = tfiGetDX(fi);
	dn = tfiGetDN(fi);
	dnlen = strlen(dn);
	if (dn == NULL)
	    continue;	/* XXX can't happen */

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
		if (strncmp(dn, *nsp, len))
		    /*@innercontinue@*/ continue;
		/* Only directories or complete file paths can be net shared */
		if (!(dn[len] == '/' || dn[len] == '\0'))
		    /*@innercontinue@*/ continue;
	    } else {
		if (len < (dnlen + bnlen))
		    /*@innercontinue@*/ continue;
		if (strncmp(dn, *nsp, dnlen))
		    /*@innercontinue@*/ continue;
		if (strncmp(bn, (*nsp) + dnlen, bnlen))
		    /*@innercontinue@*/ continue;
		len = dnlen + bnlen;
		/* Only directories or complete file paths can be net shared */
		if (!((*nsp)[len] == '/' || (*nsp)[len] == '\0'))
		    /*@innercontinue@*/ continue;
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
	    for (lang = languages; *lang != NULL; lang++) {
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
	if (noDocs && (tfiGetFFlags(fi) & RPMFILE_DOC)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}
    }

    /* Skip (now empty) directories that had skipped files. */
#ifndef	NOTYET
    if (fi != NULL)	/* XXX can't happen */
    for (j = 0; j < dc; j++)
#else
    if ((fi = tdiInit(fi)) != NULL)
    while (j = tdiNext(fi) >= 0)
#endif
    {

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
	fi = tfiInit(fi, 0);
	if (fi != NULL)		/* XXX lclint */
	while ((i = tfiNext(fi)) >= 0) {
	    const char * dir;

	    if (XFA_SKIPPING(fi->actions[i]))
		/*@innercontinue@*/ continue;
	    if (whatis(fi->fmodes[i]) != XDIR)
		/*@innercontinue@*/ continue;
	    dir = fi->dnl[fi->dil[i]];
	    if (strlen(dir) != dnlen)
		/*@innercontinue@*/ continue;
	    if (strncmp(dir, dn, dnlen))
		/*@innercontinue@*/ continue;
	    if (strlen(fi->bnl[i]) != bnlen)
		/*@innercontinue@*/ continue;
	    if (strncmp(fi->bnl[i], bn, bnlen))
		/*@innercontinue@*/ continue;
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
/*@=mustmod@*/

/**
 * Return transaction element's file info.
 * @todo Take a TFI_t refcount here.
 * @param tei		transaction element iterator
 * @return		transaction element file info
 */
static /*@null@*/
TFI_t teiGetFi(const teIterator tei)
	/*@*/
{
    TFI_t fi = NULL;

    if (tei != NULL && tei->ocsave != -1) {
	/*@-type -abstract@*/ /* FIX: transactionElement not opaque */
	transactionElement te = tei->ts->order[tei->ocsave];
	/*@-assignexpose@*/
	if ((fi = te->fi) != NULL)
	    fi->te = te;
	/*@=assignexpose@*/
	/*@=type =abstract@*/
    }
    /*@-compdef -refcounttrans -usereleased @*/
    return fi;
    /*@=compdef =refcounttrans =usereleased @*/
}

#define	NOTIFY(_ts, _al)	if ((_ts)->notify) (void) (_ts)->notify _al

int rpmRunTransactions(	rpmTransactionSet ts,
			rpmProblemSet okProbs, rpmProblemSet * newProbs,
			rpmtransFlags transFlags, rpmprobFilterFlags ignoreSet)
{
    int i, j;
    int ourrc = 0;
    int totalFileCount = 0;
    TFI_t fi;
    struct diskspaceInfo * dip;
    sharedFileInfo shared, sharedList;
    int numShared;
    int nexti;
    alKey lastKey;
    fingerPrintCache fpc;
    PSM_t psm = memset(alloca(sizeof(*psm)), 0, sizeof(*psm));
    teIterator pi;	transactionElement p;
    teIterator qi;	transactionElement q;
    int xx;

    /* FIXME: what if the same package is included in ts twice? */

    ts->transFlags = transFlags;
    if (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS)
	ts->transFlags |= (_noTransScripts | _noTransTriggers);
    if (ts->transFlags & RPMTRANS_FLAG_NOTRIGGERS)
	ts->transFlags |= _noTransTriggers;

    /* XXX MULTILIB is broken, as packages can and do execute /sbin/ldconfig. */
    if (ts->transFlags & (RPMTRANS_FLAG_JUSTDB | RPMTRANS_FLAG_MULTILIB))
	ts->transFlags |= (_noTransScripts | _noTransTriggers);

    ts->probs = rpmProblemSetFree(ts->probs);
    ts->probs = rpmProblemSetCreate();
    *newProbs = rpmpsLink(ts->probs, "RunTransactions");
    ts->ignoreSet = ignoreSet;
    ts->currDir = _free(ts->currDir);
    ts->currDir = currentDirectory();
    ts->chrootDone = 0;
    if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
    ts->id = (int_32) time(NULL);

    memset(psm, 0, sizeof(*psm));
    psm->ts = rpmtsLink(ts, "tsRun");

    /* Get available space on mounted file systems. */
    if (!(ts->ignoreSet & RPMPROB_FILTER_DISKSPACE) &&
		!rpmGetFilesystemList(&ts->filesystems, &ts->filesystemCount))
    {
	struct stat sb;

	rpmMessage(RPMMESS_DEBUG, _("getting list of mounted filesystems\n"));

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

		xx = stat(ts->filesystems[i], &sb);
		ts->di[i].dev = sb.st_dev;
	    }
	}

	if (dip) ts->di[i].bsize = 0;
    }

    /* ===============================================
     * For packages being installed:
     * - verify package arch/os.
     * - verify package epoch:version-release is newer.
     * - count files.
     * For packages being removed:
     * - count files.
     */
    /* The ordering doesn't matter here */
    pi = teInitIterator(ts);
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
	rpmdbMatchIterator mi;
	int fc;

	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = tfiGetFC(fi);

	if (!(ts->ignoreSet & RPMPROB_FILTER_IGNOREARCH))
	    if (!archOkay(teGetA(p)))
		rpmProblemSetAppend(ts->probs, RPMPROB_BADARCH,
			teGetNEVR(p), teGetKey(p),
			teGetA(p), NULL,
			NULL, 0);

	if (!(ts->ignoreSet & RPMPROB_FILTER_IGNOREOS))
	    if (!osOkay(teGetO(p)))
		rpmProblemSetAppend(ts->probs, RPMPROB_BADOS,
			teGetNEVR(p), teGetKey(p),
			teGetO(p), NULL,
			NULL, 0);

	if (!(ts->ignoreSet & RPMPROB_FILTER_OLDPACKAGE)) {
	    Header h;
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, teGetN(p), 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL)
		xx = ensureOlder(ts, p, h);
	    mi = rpmdbFreeIterator(mi);
	}

	/* XXX multilib should not display "already installed" problems */
	if (!(ts->ignoreSet & RPMPROB_FILTER_REPLACEPKG) && !teGetMultiLib(p)) {
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, teGetN(p), 0);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT,
				teGetV(p));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT,
				teGetR(p));

	    while (rpmdbNextIterator(mi) != NULL) {
		rpmProblemSetAppend(ts->probs, RPMPROB_PKG_INSTALLED,
			teGetNEVR(p), teGetKey(p),
			NULL, NULL,
			NULL, 0);
		/*@innerbreak@*/ break;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	/* Count no. of files (if any). */
	totalFileCount += fc;

    }
    pi = teFreeIterator(pi);

    /* The ordering doesn't matter here */
    pi = teInitIterator(ts);
    while ((p = teNext(pi, TR_REMOVED)) != NULL) {
	int fc;

	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = tfiGetFC(fi);

	totalFileCount += fc;
    }
    pi = teFreeIterator(pi);

    /* ===============================================
     * Initialize transaction element file info for package:
     */

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
    pi = teInitIterator(ts);
    while ((p = teNextIterator(pi)) != NULL) {
	int fc;

	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = tfiGetFC(fi);

#ifdef	DYING	/* XXX W2DO? this is now done in teiGetFi, okay ??? */
	fi->magic = TFIMAGIC;
	fi->te = p;
#endif

	/*@-branchstate@*/
	switch (teGetType(p)) {
	case TR_ADDED:
	    fi->record = 0;
	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    if (fc > 0)
		skipFiles(ts, fi);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    fi->record = teGetDBOffset(p);
	    /*@switchbreak@*/ break;
	}
	/*@=branchstate@*/

	fi->fps = (fc > 0 ? xmalloc(fc * sizeof(*fi->fps)) : NULL);
    }
    pi = teFreeIterator(pi);

    if (!ts->chrootDone) {
	xx = chdir("/");
	/*@-superuser -noeffect @*/
	xx = chroot(ts->rootDir);
	/*@=superuser =noeffect @*/
	ts->chrootDone = 1;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 1;
    }

    ts->ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    pi = teInitIterator(ts);
    while ((p = teNextIterator(pi)) != NULL) {
	int fc;

	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = tfiGetFC(fi);

	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fc, fi->fps);
	/*@-branchstate@*/
 	fi = tfiInit(fi, 0);
 	if (fi != NULL)		/* XXX lclint */
	while ((i = tfiNext(fi)) >= 0) {
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@innercontinue@*/ continue;
	    /*@-dependenttrans@*/
	    htAddEntry(ts->ht, fi->fps + i, (void *) fi);
	    /*@=dependenttrans@*/
	}
	/*@=branchstate@*/
    }
    pi = teFreeIterator(pi);

    /*@-noeffectuncon @*/ /* FIX: check rc */
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, ts->orderCount,
	NULL, ts->notifyData));
    /*@=noeffectuncon@*/

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    pi = teInitIterator(ts);
    while ((p = teNextIterator(pi)) != NULL) {
	dbiIndexSet * matches;
	int knownBad;
	int fc;

	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = tfiGetFC(fi);

	/*@-noeffectuncon @*/ /* FIX: check rc */
	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, teiGetOc(pi),
			ts->orderCount, NULL, ts->notifyData));
	/*@=noeffectuncon@*/

	if (fc == 0) continue;

	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(fc, sizeof(*matches));
	if (rpmdbFindFpList(ts->rpmdb, fi->fps, matches, fc)) {
	    psm->ts = rpmtsUnlink(ts, "tsRun (rpmFindFpList fail)");
	    return 1;	/* XXX WTFO? */
	}

	numShared = 0;
 	fi = tfiInit(fi, 0);
	while ((i = tfiNext(fi)) >= 0)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = xcalloc((numShared + 1), sizeof(*sharedList));

 	fi = tfiInit(fi, 0);
	while ((i = tfiNext(fi)) >= 0) {
	    /*
	     * Take care not to mark files as replaced in packages that will
	     * have been removed before we will get here.
	     */
	    for (j = 0; j < dbiIndexSetCount(matches[i]); j++) {
		int ro;
		ro = dbiIndexRecordOffset(matches[i], j);
		knownBad = 0;
		qi = teInitIterator(ts);
		while ((q = teNext(qi, TR_REMOVED)) != NULL) {
		    if (ro == knownBad)
			/*@innerbreak@*/ break;
		    if (teGetDBOffset(q) == ro)
			knownBad = ro;
		}
		qi = teFreeIterator(qi);

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
	/*@-branchstate@*/
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
	    if (ts->removedPackages != NULL)
	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] != shared->otherPkg)
		    /*@innercontinue@*/ continue;
		beingRemoved = 1;
		/*@innerbreak@*/ break;
	    }

	    /* Determine the fate of each file. */
	    switch (teGetType(p)) {
	    case TR_ADDED:
		xx = handleInstInstalledFiles(ts, p, fi, shared, nexti - i,
	!(beingRemoved || (ts->ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)));
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    xx = handleRmvdInstalledFiles(ts, fi, shared, nexti - i);
		/*@switchbreak@*/ break;
	    }
	}
	/*@=branchstate@*/

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
	handleOverlappedFiles(ts, p, fi);

	/* Check added package has sufficient space on each partition used. */
	switch (teGetType(p)) {
	case TR_ADDED:
	    if (!(ts->di && tfiGetFC(fi) > 0))
		/*@switchbreak@*/ break;
	    for (i = 0; i < ts->filesystemCount; i++) {

		dip = ts->di + i;

		/* XXX Avoid FAT and other file systems that have not inodes. */
		if (dip->iavail <= 0)
		    /*@innercontinue@*/ continue;

		if (adj_fs_blocks(dip->bneeded) > dip->bavail) {
		    rpmProblemSetAppend(ts->probs, RPMPROB_DISKSPACE,
				teGetNEVR(p), teGetKey(p),
				ts->filesystems[i], NULL, NULL,
	 	   (adj_fs_blocks(dip->bneeded) - dip->bavail) * dip->bsize);
		}

		if (adj_fs_blocks(dip->ineeded) > dip->iavail) {
		    rpmProblemSetAppend(ts->probs, RPMPROB_DISKNODES,
				teGetNEVR(p), teGetKey(p),
				ts->filesystems[i], NULL, NULL,
	 	    (adj_fs_blocks(dip->ineeded) - dip->iavail));
		}
	    }
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    /*@switchbreak@*/ break;
	}
    }
    pi = teFreeIterator(pi);

    if (ts->chrootDone) {
	/*@-superuser -noeffect @*/
	xx = chroot(".");
	/*@=superuser =noeffect @*/
	ts->chrootDone = 0;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
	xx = chdir(ts->currDir);
    }

    /*@-noeffectuncon @*/ /* FIX: check rc */
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, ts->orderCount,
	NULL, ts->notifyData));
    /*@=noeffectuncon @*/

    /* ===============================================
     * Free unused memory as soon as possible.
     */
    pi = teInitIterator(ts);
    while ((p = teNextIterator(pi)) != NULL) {
	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	if (tfiGetFC(fi) == 0)
	    continue;
	fi->fps = _free(fi->fps);
    }
    pi = teFreeIterator(pi);

    fpCacheFree(fpc);
    htFree(ts->ht);
    ts->ht = NULL;

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
    if ((ts->transFlags & RPMTRANS_FLAG_BUILD_PROBS)
     || (ts->probs->numProblems &&
		(okProbs != NULL || rpmProblemSetTrim(ts->probs, okProbs)))
       )
    {
	if (psm->ts != NULL)
	    psm->ts = rpmtsUnlink(psm->ts, "tsRun (problems)");
	return ts->orderCount;
    }

    /* ===============================================
     * Save removed files before erasing.
     */
    if (ts->transFlags & (RPMTRANS_FLAG_DIRSTASH | RPMTRANS_FLAG_REPACKAGE)) {
	pi = teInitIterator(ts);
	while ((p = teNextIterator(pi)) != NULL) {
	    fi = teiGetFi(pi);
	    switch (teGetType(p)) {
	    case TR_ADDED:
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (!(ts->transFlags & RPMTRANS_FLAG_REPACKAGE))
		    /*@switchbreak@*/ break;
		psm->te = p;
		psm->fi = rpmfiLink(fi, "tsRepackage");
		xx = psmStage(psm, PSM_PKGSAVE);
		(void) rpmfiUnlink(fi, "tsRepackage");
		psm->fi = NULL;
		psm->te = NULL;
		/*@switchbreak@*/ break;
	    }
	}
	pi = teFreeIterator(pi);
    }

    /* ===============================================
     * Install and remove packages.
     */
    lastKey = (alKey)-2;	/* erased packages have -1 */
    pi = teInitIterator(ts);
    /*@-branchstate@*/ /* FIX: fi reload needs work */
    while ((p = teNextIterator(pi)) != NULL) {
	alKey pkgKey;
	Header h;
	int gotfd;

	gotfd = 0;
	if ((fi = teiGetFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	psm->te = p;
	psm->fi = rpmfiLink(fi, "tsInstall");
	switch (teGetType(p)) {
	case TR_ADDED:

	    pkgKey = teGetAddedKey(p);

	    rpmMessage(RPMMESS_DEBUG, "========== +++ %s\n", teGetNEVR(p));
	    h = NULL;
	    /*@-type@*/ /* FIX: transactionElement not opaque */
	    {
		/*@-noeffectuncon@*/ /* FIX: notify annotations */
		p->fd = ts->notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
				teGetKey(p), ts->notifyData);
		/*@=noeffectuncon@*/
		if (teGetFd(p) != NULL) {
		    rpmRC rpmrc;

		    rpmrc = rpmReadPackageFile(ts, teGetFd(p),
				"rpmRunTransactions", &h);

		    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE)) {
			/*@-noeffectuncon@*/ /* FIX: notify annotations */
			p->fd = ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0,
					teGetKey(p), ts->notifyData);
			/*@=noeffectuncon@*/
			p->fd = NULL;
			ourrc++;
		    }
		    if (teGetFd(p) != NULL) gotfd = 1;
		}
	    }
	    /*@=type@*/

	    if (teGetFd(p) != NULL) {
		{
char * fstates = fi->fstates;
fileAction * actions = fi->actions;

fi->fstates = NULL;
fi->actions = NULL;
		    (void) fiFree(fi, 0);
/*@-usereleased@*/
fi->magic = TFIMAGIC;
fi->te = p;
fi->record = 0;
		    (void) fiNew(ts, fi, h, RPMTAG_BASENAMES, 1);
fi->fstates = _free(fi->fstates);
fi->fstates = fstates;
fi->actions = _free(fi->actions);
fi->actions = actions;
/*@=usereleased@*/

		}
		if (teGetMultiLib(p))
		    ts->transFlags |= RPMTRANS_FLAG_MULTILIB;

		if (psmStage(psm, PSM_PKGINSTALL)) {
		    ourrc++;
		    lastKey = pkgKey;
		}
		fi->h = headerFree(fi->h, "TR_ADDED fi->h free");
	    } else {
		ourrc++;
		lastKey = pkgKey;
	    }

	    h = headerFree(h, "TR_ADDED h free");

	    if (gotfd) {
		/*@-noeffectuncon @*/ /* FIX: check rc */
		(void) ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			teGetKey(p), ts->notifyData);
		/*@=noeffectuncon @*/
		/*@-type@*/
		p->fd = NULL;
		/*@=type@*/
	    }
	    (void) fiFree(fi, 0);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    rpmMessage(RPMMESS_DEBUG, "========== --- %s\n", teGetNEVR(p));
	    /* If install failed, then we shouldn't erase. */
	    if (teGetDependsOnKey(p) != lastKey) {
		if (psmStage(psm, PSM_PKGERASE))
		    ourrc++;
	    }
	    (void) fiFree(fi, 0);
	    /*@switchbreak@*/ break;
	}
	xx = rpmdbSync(ts->rpmdb);
	(void) rpmfiUnlink(psm->fi, "tsInstall");
	psm->fi = NULL;
	psm->te = NULL;
    }
    /*@=branchstate@*/
    pi = teFreeIterator(pi);

    psm->ts = rpmtsUnlink(psm->ts, "tsRun");

    /*@-nullstate@*/ /* FIX: ts->flList may be NULL */
    if (ourrc)
    	return -1;
    else
	return 0;
    /*@=nullstate@*/
}
