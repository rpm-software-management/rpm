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
#include <rpm/argv.h>

#include "lib/rpmdb_internal.h"	/* XXX for dbiIndexSetCount */
#include "lib/fprint.h"
#include "lib/psm.h"
#include "lib/rpmlock.h"
#include "lib/rpmfi_internal.h"	/* only internal apis */
#include "lib/rpmte_internal.h"	/* only internal apis */
#include "lib/rpmts_internal.h"
#include "lib/cpio.h"
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
    if (sb.st_dev != dev) // XXX WHY?
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
	if (end == mntPoint) { // reached "/"
	    stat("/", &sb);
	    if (dsi->dev != sb.st_dev) {
		dsi->mntPoint = xstrdup(mntPoint);
	    } else {
		dsi->mntPoint = xstrdup("/");
	    }
	    break;
	} else if (end) {
	    *end = '\0';
	} else { // dirName doesn't start with / - should not happen
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

static void rpmtsUpdateDSI(const rpmts ts, dev_t dev, const char *dirName,
		rpm_loff_t fileSize, rpm_loff_t prevSize, rpm_loff_t fixupSize,
		rpmFileAction action)
{
    rpmDiskSpaceInfo dsi;
    int64_t bneeded;

    dsi = ts->dsi;
    if (dsi) {
	while (dsi->bsize && dsi->dev != dev)
	    dsi++;
	if (dsi->bsize == 0) {
	    /* create new entry */
	    dsi = rpmtsCreateDSI(ts, dev, dirName, dsi - ts->dsi);
	}
    }
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

static void rpmtsCheckDSIProblems(const rpmts ts, const rpmte te)
{
    rpmDiskSpaceInfo dsi;
    rpmps ps;
    int fc;

    dsi = ts->dsi;
    if (dsi == NULL || !dsi->bsize)
	return;
    fc = rpmfiFC(rpmteFI(te));
    if (fc <= 0)
	return;

    ps = rpmtsProblems(ts);
    for (; dsi->bsize; dsi++) {

	if (dsi->bavail >= 0 && adj_fs_blocks(dsi->bneeded) > dsi->bavail) {
	    if (dsi->bneeded > dsi->obneeded) {
		rpmpsAppend(ps, RPMPROB_DISKSPACE,
			rpmteNEVRA(te), rpmteKey(te),
			dsi->mntPoint, NULL, NULL,
		   (adj_fs_blocks(dsi->bneeded) - dsi->bavail) * dsi->bsize);
		dsi->obneeded = dsi->bneeded;
	    }
	}

	if (dsi->iavail >= 0 && adj_fs_blocks(dsi->ineeded) > dsi->iavail) {
	    if (dsi->ineeded > dsi->oineeded) {
		rpmpsAppend(ps, RPMPROB_DISKNODES,
			rpmteNEVRA(te), rpmteKey(te),
			dsi->mntPoint, NULL, NULL,
			(adj_fs_blocks(dsi->ineeded) - dsi->iavail));
		dsi->oineeded = dsi->ineeded;
	    }
	}
    }
    ps = rpmpsFree(ps);
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


/**
 */
static int archOkay(const char * pkgArch)
{
    if (pkgArch == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch) ? 1 : 0);
}

/**
 */
static int osOkay(const char * pkgOs)
{
    if (pkgOs == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs) ? 1 : 0);
}

/* Calculate total number of files involved in transaction */
static uint64_t countFiles(rpmts ts)
{
    uint64_t fc = 0;
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;
    rpmfi fi;
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmteFI(p)) == NULL)
	    continue;   /* XXX can't happen */
	fc += rpmfiFC(fi);
    }
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
	if (tscolor != 0 && FColor != 0 && FColor != oFColor) {
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
	    rpmps ps = rpmtsProblems(ts);
	    rpmpsAppend(ps, RPMPROB_FILE_CONFLICT,
			rpmteNEVRA(p), rpmteKey(p),
			rpmfiDN(fi), rpmfiBN(fi),
			altNEVR,
			0);
	    free(altNEVR);
	    rpmpsFree(ps);
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
    rpmps ps;
    const char * fn;
    int i, j;
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpmfs fs = rpmteGetFileStates(p);
    rpmfs otherFs;

    ps = rpmtsProblems(ts);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
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
		if (tscolor != 0) {
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
		    rpmpsAppend(ps, RPMPROB_NEW_FILE_CONFLICT,
			rpmteNEVRA(p), rpmteKey(p),
			fn, NULL,
			rpmteNEVRA(otherTe),
			0);
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
	    {	pgpHashAlgo algo = 0;
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
    ps = rpmpsFree(ps);
}

/**
 * Ensure that current package is newer than installed package.
 * @param p		current transaction element
 * @param h		installed header
 * @param ps		problem set
 * @return		0 if not newer, 1 if okay
 */
static int ensureOlder(const rpmte p, const Header h, rpmps ps)
{
    rpmsenseFlags reqFlags = (RPMSENSE_LESS | RPMSENSE_EQUAL);
    rpmds req;
    int rc;

    if (p == NULL || h == NULL)
	return 1;

    req = rpmdsSingle(RPMTAG_REQUIRENAME, rpmteN(p), rpmteEVR(p), reqFlags);
    rc = rpmdsNVRMatchesDep(h, req, _rpmds_nopromote);
    req = rpmdsFree(req);

    if (rc == 0) {
	char * altNEVR = headerGetAsString(h, RPMTAG_NEVRA);
	rpmpsAppend(ps, RPMPROB_OLDPACKAGE,
		rpmteNEVRA(p), rpmteKey(p),
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
    const char * dn, * bn;
    size_t dnlen, bnlen;
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
    while ((i = rpmfiNext(fi)) >= 0)
    {
	char ** nsp;
	const char *flangs;

	bn = rpmfiBN(fi);
	bnlen = strlen(bn);
	ix = rpmfiDX(fi);
	dn = rpmfiDN(fi);
	dnlen = strlen(dn);
	if (dn == NULL)
	    continue;	/* XXX can't happen */

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
#ifndef	NOTYET
    if (fi != NULL)	/* XXX can't happen */
    for (j = 0; j < dc; j++)
#else
    if ((fi = rpmfiInitD(fi)) != NULL)
    while (j = rpmfiNextD(fi) >= 0)
#endif
    {

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
    rpmtsi pi;  rpmte p;
    rpmfi fi;
    rpmdbMatchIterator mi;
    int xx;
    const char * baseName;

    rpmStringSet baseNames = rpmStringSetCreate(fileCount, 
					hashFunctionString, strcmp, NULL);

    mi = rpmdbInitIterator(rpmtsGetRdb(ts), RPMTAG_BASENAMES, NULL, 0);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	if ((fi = rpmteFI(p)) == NULL)
	    continue;   /* XXX can't happen */
	rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_PROGRESS, rpmtsiOc(pi),
		    ts->orderCount);

	/* Gather all installed headers with matching basename's. */
	fi = rpmfiInit(fi, 0);
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
	beingRemoved = 0;
	if (ts->removedPackages != NULL)
	for (j = 0; j < ts->numRemovedPackages; j++) {
	    if (ts->removedPackages[j] != installedPkg)
	        continue;
	    beingRemoved = 1;
	    break;
	}

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

/*
 * For packages being installed:
 * - verify package arch/os.
 * - verify package epoch:version-release is newer.
 */
static rpmps checkProblems(rpmts ts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmps ps = rpmpsCreate();
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;

    /* The ordering doesn't matter here */
    /* XXX Only added packages need be checked. */
    rpmlog(RPMLOG_DEBUG, "sanity checking %d elements\n", rpmtsNElements(ts));
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmdbMatchIterator mi;
	rpmpsi psi;

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_IGNOREARCH))
	    if (!archOkay(rpmteA(p)))
		rpmpsAppend(ps, RPMPROB_BADARCH,
			rpmteNEVRA(p), rpmteKey(p),
			rpmteA(p), NULL,
			NULL, 0);

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_IGNOREOS))
	    if (!osOkay(rpmteO(p)))
		rpmpsAppend(ps, RPMPROB_BADOS,
			rpmteNEVRA(p), rpmteKey(p),
			rpmteO(p), NULL,
			NULL, 0);

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_OLDPACKAGE)) {
	    Header h;
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL)
		ensureOlder(p, h, ps);
	    mi = rpmdbFreeIterator(mi);
	}

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEPKG)) {
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
	    rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP, rpmteE(p));
	    rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP, rpmteV(p));
	    rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP, rpmteR(p));
	    if (tscolor) {
		rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP, rpmteA(p));
		rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP, rpmteO(p));
	    }

	    while (rpmdbNextIterator(mi) != NULL) {
		rpmpsAppend(ps, RPMPROB_PKG_INSTALLED,
			rpmteNEVRA(p), rpmteKey(p),
			NULL, NULL,
			NULL, 0);
		break;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	/* XXX rpmte problems can only be relocation problems atm */
	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_FORCERELOCATE)) {
	    psi = rpmpsInitIterator(rpmteProblems(p));
	    while (rpmpsNextIterator(psi) >= 0) {
		rpmpsAppendProblem(ps, rpmpsGetProblem(psi));
	    }
	    rpmpsFreeIterator(psi);
	}
    }
    pi = rpmtsiFree(pi);
    return ps;
}

/*
 * Run pre/post transaction scripts for transaction set
 * param ts	Transaction set
 * param stag	RPMTAG_PRETRANS or RPMTAG_POSTTRANS
 * return	0 on success, -1 on error (invalid script tag)
 */
static int runTransScripts(rpmts ts, rpmTag stag) 
{
    rpmtsi pi; 
    rpmte p;
    rpmpsm psm;
    rpmTag progtag = RPMTAG_NOT_FOUND;
    int xx;

    if (stag == RPMTAG_PRETRANS) {
	progtag = RPMTAG_PRETRANSPROG;
    } else if (stag == RPMTAG_POSTTRANS) {
	progtag = RPMTAG_POSTTRANSPROG;
    } else {
	return -1;
    }

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	/* Skip failed elements & those without pre/posttrans */
	if (!rpmteHaveTransScript(p, stag) || rpmteFailed(p))
	    continue;

    	if (rpmteOpen(p, ts, 0)) {
	    psm = rpmpsmNew(ts, p);
	    xx = rpmpsmScriptStage(psm, stag, progtag);
	    psm = rpmpsmFree(psm);
	    rpmteClose(p, ts, 0);
	}
    }
    pi = rpmtsiFree(pi);
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

	if ((fi = rpmteFI(p)) == NULL)
	    continue;	/* XXX can't happen */
	fi = rpmfiInit(fi, 0);
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

    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransScripts | _noTransTriggers));

    /* if SELinux isn't enabled or init fails, don't bother... */
    if (!rpmtsSELinuxEnabled(ts)) {
        rpmtsSetFlags(ts, (rpmtsFlags(ts) | RPMTRANS_FLAG_NOCONTEXTS));
    }

    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	char *fn = rpmGetPath("%{?_install_file_context_path}", NULL);
	if (matchpathcon_init(fn) == -1) {
	    rpmtsSetFlags(ts, (rpmtsFlags(ts) | RPMTRANS_FLAG_NOCONTEXTS));
	}
	free(fn);
    }

    /* XXX Make sure the database is open RDWR for package install/erase. */
    if (rpmtsOpenDB(ts, dbmode)) {
	return -1;	/* XXX W2DO? */
    }

    ts->ignoreSet = ignoreSet;
    ts->probs = rpmpsFree(ts->probs);

    {	char * currDir = rpmGetCwd();
	rpmtsSetCurrDir(ts, currDir);
	currDir = _free(currDir);
    }

    (void) rpmtsSetChrootDone(ts, 0);
    (void) rpmtsSetTid(ts, tid);

    /* Get available space on mounted file systems. */
    (void) rpmtsInitDSI(ts);

    return 0;
}

static int rpmtsFinish(rpmts ts)
{
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	matchpathcon_fini();
    }
    return 0;
}

static int rpmtsPrepare(rpmts ts)
{
    rpmtsi pi;
    rpmte p;
    rpmfi fi;
    int xx, rc = 0;
    uint64_t fileCount = countFiles(ts);
    const char * rootDir = rpmtsRootDir(ts);
    int dochroot = (rootDir != NULL && !rstreq(rootDir, "/") && *rootDir == '/');

    fingerPrintCache fpc = fpCacheCreate(fileCount/2 + 10001);
    rpmFpHash ht = rpmFpHashCreate(fileCount/2+1, fpHashFunction, fpEqual,
			     NULL, NULL);

    rpmlog(RPMLOG_DEBUG, "computing %" PRIu64 " file fingerprints\n", fileCount);

    /* Skip netshared paths, not our i18n files, and excluded docs */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmteFI(p)) == NULL)
	    continue;	/* XXX can't happen */

	if (rpmfiFC(fi) == 0)
	    continue;
	if (rpmteType(p) == TR_ADDED) {
	    skipInstallFiles(ts, p);
	} else {
	    skipEraseFiles(ts, p);
	}
    }
    pi = rpmtsiFree(pi);

    /* Enter chroot for fingerprinting if necessary */
    if (!rpmtsChrootDone(ts)) {
	xx = chdir("/");
	if (dochroot) {
	    /* opening db before chroot not optimal, see rhbz#103852 c#3 */
	    xx = rpmdbOpenAll(ts->rdb);
	    if (chroot(rootDir) == -1) {
		rpmlog(RPMLOG_ERR, _("Unable to change root directory: %m\n"));
		rc = -1;
		goto exit;
	    }
	}
	(void) rpmtsSetChrootDone(ts, 1);
    }
    
    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_START, 6, ts->orderCount);
    addFingerprints(ts, fileCount, ht, fpc);
    /* check against files in the rpmdb */
    checkInstalledFiles(ts, fileCount, ht, fpc);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmteFI(p)) == NULL)
	    continue;   /* XXX can't happen */

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	/* check files in ts against each other and update disk space
	   needs on each partition for this package. */
	handleOverlappedFiles(ts, ht, p, fi);

	/* Check added package has sufficient space on each partition used. */
	if (rpmteType(p) == TR_ADDED) {
	    rpmtsCheckDSIProblems(ts, p);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    pi = rpmtsiFree(pi);
    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_STOP, 6, ts->orderCount);

    /* return from chroot if done earlier */
    if (rpmtsChrootDone(ts)) {
	const char * currDir = rpmtsCurrDir(ts);
	if (dochroot)
	    xx = chroot(".");
	(void) rpmtsSetChrootDone(ts, 0);
	if (currDir != NULL)
	    xx = chdir(currDir);
    }

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
	int failed = 1;
	rpmElementType tetype = rpmteType(p);
	rpmtsOpX op = (tetype == TR_ADDED) ? RPMTS_OP_INSTALL : RPMTS_OP_ERASE;

	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	if (rpmteFailed(p)) {
	    /* XXX this should be a warning, need a better message though */
	    rpmlog(RPMLOG_DEBUG, "element %s marked as failed, skipping\n",
					rpmteNEVRA(p));
	    rc++;
	    continue;
	}
	
	if (rpmteOpen(p, ts, 1)) {
	    rpmpsm psm = NULL;
	    pkgStage stage = PSM_UNKNOWN;

	    switch (tetype) {
	    case TR_ADDED:
		stage = PSM_PKGINSTALL;
		break;
	    case TR_REMOVED:
		stage = PSM_PKGERASE;
		break;
	    }
	    psm = rpmpsmNew(ts, p);
	    (void) rpmswEnter(rpmtsOp(ts, op), 0);
	    failed = rpmpsmStage(psm, stage);
	    (void) rpmswExit(rpmtsOp(ts, op), 0);
	    psm = rpmpsmFree(psm);
	    rpmteClose(p, ts, 1);
	}
	if (failed) {
	    rpmteMarkFailed(p, ts);
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
    void * lock = NULL;

    /* XXX programmer error segfault avoidance. */
    if (rpmtsNElements(ts) <= 0) {
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

    /* Check package set for problems */
    ts->probs = checkProblems(ts);

    /* Run pre-transaction scripts, but only if there are no known
     * problems up to this point and not disabled otherwise. */
    if (!((rpmtsFlags(ts) & (RPMTRANS_FLAG_BUILD_PROBS|RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_NOPRE))
     	  || (rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs))))) {
	rpmlog(RPMLOG_DEBUG, "running pre-transaction scripts\n");
	runTransScripts(ts, RPMTAG_PRETRANS);
    }

    /* Compute file disposition for each package in transaction set. */
    if (rpmtsPrepare(ts)) {
	goto exit;
    }

     /* If unfiltered problems exist, free memory and return. */
    if ((rpmtsFlags(ts) & RPMTRANS_FLAG_BUILD_PROBS) ||
		(rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs)))) {
	rc = ts->orderCount;
	goto exit;
    }

    /* Actually install and remove packages, get final exit code */
    rc = rpmtsProcess(ts) ? -1 : 0;

    /* Run post-transaction scripts unless disabled */
    if (!(rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_NOPOST))) {
	rpmlog(RPMLOG_DEBUG, "running post-transaction scripts\n");
	runTransScripts(ts, RPMTAG_POSTTRANS);
    }

    /* Finish up... */
    (void) rpmtsFinish(ts);

exit:
    rpmtsFreeLock(lock);
    return rc;
}
