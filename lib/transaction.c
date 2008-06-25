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
 */
static int sharedCmp(const void * one, const void * two)
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
 * handleInstInstalledFiles.
 * @param ts		transaction set
 * @param p		current transaction element
 * @param fi		file info set
 * @param shared	shared file info
 * @param sharedCount	no. of shared elements
 * @param reportConflicts
 */
/* XXX only ts->{probs,rpmdb} modified */
static int handleInstInstalledFiles(const rpmts ts,
		rpmte p, rpmfi fi,
		sharedFileInfo shared,
		int sharedCount, int reportConflicts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpm_color_t otecolor, tecolor;
    rpm_color_t oFColor, FColor;
    char * altNEVR = NULL;
    rpmfi otherFi = NULL;
    int numReplaced = 0;
    rpmps ps;
    int i;

    {	rpmdbMatchIterator mi;
	Header h;
	int scareMem = 0;

	mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    altNEVR = headerGetNEVRA(h, NULL);
	    otherFi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
	    break;
	}
	mi = rpmdbFreeIterator(mi);
    }

    /* Compute package color. */
    tecolor = rpmteColor(p);
    tecolor &= tscolor;

    /* Compute other pkg color. */
    otecolor = 0;
    otherFi = rpmfiInit(otherFi, 0);
    if (otherFi != NULL)
    while (rpmfiNext(otherFi) >= 0)
	otecolor |= rpmfiFColor(otherFi);
    otecolor &= tscolor;

    if (otherFi == NULL)
	return 1;

    fi->replaced = xcalloc(sharedCount, sizeof(*fi->replaced));

    ps = rpmtsProblems(ts);
    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	int isCfgFile;

	otherFileNum = shared->otherFileNum;
	(void) rpmfiSetFX(otherFi, otherFileNum);
	oFColor = rpmfiFColor(otherFi);
	oFColor &= tscolor;

	fileNum = shared->pkgFileNum;
	(void) rpmfiSetFX(fi, fileNum);
	FColor = rpmfiFColor(fi);
	FColor &= tscolor;

	isCfgFile = ((rpmfiFFlags(otherFi) | rpmfiFFlags(fi)) & RPMFILE_CONFIG);

#ifdef	DYING
	/* XXX another tedious segfault, assume file state normal. */
	if (otherStates && otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;
#endif

	if (XFA_SKIPPING(fi->actions[fileNum]))
	    continue;

	if (!(fi->mapflags & CPIO_SBIT_CHECK)) {
	    rpm_mode_t omode = rpmfiFMode(otherFi);
	    if (S_ISREG(omode) && (omode & 06000) != 0) {
		fi->mapflags |= CPIO_SBIT_CHECK;
	    }
	}

	if (rpmfiCompare(otherFi, fi)) {
	    int rConflicts;

	    rConflicts = reportConflicts;
	    /* Resolve file conflicts to prefer Elf64 (if not forced). */
	    if (tscolor != 0 && FColor != 0 && FColor != oFColor)
	    {
		if (oFColor & prefcolor) {
		    fi->actions[fileNum] = FA_SKIPCOLOR;
		    rConflicts = 0;
		} else
		if (FColor & prefcolor) {
		    fi->actions[fileNum] = FA_CREATE;
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
	    if ( !(isCfgFile || XFA_SKIPPING(fi->actions[fileNum])) ) {
		/* FIX: p->replaced, not fi */
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
	    }
	}

	/* Determine config file dispostion, skipping missing files (if any). */
	if (isCfgFile) {
	    int skipMissing =
		((rpmtsFlags(ts) & RPMTRANS_FLAG_ALLFILES) ? 0 : 1);
	    rpmFileAction action = rpmfiDecideFate(otherFi, fi, skipMissing);
	    fi->actions[fileNum] = action;
	}
	/* XXX watch out, replacedSizes is not rpm_loff_t (yet) */
	fi->replacedSizes[fileNum] = (rpm_off_t) rpmfiFSize(otherFi);
    }
    ps = rpmpsFree(ps);

    altNEVR = _free(altNEVR);
    otherFi = rpmfiFree(otherFi);

    fi->replaced = xrealloc(fi->replaced,	/* XXX memory leak */
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}

/**
 */
/* XXX only ts->rpmdb modified */
static int handleRmvdInstalledFiles(const rpmts ts, rpmfi fi,
		sharedFileInfo shared, int sharedCount)
{
    Header h;
    struct rpmtd_s ostates;
    const char * otherStates;

    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    (void) headerGet(h, RPMTAG_FILESTATES, &ostates, HEADERGET_MINMEM);
    otherStates = ostates.data;

    for (int i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	fi->actions[fileNum] = FA_SKIP;
    }

    rpmtdFreeData(&ostates);
    mi = rpmdbFreeIterator(mi);

    return 0;
}

#define	ISROOT(_d)	(((_d)[0] == '/' && (_d)[1] == '\0') ? "" : (_d))

int _fps_debug = 0;

static int fpsCompare (const void * one, const void * two)
{
    const struct fingerPrint_s * a = (const struct fingerPrint_s *)one;
    const struct fingerPrint_s * b = (const struct fingerPrint_s *)two;
    size_t adnlen = strlen(a->entry->dirName);
    size_t asnlen = (a->subDir ? strlen(a->subDir) : 0);
    size_t abnlen = strlen(a->baseName);
    size_t bdnlen = strlen(b->entry->dirName);
    size_t bsnlen = (b->subDir ? strlen(b->subDir) : 0);
    size_t bbnlen = strlen(b->baseName);
    char *afn = NULL, *bfn = NULL;
    int rc = 0;

    if (adnlen == 1 && asnlen != 0) adnlen = 0;
    if (bdnlen == 1 && bsnlen != 0) bdnlen = 0;

    if (adnlen) rstrcat(&afn, a->entry->dirName);
    rstrcat(&afn, "/");
    if (a->subDir && asnlen) rstrcat(&afn, a->subDir);
    if (abnlen) rstrcat(&afn, a->baseName);
    if (afn[0] == '/' && afn[1] == '/') {
	memmove(afn, afn+1, strlen(afn+1)+1);
    }

    if (bdnlen) rstrcat(&bfn, b->entry->dirName);
    rstrcat(&bfn, "/");
    if (b->subDir && bsnlen) rstrcat(&bfn, b->subDir);
    if (bbnlen) rstrcat(&bfn, b->baseName);
    if (bfn[0] == '/' && bfn[1] == '/') {
	memmove(bfn, bfn+1, strlen(bfn+1)+1);
    }

    rc = strcmp(afn, bfn);
if (_fps_debug)
fprintf(stderr, "\trc(%d) = strcmp(\"%s\", \"%s\")\n", rc, afn, bfn);

if (_fps_debug)
fprintf(stderr, "\t%s/%s%s\trc %d\n",
ISROOT(b->entry->dirName),
(b->subDir ? b->subDir : ""),
b->baseName,
rc
);
    free(afn);
    free(bfn);

    return rc;
}

static int _linear_fps_search = 0;

static int findFps(const struct fingerPrint_s * fiFps,
		const struct fingerPrint_s * otherFps,
		int otherFc)
{
    int otherFileNum;

if (_fps_debug)
fprintf(stderr, "==> %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);

  if (_linear_fps_search) {

linear:
    for (otherFileNum = 0; otherFileNum < otherFc; otherFileNum++, otherFps++) {

if (_fps_debug)
fprintf(stderr, "\t%4d %s/%s%s\n", otherFileNum,
ISROOT(otherFps->entry->dirName),
(otherFps->subDir ? otherFps->subDir : ""),
otherFps->baseName);

	/* If the addresses are the same, so are the values. */
	if (fiFps == otherFps)
	    break;

	/* Otherwise, compare fingerprints by value. */
	if (FP_EQUAL((*fiFps), (*otherFps)))
	    break;
    }

if (otherFileNum == otherFc) {
if (_fps_debug)
fprintf(stderr, "*** FP_EQUAL NULL %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);
}

    return otherFileNum;

  } else {

    const struct fingerPrint_s * bingoFps;

    bingoFps = bsearch(fiFps, otherFps, otherFc, sizeof(*otherFps), fpsCompare);
    if (bingoFps == NULL) {
if (_fps_debug)
fprintf(stderr, "*** bingoFps NULL %s/%s%s\n",
ISROOT(fiFps->entry->dirName),
(fiFps->subDir ? fiFps->subDir : ""),
fiFps->baseName);
	goto linear;
    }

    /* If the addresses are the same, so are the values. */
   	/* LCL: looks good to me */
    if (!(fiFps == bingoFps || FP_EQUAL((*fiFps), (*bingoFps)))) {
if (_fps_debug)
fprintf(stderr, "***  BAD %s/%s%s\n",
ISROOT(bingoFps->entry->dirName),
(bingoFps->subDir ? bingoFps->subDir : ""),
bingoFps->baseName);
	goto linear;
    }

    otherFileNum = (bingoFps != NULL ? (bingoFps - otherFps) : 0);

  }

    return otherFileNum;
}

/**
 * Update disk space needs on each partition for this package's files.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(const rpmts ts,
		const rpmte p, rpmfi fi)
{
    rpm_loff_t fixupSize = 0;
    rpmps ps;
    const char * fn;
    int i, j;

    ps = rpmtsProblems(ts);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((i = rpmfiNext(fi)) >= 0) {
	rpm_color_t tscolor = rpmtsColor(ts);
	rpm_color_t prefcolor = rpmtsPrefColor(ts);
	rpm_color_t oFColor, FColor;
	struct fingerPrint_s * fiFps;
	int otherPkgNum, otherFileNum;
	rpmfi otherFi;
	rpmfileAttrs FFlags;
	rpm_mode_t FMode;
	const rpmfi * recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
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
	(void) htGetEntry(ts->ht, fiFps,
			(const void ***) &recs, &numRecs, NULL);

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
	    if (rpmteType(p) == TR_ADDED && rpmteType(otherFi->te) != TR_ADDED)
		continue;

	    otherFps = otherFi->fps;
	    otherFc = rpmfiFC(otherFi);

	    otherFileNum = findFps(fiFps, otherFps, otherFc);
	    (void) rpmfiSetFX(otherFi, otherFileNum);

	    /* XXX Happens iff fingerprint for incomplete package install. */
	    if (otherFi->actions[otherFileNum] != FA_UNKNOWN)
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
		if (fi->actions[i] != FA_UNKNOWN)
		    break;
		if (rpmfiConfigConflict(fi)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (FFlags & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
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
			if (!XFA_SKIPPING(fi->actions[i])) {
			    /* XXX static helpers are order dependent. Ick. */
			    if (strcmp(fn, "/usr/sbin/libgcc_post_upgrade")
			     && strcmp(fn, "/usr/sbin/glibc_post_upgrade"))
				otherFi->actions[otherFileNum] = FA_SKIPCOLOR;
			}
			fi->actions[i] = FA_CREATE;
			rConflicts = 0;
		    } else
		    if (oFColor & prefcolor) {
			/* ... first file of preferred colour is installed ... */
			if (XFA_SKIPPING(fi->actions[i]))
			    otherFi->actions[otherFileNum] = FA_CREATE;
			fi->actions[i] = FA_SKIPCOLOR;
			rConflicts = 0;
		    } else
		    if (FColor == 0 && oFColor == 0) {
			/* ... otherwise, do both, last in wins. */
			otherFi->actions[otherFileNum] = FA_CREATE;
			fi->actions[i] = FA_CREATE;
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
		fi->actions[i] = (FFlags & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_SKIP;
	    } else {
		if (!done)
		    fi->actions[i] = FA_CREATE;
	    }
	  } break;

	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
assert(otherFi != NULL);
		/* Here is an overlapped added file we don't want to nuke. */
		if (otherFi->actions[otherFileNum] != FA_ERASE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		otherFi->actions[otherFileNum] = FA_SKIP;
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		break;
	    if (rpmfiFState(fi) != RPMFILE_STATE_NORMAL)
		break;
	    if (!(S_ISREG(FMode) && (FFlags & RPMFILE_CONFIG))) {
		fi->actions[i] = FA_ERASE;
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
			fi->actions[i] = FA_BACKUP;
			break;
		    }
		}
	    }
	    fi->actions[i] = FA_ERASE;
	    break;
	}

	/* Update disk space info for a file. */
	rpmtsUpdateDSI(ts, fiFps->entry->dev, rpmfiFSize(fi),
		fi->replacedSizes[i], fixupSize, fi->actions[i]);

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
/* FIX: fi->actions is modified. */
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

	bn = rpmfiBN(fi);
	bnlen = strlen(bn);
	ix = rpmfiDX(fi);
	dn = rpmfiDN(fi);
	dnlen = strlen(dn);
	if (dn == NULL)
	    continue;	/* XXX can't happen */

	drc[ix]++;

	/* Don't bother with skipped files */
	if (XFA_SKIPPING(fi->actions[i])) {
	    drc[ix]--; dff[ix] = 1;
	    continue;
	}

	/* Ignore colored files not in our rainbow. */
	FColor = rpmfiFColor(fi);
	if (tscolor && FColor && !(tscolor & FColor)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPCOLOR;
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
	    fi->actions[i] = FA_SKIPNETSHARED;
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	if (languages != NULL && fi->flangs != NULL && *fi->flangs[i]) {
	    const char *l, *le;
	    char **lang;
	    for (lang = languages; *lang != NULL; lang++) {
		if (!strcmp(*lang, "all"))
		    break;
		for (l = fi->flangs[i]; *l != '\0'; l = le) {
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
		fi->actions[i] = FA_SKIPNSTATE;
		continue;
	    }
	}

	/*
	 * Skip config files if requested.
	 */
	if (noConfigs && (rpmfiFFlags(fi) & RPMFILE_CONFIG)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (rpmfiFFlags(fi) & RPMFILE_DOC)) {
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

	    if (XFA_SKIPPING(fi->actions[i]))
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
	    fi->actions[i] = FA_SKIPNSTATE;
	    break;
	}
    }

    if (netsharedPaths) argvFree(netsharedPaths);
#ifdef	DYING	/* XXX freeFi will deal with this later. */
    fi->flangs = _free(fi->flangs);
#endif
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
    	const char * script = NULL, * scriptprog = NULL;
    	rpmTag progtag = RPMTAG_NOT_FOUND;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	switch (stag) {
	case RPMTAG_PRETRANS:
	    script = fi->pretrans;
	    scriptprog = fi->pretransprog;
	    progtag = RPMTAG_PRETRANSPROG;
	    break;
	case RPMTAG_POSTTRANS:
	    script = fi->posttrans;
	    scriptprog = fi->posttransprog;
	    progtag = RPMTAG_POSTTRANSPROG;
	    p->fi = rpmfiFree(p->fi);
	    break;
	default:
	    /* programmer error, blow up */
	    assert(progtag != RPMTAG_NOT_FOUND);
	    break;
	}
	
    	/* If no pre/post-transaction script, then don't bother. */
	if (script == NULL && scriptprog == NULL)
 	    continue;

    	p->fd = ts->notify(p->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
		    rpmteKey(p), ts->notifyData);
    	p->h = NULL;
    	if (rpmteFd(p) != NULL) {
	    rpmVSFlags ovsflags = rpmtsVSFlags(ts);
    	    rpmVSFlags vsflags = ovsflags | RPMVSF_NEEDPAYLOAD;
	    rpmRC rpmrc;
	    ovsflags = rpmtsSetVSFlags(ts, vsflags);
	    rpmrc = rpmReadPackageFile(ts, rpmteFd(p),
		    rpmteNEVR(p), &p->h);
	    vsflags = rpmtsSetVSFlags(ts, ovsflags);
	    switch (rpmrc) {
	    default:
	        /* FIX: notify annotations */
	        p->fd = ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE,
			    0, 0, rpmteKey(p), ts->notifyData);
	        p->fd = NULL;
	        break;
	    case RPMRC_NOTTRUSTED:
	    case RPMRC_NOKEY:
	    case RPMRC_OK:
	        break;
	    }
        }

    	if (rpmteFd(p) != NULL) {
 	    fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, 1);
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
	    assert(psm != NULL);
	    xx = rpmpsmScriptStage(psm, stag, progtag);
	    psm = rpmpsmFree(psm);

	    (void) ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			  rpmteKey(p), ts->notifyData);
	    p->fd = NULL;
	    p->h = headerFree(p->h);
	}
    }
    pi = rpmtsiFree(pi);
    return 0;
}

#define	NOTIFY(_ts, _al) if ((_ts)->notify) (void) (_ts)->notify _al

int rpmtsRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    int i, j;
    int ourrc = 0;
    int totalFileCount = 0;
    rpmfi fi;
    sharedFileInfo shared, sharedList;
    int numShared;
    int nexti;
    rpmalKey lastFailKey;
    fingerPrintCache fpc;
    rpmps ps;
    rpmpsm psm;
    rpmtsi pi;	rpmte p;
    rpmtsi qi;	rpmte q;
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

    if (!rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS) {
	char *fn = rpmGetPath("%{?_install_file_context_path}", NULL);
	if (matchpathcon_init(fn) == -1) {
	    rpmtsSetFlags(ts, (rpmtsFlags(ts) | RPMTRANS_FLAG_NOCONTEXTS));
	}
	_free(fn);
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
	    fi->record = 0;
	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    if (fc > 0)
		skipFiles(ts, fi);
	    break;
	case TR_REMOVED:
	    numRemoved++;
	    fi->record = rpmteDBOffset(p);
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

    ts->ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

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
	    if (XFA_SKIPPING(fi->actions[i]))
		continue;
	    htAddEntry(ts->ht, fi->fps + i, (void *) fi);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);

    }
    pi = rpmtsiFree(pi);

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, ts->orderCount,
	NULL, ts->notifyData));

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
    rpmlog(RPMLOG_DEBUG, "computing file dispositions\n");
    ps = rpmtsProblems(ts);
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	dbiIndexSet * matches;
	int knownBad;
	int fc;

	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, rpmtsiOc(pi),
			ts->orderCount, NULL, ts->notifyData));

	if (fc == 0) continue;

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(fc, sizeof(*matches));
	if (rpmdbFindFpList(rpmtsGetRdb(ts), fi->fps, matches, fc)) {
	    ps = rpmpsFree(ps);
	    rpmtsFreeLock(lock);
	    return 1;	/* XXX WTFO? */
	}

	numShared = 0;
 	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = xcalloc((numShared + 1), sizeof(*sharedList));

 	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0) {
	    /*
	     * Take care not to mark files as replaced in packages that will
	     * have been removed before we will get here.
	     */
	    for (j = 0; j < dbiIndexSetCount(matches[i]); j++) {
		int ro;
		ro = dbiIndexRecordOffset(matches[i], j);
		knownBad = 0;
		qi = rpmtsiInit(ts);
		while ((q = rpmtsiNext(qi, TR_REMOVED)) != NULL) {
		    if (ro == knownBad)
			break;
		    if (rpmteDBOffset(q) == ro)
			knownBad = ro;
		}
		qi = rpmtsiFree(qi);

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
		    break;
	    }

	    /* Is this file from a package being removed? */
	    beingRemoved = 0;
	    if (ts->removedPackages != NULL)
	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] != shared->otherPkg)
		    continue;
		beingRemoved = 1;
		break;
	    }

	    /* Determine the fate of each file. */
	    switch (rpmteType(p)) {
	    case TR_ADDED:
		xx = handleInstInstalledFiles(ts, p, fi, shared, nexti - i,
	!(beingRemoved || (rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEOLDFILES)));
		break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    xx = handleRmvdInstalledFiles(ts, fi, shared, nexti - i);
		break;
	    }
	}

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
	handleOverlappedFiles(ts, p, fi);

	/* Check added package has sufficient space on each partition used. */
	switch (rpmteType(p)) {
	case TR_ADDED:
	    rpmtsCheckDSIProblems(ts, p);
	    break;
	case TR_REMOVED:
	    break;
	}
	/* check for s-bit files to be removed */
	if (rpmteType(p) == TR_REMOVED) {
	    fi = rpmfiInit(fi, 0);
	    while ((i = rpmfiNext(fi)) >= 0) {
		rpm_mode_t mode;
		if (XFA_SKIPPING(fi->actions[i]))
		    continue;
		(void) rpmfiSetFX(fi, i);
		mode = rpmfiFMode(fi);
		if (S_ISREG(mode) && (mode & 06000) != 0) {
		    fi->mapflags |= CPIO_SBIT_CHECK;
		}
	    }
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);
    }
    pi = rpmtsiFree(pi);
    ps = rpmpsFree(ps);

    if (rpmtsChrootDone(ts)) {
	const char * rootDir = rpmtsRootDir(ts);
	const char * currDir = rpmtsCurrDir(ts);
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
	    xx = chroot(".");
	(void) rpmtsSetChrootDone(ts, 0);
	if (currDir != NULL)
	    xx = chdir(currDir);
    }

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, ts->orderCount,
	NULL, ts->notifyData));

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
    ts->ht = htFree(ts->ht);

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

    /* ===============================================
     * Install and remove packages.
     */
    lastFailKey = (rpmalKey)-2;	/* erased packages have -1 */
    pi = rpmtsiInit(ts);
    /* FIX: fi reload needs work */
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmalKey pkgKey;
	int gotfd, async;

	gotfd = 0;
	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	psm = rpmpsmNew(ts, p, fi);
	async = (rpmtsiOc(pi) >= rpmtsUnorderedSuccessors(ts, -1) ? 1 : 0);
	rpmpsmSetAsync(psm, async);

	switch (rpmteType(p)) {
	case TR_ADDED:
	    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_INSTALL), 0);

	    pkgKey = rpmteAddedKey(p);

	    rpmlog(RPMLOG_DEBUG, "========== +++ %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	    p->h = NULL;
	    /* FIX: rpmte not opaque */
	    {
		/* FIX: notify annotations */
		p->fd = ts->notify(p->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
				rpmteKey(p), ts->notifyData);
		if (rpmteFd(p) != NULL) {
		    rpmVSFlags ovsflags = rpmtsVSFlags(ts);
		    rpmVSFlags vsflags = ovsflags | RPMVSF_NEEDPAYLOAD;
		    rpmRC rpmrc;

		    ovsflags = rpmtsSetVSFlags(ts, vsflags);
		    rpmrc = rpmReadPackageFile(ts, rpmteFd(p),
				rpmteNEVR(p), &p->h);
		    vsflags = rpmtsSetVSFlags(ts, ovsflags);

		    switch (rpmrc) {
		    default:
			/* FIX: notify annotations */
			p->fd = ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0,
					rpmteKey(p), ts->notifyData);
			p->fd = NULL;
			ourrc++;
			break;
		    case RPMRC_NOTTRUSTED:
		    case RPMRC_NOKEY:
		    case RPMRC_OK:
			break;
		    }
		    if (rpmteFd(p) != NULL) gotfd = 1;
		}
	    }

	    if (rpmteFd(p) != NULL) {
		/*
		 * XXX Sludge necessary to tranfer existing fstates/actions
		 * XXX around a recreated file info set.
		 */
		rpmpsmSetFI(psm, NULL);
		{
		    char * fstates = fi->fstates;
		    rpmFileAction * actions = fi->actions;
		    sharedFileInfo replaced = fi->replaced;
		    int mapflags = fi->mapflags;
		    rpmte savep;
		    int numShared = 0;

		    if (replaced != NULL) {
                        for (; replaced->otherPkg; replaced++) {
                            numShared++;
                        }
                        if (numShared > 0) {
                            replaced = xcalloc(numShared + 1, 
                                               sizeof(*fi->replaced));
                            memcpy(replaced, fi->replaced, 
                                   sizeof(*fi->replaced) * (numShared + 1));
                        }
                    }

		    fi->fstates = NULL;
		    fi->actions = NULL;
		    fi->replaced = NULL;
/* FIX: fi->actions is NULL */
		    fi = rpmfiFree(fi);

		    savep = rpmtsSetRelocateElement(ts, p);
		    fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, 1);
		    (void) rpmtsSetRelocateElement(ts, savep);

		    if (fi != NULL) {	/* XXX can't happen */
			fi->te = p;
			fi->fstates = _free(fi->fstates);
			fi->fstates = fstates;
			fi->actions = _free(fi->actions);
			fi->actions = actions;
                        if (replaced != NULL)
                            fi->replaced = replaced;
			if (mapflags & CPIO_SBIT_CHECK)
			    fi->mapflags |= CPIO_SBIT_CHECK;
			p->fi = fi;
		    }
		}
		rpmpsmSetFI(psm, p->fi);

/* FIX: psm->fi may be NULL */
		if (rpmpsmStage(psm, PSM_PKGINSTALL)) {
		    ourrc++;
		    lastFailKey = pkgKey;
		}
		
	    } else {
		ourrc++;
		lastFailKey = pkgKey;
	    }

	    if (gotfd) {
		/* FIX: check rc */
		(void) ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			rpmteKey(p), ts->notifyData);
		p->fd = NULL;
	    }

	    p->h = headerFree(p->h);

	    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_INSTALL), 0);

	    break;

	case TR_REMOVED:
	    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ERASE), 0);

	    rpmlog(RPMLOG_DEBUG, "========== --- %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	    /*
	     * XXX This has always been a hack, now mostly broken.
	     * If install failed, then we shouldn't erase.
	     */
	    if (rpmteDependsOnKey(p) != lastFailKey) {
		if (rpmpsmStage(psm, PSM_PKGERASE)) {
		    ourrc++;
		}
	    }

	    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ERASE), 0);

	    break;
	}
	xx = rpmdbSync(rpmtsGetRdb(ts));

/* FIX: psm->fi may be NULL */
	psm = rpmpsmFree(psm);

#ifdef	DYING
/* FIX: p is almost opaque */
	p->fi = rpmfiFree(p->fi);
#endif

    }
    pi = rpmtsiFree(pi);

    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)) {
	rpmlog(RPMLOG_DEBUG, "running post-transaction scripts\n");
	runTransScripts(ts, RPMTAG_POSTTRANS);
    }

    rpmtsFreeLock(lock);

    /* FIX: ts->flList may be NULL */
    if (ourrc)
    	return -1;
    else
	return 0;
}
