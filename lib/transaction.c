/** \ingroup rpmts
 * \file lib/transaction.c
 */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmMachineScore, rpmReadPackageFile */
#include <rpm/rpmmacro.h>	/* XXX for rpmExpand */
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmstring.h>

#include "lib/fprint.h"
#include "lib/misc.h"
#include "lib/rpmchroot.h"
#include "lib/rpmlock.h"
#include "lib/rpmfi_internal.h"	/* only internal apis */
#include "lib/rpmte_internal.h"	/* only internal apis */
#include "lib/rpmts_internal.h"
#include "rpmio/rpmhook.h"

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_STATVFS
#include <sys/statvfs.h>

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

struct diskspaceInfo_s {
    char * mntPoint;	/*!< File system mount point */
    dev_t dev;		/*!< File system device number. */
    int64_t bneeded;	/*!< No. of blocks needed. */
    int64_t ineeded;	/*!< No. of inodes needed. */
    int64_t bsize;	/*!< File system block size. */
    int64_t bavail;	/*!< No. of blocks available. */
    int64_t iavail;	/*!< No. of inodes available. */
    int64_t obneeded;	/*!< Bookkeeping to avoid duplicate reports */
    int64_t oineeded;	/*!< Bookkeeping to avoid duplicate reports */
};

/* Adjust for root only reserved space. On linux e2fs, this is 5%. */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

static int rpmtsInitDSI(const rpmts ts)
{
    if (rpmtsFilterFlags(ts) & RPMPROB_FILTER_DISKSPACE)
	return 0;
    ts->dsi = _free(ts->dsi);
    ts->dsi = xcalloc(1, sizeof(*ts->dsi));
    return 0;
}

static rpmDiskSpaceInfo rpmtsCreateDSI(const rpmts ts, dev_t dev,
				       const char * dirName, int count)
{
    rpmDiskSpaceInfo dsi;
    struct stat sb;
    char * resolved_path;
    char mntPoint[PATH_MAX];
    int rc;

#if STATFS_IN_SYS_STATVFS
    struct statvfs sfb;
    memset(&sfb, 0, sizeof(sfb));
    rc = statvfs(dirName, &sfb);
#else
    struct statfs sfb;
    memset(&sfb, 0, sizeof(sfb));
#  if STAT_STATFS4
/* This platform has the 4-argument version of the statfs call.  The last two
 * should be the size of struct statfs and 0, respectively.  The 0 is the
 * filesystem type, and is always 0 when statfs is called on a mounted
 * filesystem, as we're doing.
 */
    rc = statfs(dirName, &sfb, sizeof(sfb), 0);
#  else
    rc = statfs(dirName, &sfb);
#  endif
#endif
    if (rc)
	return NULL;

    rc = stat(dirName, &sb);
    if (rc)
	return NULL;
    if (sb.st_dev != dev)
	return NULL;

    ts->dsi = xrealloc(ts->dsi, (count + 2) * sizeof(*ts->dsi));
    dsi = ts->dsi + count;
    memset(dsi, 0, 2 * sizeof(*dsi));

    dsi->dev = sb.st_dev;
    dsi->bsize = sfb.f_bsize;
    if (!dsi->bsize)
	dsi->bsize = 512;       /* we need a bsize */
    dsi->bneeded = 0;
    dsi->ineeded = 0;
#ifdef STATFS_HAS_F_BAVAIL
    dsi->bavail = (sfb.f_flag & ST_RDONLY) ? 0 : sfb.f_bavail;
#else
/* FIXME: the statfs struct doesn't have a member to tell how many blocks are
 * available for non-superusers.  f_blocks - f_bfree is probably too big, but
 * it's about all we can do.
 */
    dsi->bavail = sfb.f_blocks - sfb.f_bfree;
#endif
    /* XXX Avoid FAT and other file systems that have not inodes. */
    /* XXX assigning negative value to unsigned type */
    dsi->iavail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
	? sfb.f_ffree : -1;

    /* Find mount point belonging to this device number */
    resolved_path = realpath(dirName, mntPoint);
    if (!resolved_path) {
	strncpy(mntPoint, dirName, PATH_MAX);
	mntPoint[PATH_MAX-1] = '\0';
    }
    char * end = NULL;
    while (end != mntPoint) {
	end = strrchr(mntPoint, '/');
	if (end == mntPoint) { /* reached "/" */
	    stat("/", &sb);
	    if (dsi->dev != sb.st_dev) {
		dsi->mntPoint = xstrdup(mntPoint);
	    } else {
		dsi->mntPoint = xstrdup("/");
	    }
	    break;
	} else if (end) {
	    *end = '\0';
	} else { /* dirName doesn't start with / - should not happen */
	    dsi->mntPoint = xstrdup(dirName);
	    break;
	}
	stat(mntPoint, &sb);
	if (dsi->dev != sb.st_dev) {
	    *end = '/';
	    dsi->mntPoint = xstrdup(mntPoint);
	    break;
	}
    }

    rpmlog(RPMLOG_DEBUG,
	   "0x%08x %8" PRId64 " %12" PRId64 " %12" PRId64" %s\n",
	   (unsigned) dsi->dev, dsi->bsize,
	   dsi->bavail, dsi->iavail,
	   dsi->mntPoint);
    return dsi;
}

static rpmDiskSpaceInfo rpmtsGetDSI(const rpmts ts, dev_t dev,
				    const char *dirName) {
    rpmDiskSpaceInfo dsi;
    dsi = ts->dsi;
    if (dsi) {
	while (dsi->bsize && dsi->dev != dev)
	    dsi++;
	if (dsi->bsize == 0) {
	    /* create new entry */
	    dsi = rpmtsCreateDSI(ts, dev, dirName, dsi - ts->dsi);
	}
    }
    return dsi;
}

static void rpmtsUpdateDSI(const rpmts ts, dev_t dev, const char *dirName,
		rpm_loff_t fileSize, rpm_loff_t prevSize, rpm_loff_t fixupSize,
		rpmFileAction action)
{
    int64_t bneeded;
    rpmDiskSpaceInfo dsi = rpmtsGetDSI(ts, dev, dirName);
    if (dsi == NULL)
	return;

    bneeded = BLOCK_ROUND(fileSize, dsi->bsize);

    switch (action) {
    case FA_BACKUP:
    case FA_SAVE:
    case FA_ALTNAME:
	dsi->ineeded++;
	dsi->bneeded += bneeded;
	break;

    /*
     * FIXME: If two packages share a file (same md5sum), and
     * that file is being replaced on disk, will dsi->bneeded get
     * adjusted twice? Quite probably!
     */
    case FA_CREATE:
	dsi->bneeded += bneeded;
	dsi->bneeded -= BLOCK_ROUND(prevSize, dsi->bsize);
	break;

    case FA_ERASE:
	dsi->ineeded--;
	dsi->bneeded -= bneeded;
	break;

    default:
	break;
    }

    if (fixupSize)
	dsi->bneeded -= BLOCK_ROUND(fixupSize, dsi->bsize);

    /* adjust bookkeeping when requirements shrink */
    if (dsi->bneeded < dsi->obneeded) dsi->obneeded = dsi->bneeded;
    if (dsi->ineeded < dsi->oineeded) dsi->oineeded = dsi->ineeded;
}

/* return DSI of the device the rpmdb lives on */
static rpmDiskSpaceInfo rpmtsDbDSI(const rpmts ts) {
    const char *dbhome = rpmdbHome(rpmtsGetRdb(ts));
    struct stat sb;
    int rc;

    rc = stat(dbhome, &sb);
    if (rc) {
	return NULL;
    }
    return rpmtsGetDSI(ts, sb.st_dev, dbhome);
}

/* Update DSI for changing size of the rpmdb */
static void rpmtsUpdateDSIrpmDBSize(const rpmte p,
				    rpmDiskSpaceInfo dsi) {
    rpm_loff_t headerSize;
    int64_t bneeded;

    /* XXX somehow we can end up here with bsize 0 (RhBug:671056) */
    if (dsi == NULL || dsi->bsize == 0) return;

    headerSize = rpmteHeaderSize(p);
    bneeded = BLOCK_ROUND(headerSize, dsi->bsize);
    /* REMOVE doesn't neccessarily shrink the database */
    if (rpmteType(p) == TR_ADDED) {
	/* guessing that db grows 4 times more than the header size */
	dsi->bneeded += (bneeded * 4);
    }
}


static void rpmtsCheckDSIProblems(const rpmts ts, const rpmte te)
{
    rpmDiskSpaceInfo dsi = ts->dsi;

    if (dsi == NULL || !dsi->bsize)
	return;
    if (rpmfiFC(rpmteFI(te)) <= 0)
	return;

    for (; dsi->bsize; dsi++) {

	if (dsi->bavail >= 0 && adj_fs_blocks(dsi->bneeded) > dsi->bavail) {
	    if (dsi->bneeded > dsi->obneeded) {
		rpmteAddProblem(te, RPMPROB_DISKSPACE, NULL, dsi->mntPoint,
		   (adj_fs_blocks(dsi->bneeded) - dsi->bavail) * dsi->bsize);
		dsi->obneeded = dsi->bneeded;
	    }
	}

	if (dsi->iavail >= 0 && adj_fs_blocks(dsi->ineeded) > dsi->iavail) {
	    if (dsi->ineeded > dsi->oineeded) {
		rpmteAddProblem(te, RPMPROB_DISKNODES, NULL, dsi->mntPoint,
			(adj_fs_blocks(dsi->ineeded) - dsi->iavail));
		dsi->oineeded = dsi->ineeded;
	    }
	}
    }
}

static void rpmtsFreeDSI(rpmts ts){
    rpmDiskSpaceInfo dsi;
    if (ts == NULL)
	return;
    dsi = ts->dsi;
    while (dsi && dsi->bsize != 0) {
	dsi->mntPoint = _free(dsi->mntPoint);
	dsi++;
    }

    ts->dsi = _free(ts->dsi);
}


/* Calculate total number of files involved in transaction */
static uint64_t countFiles(rpmts ts)
{
    uint64_t fc = 0;
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	fc += rpmfiFC(rpmteFI(p));
    pi = rpmtsiFree(pi);
    return fc;
}

/**
 * handleInstInstalledFiles.
 * @param ts		transaction set
 * @param p		current transaction element
 * @param fi		file info set
 * @param shared	shared file info
 * @param sharedCount	no. of shared elements
 * @param reportConflicts
 */
/* XXX only ts->{probs,rpmdb} modified */
static int handleInstInstalledFile(const rpmts ts, rpmte p, rpmfi fi,
				   Header otherHeader, rpmfi otherFi,
				   int beingRemoved)
{
    unsigned int fx = rpmfiFX(fi);
    rpmfs fs = rpmteGetFileStates(p);
    int isCfgFile = ((rpmfiFFlags(otherFi) | rpmfiFFlags(fi)) & RPMFILE_CONFIG);

    if (XFA_SKIPPING(rpmfsGetAction(fs, fx)))
	return 0;

    if (rpmfiCompare(otherFi, fi)) {
	rpm_color_t tscolor = rpmtsColor(ts);
	rpm_color_t prefcolor = rpmtsPrefColor(ts);
	rpm_color_t FColor = rpmfiFColor(fi) & tscolor;
	rpm_color_t oFColor = rpmfiFColor(otherFi) & tscolor;
	int rConflicts;

	rConflicts = !(beingRemoved || (rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEOLDFILES));
	/* Resolve file conflicts to prefer Elf64 (if not forced). */
	if (tscolor != 0 && FColor != 0 && oFColor != 0 && FColor != oFColor) {
	    if (oFColor & prefcolor) {
		rpmfsSetAction(fs, fx, FA_SKIPCOLOR);
		rConflicts = 0;
	    } else if (FColor & prefcolor) {
		rpmfsSetAction(fs, fx, FA_CREATE);
		rConflicts = 0;
	    }
	}

	if (rConflicts) {
	    char *altNEVR = headerGetAsString(otherHeader, RPMTAG_NEVRA);
	    rpmteAddProblem(p, RPMPROB_FILE_CONFLICT, altNEVR, rpmfiFN(fi),
			    headerGetInstance(otherHeader));
	    free(altNEVR);
	}

	/* Save file identifier to mark as state REPLACED. */
	if ( !(isCfgFile || XFA_SKIPPING(rpmfsGetAction(fs, fx))) ) {
	    if (!beingRemoved)
		rpmfsAddReplaced(rpmteGetFileStates(p), rpmfiFX(fi),
				 headerGetInstance(otherHeader),
				 rpmfiFX(otherFi));
	}
    }

    /* Determine config file dispostion, skipping missing files (if any). */
    if (isCfgFile) {
	int skipMissing = ((rpmtsFlags(ts) & RPMTRANS_FLAG_ALLFILES) ? 0 : 1);
	rpmFileAction action = rpmfiDecideFate(otherFi, fi, skipMissing);
	rpmfsSetAction(fs, fx, action);
    }
    rpmfiSetFReplacedSize(fi, rpmfiFSize(otherFi));

    return 0;
}

/**
 * Update disk space needs on each partition for this package's files.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(rpmts ts, rpmFpHash ht, rpmte p, rpmfi fi)
{
    rpm_loff_t fixupSize = 0;
    const char * fn;
    int i, j;
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpmfs fs = rpmteGetFileStates(p);
    rpmfs otherFs;

    fi = rpmfiInit(fi, 0);
    while ((i = rpmfiNext(fi)) >= 0) {
	rpm_color_t oFColor, FColor;
	struct fingerPrint_s * fiFps;
	int otherPkgNum, otherFileNum;
	rpmfi otherFi;
	rpmte otherTe;
	rpmfileAttrs FFlags;
	rpm_mode_t FMode;
	struct rpmffi_s * recs;
	int numRecs;

	if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
	    continue;

	fn = rpmfiFN(fi);
	fiFps = rpmfiFpsIndex(fi, i);
	FFlags = rpmfiFFlags(fi);
	FMode = rpmfiFMode(fi);
	FColor = rpmfiFColor(fi);
	FColor &= tscolor;

	fixupSize = 0;

	/*
	 * Retrieve all records that apply to this file. Note that the
	 * file info records were built in the same order as the packages
	 * will be installed and removed so the records for an overlapped
	 * files will be sorted in exactly the same order.
	 */
	(void) rpmFpHashGetEntry(ht, fiFps, &recs, &numRecs, NULL);

	/*
	 * If this package is being added, look only at other packages
	 * being added -- removed packages dance to a different tune.
	 *
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
	for (j = 0; j < numRecs && recs[j].p != p; j++)
	    {};

	/* Find what the previous disposition of this file was. */
	otherFileNum = -1;			/* keep gcc quiet */
	otherFi = NULL;
	otherTe = NULL;
	otherFs = NULL;

	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    otherTe = recs[otherPkgNum].p;
	    otherFi = rpmteFI(otherTe);
	    otherFileNum = recs[otherPkgNum].fileno;
	    otherFs = rpmteGetFileStates(otherTe);

	    /* Added packages need only look at other added packages. */
	    if (rpmteType(p) == TR_ADDED && rpmteType(otherTe) != TR_ADDED)
		continue;

	    (void) rpmfiSetFX(otherFi, otherFileNum);

	    /* XXX Happens iff fingerprint for incomplete package install. */
	    if (rpmfsGetAction(otherFs, otherFileNum) != FA_UNKNOWN)
		break;
	}

	oFColor = rpmfiFColor(otherFi);
	oFColor &= tscolor;

	switch (rpmteType(p)) {
	case TR_ADDED:
	  {
	    int reportConflicts =
		!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACENEWFILES);
	    int done = 0;

	    if (otherPkgNum < 0) {
		/* XXX is this test still necessary? */
		rpmFileAction action;
		if (rpmfsGetAction(fs, i) != FA_UNKNOWN)
		    break;
		if (rpmfiConfigConflict(fi)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    action = (FFlags & RPMFILE_NOREPLACE) ?
			      FA_ALTNAME : FA_BACKUP;
		} else {
		    action = FA_CREATE;
		}
		rpmfsSetAction(fs, i, action);
		break;
	    }

assert(otherFi != NULL);
	    /* Mark added overlapped non-identical files as a conflict. */
	    if (rpmfiCompare(otherFi, fi)) {
		int rConflicts;

		rConflicts = reportConflicts;
		/* Resolve file conflicts to prefer Elf64 (if not forced) ... */
		if (tscolor != 0 && FColor != 0 && oFColor != 0 && FColor != oFColor) {
		    if (FColor & prefcolor) {
			/* ... last file of preferred colour is installed ... */
			if (!XFA_SKIPPING(rpmfsGetAction(fs, i)))
			    rpmfsSetAction(otherFs, otherFileNum, FA_SKIPCOLOR);
			rpmfsSetAction(fs, i, FA_CREATE);
			rConflicts = 0;
		    } else
		    if (oFColor & prefcolor) {
			/* ... first file of preferred colour is installed ... */
			if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
			    rpmfsSetAction(otherFs, otherFileNum, FA_CREATE);
			rpmfsSetAction(fs, i, FA_SKIPCOLOR);
			rConflicts = 0;
		    }
		    done = 1;
		}
		if (rConflicts) {
		    rpmteAddProblem(p, RPMPROB_NEW_FILE_CONFLICT,
				    rpmteNEVRA(otherTe), fn, 0);
		}
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = rpmfiFSize(otherFi);

	    if (rpmfiConfigConflict(fi)) {
		/* Here is an overlapped  pre-existing config file. */
		rpmFileAction action;
		action = (FFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SKIP;
		rpmfsSetAction(fs, i, action);
	    } else {
		if (!done)
		    rpmfsSetAction(fs, i, FA_CREATE);
	    }
	  } break;

	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
		assert(otherFi != NULL);
                /* Here is an overlapped added file we don't want to nuke. */
		if (rpmfsGetAction(otherFs, otherFileNum) != FA_ERASE) {
		    /* On updates, don't remove files. */
		    rpmfsSetAction(fs, i, FA_SKIP);
		    break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		rpmfsSetAction(otherFs, otherFileNum, FA_SKIP);
	    }
	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		break;
	    if (rpmfiFState(fi) != RPMFILE_STATE_NORMAL)
		break;
	    if (!(S_ISREG(FMode) && (FFlags & RPMFILE_CONFIG))) {
		rpmfsSetAction(fs, i, FA_ERASE);
		break;
	    }
		
	    /* Here is a pre-existing modified config file that needs saving. */
	    {	int algo = 0;
		size_t diglen = 0;
		const unsigned char *digest;
		if ((digest = rpmfiFDigest(fi, &algo, &diglen))) {
		    unsigned char fdigest[diglen];
		    if (!rpmDoDigest(algo, fn, 0, fdigest, NULL) &&
			memcmp(digest, fdigest, diglen)) {
			rpmfsSetAction(fs, i, FA_BACKUP);
			break;
		    }
		}
	    }
	    rpmfsSetAction(fs, i, FA_ERASE);
	    break;
	}

	/* Update disk space info for a file. */
	rpmtsUpdateDSI(ts, fiFps->entry->dev, fiFps->entry->dirName,
		       rpmfiFSize(fi), rpmfiFReplacedSize(fi),
		       fixupSize, rpmfsGetAction(fs, i));

    }
}

/**
 * Ensure that current package is newer than installed package.
 * @param p		current transaction element
 * @param h		installed header
 * @param ps		problem set
 */
static void ensureOlder(const rpmte p, const Header h)
{
    rpmsenseFlags reqFlags = (RPMSENSE_LESS | RPMSENSE_EQUAL);
    rpmds req;

    req = rpmdsSingle(RPMTAG_REQUIRENAME, rpmteN(p), rpmteEVR(p), reqFlags);
    if (rpmdsNVRMatchesDep(h, req, _rpmds_nopromote) == 0) {
	char * altNEVR = headerGetAsString(h, RPMTAG_NEVRA);
	rpmteAddProblem(p, RPMPROB_OLDPACKAGE, altNEVR, NULL,
			headerGetInstance(h));
	free(altNEVR);
    }
    rpmdsFree(req);
}

/**
 * Check if the curent file in the file iterator is in the
 * netshardpath and though should be excluded.
 * @param ts            transaction set
 * @param fi            file info set
 * @returns pointer to matching path or NULL
 */
static char ** matchNetsharedpath(const rpmts ts, rpmfi fi)
{
    char ** nsp;
    const char * dn, * bn;
    size_t dnlen, bnlen;
    char * s;
    bn = rpmfiBN(fi);
    bnlen = strlen(bn);
    dn = rpmfiDN(fi);
    dnlen = strlen(dn);
    for (nsp = ts->netsharedPaths; nsp && *nsp; nsp++) {
	size_t len;

	len = strlen(*nsp);
	if (dnlen >= len) {
	    if (!rstreqn(dn, *nsp, len))
		continue;
	    /* Only directories or complete file paths can be net shared */
	    if (!(dn[len] == '/' || dn[len] == '\0'))
		continue;
	} else {
	    if (len < (dnlen + bnlen))
		continue;
	    if (!rstreqn(dn, *nsp, dnlen))
		continue;
	    /* Insure that only the netsharedpath basename is compared. */
	    if ((s = strchr((*nsp) + dnlen, '/')) != NULL && s[1] != '\0')
		continue;
	    if (!rstreqn(bn, (*nsp) + dnlen, bnlen))
		continue;
	    len = dnlen + bnlen;
	    /* Only directories or complete file paths can be net shared */
	    if (!((*nsp)[len] == '/' || (*nsp)[len] == '\0'))
		continue;
	}

	break;
    }
    return nsp;
}

static void skipEraseFiles(const rpmts ts, rpmte p)
{
    rpmfi fi = rpmteFI(p);
    rpmfs fs = rpmteGetFileStates(p);
    int i;
    char ** nsp;
    /*
     * Skip net shared paths.
     * Net shared paths are not relative to the current root (though
     * they do need to take package relocations into account).
     */
    fi = rpmfiInit(fi, 0);
    while ((i = rpmfiNext(fi)) >= 0)
    {
	nsp = matchNetsharedpath(ts, fi);
	if (nsp && *nsp) {
	    rpmfsSetAction(fs, i, FA_SKIPNETSHARED);
	}
    }
}


/**
 * Skip any files that do not match install policies.
 * @param ts		transaction set
 * @param fi		file info set
 */
static void skipInstallFiles(const rpmts ts, rpmte p)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t FColor;
    int noConfigs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONFIGS);
    int noDocs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NODOCS);
    int * drc;
    char * dff;
    int dc;
    int i, j, ix;
    rpmfi fi = rpmteFI(p);
    rpmfs fs = rpmteGetFileStates(p);

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    /* Compute directory refcount, skip directory if now empty. */
    dc = rpmfiDC(fi);
    drc = xcalloc(dc, sizeof(*drc));
    dff = xcalloc(dc, sizeof(*dff));

    fi = rpmfiInit(fi, 0);
    while ((i = rpmfiNext(fi)) >= 0) {
	char ** nsp;
	const char *flangs;

	ix = rpmfiDX(fi);
	drc[ix]++;

	/* Don't bother with skipped files */
	if (XFA_SKIPPING(rpmfsGetAction(fs, i))) {
	    drc[ix]--; dff[ix] = 1;
	    continue;
	}

	/* Ignore colored files not in our rainbow. */
	FColor = rpmfiFColor(fi);
	if (tscolor && FColor && !(tscolor & FColor)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfsSetAction(fs, i, FA_SKIPCOLOR);
	    continue;
	}

	/*
	 * Skip net shared paths.
	 * Net shared paths are not relative to the current root (though
	 * they do need to take package relocations into account).
	 */
	nsp = matchNetsharedpath(ts, fi);
	if (nsp && *nsp) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfsSetAction(fs, i, FA_SKIPNETSHARED);
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	flangs = (ts->installLangs != NULL) ? rpmfiFLangs(fi) : NULL;
	if (flangs != NULL && *flangs != '\0') {
	    const char *l, *le;
	    char **lang;
	    for (lang = ts->installLangs; *lang != NULL; lang++) {
		for (l = flangs; *l != '\0'; l = le) {
		    for (le = l; *le != '\0' && *le != '|'; le++)
			{};
		    if ((le-l) > 0 && rstreqn(*lang, l, (le-l)))
			break;
		    if (*le == '|') le++;	/* skip over | */
		}
		if (*l != '\0')
		    break;
	    }
	    if (*lang == NULL) {
		drc[ix]--;	dff[ix] = 1;
		rpmfsSetAction(fs, i, FA_SKIPNSTATE);
		continue;
	    }
	}

	/*
	 * Skip config files if requested.
	 */
	if (noConfigs && (rpmfiFFlags(fi) & RPMFILE_CONFIG)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfsSetAction(fs, i, FA_SKIPNSTATE);
	    continue;
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (rpmfiFFlags(fi) & RPMFILE_DOC)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfsSetAction(fs, i, FA_SKIPNSTATE);
	    continue;
	}
    }

    /* Skip (now empty) directories that had skipped files. */
    for (j = 0; j < dc; j++) {
	const char * dn, * bn;
	size_t dnlen, bnlen;

	if (drc[j]) continue;	/* dir still has files. */
	if (!dff[j]) continue;	/* dir was not emptied here. */
	
	/* Find parent directory and basename. */
	dn = rpmfiDNIndex(fi, j); dnlen = strlen(dn) - 1;
	bn = dn + dnlen;	bnlen = 0;
	while (bn > dn && bn[-1] != '/') {
		bnlen++;
		dnlen--;
		bn--;
	}

	/* If explicitly included in the package, skip the directory. */
	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0) {
	    const char * fdn, * fbn;
	    rpm_mode_t fFMode;

	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		continue;

	    fFMode = rpmfiFMode(fi);

	    if (rpmfiWhatis(fFMode) != XDIR)
		continue;
	    fdn = rpmfiDN(fi);
	    if (strlen(fdn) != dnlen)
		continue;
	    if (!rstreqn(fdn, dn, dnlen))
		continue;
	    fbn = rpmfiBN(fi);
	    if (strlen(fbn) != bnlen)
		continue;
	    if (!rstreqn(fbn, bn, bnlen))
		continue;
	    rpmlog(RPMLOG_DEBUG, "excluding directory %s\n", dn);
	    rpmfsSetAction(fs, i, FA_SKIPNSTATE);
	    break;
	}
    }

    free(drc);
    free(dff);
}

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE rpmStringSet
#define HTKEYTYPE const char *
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/* Get a rpmdbMatchIterator containing all files in
 * the rpmdb that share the basename with one from
 * the transaction.
 * @param ts		transaction set
 * @return		rpmdbMatchIterator sorted
			by (package, fileNum)
 */
static
rpmdbMatchIterator rpmFindBaseNamesInDB(rpmts ts, uint64_t fileCount)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpmtsi pi;  rpmte p;
    rpmfi fi;
    rpmdbMatchIterator mi;
    int xx;
    int oc = 0;
    const char * baseName;

    rpmStringSet baseNames = rpmStringSetCreate(fileCount, 
					hashFunctionString, strcmp, NULL);

    mi = rpmdbNewIterator(rpmtsGetRdb(ts), RPMDBI_BASENAMES);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_PROGRESS, oc++, tsmem->orderCount);

	/* Gather all installed headers with matching basename's. */
	fi = rpmfiInit(rpmteFI(p), 0);
	while (rpmfiNext(fi) >= 0) {
	    size_t keylen;
	    baseName = rpmfiBN(fi);
	    if (rpmStringSetHasEntry(baseNames, baseName))
		continue;

	    keylen = strlen(baseName);
	    if (keylen == 0)
		keylen++;	/* XXX "/" fixup. */
	    xx = rpmdbExtendIterator(mi, baseName, keylen);
	    rpmStringSetAddEntry(baseNames, baseName);
	 }
    }
    pi = rpmtsiFree(pi);
    rpmStringSetFree(baseNames);

    rpmdbSortIterator(mi);
    /* iterator is now sorted by (recnum, filenum) */
    return mi;
}

/* Check files in the transactions against the rpmdb
 * Lookup all files with the same basename in the rpmdb
 * and then check for matching finger prints
 * @param ts		transaction set
 * @param fpc		global finger print cache
 */
static
void checkInstalledFiles(rpmts ts, uint64_t fileCount, rpmFpHash ht, fingerPrintCache fpc)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpmte p;
    rpmfi fi;
    rpmfs fs;
    rpmfi otherFi=NULL;
    int j;
    int xx;
    unsigned int fileNum;
    const char * oldDir;

    rpmdbMatchIterator mi;
    Header h, newheader;

    int beingRemoved;

    rpmlog(RPMLOG_DEBUG, "computing file dispositions\n");

    mi = rpmFindBaseNamesInDB(ts, fileCount);

    /* For all installed headers with matching basename's ... */
    if (mi == NULL)
	 return;

    if (rpmdbGetIteratorCount(mi) == 0) {
	mi = rpmdbFreeIterator(mi);
	return;
    }

    /* Loop over all packages from the rpmdb */
    h = newheader = rpmdbNextIterator(mi);
    while (h != NULL) {
	headerGetFlags hgflags = HEADERGET_MINMEM;
	struct rpmtd_s bnames, dnames, dindexes, ostates;
	fingerPrint fp;
	unsigned int installedPkg;

	/* Is this package being removed? */
	installedPkg = rpmdbGetIteratorOffset(mi);
	beingRemoved = intHashHasEntry(tsmem->removedPackages, installedPkg);

	h = headerLink(h);
	headerGet(h, RPMTAG_BASENAMES, &bnames, hgflags);
	headerGet(h, RPMTAG_DIRNAMES, &dnames, hgflags);
	headerGet(h, RPMTAG_DIRINDEXES, &dindexes, hgflags);
	headerGet(h, RPMTAG_FILESTATES, &ostates, hgflags);

	oldDir = NULL;
	/* loop over all interesting files in that package */
	do {
	    int gotRecs;
	    struct rpmffi_s * recs;
	    int numRecs;
	    const char * dirName;
	    const char * baseName;

	    fileNum = rpmdbGetIteratorFileNum(mi);
	    rpmtdSetIndex(&bnames, fileNum);
	    rpmtdSetIndex(&dindexes, fileNum);
	    rpmtdSetIndex(&dnames, *rpmtdGetUint32(&dindexes));
	    rpmtdSetIndex(&ostates, fileNum);

	    dirName = rpmtdGetString(&dnames);
	    baseName = rpmtdGetString(&bnames);

	    /* lookup finger print for this file */
	    if ( dirName == oldDir) {
	        /* directory is the same as last round */
	        fp.baseName = baseName;
	    } else {
	        fp = fpLookup(fpc, dirName, baseName, 1);
		oldDir = dirName;
	    }
	    /* search for files in the transaction with same finger print */
	    gotRecs = rpmFpHashGetEntry(ht, &fp, &recs, &numRecs, NULL);

	    for (j=0; (j<numRecs)&&gotRecs; j++) {
	        p = recs[j].p;
		fi = rpmteFI(p);
		fs = rpmteGetFileStates(p);

		/* Determine the fate of each file. */
		switch (rpmteType(p)) {
		case TR_ADDED:
		    if (!otherFi) {
		        otherFi = rpmfiNew(ts, h, RPMTAG_BASENAMES, RPMFI_KEEPHEADER);
		    }
		    rpmfiSetFX(fi, recs[j].fileno);
		    rpmfiSetFX(otherFi, fileNum);
		    xx = handleInstInstalledFile(ts, p, fi, h, otherFi, beingRemoved);
		    break;
		case TR_REMOVED:
		    if (!beingRemoved) {
		        rpmfiSetFX(fi, recs[j].fileno);
			if (*rpmtdGetChar(&ostates) == RPMFILE_STATE_NORMAL)
			    rpmfsSetAction(fs, recs[j].fileno, FA_SKIP);
		    }
		    break;
		}
	    }

	    newheader = rpmdbNextIterator(mi);

	} while (newheader==h);

	otherFi = rpmfiFree(otherFi);
	rpmtdFreeData(&ostates);
	rpmtdFreeData(&bnames);
	rpmtdFreeData(&dnames);
	rpmtdFreeData(&dindexes);
	headerFree(h);
	h = newheader;
    }

    mi = rpmdbFreeIterator(mi);
}

#define badArch(_a) (rpmMachineScore(RPM_MACHTABLE_INSTARCH, (_a)) == 0)
#define badOs(_a) (rpmMachineScore(RPM_MACHTABLE_INSTOS, (_a)) == 0)

/*
 * For packages being installed:
 * - verify package arch/os.
 * - verify package epoch:version-release is newer.
 */
static rpmps checkProblems(rpmts ts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmprobFilterFlags probFilter = rpmtsFilterFlags(ts);
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;

    /* The ordering doesn't matter here */
    /* XXX Only added packages need be checked. */
    rpmlog(RPMLOG_DEBUG, "sanity checking %d elements\n", rpmtsNElements(ts));
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmdbMatchIterator mi;

	if (!(probFilter & RPMPROB_FILTER_IGNOREARCH) && badArch(rpmteA(p)))
	    rpmteAddProblem(p, RPMPROB_BADARCH, rpmteA(p), NULL, 0);

	if (!(probFilter & RPMPROB_FILTER_IGNOREOS) && badOs(rpmteO(p)))
	    rpmteAddProblem(p, RPMPROB_BADOS, rpmteO(p), NULL, 0);

	if (!(probFilter & RPMPROB_FILTER_OLDPACKAGE)) {
	    Header h;
	    mi = rpmtsInitIterator(ts, RPMDBI_NAME, rpmteN(p), 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL)
		ensureOlder(p, h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (!(probFilter & RPMPROB_FILTER_REPLACEPKG)) {
	    Header h;
	    mi = rpmtsInitIterator(ts, RPMDBI_NAME, rpmteN(p), 0);
	    rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP, rpmteE(p));
	    rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP, rpmteV(p));
	    rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP, rpmteR(p));
	    if (tscolor) {
		rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP, rpmteA(p));
		rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP, rpmteO(p));
	    }

	    if ((h = rpmdbNextIterator(mi)) != NULL) {
		rpmteAddProblem(p, RPMPROB_PKG_INSTALLED, NULL, NULL,
				headerGetInstance(h));
	    }
	    mi = rpmdbFreeIterator(mi);
	}
    }
    pi = rpmtsiFree(pi);
    return rpmtsProblems(ts);
}

/*
 * Run pre/post transaction scripts for transaction set
 * param ts	Transaction set
 * param goal	PKG_PRETRANS/PKG_POSTTRANS
 * return	0 on success
 */
static int runTransScripts(rpmts ts, pkgGoal goal) 
{
    rpmte p;
    rpmtsi pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmteProcess(p, goal);
    }
    pi = rpmtsiFree(pi);
    return 0; /* what to do about failures? */
}

static int rpmtsSetupCollections(rpmts ts)
{
    /* seenCollectionsPost and TEs are basically a key-value pair. each item in
     * seenCollectionsPost is a collection that has been seen from any package,
     * and the associated index in the TEs is the last transaction element
     * where that collection was seen. */
    ARGV_t seenCollectionsPost = NULL;
    rpmte *TEs = NULL;
    int numSeenPost = 0;

    /* seenCollectionsPre is a list of collections that have been seen from
     * only removed packages */
    ARGV_t seenCollectionsPre = NULL;
    int numSeenPre = 0;

    ARGV_const_t collname;
    int installing = 1;
    int i;

    rpmte p;
    rpmtsi pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	/* detect when we switch from installing to removing packages, and
	 * update the lastInCollectionAdd lists */
	if (installing && rpmteType(p) == TR_REMOVED) {
	    installing = 0;
	    for (i = 0; i < numSeenPost; i++) {
		rpmteAddToLastInCollectionAdd(TEs[i], seenCollectionsPost[i]);
	    }
	}

	rpmteSetupCollectionPlugins(p);

	for (collname = rpmteCollections(p); collname && *collname; collname++) {
	    /* figure out if we've seen this collection in post before */
	    for (i = 0; i < numSeenPost && strcmp(*collname, seenCollectionsPost[i]); i++) {
	    }
	    if (i < numSeenPost) {
		/* we've seen the collection, update the index */
		TEs[i] = p;
	    } else {
		/* haven't seen the collection yet, add it */
		argvAdd(&seenCollectionsPost, *collname);
		TEs = xrealloc(TEs, sizeof(*TEs) * (numSeenPost + 1));
		TEs[numSeenPost] = p;
		numSeenPost++;
	    }

	    /* figure out if we've seen this collection in pre remove before */
	    if (installing == 0) {
		for (i = 0; i < numSeenPre && strcmp(*collname, seenCollectionsPre[i]); i++) {
		}
		if (i >= numSeenPre) {
		    /* haven't seen this collection, add it */
		    rpmteAddToFirstInCollectionRemove(p, *collname);
		    argvAdd(&seenCollectionsPre, *collname);
		    numSeenPre++;
		}
	    }
	}
    }
    pi = rpmtsiFree(pi);

    /* we've looked at all the rpmte's, update the lastInCollectionAny lists */
    for (i = 0; i < numSeenPost; i++) {
	rpmteAddToLastInCollectionAny(TEs[i], seenCollectionsPost[i]);
	if (installing == 1) {
	    /* lastInCollectionAdd is only updated above if packages were
	     * removed. if nothing is removed in the transaction, we need to
	     * update that list here */
	    rpmteAddToLastInCollectionAdd(TEs[i], seenCollectionsPost[i]);
	}
    }

    argvFree(seenCollectionsPost);
    argvFree(seenCollectionsPre);
    _free(TEs);

    return 0;
}

/* Add fingerprint for each file not skipped. */
static void addFingerprints(rpmts ts, uint64_t fileCount, rpmFpHash ht, fingerPrintCache fpc)
{
    rpmtsi pi;
    rpmte p;
    rpmfi fi;
    int i;

    rpmFpHash symlinks = rpmFpHashCreate(fileCount/16+16, fpHashFunction, fpEqual, NULL, NULL);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	if ((fi = rpmteFI(p)) == NULL)
	    continue;	/* XXX can't happen */

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfiFpLookup(fi, fpc);
	/* collect symbolic links */
 	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0) {
	    struct rpmffi_s ffi;
	    char const *linktarget;
	    linktarget = rpmfiFLink(fi);
	    if (!(linktarget && *linktarget != '\0'))
		continue;
	    if (XFA_SKIPPING(rpmfsGetAction(rpmteGetFileStates(p), i)))
		continue;
	    ffi.p = p;
	    ffi.fileno = i;
	    rpmFpHashAddEntry(symlinks, rpmfiFpsIndex(fi, i), ffi);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), rpmfiFC(fi));

    }
    pi = rpmtsiFree(pi);

    /* ===============================================
     * Check fingerprints if they contain symlinks
     * and add them to the hash table
     */

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	fi = rpmfiInit(rpmteFI(p), 0);
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	while ((i = rpmfiNext(fi)) >= 0) {
	    if (XFA_SKIPPING(rpmfsGetAction(rpmteGetFileStates(p), i)))
		continue;
	    fpLookupSubdir(symlinks, ht, fpc, p, i);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    pi = rpmtsiFree(pi);

    rpmFpHashFree(symlinks);
}

static int rpmtsSetup(rpmts ts, rpmprobFilterFlags ignoreSet)
{
    rpm_tid_t tid = (rpm_tid_t) time(NULL);
    int dbmode = (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST) ?  O_RDONLY : (O_RDWR|O_CREAT);

    if (rpmtsFlags(ts) & RPMTRANS_FLAG_NOSCRIPTS)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransScripts | _noTransTriggers));
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERS)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransTriggers));

    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_JUSTDB | RPMTRANS_FLAG_TEST))
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransScripts | _noTransTriggers | RPMTRANS_FLAG_NOCOLLECTIONS));

    /* if SELinux isn't enabled, init fails or test run, don't bother... */
    if (!is_selinux_enabled() || (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)) {
        rpmtsSetFlags(ts, (rpmtsFlags(ts) | RPMTRANS_FLAG_NOCONTEXTS));
    }

    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	rpmtsSELabelInit(ts, selinux_file_context_path());
    }

    /* 
     * Make sure the database is open RDWR for package install/erase.
     * Note that we initialize chroot state here even if it's just "/" as
     * this ensures we can successfully perform open(".") which is
     * required to reliably restore cwd after Lua scripts.
     */ 
    if (rpmtsOpenDB(ts, dbmode) || rpmChrootSet(rpmtsRootDir(ts)))
	return -1;

    ts->ignoreSet = ignoreSet;
    (void) rpmtsSetTid(ts, tid);

    /* Get available space on mounted file systems. */
    (void) rpmtsInitDSI(ts);

    return 0;
}

static int rpmtsFinish(rpmts ts)
{
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	rpmtsSELabelFini(ts);
    }
    return rpmChrootSet(NULL);
}

static int rpmtsPrepare(rpmts ts)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpmtsi pi;
    rpmte p;
    rpmfi fi;
    int rc = 0;
    uint64_t fileCount = countFiles(ts);

    fingerPrintCache fpc = fpCacheCreate(fileCount/2 + 10001);
    rpmFpHash ht = rpmFpHashCreate(fileCount/2+1, fpHashFunction, fpEqual,
			     NULL, NULL);
    rpmDiskSpaceInfo dsi;

    rpmlog(RPMLOG_DEBUG, "computing %" PRIu64 " file fingerprints\n", fileCount);

    /* Skip netshared paths, not our i18n files, and excluded docs */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if (rpmfiFC(rpmteFI(p)) == 0)
	    continue;
	if (rpmteType(p) == TR_ADDED) {
	    skipInstallFiles(ts, p);
	} else {
	    skipEraseFiles(ts, p);
	}
    }
    pi = rpmtsiFree(pi);

    /* Open rpmdb & enter chroot for fingerprinting if necessary */
    if (rpmdbOpenAll(ts->rdb) || rpmChrootIn()) {
	rc = -1;
	goto exit;
    }
    
    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_START, 6, tsmem->orderCount);
    addFingerprints(ts, fileCount, ht, fpc);
    /* check against files in the rpmdb */
    checkInstalledFiles(ts, fileCount, ht, fpc);

    dsi = rpmtsDbDSI(ts);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmteFI(p)) == NULL)
	    continue;   /* XXX can't happen */

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	/* check files in ts against each other and update disk space
	   needs on each partition for this package. */
	handleOverlappedFiles(ts, ht, p, fi);

	rpmtsUpdateDSIrpmDBSize(p, dsi);

	/* Check added package has sufficient space on each partition used. */
	if (rpmteType(p) == TR_ADDED) {
	    rpmtsCheckDSIProblems(ts, p);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    pi = rpmtsiFree(pi);
    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_STOP, 6, tsmem->orderCount);

    /* return from chroot if done earlier */
    if (rpmChrootOut())
	rc = -1;

    /* File info sets, fp caches etc not needed beyond here, free 'em up. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmteSetFI(p, NULL);
    }
    pi = rpmtsiFree(pi);

exit:
    ht = rpmFpHashFree(ht);
    fpc = fpCacheFree(fpc);
    rpmtsFreeDSI(ts);
    return rc;
}

/*
 * Transaction main loop: install and remove packages
 */
static int rpmtsProcess(rpmts ts)
{
    rpmtsi pi;	rpmte p;
    int rc = 0;

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int failed;

	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	failed = rpmteProcess(p, rpmteType(p));
	if (failed) {
	    rpmlog(RPMLOG_ERR, "%s: %s %s\n", rpmteNEVRA(p),
		   rpmteTypeString(p), failed > 1 ? _("skipped") : _("failed"));
	    rc++;
	}
	(void) rpmdbSync(rpmtsGetRdb(ts));
    }
    pi = rpmtsiFree(pi);
    return rc;
}

int rpmtsRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    int rc = -1; /* assume failure */
    rpmlock lock = NULL;
    rpmps tsprobs = NULL;
    /* Force default 022 umask during transaction for consistent results */
    mode_t oldmask = umask(022);

    /* Empty transaction, nothing to do */
    if (rpmtsNElements(ts) <= 0) {
	rc = 0;
	goto exit;
    }

    /* If we are in test mode, then there's no need for transaction lock. */
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)) {
	if (!(lock = rpmtsAcquireLock(ts))) {
	    goto exit;
	}
    }

    /* Setup flags and such, open the DB */
    if (rpmtsSetup(ts, ignoreSet)) {
	goto exit;
    }

    rpmtsSetupCollections(ts);

    /* Check package set for problems */
    tsprobs = checkProblems(ts);

    /* Run pre-transaction scripts, but only if there are no known
     * problems up to this point and not disabled otherwise. */
    if (!((rpmtsFlags(ts) & (RPMTRANS_FLAG_BUILD_PROBS|RPMTRANS_FLAG_NOPRE))
     	  || (rpmpsNumProblems(tsprobs)))) {
	rpmlog(RPMLOG_DEBUG, "running pre-transaction scripts\n");
	runTransScripts(ts, PKG_PRETRANS);
    }
    tsprobs = rpmpsFree(tsprobs);

    /* Compute file disposition for each package in transaction set. */
    if (rpmtsPrepare(ts)) {
	goto exit;
    }
    /* Check again for problems (now including file conflicts,  duh */
    tsprobs = rpmtsProblems(ts);

     /* If unfiltered problems exist, free memory and return. */
    if ((rpmtsFlags(ts) & RPMTRANS_FLAG_BUILD_PROBS) || (rpmpsNumProblems(tsprobs))) {
	tsMembers tsmem = rpmtsMembers(ts);
	rc = tsmem->orderCount;
	goto exit;
    }

    /* Free up memory taken by problem sets */
    tsprobs = rpmpsFree(tsprobs);
    rpmtsCleanProblems(ts);

    /* Actually install and remove packages, get final exit code */
    rc = rpmtsProcess(ts) ? -1 : 0;

    /* Run post-transaction scripts unless disabled */
    if (!(rpmtsFlags(ts) & (RPMTRANS_FLAG_NOPOST))) {
	rpmlog(RPMLOG_DEBUG, "running post-transaction scripts\n");
	runTransScripts(ts, PKG_POSTTRANS);
    }

exit:
    /* Finish up... */
    (void) umask(oldmask);
    (void) rpmtsFinish(ts);
    tsprobs = rpmpsFree(tsprobs);
    rpmlockFree(lock);
    return rc;
}
