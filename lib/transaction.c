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
#include "lib/rpmfi_internal.h"	/* fi->replaced, fi->actions... */
#include "lib/rpmte_internal.h"	/* XXX te->h, te->fd, te->h */
#include "lib/rpmts_internal.h"
#include "lib/cpio.h"

#include "debug.h"


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
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpm_color_t oFColor, FColor;
    char * altNEVR = NULL;
    rpmps ps;

    altNEVR = headerGetNEVRA(otherHeader, NULL);

    ps = rpmtsProblems(ts);
    int isCfgFile;

    oFColor = rpmfiFColor(otherFi);
    oFColor &= tscolor;

    FColor = rpmfiFColor(fi);
    FColor &= tscolor;

    isCfgFile = ((rpmfiFFlags(otherFi) | rpmfiFFlags(fi)) & RPMFILE_CONFIG);

    if (XFA_SKIPPING(rpmfiFAction(fi)))
	return 0;

    if (rpmfiCompare(otherFi, fi)) {
	int rConflicts;

	rConflicts = !(beingRemoved || (rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEOLDFILES));
	/* Resolve file conflicts to prefer Elf64 (if not forced). */
	if (tscolor != 0 && FColor != 0 && FColor != oFColor) {
	    if (oFColor & prefcolor) {
		rpmfiSetFAction(fi, FA_SKIPCOLOR);
		rConflicts = 0;
	    } else if (FColor & prefcolor) {
		rpmfiSetFAction(fi, FA_CREATE);
		rConflicts = 0;
	    }
	}

	if (rConflicts) {
	    rpmpsAppend(ps, RPMPROB_FILE_CONFLICT,
			rpmteNEVRA(p), rpmteKey(p),
			rpmfiDN(fi), rpmfiBN(fi),
			altNEVR,
			0);
	}

	/* Save file identifier to mark as state REPLACED. */
	if ( !(isCfgFile || XFA_SKIPPING(rpmfiFAction(fi))) ) {
	    if (!beingRemoved)
		rpmfiAddReplaced(fi, rpmfiFX(fi),
				 headerGetInstance(otherHeader),
				 rpmfiFX(otherFi));
	}
    }

    /* Determine config file dispostion, skipping missing files (if any). */
    if (isCfgFile) {
	int skipMissing =
	    ((rpmtsFlags(ts) & RPMTRANS_FLAG_ALLFILES) ? 0 : 1);
	rpmFileAction action = rpmfiDecideFate(otherFi, fi, skipMissing);
	rpmfiSetFAction(fi, action);
    }
    rpmfiSetFReplacedSize(fi, rpmfiFSize(otherFi));

    ps = rpmpsFree(ps);
    altNEVR = _free(altNEVR);
    return 0;
}

/**
 */
/* XXX only ts->rpmdb modified */
static int handleRmvdInstalledFile(const rpmts ts, rpmfi fi,
				   const char * otherStates, int otherFileNum)
{
    if (otherStates[otherFileNum] == RPMFILE_STATE_NORMAL)
	rpmfiSetFAction(fi, FA_SKIP);
    return 0;
}

/**
 * Update disk space needs on each partition for this package's files.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(const rpmts ts, const rpmte p, rpmfi fi)
{
    rpm_loff_t fixupSize = 0;
    rpmps ps;
    const char * fn;
    int i, j;
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t prefcolor = rpmtsPrefColor(ts);

    ps = rpmtsProblems(ts);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((i = rpmfiNext(fi)) >= 0) {
	rpm_color_t oFColor, FColor;
	struct fingerPrint_s * fiFps;
	int otherPkgNum, otherFileNum;
	rpmfi otherFi;
	rpmfileAttrs FFlags;
	rpm_mode_t FMode;
	struct rpmffi_s * recs;
	int numRecs;

	if (XFA_SKIPPING(rpmfiFAction(fi)))
	    continue;

	fn = rpmfiFN(fi);
	fiFps = fi->fps + i;
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
	(void) rpmFpHashGetEntry(ts->ht, fiFps, &recs, &numRecs, NULL);

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
	for (j = 0; j < numRecs && recs[j].fi != fi; j++)
	    {};

	/* Find what the previous disposition of this file was. */
	otherFileNum = -1;			/* keep gcc quiet */
	otherFi = NULL;

	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    struct fingerPrint_s * otherFps;
	    int otherFc;

	    otherFi = recs[otherPkgNum].fi;
	    otherFileNum = recs[otherPkgNum].fileno;

	    /* Added packages need only look at other added packages. */
	    if (rpmteType(p) == TR_ADDED && rpmteType(otherFi->te) != TR_ADDED)
		continue;

	    otherFps = otherFi->fps;
	    otherFc = rpmfiFC(otherFi);

	    (void) rpmfiSetFX(otherFi, otherFileNum);

	    /* XXX Happens iff fingerprint for incomplete package install. */
	    if (rpmfiFAction(otherFi) != FA_UNKNOWN);
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
		if (rpmfiFAction(fi) != FA_UNKNOWN)
		    break;
		if (rpmfiConfigConflict(fi)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    action = (FFlags & RPMFILE_NOREPLACE) ?
			      FA_ALTNAME : FA_BACKUP;
		} else {
		    action = FA_CREATE;
		}
		rpmfiSetFAction(fi, action);
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
			if (!XFA_SKIPPING(rpmfiFAction(fi))) {
			    /* XXX static helpers are order dependent. Ick. */
			    if (strcmp(fn, "/usr/sbin/libgcc_post_upgrade")
			     && strcmp(fn, "/usr/sbin/glibc_post_upgrade"))
				rpmfiSetFAction(otherFi, FA_SKIPCOLOR);
			}
			rpmfiSetFAction(fi, FA_CREATE);
			rConflicts = 0;
		    } else
		    if (oFColor & prefcolor) {
			/* ... first file of preferred colour is installed ... */
			if (XFA_SKIPPING(rpmfiFAction(fi)))
			    rpmfiSetFAction(otherFi, FA_CREATE);
			rpmfiSetFAction(fi, FA_SKIPCOLOR);
			rConflicts = 0;
		    }
		    done = 1;
		}
		if (rConflicts) {
		    rpmpsAppend(ps, RPMPROB_NEW_FILE_CONFLICT,
			rpmteNEVRA(p), rpmteKey(p),
			fn, NULL,
			rpmteNEVRA(otherFi->te),
			0);
		}
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = rpmfiFSize(otherFi);

	    if (rpmfiConfigConflict(fi)) {
		/* Here is an overlapped  pre-existing config file. */
		rpmFileAction action;
		action = (FFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SKIP;
		rpmfiSetFAction(fi, action);
	    } else {
		if (!done)
		    rpmfiSetFAction(fi, FA_CREATE);
	    }
	  } break;

	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
assert(otherFi != NULL);
		/* Here is an overlapped added file we don't want to nuke. */
		if (rpmfiFAction(otherFi) != FA_ERASE) {
		    /* On updates, don't remove files. */
		    rpmfiSetFAction(fi, FA_SKIP);
		    break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		rpmfiSetFAction(otherFi, FA_SKIP);
	    }
	    if (XFA_SKIPPING(rpmfiFAction(fi)))
		break;
	    if (rpmfiFState(fi) != RPMFILE_STATE_NORMAL)
		break;
	    if (!(S_ISREG(FMode) && (FFlags & RPMFILE_CONFIG))) {
		rpmfiSetFAction(fi, FA_ERASE);
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
			rpmfiSetFAction(fi, FA_BACKUP);
			break;
		    }
		}
	    }
	    rpmfiSetFAction(fi, FA_ERASE);
	    break;
	}

	/* Update disk space info for a file. */
	rpmtsUpdateDSI(ts, fiFps->entry->dev, rpmfiFSize(fi),
		rpmfiFReplacedSize(fi), fixupSize, rpmfiFAction(fi));

    }
    ps = rpmpsFree(ps);
}

/**
 * Ensure that current package is newer than installed package.
 * @param ts		transaction set
 * @param p		current transaction element
 * @param h		installed header
 * @return		0 if not newer, 1 if okay
 */
static int ensureOlder(rpmts ts,
		const rpmte p, const Header h)
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
	rpmps ps = rpmtsProblems(ts);
	char * altNEVR = headerGetNEVRA(h, NULL);
	rpmpsAppend(ps, RPMPROB_OLDPACKAGE,
		rpmteNEVRA(p), rpmteKey(p),
		NULL, NULL,
		altNEVR,
		0);
	altNEVR = _free(altNEVR);
	ps = rpmpsFree(ps);
	rc = 1;
    } else
	rc = 0;

    return rc;
}

/**
 * Skip any files that do not match install policies.
 * @param ts		transaction set
 * @param fi		file info set
 */
static void skipFiles(const rpmts ts, rpmfi fi)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t FColor;
    int noConfigs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONFIGS);
    int noDocs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NODOCS);
    ARGV_t netsharedPaths = NULL;
    ARGV_t languages = NULL;
    const char * dn, * bn;
    size_t dnlen, bnlen;
    char * s;
    int * drc;
    char * dff;
    int dc;
    int i, j, ix;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	if (tmpPath && *tmpPath != '%') {
	    argvSplit(&netsharedPaths, tmpPath, ":");
	}
	tmpPath = _free(tmpPath);
    }

    s = rpmExpand("%{_install_langs}", NULL);
    if (!(s && *s != '%'))
	s = _free(s);
    if (s) {
	argvSplit(&languages, s, ":");	
	s = _free(s);
    } else
	languages = NULL;

    /* Compute directory refcount, skip directory if now empty. */
    dc = rpmfiDC(fi);
    drc = xcalloc(dc, sizeof(*drc));
    dff = xcalloc(dc, sizeof(*dff));

    fi = rpmfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
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
	if (XFA_SKIPPING(rpmfiFAction(fi))) {
	    drc[ix]--; dff[ix] = 1;
	    continue;
	}

	/* Ignore colored files not in our rainbow. */
	FColor = rpmfiFColor(fi);
	if (tscolor && FColor && !(tscolor & FColor)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfiSetFAction(fi, FA_SKIPCOLOR);
	    continue;
	}

	/*
	 * Skip net shared paths.
	 * Net shared paths are not relative to the current root (though
	 * they do need to take package relocations into account).
	 */
	for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
	    size_t len;

	    len = strlen(*nsp);
	    if (dnlen >= len) {
		if (strncmp(dn, *nsp, len))
		    continue;
		/* Only directories or complete file paths can be net shared */
		if (!(dn[len] == '/' || dn[len] == '\0'))
		    continue;
	    } else {
		if (len < (dnlen + bnlen))
		    continue;
		if (strncmp(dn, *nsp, dnlen))
		    continue;
		/* Insure that only the netsharedpath basename is compared. */
		if ((s = strchr((*nsp) + dnlen, '/')) != NULL && s[1] != '\0')
		    continue;
		if (strncmp(bn, (*nsp) + dnlen, bnlen))
		    continue;
		len = dnlen + bnlen;
		/* Only directories or complete file paths can be net shared */
		if (!((*nsp)[len] == '/' || (*nsp)[len] == '\0'))
		    continue;
	    }

	    break;
	}

	if (nsp && *nsp) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfiSetFAction(fi, FA_SKIPNETSHARED);
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	flangs = rpmfiFLangs(fi);
	if (languages != NULL && flangs != NULL) {
	    const char *l, *le;
	    char **lang;
	    for (lang = languages; *lang != NULL; lang++) {
		if (!strcmp(*lang, "all"))
		    break;
		for (l = flangs; *l != '\0'; l = le) {
		    for (le = l; *le != '\0' && *le != '|'; le++)
			{};
		    if ((le-l) > 0 && !strncmp(*lang, l, (le-l)))
			break;
		    if (*le == '|') le++;	/* skip over | */
		}
		if (*l != '\0')
		    break;
	    }
	    if (*lang == NULL) {
		drc[ix]--;	dff[ix] = 1;
		rpmfiSetFAction(fi, FA_SKIPNSTATE);
		continue;
	    }
	}

	/*
	 * Skip config files if requested.
	 */
	if (noConfigs && (rpmfiFFlags(fi) & RPMFILE_CONFIG)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfiSetFAction(fi, FA_SKIPNSTATE);
	    continue;
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (rpmfiFFlags(fi) & RPMFILE_DOC)) {
	    drc[ix]--;	dff[ix] = 1;
	    rpmfiSetFAction(fi, FA_SKIPNSTATE);
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
	dn = fi->dnl[j];	dnlen = strlen(dn) - 1;
	bn = dn + dnlen;	bnlen = 0;
	while (bn > dn && bn[-1] != '/') {
		bnlen++;
		dnlen--;
		bn--;
	}

	/* If explicitly included in the package, skip the directory. */
	fi = rpmfiInit(fi, 0);
	if (fi != NULL)		/* XXX lclint */
	while ((i = rpmfiNext(fi)) >= 0) {
	    const char * fdn, * fbn;
	    rpm_mode_t fFMode;

	    if (XFA_SKIPPING(rpmfiFAction(fi)))
		continue;

	    fFMode = rpmfiFMode(fi);

	    if (rpmfiWhatis(fFMode) != XDIR)
		continue;
	    fdn = rpmfiDN(fi);
	    if (strlen(fdn) != dnlen)
		continue;
	    if (strncmp(fdn, dn, dnlen))
		continue;
	    fbn = rpmfiBN(fi);
	    if (strlen(fbn) != bnlen)
		continue;
	    if (strncmp(fbn, bn, bnlen))
		continue;
	    rpmlog(RPMLOG_DEBUG, "excluding directory %s\n", dn);
	    rpmfiSetFAction(fi, FA_SKIPNSTATE);
	    break;
	}
    }

    if (netsharedPaths) argvFree(netsharedPaths);
    if (languages) argvFree(languages);
    free(drc);
    free(dff);
}

/**
 * Return transaction element's file info.
 * @todo Take a rpmfi refcount here.
 * @param tsi		transaction element iterator
 * @return		transaction element file info
 */
static
rpmfi rpmtsiFi(const rpmtsi tsi)
{
    rpmfi fi = NULL;

    if (tsi != NULL && tsi->ocsave != -1) {
	/* FIX: rpmte not opaque */
	rpmte te = rpmtsElement(tsi->ts, tsi->ocsave);
	if (te != NULL && (fi = te->fi) != NULL)
	    fi->te = te;
    }
    return fi;
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
rpmdbMatchIterator rpmFindBaseNamesInDB(rpmts ts)
{
    rpmtsi pi;  rpmte p;
    rpmfi fi;
    int fc=0;
    rpmdbMatchIterator mi;
    int i, xx;
    const char * baseName;

    /* get number of files in transaction */
    // XXX move to ts
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;   /* XXX can't happen */
	fc += rpmfiFC(fi);
    }
    pi = rpmtsiFree(pi);

    rpmStringSet baseNames = rpmStringSetCreate(fc, hashFunctionString, strcmp, NULL);

    mi = rpmdbInitIterator(rpmtsGetRdb(ts), RPMTAG_BASENAMES, NULL, 0);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;   /* XXX can't happen */
	rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_PROGRESS, rpmtsiOc(pi),
		    ts->orderCount);

	/* Gather all installed headers with matching basename's. */
	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0) {
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
void checkInstalledFiles(rpmts ts, fingerPrintCache fpc)
{
    rpmps ps;
    rpmte p;
    rpmfi otherFi=NULL;
    int j;
    int xx;
    unsigned int fileNum;
    const char * oldDir;

    rpmdbMatchIterator mi;
    Header h, newheader;

    int beingRemoved;

    rpmlog(RPMLOG_DEBUG, "computing file dispositions\n");

    mi = rpmFindBaseNamesInDB(ts);

    /* For all installed headers with matching basename's ... */
    if (mi == NULL)
	 return;

    if (rpmdbGetIteratorCount(mi) == 0) {
	mi = rpmdbFreeIterator(mi);
	return;
    }

    ps = rpmtsProblems(ts);

    /* Loop over all packages from the rpmdb */
    h = newheader = rpmdbNextIterator(mi);
    while (h != NULL) {
	headerGetFlags hgflags = HEADERGET_MINMEM;
	struct rpmtd_s bnames, dnames, dindexes, ostates;
	const char ** dirNames;
	const char ** fullBaseNames;
	uint32_t * fullDirIndexes;
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
	fullBaseNames = bnames.data;
	dirNames = dnames.data;
	fullDirIndexes = dindexes.data;

	oldDir = NULL;
	/* loop over all intersting files in that package */
	do {
	    int gotRecs;
	    struct rpmffi_s * recs;
	    int numRecs;
	    fileNum = rpmdbGetIteratorFileNum(mi);
	    /* lookup finger print for this file */
	    if ( dirNames[fullDirIndexes[fileNum]] == oldDir) {
	        /* directory is the same as last round */
	        fp.baseName = fullBaseNames[fileNum];
	    } else {
	        fp = fpLookup(fpc, dirNames[fullDirIndexes[fileNum]], fullBaseNames[fileNum], 1);
		oldDir = dirNames[fullDirIndexes[fileNum]];
	    }
	    /* search for files in the transaction with same finger print */
	    gotRecs = rpmFpHashGetEntry(ts->ht, &fp, &recs, &numRecs, NULL);

	    for (j=0; (j<numRecs)&&gotRecs; j++) {
	        p = recs[j].fi->te;

		/* Determine the fate of each file. */
		switch (rpmteType(p)) {
		case TR_ADDED:
		    if (!otherFi) {
		        otherFi = rpmfiNew(ts, h, RPMTAG_BASENAMES, 1);
		    }
		    rpmfiSetFX(recs[j].fi, recs[j].fileno);
		    rpmfiSetFX(otherFi, fileNum);
		    xx = handleInstInstalledFile(ts, p, recs[j].fi, h, otherFi, beingRemoved);
		    break;
		case TR_REMOVED:
		    if (!beingRemoved) {
		        rpmfiSetFX(recs[j].fi, recs[j].fileno);
		        xx = handleRmvdInstalledFile(ts, recs[j].fi,
						     ostates.data, fileNum);
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
 * Run pre/post transaction scripts for transaction set
 * param ts	Transaction set
 * param stag	RPMTAG_PRETRANS or RPMTAG_POSTTRANS
 * return	0 on success, -1 on error (invalid script tag)
 */
static int runTransScripts(rpmts ts, rpmTag stag) 
{
    rpmtsi pi; 
    rpmte p;
    rpmfi fi;
    rpmpsm psm;
    int xx;

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
    	rpmTag progtag = RPMTAG_NOT_FOUND;
	int havescript = 0;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	switch (stag) {
	case RPMTAG_PRETRANS:
	    havescript = fi->transscripts & RPMFI_HAVE_PRETRANS;
	    progtag = RPMTAG_PRETRANSPROG;
	    break;
	case RPMTAG_POSTTRANS:
	    havescript = fi->transscripts & RPMFI_HAVE_POSTTRANS;
	    progtag = RPMTAG_POSTTRANSPROG;
	    break;
	default:
	    /* programmer error, blow up */
	    assert(progtag != RPMTAG_NOT_FOUND);
	    break;
	}
	
    	/* If no pre/post-transaction script, then don't bother. */
	if (!havescript)
 	    continue;

    	if (rpmteOpen(p, ts)) {
	    p->fi = rpmfiFree(p->fi);
 	    fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, RPMFI_KEEPHEADER);
	    if (fi != NULL) {	/* XXX can't happen */
		if (stag == RPMTAG_PRETRANS) {
		    fi->te = p;
		    p->fi = fi;
		} else {
		    p->fi = fi;
		    p->fi->te = p;
		}
	    }
	    psm = rpmpsmNew(ts, p, p->fi);
	    xx = rpmpsmScriptStage(psm, stag, progtag);
	    psm = rpmpsmFree(psm);

	    rpmteClose(p, ts);
	}
    }
    pi = rpmtsiFree(pi);
    return 0;
}

/*
 * Transaction main loop: install and remove packages
 */
static int rpmtsProcess(rpmts ts)
{
    rpmalKey lastFailKey = (rpmalKey)-2;	/* erased packages have -1 */
    rpmtsi pi;	rpmte p;
    int rc = 0;

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmalKey pkgKey;
	rpmpsm psm;
	rpmfi fi;
	int async;
	rpmElementType tetype = rpmteType(p);
	rpmtsOpX op = (tetype == TR_ADDED) ? RPMTS_OP_INSTALL : RPMTS_OP_ERASE;

	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	psm = rpmpsmNew(ts, p, fi);
	async = (rpmtsiOc(pi) >= rpmtsUnorderedSuccessors(ts, -1) ? 1 : 0);
	rpmpsmSetAsync(psm, async);

	(void) rpmswEnter(rpmtsOp(ts, op), 0);
	switch (tetype) {
	case TR_ADDED:
	    pkgKey = rpmteAddedKey(p);
	    if (rpmteOpen(p, ts)) {
		/*
		 * XXX Sludge necessary to tranfer existing fstates/actions
		 * XXX around a recreated file info set.
		 */
		rpmpsmSetFI(psm, NULL);
		fi = rpmfiUpdateState(fi, ts, p);
		rpmpsmSetFI(psm, p->fi);

		if (rpmpsmStage(psm, PSM_PKGINSTALL)) {
		    rc++;
		    lastFailKey = pkgKey;
		}
		rpmteClose(p, ts);
		
	    } else {
		rc++;
		lastFailKey = pkgKey;
	    }
	    break;
	case TR_REMOVED:
	    /*
	     * XXX This has always been a hack, now mostly broken.
	     * If install failed, then we shouldn't erase.
	     */
	    if (rpmteDependsOnKey(p) != lastFailKey) {
		if (rpmpsmStage(psm, PSM_PKGERASE)) {
		    rc++;
		}
	    }
	    break;
	}
	(void) rpmswExit(rpmtsOp(ts, op), 0);
	(void) rpmdbSync(rpmtsGetRdb(ts));

/* FIX: psm->fi may be NULL */
	psm = rpmpsmFree(psm);

    }
    pi = rpmtsiFree(pi);
    return rc;
}

int rpmtsRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    int i;
    int rc = 0;
    int totalFileCount = 0;
    rpmfi fi;
    fingerPrintCache fpc;
    rpmps ps;
    rpmtsi pi;	rpmte p;
    int numAdded;
    int numRemoved;
    void * lock = NULL;
    int xx;

    /* XXX programmer error segfault avoidance. */
    if (rpmtsNElements(ts) <= 0)
	return -1;

    /* If we are in test mode, then there's no need for transaction lock. */
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)) {
	lock = rpmtsAcquireLock(ts);
	if (lock == NULL)
	    return -1;	/* XXX W2DO? */
    }

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

    ts->probs = rpmpsFree(ts->probs);
    ts->probs = rpmpsCreate();

    /* XXX Make sure the database is open RDWR for package install/erase. */
    {	int dbmode = (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)
		? O_RDONLY : (O_RDWR|O_CREAT);

	/* Open database RDWR for installing packages. */
	if (rpmtsOpenDB(ts, dbmode)) {
	    rpmtsFreeLock(lock);
	    return -1;	/* XXX W2DO? */
	}
    }

    ts->ignoreSet = ignoreSet;
    {	char * currDir = rpmGetCwd();
	rpmtsSetCurrDir(ts, currDir);
	currDir = _free(currDir);
    }

    (void) rpmtsSetChrootDone(ts, 0);

    {	rpm_tid_t tid = (rpm_tid_t) time(NULL);
	(void) rpmtsSetTid(ts, tid);
    }

    /* Get available space on mounted file systems. */
    xx = rpmtsInitDSI(ts);

    /* ===============================================
     * For packages being installed:
     * - verify package arch/os.
     * - verify package epoch:version-release is newer.
     * - count files.
     * For packages being removed:
     * - count files.
     */

    rpmlog(RPMLOG_DEBUG, "sanity checking %d elements\n", rpmtsNElements(ts));
    ps = rpmtsProblems(ts);
    /* The ordering doesn't matter here */
    pi = rpmtsiInit(ts);
    /* XXX Only added packages need be checked. */
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmdbMatchIterator mi;
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

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
		xx = ensureOlder(ts, p, h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEPKG)) {
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP,
				rpmteE(p));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP,
				rpmteV(p));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP,
				rpmteR(p));
	    if (tscolor) {
		xx = rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP,
				rpmteA(p));
		xx = rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP,
				rpmteO(p));
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

	/* Count no. of files (if any). */
	totalFileCount += fc;

    }
    pi = rpmtsiFree(pi);
    ps = rpmpsFree(ps);

    /* The ordering doesn't matter here */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	totalFileCount += fc;
    }
    pi = rpmtsiFree(pi);


    /* Run pre-transaction scripts, but only if there are no known
     * problems up to this point. */
    if (!((rpmtsFlags(ts) & (RPMTRANS_FLAG_BUILD_PROBS|RPMTRANS_FLAG_TEST))
     	  || (rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs))))) {
	rpmlog(RPMLOG_DEBUG, "running pre-transaction scripts\n");
	runTransScripts(ts, RPMTAG_PRETRANS);
    }

    /* ===============================================
     * Initialize transaction element file info for package:
     */

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
    rpmlog(RPMLOG_DEBUG, "computing %d file fingerprints\n", totalFileCount);

    numAdded = numRemoved = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	switch (rpmteType(p)) {
	case TR_ADDED:
	    numAdded++;
	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    if (fc > 0)
		skipFiles(ts, fi);
	    break;
	case TR_REMOVED:
	    numRemoved++;
	    break;
	}

	fi->fps = (fc > 0 ? xmalloc(fc * sizeof(*fi->fps)) : NULL);
    }
    pi = rpmtsiFree(pi);

    if (!rpmtsChrootDone(ts)) {
	const char * rootDir = rpmtsRootDir(ts);
	xx = chdir("/");
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/') {
	    /* opening db before chroot not optimal, see rhbz#103852 c#3 */
	    xx = rpmdbOpenAll(ts->rdb);
	    if (chroot(rootDir) == -1) {
		rpmlog(RPMLOG_ERR, _("Unable to change root directory: %m\n"));
		return -1;
	    }
	}
	(void) rpmtsSetChrootDone(ts, 1);
    }

    ts->ht = rpmFpHashCreate(totalFileCount * 2 + 1, fpHashFunction, fpEqual,
			     NULL, NULL);
    fpc = fpCacheCreate(totalFileCount + 10001);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int fc;

	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fc, fi->fps);
 	fi = rpmfiInit(fi, 0);
 	if (fi != NULL)		/* XXX lclint */
	while ((i = rpmfiNext(fi)) >= 0) {
	    struct rpmffi_s ffi;
	    ffi.fi = fi;
	    ffi.fileno = i;
	    if (XFA_SKIPPING(rpmfiFAction(fi)))
		continue;
	    rpmFpHashAddEntry(ts->ht, fi->fps + i, ffi);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);

    }
    pi = rpmtsiFree(pi);

    /* ===============================================
     * Check fingerprints if they contain symlinks
     */
    rpmFpHash newht = rpmFpHashCreate(totalFileCount * 2, fpHashFunction, fpEqual, NULL, NULL);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fi = rpmfiInit(fi, 0);
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	if (fi != NULL)		/* XXX lclint */
	while ((i = rpmfiNext(fi)) >= 0) {
	    if (XFA_SKIPPING(rpmfiFAction(fi)))
		continue;
	    fpLookupSubdir(ts->ht, newht, fpc, fi, i);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    pi = rpmtsiFree(pi);

    rpmFpHashFree(ts->ht);
    ts->ht = newht;

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_START, 6, ts->orderCount);

    /* check against files in the rpmdb */
    checkInstalledFiles(ts, fpc);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;   /* XXX can't happen */

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	/* check files in ts against each other and update disk space
	   needs on each partition for this package. */
	handleOverlappedFiles(ts, p, fi);

	/* Check added package has sufficient space on each partition used. */
	if (rpmteType(p) == TR_ADDED) {
	    rpmtsCheckDSIProblems(ts, p);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    pi = rpmtsiFree(pi);

    if (rpmtsChrootDone(ts)) {
	const char * rootDir = rpmtsRootDir(ts);
	const char * currDir = rpmtsCurrDir(ts);
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
	    xx = chroot(".");
	(void) rpmtsSetChrootDone(ts, 0);
	if (currDir != NULL)
	    xx = chdir(currDir);
    }

    rpmtsNotify(ts, NULL, RPMCALLBACK_TRANS_STOP, 6, ts->orderCount);

    /* ===============================================
     * Free unused memory as soon as possible.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	if (rpmfiFC(fi) == 0)
	    continue;
	fi->fps = _free(fi->fps);
    }
    pi = rpmtsiFree(pi);

    fpc = fpCacheFree(fpc);
    ts->ht = rpmFpHashFree(ts->ht);

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
    if ((rpmtsFlags(ts) & RPMTRANS_FLAG_BUILD_PROBS)
     || (rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs)))
       )
    {
	rpmtsFreeLock(lock);
	return ts->orderCount;
    }

    /* Actually install and remove packages */
    rc = rpmtsProcess(ts);

    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)) {
	rpmlog(RPMLOG_DEBUG, "running post-transaction scripts\n");
	runTransScripts(ts, RPMTAG_POSTTRANS);
    }

    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	matchpathcon_fini();
    }

    rpmtsFreeLock(lock);

    /* FIX: ts->flList may be NULL */
    if (rc)
    	return -1;
    else
	return 0;
}
