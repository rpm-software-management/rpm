/** \ingroup rpmtrans
 * \file lib/transaction.c
 */

#include "system.h"

#define _NEED_TEITERATOR	1
#include "psm.h"
#include "rpmds.h"

#include "rpmal.h"
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "fprint.h"
#include "legacy.h"	/* XXX mdfile */
#include "misc.h" /* XXX stripTrailingChar, splitString, currentDirectory */
#include "rpmdb.h"

/*@-redecl -exportheadervar@*/
/*@unchecked@*/
extern const char * chroot_prefix;
/*@=redecl =exportheadervar@*/

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

/*@access rpmFNSet @*/
/*@access TFI_t @*/

/*@access teIterator @*/
/*@access transactionElement @*/
/*@access rpmTransactionSet @*/

/**
 */
struct diskspaceInfo {
    dev_t dev;			/*!< file system device number. */
    signed long bneeded;	/*!< no. of blocks needed. */
    signed long ineeded;	/*!< no. of inodes needed. */
    int bsize;			/*!< file system block size. */
    signed long bavail;		/*!< no. of blocks available. */
    signed long iavail;		/*!< no. of inodes available. */
};

/**
 * Adjust for root only reserved space. On linux e2fs, this is 5%.
 */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

/**
 */
static /*@null@*/ void * freeFl(rpmTransactionSet ts,
		/*@only@*/ /*@null@*/ TFI_t flList)
	/*@*/
{
    if (flList) {
	TFI_t fi;
	int oc;

	/*@-usereleased -onlytrans @*/ /* FIX: fi needs to be only */
	for (oc = 0, fi = flList; oc < ts->orderCount; oc++, fi++)
	    freeFi(fi);
	flList = _free(flList);
	/*@=usereleased =onlytrans @*/
    }
    return NULL;
}

void rpmtransSetScriptFd(rpmTransactionSet ts, FD_t fd)
{
    /*@-type@*/ /* FIX: cast? */
    ts->scriptFd = (fd ? fdLink(fd, "rpmtransSetScriptFd") : NULL);
    /*@=type@*/
}

int rpmtransGetKeys(const rpmTransactionSet ts, fnpyKey ** ep, int * nep)
{
    int rc = 0;

    if (nep) *nep = ts->orderCount;
    if (ep) {
	fnpyKey * e;
	int oc;

	*ep = e = xmalloc(ts->orderCount * sizeof(*e));
	for (oc = 0; oc < ts->orderCount; oc++, e++) {
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		*e = ts->order[oc].key;
		/*@switchbreak@*/ break;
	    default:
	    case TR_REMOVED:
		/*@-mods@*/	/* FIX: double indirection. */
		*e = NULL;
		/*@=mods@*/
		/*@switchbreak@*/ break;
	    }
	}
    }
    return rc;
}

/**
 */
static int archOkay(Header h, /*@out@*/ const char ** pkgArchPtr)
	/*@modifies *pkgArchPtr @*/
{
    const char * pkgArch;
    int type, count;
    int rc = 1;	/* assume AOK */

    if (pkgArchPtr != NULL) *pkgArchPtr = pkgArch = NULL;

    /* make sure we're trying to install this on the proper architecture */
    (void) headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);

#ifdef	DYING
    if (type == RPM_INT8_TYPE) {
	int_8 * pkgArchNum;
	int archNum;

	/* old arch handling */
	rpmGetArchInfo(NULL, &archNum);
	pkgArchNum = pkgArch;
	if (archNum != *pkgArchNum)
	    rc = 0;
    } else
#endif

    if (!rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch)) {
	rc = 0;
	if (pkgArchPtr != NULL) *pkgArchPtr = pkgArch;
    }

    return rc;
}

/**
 */
static int osOkay(Header h, /*@out@*/ const char ** pkgOsPtr)
	/*@modifies *pkgOsPtr @*/
{
    const char * pkgOs;
    int type, count;
    int rc = 1;	/* assume AOK */

    if (pkgOsPtr != NULL) *pkgOsPtr = pkgOs = NULL;

    /* make sure we're trying to install this on the proper os */
    (void) headerGetEntry(h, RPMTAG_OS, &type, (void **) &pkgOs, &count);

#ifdef	DYING
    if (type == RPM_INT8_TYPE) {
	/* v1 packages and v2 packages both used improper OS numbers, so just
	   deal with it hope things work */
	return 1;
    } else
#endif

    if (!rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs)) {
	rc = 0;
	if (pkgOsPtr != NULL) *pkgOsPtr = pkgOs;
    }

    return rc;
}

/**
 */
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

/**
 */
static fileAction decideFileFate(const char * dirName,
			const char * baseName, short dbMode,
			const char * dbMd5, const char * dbLink, short newMode,
			const char * newMd5, const char * newLink, int newFlags,
			rpmtransFlags transFlags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
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

/**
 */
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

/**
 */
/* XXX only ts->{probs,rpmdb} modified */
static int handleInstInstalledFiles(const rpmTransactionSet ts, TFI_t fi,
		struct sharedFileInfo * shared,
		int sharedCount, int reportConflicts)
	/*@globals fileSystem @*/
	/*@modifies ts, fi, fileSystem @*/
{
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpmtransFlags transFlags = ts->transFlags;
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
    int xx;

    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    xx = hge(h, RPMTAG_FILEMD5S, &omtype, (void **) &otherMd5s, NULL);
    xx = hge(h, RPMTAG_FILELINKTOS, &oltype, (void **) &otherLinks, NULL);
    xx = hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);
    xx = hge(h, RPMTAG_FILEMODES, NULL, (void **) &otherModes, NULL);
    xx = hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &otherFlags, NULL);
    xx = hge(h, RPMTAG_FILESIZES, NULL, (void **) &otherSizes, NULL);

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
	    /*@-compdef@*/ /* FIX: *fi->replaced undefined */
	    if (reportConflicts) {
		const char * pkgNEVR = fiGetNEVR(fi);
		const char * altNEVR = hGetNEVR(h, NULL);
		rpmProblemSetAppend(ts->probs, RPMPROB_FILE_CONFLICT,
			pkgNEVR, fi->key,
			fi->dnl[fi->dil[fileNum]], fi->bnl[fileNum],
			altNEVR,
			0);
		pkgNEVR = _free(pkgNEVR);
		altNEVR = _free(altNEVR);
	    }
	    /*@=compdef@*/
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

/**
 */
/* XXX only ts->rpmdb modified */
static int handleRmvdInstalledFiles(const rpmTransactionSet ts, TFI_t fi,
		struct sharedFileInfo * shared, int sharedCount)
	/*@globals fileSystem @*/
	/*@modifies fi, fileSystem @*/
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

/**
 * Update disk space needs on each partition for this package.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(const rpmTransactionSet ts, TFI_t fi)
	/*@globals fileSystem @*/
	/*@modifies ts, fi, fileSystem @*/
{
    struct diskspaceInfo * ds = NULL;
    uint_32 fixupSize = 0;
    char * filespec = NULL;
    int fileSpecAlloced = 0;
    int i, j;
  
    for (i = 0; i < fi->fc; i++) {
	int otherPkgNum, otherFileNum;
	const TFI_t * recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	j = strlen(fi->dnl[fi->dil[i]]) + strlen(fi->bnl[i]) + 1;
	/*@-branchstate@*/
	if (j > fileSpecAlloced) {
	    fileSpecAlloced = j * 2;
	    filespec = xrealloc(filespec, fileSpecAlloced);
	}
	/*@=branchstate@*/

	(void) stpcpy( stpcpy( filespec, fi->dnl[fi->dil[i]]), fi->bnl[i]);

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
	(void) htGetEntry(ts->ht, &fi->fps[i],
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
	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    /* Added packages need only look at other added packages. */
	    if (fi->type == TR_ADDED && recs[otherPkgNum]->type != TR_ADDED)
		/*@innercontinue@*/ continue;

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
		    /*@switchbreak@*/ break;
		if ((fi->fflags[i] & RPMFILE_CONFIG) && 
			!lstat(filespec, &sb)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (fi->fflags[i] & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
		/*@switchbreak@*/ break;
	    }

	    /* Mark added overlapped non-identical files as a conflict. */
	    if ((ts->ignoreSet & RPMPROB_FILTER_REPLACENEWFILES)
	     && filecmp(recs[otherPkgNum]->fmodes[otherFileNum],
			recs[otherPkgNum]->fmd5s[otherFileNum],
			recs[otherPkgNum]->flinks[otherFileNum],
			fi->fmodes[i],
			fi->fmd5s[i],
			fi->flinks[i]))
	    {
		const char * pkgNEVR = fiGetNEVR(fi);
		const char * altNEVR = fiGetNEVR(recs[otherPkgNum]);
		rpmProblemSetAppend(ts->probs, RPMPROB_NEW_FILE_CONFLICT,
			pkgNEVR, fi->key,
			filespec, NULL,
			altNEVR,
			0);
		pkgNEVR = _free(pkgNEVR);
		altNEVR = _free(altNEVR);
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
	  } /*@switchbreak@*/ break;
	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
		/* Here is an overlapped added file we don't want to nuke. */
		if (recs[otherPkgNum]->actions[otherFileNum] != FA_ERASE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    /*@switchbreak@*/ break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		recs[otherPkgNum]->actions[otherFileNum] = FA_SKIP;
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@switchbreak@*/ break;
	    if (fi->fstates && fi->fstates[i] != RPMFILE_STATE_NORMAL)
		/*@switchbreak@*/ break;
	    if (!(S_ISREG(fi->fmodes[i]) && (fi->fflags[i] & RPMFILE_CONFIG))) {
		fi->actions[i] = FA_ERASE;
		/*@switchbreak@*/ break;
	    }
		
	    /* Here is a pre-existing modified config file that needs saving. */
	    {	char mdsum[50];
		if (!mdfile(filespec, mdsum) && strcmp(fi->fmd5s[i], mdsum)) {
		    fi->actions[i] = FA_BACKUP;
		    /*@switchbreak@*/ break;
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
    filespec = _free(filespec);
}

/**
 */
static int ensureOlder(rpmTransactionSet ts,
		const Header h, /*@null@*/ const Header old,
		/*@dependent@*/ /*@null@*/ const void * key)
	/*@modifies ts @*/
{
    int result, rc = 0;

    if (old == NULL) return 1;

    result = rpmVersionCompare(old, h);
    if (result <= 0)
	rc = 0;
    else if (result > 0) {
	const char * pkgNEVR = hGetNEVR(h, NULL);
	const char * altNEVR = hGetNEVR(old, NULL);
	/*@-evalorder@*/ /* LCL: is confused */
	rpmProblemSetAppend(ts->probs, RPMPROB_OLDPACKAGE,
		pkgNEVR, key,
		NULL, NULL,
		altNEVR,
		0);
	/*@=evalorder@*/
	pkgNEVR = _free(pkgNEVR);
	altNEVR = _free(altNEVR);
	rc = 1;
    }

    return rc;
}

/**
 */
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

/**
 * Return next transaction element's file info.
 * @param tei		transaction element iterator
 * @return		nest transaction element file info, NULL on termination
 */
/*@unused@*/ static inline
TFI_t teNextFi(teIterator tei)
	/*@modifies tei @*/
{
    TFI_t fi = NULL;

    if (teNextIterator(tei) != NULL && tei->ocsave != -1)
	fi = tei->ts->flList + tei->ocsave;
    /*@-compdef -onlytrans -usereleased@*/ /* FIX: ts->flList may be released */
    return fi;
    /*@=compdef =onlytrans =usereleased@*/
}

#define	NOTIFY(_ts, _al)	if ((_ts)->notify) (void) (_ts)->notify _al

int rpmRunTransactions(	rpmTransactionSet ts,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			rpmProblemSet okProbs, rpmProblemSet * newProbs,
			rpmtransFlags transFlags, rpmprobFilterFlags ignoreSet)
{
    int i, j;
    int ourrc = 0;
    int totalFileCount = 0;
    TFI_t fi;
    struct diskspaceInfo * dip;
    struct sharedFileInfo * shared, * sharedList;
    int numShared;
    int nexti;
    alKey pkgKey, lastKey;
    int oc;
    transactionElement p;
    fingerPrintCache fpc;
    struct psm_s psmbuf;
    PSM_t psm = &psmbuf;
    teIterator tei;
    int xx;

int keep_header = 1;	/* XXX rpmProblemSetAppend prevents dumping headers. */

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
    ts->probs = *newProbs = rpmProblemSetCreate();
    *newProbs = rpmpsLink(ts->probs, "RunTransactions");
    /*@=assignexpose@*/
    ts->ignoreSet = ignoreSet;
    ts->currDir = _free(ts->currDir);
    ts->currDir = currentDirectory();
    ts->chrootDone = 0;
    if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
    ts->id = (int_32) time(NULL);

    memset(psm, 0, sizeof(*psm));
    /*@-assignexpose@*/
    psm->ts = rpmtsLink(ts, "tsRun");
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
    tei = teInitIterator(ts);
    while ((p = teNext(tei, TR_ADDED)) != NULL) {
	const char * n, * v, * r;
	fnpyKey key;
	rpmdbMatchIterator mi;
	const char * str1;
	Header h;

	pkgKey = p->u.addedKey;

	h = alGetHeader(ts->addedPackages, pkgKey, 0);
	if (h == NULL)	/* XXX can't happen */
	    continue;

	(void) headerNVR(h, &n, &v, &r);
	key = p->key;

	str1 = NULL;
	if (!archOkay(h, &str1) && !(ts->ignoreSet & RPMPROB_FILTER_IGNOREARCH)) {
	    const char * pkgNEVR = hGetNEVR(h, NULL);
	    rpmProblemSetAppend(ts->probs, RPMPROB_BADARCH,
			pkgNEVR, key,
			str1, NULL,
			NULL, 0);
	    pkgNEVR = _free(pkgNEVR);
	}

	str1 = NULL;
	if (!osOkay(h, &str1) && !(ts->ignoreSet & RPMPROB_FILTER_IGNOREOS)) {
	    const char * pkgNEVR = hGetNEVR(h, NULL);
	    rpmProblemSetAppend(ts->probs, RPMPROB_BADOS,
			pkgNEVR, key,
			str1, NULL,
			NULL, 0);
	    pkgNEVR = _free(pkgNEVR);
	}

	if (!(ts->ignoreSet & RPMPROB_FILTER_OLDPACKAGE)) {
	    Header oldH;
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, n, 0);
	    while ((oldH = rpmdbNextIterator(mi)) != NULL)
		xx = ensureOlder(ts, h, oldH, key);
	    mi = rpmdbFreeIterator(mi);
	}

	/* XXX multilib should not display "already installed" problems */
	if (!(ts->ignoreSet & RPMPROB_FILTER_REPLACEPKG)
#ifdef DYING	/* XXX MULTILIB multiLib from transactionElement */
	 && !alGetMultiLib(ts->addedPackages, i)
#endif
	) {
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, n, 0);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT, v);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT, r);

	    while (rpmdbNextIterator(mi) != NULL) {
		const char * pkgNEVR = hGetNEVR(h, NULL);
		rpmProblemSetAppend(ts->probs, RPMPROB_PKG_INSTALLED,
			pkgNEVR, key,
			NULL, NULL,
			NULL, 0);
		pkgNEVR = _free(pkgNEVR);
		/*@innerbreak@*/ break;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	/* Count no. of files (if any). */
	if (p->fns != NULL)
	    totalFileCount += p->fns->fc;

	h = headerFree(h, "alGetHeader (rpmtsRun sanity)");

    }
    tei = teFreeIterator(tei);

    /* FIXME: it seems a bit silly to read in all of these headers twice */
    /* The ordering doesn't matter here */
    if (ts->numRemovedPackages > 0) {
	rpmdbMatchIterator mi;
	Header h;
	int fileCount;

	mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
	xx = rpmdbAppendIterator(mi, ts->removedPackages, ts->numRemovedPackages);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (headerGetEntry(h, RPMTAG_BASENAMES, NULL, NULL, &fileCount))
		totalFileCount += fileCount;
	}
	mi = rpmdbFreeIterator(mi);
    }

    /* ===============================================
     * Initialize transaction element file info for package:
     */
    ts->flEntries = alGetSize(ts->addedPackages) + ts->numRemovedPackages;
    ts->flList = xcalloc(ts->flEntries, sizeof(*ts->flList));

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
    tei = teInitIterator(ts);
    while ((fi = teNextFi(tei)) != NULL) {

	oc = teGetOc(tei);
	fi->magic = TFIMAGIC;

	fi->type = ts->order[oc].type;

	/*@-branchstate@*/
	switch (fi->type) {
	case TR_ADDED:
	    fi->record = 0;

	    pkgKey = ts->order[oc].u.addedKey;

	    fi->h = alGetHeader(ts->addedPackages, pkgKey, 1);
#ifdef DYING	/* XXX MULTILIB multiLib from transactionElement */
	    fi->multiLib = alGetMultiLib(ts->addedPackages, i);
#else
	    fi->multiLib = ts->order[oc].multiLib;
#endif

/*@i@*/	    fi->key = ts->order[oc].key;
	    fi->relocs = ts->order[oc].relocs;
/*@i@*/	    fi->fd = ts->order[oc].fd;

	    /* XXX availablePackage can be dumped here XXX */

	    /* XXX header arg unused. */
	    loadFi(ts, fi, fi->h, keep_header);

	    if (fi->fc == 0)
		continue;

	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    skipFiles(ts, fi);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    fi->record = ts->order[oc].u.removed.dboffset;
	    /* Retrieve erased package header from the database. */
	    {	rpmdbMatchIterator mi;

		mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));
		if ((fi->h = rpmdbNextIterator(mi)) != NULL)
		    fi->h = headerLink(fi->h,  "TR_REMOVED loadFi");
		mi = rpmdbFreeIterator(mi);
	    }
	    if (fi->h == NULL) {
		/* ACK! */
		continue;
	    }
	    /* XXX header arg unused. */
	    loadFi(ts, fi, fi->h, 0);
	    /*@switchbreak@*/ break;
	}
	/*@=branchstate@*/

	if (fi->fc)
	    fi->fps = xmalloc(fi->fc * sizeof(*fi->fps));
    }
    tei = teFreeIterator(tei);

    if (!ts->chrootDone) {
	xx = chdir("/");
	/*@-superuser -noeffect @*/
	xx = chroot(ts->rootDir);
	/*@=superuser =noeffect @*/
	ts->chrootDone = 1;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 1;
	/*@-onlytrans@*/
	/*@-mods@*/
	chroot_prefix = ts->rootDir;
	/*@=mods@*/
	/*@=onlytrans@*/
    }

    ts->ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    tei = teInitIterator(ts);
    while ((fi = teNextFi(tei)) != NULL) {
	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fi->fc, fi->fps);
	for (i = 0; i < fi->fc; i++) {
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@innercontinue@*/ continue;
	    /*@-dependenttrans@*/
	    htAddEntry(ts->ht, fi->fps + i, fi);
	    /*@=dependenttrans@*/
	}
    }
    tei = teFreeIterator(tei);

    /*@-noeffectuncon @*/ /* FIX: check rc */
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, ts->flEntries,
	NULL, ts->notifyData));
    /*@=noeffectuncon@*/

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    tei = teInitIterator(ts);
    while ((fi = teNextFi(tei)) != NULL) {
	dbiIndexSet * matches;
	int knownBad;

	/*@-noeffectuncon @*/ /* FIX: check rc */
	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, (fi - ts->flList),
			ts->flEntries, NULL, ts->notifyData));
	/*@=noeffectuncon@*/

	if (fi->fc == 0) continue;

	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(fi->fc, sizeof(*matches));
	if (rpmdbFindFpList(ts->rpmdb, fi->fps, matches, fi->fc)) {
	    psm->ts = rpmtsUnlink(ts, "tsRun (rpmFindFpList fail)");
	    return 1;	/* XXX WTFO? */
	}

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
			/*@switchbreak@*/ break;
		    case TR_ADDED:
			/*@switchbreak@*/ break;
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
		    /*@innercontinue@*/ continue;
		beingRemoved = 1;
		/*@innerbreak@*/ break;
	    }

	    /* Determine the fate of each file. */
	    switch (fi->type) {
	    case TR_ADDED:
		xx = handleInstInstalledFiles(ts, fi, shared, nexti - i,
	!(beingRemoved || (ts->ignoreSet & RPMPROB_FILTER_REPLACEOLDFILES)));
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    xx = handleRmvdInstalledFiles(ts, fi, shared, nexti - i);
		/*@switchbreak@*/ break;
	    }
	}

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
	handleOverlappedFiles(ts, fi);

	/* Check added package has sufficient space on each partition used. */
	switch (fi->type) {
	case TR_ADDED:
	    if (!(ts->di && fi->fc))
		/*@switchbreak@*/ break;
	    for (i = 0; i < ts->filesystemCount; i++) {

		dip = ts->di + i;

		/* XXX Avoid FAT and other file systems that have not inodes. */
		if (dip->iavail <= 0)
		    /*@innercontinue@*/ continue;

		if (adj_fs_blocks(dip->bneeded) > dip->bavail) {
		    const char * pkgNEVR = fiGetNEVR(fi);
		    rpmProblemSetAppend(ts->probs, RPMPROB_DISKSPACE,
				pkgNEVR, fi->key,
				ts->filesystems[i], NULL, NULL,
	 	   (adj_fs_blocks(dip->bneeded) - dip->bavail) * dip->bsize);
		    pkgNEVR = _free(pkgNEVR);
		}

		if (adj_fs_blocks(dip->ineeded) > dip->iavail) {
		    const char * pkgNEVR = fiGetNEVR(fi);
		    rpmProblemSetAppend(ts->probs, RPMPROB_DISKNODES,
				pkgNEVR, fi->key,
				ts->filesystems[i], NULL, NULL,
	 	    (adj_fs_blocks(dip->ineeded) - dip->iavail));
		    pkgNEVR = _free(pkgNEVR);
		}
	    }
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    /*@switchbreak@*/ break;
	}
    }
    tei = teFreeIterator(tei);

    if (ts->chrootDone) {
	/*@-superuser -noeffect @*/
	xx = chroot(".");
	/*@=superuser =noeffect @*/
	ts->chrootDone = 0;
	if (ts->rpmdb) ts->rpmdb->db_chrootDone = 0;
	/*@-mods@*/
	chroot_prefix = NULL;
	/*@=mods@*/
	xx = chdir(ts->currDir);
    }

    /*@-noeffectuncon @*/ /* FIX: check rc */
    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, ts->flEntries,
	NULL, ts->notifyData));
    /*@=noeffectuncon @*/

    /* ===============================================
     * Free unused memory as soon as possible.
     */

    tei = teInitIterator(ts);
    while ((fi = teNextFi(tei)) != NULL) {
	if (fi->fc == 0)
	    continue;
	fi->fps = _free(fi->fps);
    }
    tei = teFreeIterator(tei);

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
	ts->flList = freeFl(ts, ts->flList);
	ts->flEntries = 0;
	if (psm->ts != NULL)
	    psm->ts = rpmtsUnlink(psm->ts, "tsRun (problems)");
	/*@-nullstate@*/ /* FIX: ts->flList may be NULL */
	return ts->orderCount;
	/*@=nullstate@*/
    }

    /* ===============================================
     * Save removed files before erasing.
     */
    if (ts->transFlags & (RPMTRANS_FLAG_DIRSTASH | RPMTRANS_FLAG_REPACKAGE)) {
	tei = teInitIterator(ts);
	while ((fi = teNextFi(tei)) != NULL) {
	    switch (fi->type) {
	    case TR_ADDED:
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (ts->transFlags & RPMTRANS_FLAG_REPACKAGE) {
		    psm->fi = rpmfiLink(fi, "tsRepackage");
		    xx = psmStage(psm, PSM_PKGSAVE);
		    (void) rpmfiUnlink(fi, "tsRepackage");
		    psm->fi = NULL;
		}
		/*@switchbreak@*/ break;
	    }
	}
	tei = teFreeIterator(tei);
    }

    /* ===============================================
     * Install and remove packages.
     */

    lastKey = (alKey)-2;	/* erased packages have -1 */
    tei = teInitIterator(ts);
    /*@-branchstate@*/ /* FIX: fi reload needs work */
    while ((fi = teNextFi(tei)) != NULL) {
	Header h;
	int gotfd;

	oc = teGetOc(tei);
	gotfd = 0;
	psm->fi = rpmfiLink(fi, "tsInstall");
	switch (fi->type) {
	case TR_ADDED:

	    pkgKey = ts->order[oc].u.addedKey;

	    rpmMessage(RPMMESS_DEBUG, "========== +++ %s-%s-%s\n",
			fi->name, fi->version, fi->release);
	    h = (fi->h ? headerLink(fi->h, "TR_ADDED install") : NULL);
	    /*@-branchstate@*/
	    if (fi->fd == NULL) {
		/*@-noeffectuncon @*/ /* FIX: ??? */
		fi->fd = ts->notify(fi->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
				fi->key, ts->notifyData);
		/*@=noeffectuncon @*/
		if (fi->fd != NULL) {
		    rpmRC rpmrc;

		    h = headerFree(h, "TR_ADDED install");

		    /*@-mustmod@*/	/* LCL: segfault */
		    rpmrc = rpmReadPackageFile(ts, fi->fd,
				"rpmRunTransactions", &h);
		    /*@=mustmod@*/

		    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE)) {
			/*@-noeffectuncon @*/ /* FIX: check rc */
			(void) ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0,
					fi->key, ts->notifyData);
			/*@=noeffectuncon @*/
			fi->fd = NULL;
			ourrc++;
		    } else if (fi->h != NULL) {
			Header foo = relocateFileList(ts, fi, h, NULL);
			h = headerFree(h, "TR_ADDED read free");
			h = headerLink(foo, "TR_ADDED relocate xfer");
			foo = headerFree(foo, "TR_ADDED relocate");
		    }
		    if (fi->fd != NULL) gotfd = 1;
		}
	    }
	    /*@=branchstate@*/

	    if (fi->fd != NULL) {
		Header hsave = NULL;

		if (fi->h) {
		    hsave = headerLink(fi->h, "TR_ADDED fi->h hsave");
		    fi->h = headerFree(fi->h, "TR_ADDED fi->h free");
		    fi->h = headerLink(h, "TR_ADDED fi->h link");
		} else {
char * fstates = fi->fstates;
fileAction * actions = fi->actions;
uint_32 multiLib = fi->multiLib;
const void * key = fi->key;
rpmRelocation * relocs = fi->relocs;
FD_t fd = fi->fd;

fi->fstates = NULL;
fi->actions = NULL;
fi->key = NULL;
fi->relocs = NULL;
fi->fd = NULL;
		    freeFi(fi);
oc = teGetOc(tei);
fi->magic = TFIMAGIC;
fi->type = ts->order[oc].type;
fi->record = 0;
		    loadFi(ts, fi, h, 1);
fi->fstates = _free(fi->fstates);
fi->fstates = fstates;
fi->actions = _free(fi->actions);
fi->actions = actions;
fi->multiLib = multiLib;
fi->key = key;
fi->relocs = relocs;
/*@-newreftrans@*/
/*@i@*/ fi->fd = fd;
/*@=newreftrans@*/

		}
		if (fi->multiLib)
		    ts->transFlags |= RPMTRANS_FLAG_MULTILIB;

		if (psmStage(psm, PSM_PKGINSTALL)) {
		    ourrc++;
		    lastKey = pkgKey;
		}
		fi->h = headerFree(fi->h, "TR_ADDED fi->h free");
		if (hsave) {
		    fi->h = headerLink(hsave, "TR_ADDED fi->h restore");
		    hsave = headerFree(hsave, "TR_ADDED hsave free");
		}
	    } else {
		ourrc++;
		lastKey = pkgKey;
	    }

	    h = headerFree(h, "TR_ADDED h free");

	    if (gotfd) {
		/*@-noeffectuncon @*/ /* FIX: check rc */
		(void)ts->notify(fi->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			fi->key, ts->notifyData);
		/*@=noeffectuncon @*/
		fi->fd = NULL;
	    }
	    freeFi(fi);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    rpmMessage(RPMMESS_DEBUG, "========== --- %s-%s-%s\n",
			fi->name, fi->version, fi->release);
	    oc = teGetOc(tei);
	    /* If install failed, then we shouldn't erase. */
	    if (ts->order[oc].u.removed.dependsOnKey != lastKey) {
		if (psmStage(psm, PSM_PKGERASE))
		    ourrc++;
	    }
	    freeFi(fi);
	    /*@switchbreak@*/ break;
	}
	xx = rpmdbSync(ts->rpmdb);
	(void) rpmfiUnlink(fi, "tsInstall");
	psm->fi = NULL;
    }
    /*@=branchstate@*/
    tei = teFreeIterator(tei);

    ts->flList = freeFl(ts, ts->flList);
    ts->flEntries = 0;

    psm->ts = rpmtsUnlink(psm->ts, "tsRun");

    /*@-nullstate@*/ /* FIX: ts->flList may be NULL */
    if (ourrc)
    	return -1;
    else
	return 0;
    /*@=nullstate@*/
}
