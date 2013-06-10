/** \ingroup rpmdep
 * \file lib/rpmfi.c
 * Routines to handle file info tag sets.
 */

#include "system.h"

#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfileutil.h>	/* XXX rpmDoDigest */
#include <rpm/rpmstring.h>
#include <rpm/rpmmacro.h>	/* XXX rpmCleanPath */
#include <rpm/rpmds.h>

#include "lib/rpmfi_internal.h"
#include "lib/rpmte_internal.h"	/* relocations */
#include "lib/cpio.h"	/* XXX CPIO_FOO */

#include "debug.h"

static rpmfi rpmfiUnlink(rpmfi fi)
{
    if (fi)
	fi->nrefs--;
    return NULL;
}

rpmfi rpmfiLink(rpmfi fi)
{
    if (fi)
	fi->nrefs++;
    return fi;
}

rpm_count_t rpmfiFC(rpmfi fi)
{
    return (fi != NULL ? fi->fc : 0);
}

rpm_count_t rpmfiDC(rpmfi fi)
{
    return (fi != NULL ? fi->dc : 0);
}

#ifdef	NOTYET
int rpmfiDI(rpmfi fi)
{
}
#endif

int rpmfiFX(rpmfi fi)
{
    return (fi != NULL ? fi->i : -1);
}

int rpmfiSetFX(rpmfi fi, int fx)
{
    int i = -1;

    if (fi != NULL && fx >= 0 && fx < fi->fc) {
	i = fi->i;
	fi->i = fx;
	fi->j = fi->dil[fi->i];
    }
    return i;
}

int rpmfiDX(rpmfi fi)
{
    return (fi != NULL ? fi->j : -1);
}

int rpmfiSetDX(rpmfi fi, int dx)
{
    int j = -1;

    if (fi != NULL && dx >= 0 && dx < fi->dc) {
	j = fi->j;
	fi->j = dx;
    }
    return j;
}

int rpmfiDIIndex(rpmfi fi, int dx)
{
    int j = -1;
    if (fi != NULL && dx >= 0 && dx < fi->fc) {
	if (fi->dil != NULL)
	    j = fi->dil[dx];
    }
    return j;
}

rpmsid rpmfiBNIdIndex(rpmfi fi, int ix)
{
    rpmsid id = 0;
    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->bnid != NULL)
	    id = fi->bnid[ix];
    }
    return id;
}

rpmsid rpmfiDNIdIndex(rpmfi fi, int jx)
{
    rpmsid id = 0;
    if (fi != NULL && jx >= 0 && jx < fi->fc) {
	if (fi->dnid != NULL)
	    id = fi->dnid[jx];
    }
    return id;
}

const char * rpmfiBNIndex(rpmfi fi, int ix)
{
    const char * BN = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->bnid != NULL)
	    BN = rpmstrPoolStr(fi->pool, fi->bnid[ix]);
    }
    return BN;
}

const char * rpmfiDNIndex(rpmfi fi, int jx)
{
    const char * DN = NULL;

    if (fi != NULL && jx >= 0 && jx < fi->dc) {
	if (fi->dnid != NULL)
	    DN = rpmstrPoolStr(fi->pool, fi->dnid[jx]);
    }
    return DN;
}

char * rpmfiFNIndex(rpmfi fi, int ix)
{
    char *fn = NULL;
    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	fn = rstrscat(NULL, rpmstrPoolStr(fi->pool, fi->dnid[fi->dil[ix]]),
			    rpmstrPoolStr(fi->pool, fi->bnid[ix]), NULL);
    }
    return fn;
}

rpmfileAttrs rpmfiFFlagsIndex(rpmfi fi, int ix)
{
    rpmfileAttrs FFlags = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fflags != NULL)
	    FFlags = fi->fflags[ix];
    }
    return FFlags;
}

rpmVerifyAttrs rpmfiVFlagsIndex(rpmfi fi, int ix)
{
    rpmVerifyAttrs VFlags = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->vflags != NULL)
	    VFlags = fi->vflags[ix];
    }
    return VFlags;
}

rpm_mode_t rpmfiFModeIndex(rpmfi fi, int ix)
{
    rpm_mode_t fmode = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fmodes != NULL)
	    fmode = fi->fmodes[ix];
    }
    return fmode;
}

rpmfileState rpmfiFStateIndex(rpmfi fi, int ix)
{
    rpmfileState fstate = RPMFILE_STATE_MISSING;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fstates != NULL)
	    fstate = fi->fstates[ix];
    }
    return fstate;
}

const unsigned char * rpmfiMD5(rpmfi fi)
{
    const unsigned char *digest;
    int algo = 0;

    digest = rpmfiFDigest(fi, &algo, NULL);
    return (algo == PGPHASHALGO_MD5) ? digest : NULL;
}

int rpmfiDigestAlgo(rpmfi fi)
{
    return fi ? fi->digestalgo : 0;
}

const unsigned char * rpmfiFDigestIndex(rpmfi fi, int ix, int *algo, size_t *len)
{
    const unsigned char *digest = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
    	size_t diglen = rpmDigestLength(fi->digestalgo);
	if (fi->digests != NULL)
	    digest = fi->digests + (diglen * ix);
	if (len) 
	    *len = diglen;
	if (algo) 
	    *algo = fi->digestalgo;
    }
    return digest;
}

char * rpmfiFDigestHex(rpmfi fi, int *algo)
{
    size_t diglen = 0;
    char *fdigest = NULL;
    const unsigned char *digest = rpmfiFDigest(fi, algo, &diglen);
    if (digest) {
	fdigest = pgpHexStr(digest, diglen);
    }
    return fdigest;
}

const char * rpmfiFLinkIndex(rpmfi fi, int ix)
{
    const char * flink = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->flinks != NULL)
	    flink = rpmstrPoolStr(fi->pool, fi->flinks[ix]);
    }
    return flink;
}

rpm_loff_t rpmfiFSizeIndex(rpmfi fi, int ix)
{
    rpm_loff_t fsize = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fsizes != NULL)
	    fsize = fi->fsizes[ix];
    }
    return fsize;
}

rpm_rdev_t rpmfiFRdevIndex(rpmfi fi, int ix)
{
    rpm_rdev_t frdev = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->frdevs != NULL)
	    frdev = fi->frdevs[ix];
    }
    return frdev;
}

rpm_ino_t rpmfiFInodeIndex(rpmfi fi, int ix)
{
    rpm_ino_t finode = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->finodes != NULL)
	    finode = fi->finodes[ix];
    }
    return finode;
}

rpm_color_t rpmfiColor(rpmfi fi)
{
    rpm_color_t color = 0;

    if (fi != NULL && fi->fcolors != NULL) {
	for (int i = 0; i < fi->fc; i++)
	    color |= fi->fcolors[i];
	/* XXX ignore all but lsnibble for now. */
	color &= 0xf;
    }
    return color;
}

rpm_color_t rpmfiFColorIndex(rpmfi fi, int ix)
{
    rpm_color_t fcolor = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fcolors != NULL)
	    /* XXX ignore all but lsnibble for now. */
	    fcolor = (fi->fcolors[ix] & 0x0f);
    }
    return fcolor;
}

const char * rpmfiFClassIndex(rpmfi fi, int ix)
{
    const char * fclass = NULL;
    int cdictx;

    if (fi != NULL && fi->fcdictx != NULL && ix >= 0 && ix < fi->fc) {
	cdictx = fi->fcdictx[ix];
	if (fi->cdict != NULL && cdictx >= 0 && cdictx < fi->ncdict)
	    fclass = fi->cdict[cdictx];
    }
    return fclass;
}

uint32_t rpmfiFDependsIndex(rpmfi fi, int ix, const uint32_t ** fddictp)
{
    int fddictx = -1;
    int fddictn = 0;
    const uint32_t * fddict = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fddictn != NULL)
	    fddictn = fi->fddictn[ix];
	if (fddictn > 0 && fi->fddictx != NULL)
	    fddictx = fi->fddictx[ix];
	if (fi->ddict != NULL && fddictx >= 0 && (fddictx+fddictn) <= fi->nddict)
	    fddict = fi->ddict + fddictx;
    }
    if (fddictp)
	*fddictp = fddict;
    return fddictn;
}

uint32_t rpmfiFNlinkIndex(rpmfi fi, int ix)
{
    uint32_t nlink = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	/* XXX rpm-2.3.12 has not RPMTAG_FILEINODES */
	if (fi->finodes && fi->finodes[ix] > 0 && fi->frdevs) {
	    rpm_ino_t finode = fi->finodes[ix];
	    rpm_rdev_t frdev = fi->frdevs[ix];
	    int j;

	    for (j = 0; j < fi->fc; j++) {
		if (fi->frdevs[j] == frdev && fi->finodes[j] == finode)
		    nlink++;
	    }
	}
    }
    return nlink;
}

rpm_time_t rpmfiFMtimeIndex(rpmfi fi, int ix)
{
    rpm_time_t fmtime = 0;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fmtimes != NULL)
	    fmtime = fi->fmtimes[ix];
    }
    return fmtime;
}

const char * rpmfiFUserIndex(rpmfi fi, int ix)
{
    const char * fuser = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fuser != NULL)
	    fuser = rpmstrPoolStr(fi->pool, fi->fuser[ix]);
    }
    return fuser;
}

const char * rpmfiFGroupIndex(rpmfi fi, int ix)
{
    const char * fgroup = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fgroup != NULL)
	    fgroup = rpmstrPoolStr(fi->pool, fi->fgroup[ix]);
    }
    return fgroup;
}

const char * rpmfiFCapsIndex(rpmfi fi, int ix)
{
    const char *fcaps = NULL;
    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	fcaps = fi->fcaps ? fi->fcaps[ix] : "";
    }
    return fcaps;
}

const char * rpmfiFLangsIndex(rpmfi fi, int ix)
{
    const char *flangs = NULL;
    if (fi != NULL && fi->flangs != NULL && ix >= 0 && ix < fi->fc) {
	flangs = rpmstrPoolStr(fi->pool, fi->flangs[ix]);
    }
    return flangs;
}

struct fingerPrint_s *rpmfiFps(rpmfi fi)
{
    return (fi != NULL) ? fi->fps : NULL;
}

int rpmfiNext(rpmfi fi)
{
    int i = -1;

    if (fi != NULL && ++fi->i >= 0) {
	if (fi->i < fi->fc) {
	    i = fi->i;
	    if (fi->dil != NULL)
		fi->j = fi->dil[fi->i];
	} else
	    fi->i = -1;
    }

    return i;
}

rpmfi rpmfiInit(rpmfi fi, int fx)
{
    if (fi != NULL) {
	if (fx >= 0 && fx < fi->fc) {
	    fi->i = fx - 1;
	    fi->j = -1;
	}
    }

    return fi;
}

int rpmfiNextD(rpmfi fi)
{
    int j = -1;

    if (fi != NULL && ++fi->j >= 0) {
	if (fi->j < fi->dc)
	    j = fi->j;
	else
	    fi->j = -1;
    }

    return j;
}

rpmfi rpmfiInitD(rpmfi fi, int dx)
{
    if (fi != NULL) {
	if (dx >= 0 && dx < fi->fc)
	    fi->j = dx - 1;
	else
	    fi = NULL;
    }

    return fi;
}

/**
 * Identify a file type.
 * @param ft		file type
 * @return		string to identify a file type
 */
static
const char * ftstring (rpmFileTypes ft)
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
}

rpmFileTypes rpmfiWhatis(rpm_mode_t mode)
{
    if (S_ISDIR(mode))	return XDIR;
    if (S_ISCHR(mode))	return CDEV;
    if (S_ISBLK(mode))	return BDEV;
    if (S_ISLNK(mode))	return LINK;
    if (S_ISSOCK(mode))	return SOCK;
    if (S_ISFIFO(mode))	return PIPE;
    return REG;
}

int rpmfiCompareIndex(rpmfi afi, int aix, rpmfi bfi, int bix)
{
    mode_t amode = rpmfiFModeIndex(afi, aix);
    mode_t bmode = rpmfiFModeIndex(bfi, bix);
    rpmFileTypes awhat = rpmfiWhatis(amode);

    if ((rpmfiFFlagsIndex(afi, aix) & RPMFILE_GHOST) ||
	(rpmfiFFlagsIndex(bfi, bix) & RPMFILE_GHOST)) return 0;

    /* Mode difference is a conflict, except for symlinks */
    if (!(awhat == LINK && rpmfiWhatis(bmode) == LINK) && amode != bmode)
	return 1;

    if (awhat == LINK || awhat == REG) {
	if (rpmfiFSizeIndex(afi, aix) != rpmfiFSizeIndex(bfi, bix))
	    return 1;
    }

    if (!rstreq(rpmfiFUserIndex(afi, aix), rpmfiFUserIndex(bfi, bix)))
	return 1;
    if (!rstreq(rpmfiFGroupIndex(afi, aix), rpmfiFGroupIndex(bfi, bix)))
	return 1;

    if (awhat == LINK) {
	const char * alink = rpmfiFLinkIndex(afi, aix);
	const char * blink = rpmfiFLinkIndex(bfi, bix);
	if (alink == blink) return 0;
	if (alink == NULL) return 1;
	if (blink == NULL) return -1;
	return strcmp(alink, blink);
    } else if (awhat == REG) {
	size_t adiglen, bdiglen;
	int aalgo, balgo;
	const unsigned char * adigest, * bdigest;
	adigest = rpmfiFDigestIndex(afi, aix, &aalgo, &adiglen);
	bdigest = rpmfiFDigestIndex(bfi, bix, &balgo, &bdiglen);
	if (adigest == bdigest) return 0;
	if (adigest == NULL) return 1;
	if (bdigest == NULL) return -1;
	/* can't meaningfully compare different hash types */
	if (aalgo != balgo || adiglen != bdiglen) return -1;
	return memcmp(adigest, bdigest, adiglen);
    } else if (awhat == CDEV || awhat == BDEV) {
	if (rpmfiFRdevIndex(afi, aix) != rpmfiFRdevIndex(bfi, bix))
	    return 1;
    }

    return 0;
}

rpmFileAction rpmfiDecideFateIndex(rpmfi ofi, int oix, rpmfi nfi, int nix,
				   int skipMissing)
{
    char * fn = rpmfiFNIndex(nfi, nix);
    rpmfileAttrs newFlags = rpmfiFFlagsIndex(nfi, nix);
    char buffer[1024];
    rpmFileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;
    int action = FA_CREATE; /* assume we can create */

    /* If the new file is a ghost, leave whatever might be on disk alone. */
    if (newFlags & RPMFILE_GHOST) {
	action = FA_SKIP;
	goto exit;
    }

    if (lstat(fn, &sb)) {
	/*
	 * The file doesn't exist on the disk. Create it unless the new
	 * package has marked it as missingok, or allfiles is requested.
	 */
	if (skipMissing && (newFlags & RPMFILE_MISSINGOK)) {
	    rpmlog(RPMLOG_DEBUG, "%s skipped due to missingok flag\n",
			fn);
	    action = FA_SKIP;
	    goto exit;
	} else {
	    goto exit;
	}
    }

    diskWhat = rpmfiWhatis((rpm_mode_t)sb.st_mode);
    dbWhat = rpmfiWhatis(rpmfiFModeIndex(ofi, oix));
    newWhat = rpmfiWhatis(rpmfiFModeIndex(nfi, nix));

    /*
     * This order matters - we'd prefer to CREATE the file if at all
     * possible in case something else (like the timestamp) has changed.
     * Only regular files and symlinks might need a backup, everything
     * else falls through here with FA_CREATE.
     */
    memset(buffer, 0, sizeof(buffer));
    if (dbWhat == REG) {
	int oalgo, nalgo;
	size_t odiglen, ndiglen;
	const unsigned char * odigest, * ndigest;

	/* See if the file on disk is identical to the one in old pkg */
	odigest = rpmfiFDigestIndex(ofi, oix, &oalgo, &odiglen);
	if (diskWhat == REG) {
	    if (rpmDoDigest(oalgo, fn, 0, (unsigned char *)buffer, NULL))
	        goto exit;	/* assume file has been removed */
	    if (odigest && memcmp(odigest, buffer, odiglen) == 0)
	        goto exit;	/* unmodified config file, replace. */
	}

	/* See if the file on disk is identical to the one in new pkg */
	ndigest = rpmfiFDigestIndex(nfi, nix, &nalgo, &ndiglen);
	if (diskWhat == REG && newWhat == REG) {
	    /* hash algo changed in new, recalculate digest */
	    if (oalgo != nalgo)
		if (rpmDoDigest(nalgo, fn, 0, (unsigned char *)buffer, NULL))
		    goto exit;		/* assume file has been removed */
	    if (ndigest && memcmp(ndigest, buffer, ndiglen) == 0)
	        goto exit;		/* file identical in new, replace. */
	}

	/* If file can be determined identical in old and new pkg, let it be */
	if (newWhat == REG && oalgo == nalgo && odiglen == ndiglen) {
	    if (odigest && ndigest && memcmp(odigest, ndigest, odiglen) == 0) {
		action = FA_SKIP; /* identical file, dont bother */
		goto exit;
	    }
	}
	
	/* ...but otherwise a backup will be needed */
	action = save;
    } else if (dbWhat == LINK) {
	const char * oFLink, * nFLink;

	/* See if the link on disk is identical to the one in old pkg */
	oFLink = rpmfiFLinkIndex(ofi, oix);
	if (diskWhat == LINK) {
	    ssize_t link_len = readlink(fn, buffer, sizeof(buffer) - 1);
	    if (link_len == -1)
		goto exit;		/* assume file has been removed */
	    buffer[link_len] = '\0';
	    if (oFLink && rstreq(oFLink, buffer))
		goto exit;		/* unmodified config file, replace. */
	}

	/* See if the link on disk is identical to the one in new pkg */
	nFLink = rpmfiFLinkIndex(nfi, nix);
	if (diskWhat == LINK && newWhat == LINK) {
	    if (nFLink && rstreq(nFLink, buffer))
		goto exit;		/* unmodified config file, replace. */
	}

	/* If link is identical in old and new pkg, let it be */
	if (newWhat == LINK && oFLink && nFLink && rstreq(oFLink, nFLink)) {
	    action = FA_SKIP;		/* identical file, don't bother. */
	    goto exit;
	}

	/* ...but otherwise a backup will be needed */
	action = save;
    }

exit:
    free(fn);
    return action;
}

int rpmfiConfigConflictIndex(rpmfi fi, int ix)
{
    char * fn = NULL;
    rpmfileAttrs flags = rpmfiFFlagsIndex(fi, ix);
    char buffer[1024];
    rpmFileTypes newWhat, diskWhat;
    struct stat sb;
    int rc = 0;

    /* Non-configs are not config conflicts. */
    if (!(flags & RPMFILE_CONFIG))
	return 0;

    /* Only links and regular files can be %config, this is kinda moot */
    /* XXX: Why are we returning 1 here? */
    newWhat = rpmfiWhatis(rpmfiFModeIndex(fi, ix));
    if (newWhat != LINK && newWhat != REG)
	return 1;

    /* If it's not on disk, there's nothing to be saved */
    fn = rpmfiFNIndex(fi, ix);
    if (lstat(fn, &sb))
	goto exit;

    /*
     * Preserve legacy behavior: an existing %ghost %config is considered
     * "modified" but unlike regular %config, its never removed and
     * never backed up. Whether this actually makes sense is a whole
     * another question, but this is very long-standing behavior that
     * people might be depending on. The resulting FA_ALTNAME etc action
     * is special-cased in FSM to avoid actually creating backups on ghosts.
     */
    if (flags & RPMFILE_GHOST) {
	rc = 1;
	goto exit;
    }

    /* Files of different types obviously are not identical */
    diskWhat = rpmfiWhatis((rpm_mode_t)sb.st_mode);
    if (diskWhat != newWhat) {
	rc = 1;
	goto exit;
    }

    /* Files of different sizes obviously are not identical */
    if (rpmfiFSizeIndex(fi, ix) != sb.st_size) {
	rc = 1;
	goto exit;
    }
    
    memset(buffer, 0, sizeof(buffer));
    if (newWhat == REG) {
	int algo;
	size_t diglen;
	const unsigned char *ndigest = rpmfiFDigestIndex(fi,ix, &algo, &diglen);
	if (rpmDoDigest(algo, fn, 0, (unsigned char *)buffer, NULL))
	    goto exit;	/* assume file has been removed */
	if (ndigest && memcmp(ndigest, buffer, diglen) == 0)
	    goto exit;	/* unmodified config file */
    } else /* newWhat == LINK */ {
	const char * nFLink;
	ssize_t link_len = readlink(fn, buffer, sizeof(buffer) - 1);
	if (link_len == -1)
	    goto exit;	/* assume file has been removed */
	buffer[link_len] = '\0';
	nFLink = rpmfiFLinkIndex(fi, ix);
	if (nFLink && rstreq(nFLink, buffer))
	    goto exit;	/* unmodified config file */
    }

    rc = 1;

exit:
    free(fn);
    return rc;
}

static char **duparray(char ** src, int size)
{
    char **dest = xmalloc((size+1) * sizeof(*dest));
    for (int i = 0; i < size; i++) {
	dest[i] = xstrdup(src[i]);
    }
    free(src);
    return dest;
}

static int addPrefixes(Header h, rpmRelocation *relocations, int numRelocations)
{
    struct rpmtd_s validRelocs;
    const char *validprefix;
    const char ** actualRelocations;
    int numActual = 0;

    headerGet(h, RPMTAG_PREFIXES, &validRelocs, HEADERGET_MINMEM);
    /*
     * If no relocations are specified (usually the case), then return the
     * original header. If there are prefixes, however, then INSTPREFIXES
     * should be added for RPM_INSTALL_PREFIX environ variables in scriptlets, 
     * but, since relocateFileList() can be called more than once for 
     * the same header, don't bother if already present.
     */
    if (relocations == NULL || numRelocations == 0) {
	if (rpmtdCount(&validRelocs) > 0) {
	    if (!headerIsEntry(h, RPMTAG_INSTPREFIXES)) {
		rpmtdSetTag(&validRelocs, RPMTAG_INSTPREFIXES);
		headerPut(h, &validRelocs, HEADERPUT_DEFAULT);
	    }
	    rpmtdFreeData(&validRelocs);
	}
	return 0;
    }

    actualRelocations = xmalloc(rpmtdCount(&validRelocs) * sizeof(*actualRelocations));
    rpmtdInit(&validRelocs);
    while ((validprefix = rpmtdNextString(&validRelocs))) {
	int j;
	for (j = 0; j < numRelocations; j++) {
	    if (relocations[j].oldPath == NULL || /* XXX can't happen */
		!rstreq(validprefix, relocations[j].oldPath))
		continue;
	    /* On install, a relocate to NULL means skip the path. */
	    if (relocations[j].newPath) {
		actualRelocations[numActual] = relocations[j].newPath;
		numActual++;
	    }
	    break;
	}
	if (j == numRelocations) {
	    actualRelocations[numActual] = validprefix;
	    numActual++;
	}
    }
    rpmtdFreeData(&validRelocs);

    if (numActual) {
	headerPutStringArray(h, RPMTAG_INSTPREFIXES, actualRelocations, numActual);
    }
    free(actualRelocations);
    return numActual;
}

static void saveRelocs(Header h, rpmtd bnames, rpmtd dnames, rpmtd dindexes)
{
	struct rpmtd_s td;
	headerGet(h, RPMTAG_BASENAMES, &td, HEADERGET_MINMEM);
	rpmtdSetTag(&td, RPMTAG_ORIGBASENAMES);
	headerPut(h, &td, HEADERPUT_DEFAULT);
	rpmtdFreeData(&td);

	headerGet(h, RPMTAG_DIRNAMES, &td, HEADERGET_MINMEM);
	rpmtdSetTag(&td, RPMTAG_ORIGDIRNAMES);
	headerPut(h, &td, HEADERPUT_DEFAULT);
	rpmtdFreeData(&td);

	headerGet(h, RPMTAG_DIRINDEXES, &td, HEADERGET_MINMEM);
	rpmtdSetTag(&td, RPMTAG_ORIGDIRINDEXES);
	headerPut(h, &td, HEADERPUT_DEFAULT);
	rpmtdFreeData(&td);

	headerMod(h, bnames);
	headerMod(h, dnames);
	headerMod(h, dindexes);
}

void rpmRelocateFileList(rpmRelocation *relocations, int numRelocations, 
			 rpmfs fs, Header h)
{
    static int _printed = 0;
    char ** baseNames;
    char ** dirNames;
    uint32_t * dirIndexes;
    rpm_count_t fileCount, dirCount;
    int nrelocated = 0;
    int fileAlloced = 0;
    char * fn = NULL;
    int haveRelocatedBase = 0;
    size_t maxlen = 0;
    int i, j;
    struct rpmtd_s bnames, dnames, dindexes, fmodes;

    addPrefixes(h, relocations, numRelocations);

    if (!_printed) {
	_printed = 1;
	rpmlog(RPMLOG_DEBUG, "========== relocations\n");
	for (i = 0; i < numRelocations; i++) {
	    if (relocations[i].oldPath == NULL) continue; /* XXX can't happen */
	    if (relocations[i].newPath == NULL)
		rpmlog(RPMLOG_DEBUG, "%5d exclude  %s\n",
			i, relocations[i].oldPath);
	    else
		rpmlog(RPMLOG_DEBUG, "%5d relocate %s -> %s\n",
			i, relocations[i].oldPath, relocations[i].newPath);
	}
    }

    for (i = 0; i < numRelocations; i++) {
	if (relocations[i].newPath == NULL) continue;
	size_t len = strlen(relocations[i].newPath);
	if (len > maxlen) maxlen = len;
    }

    headerGet(h, RPMTAG_BASENAMES, &bnames, HEADERGET_MINMEM);
    headerGet(h, RPMTAG_DIRINDEXES, &dindexes, HEADERGET_ALLOC);
    headerGet(h, RPMTAG_DIRNAMES, &dnames, HEADERGET_MINMEM);
    headerGet(h, RPMTAG_FILEMODES, &fmodes, HEADERGET_MINMEM);
    /* TODO XXX ugh.. use rpmtd iterators & friends instead */
    baseNames = bnames.data;
    dirIndexes = dindexes.data;
    fileCount = rpmtdCount(&bnames);
    dirCount = rpmtdCount(&dnames);
    /* XXX TODO: use rpmtdDup() instead */
    dirNames = dnames.data = duparray(dnames.data, dirCount);
    dnames.flags |= RPMTD_PTR_ALLOCED;

    /*
     * For all relocations, we go through sorted file/relocation lists 
     * backwards so that /usr/local relocations take precedence over /usr 
     * ones.
     */

    /* Relocate individual paths. */

    for (i = fileCount - 1; i >= 0; i--) {
	rpmFileTypes ft;
	int fnlen;

	size_t len = maxlen +
		strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}

assert(fn != NULL);		/* XXX can't happen */
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
	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		continue;
	    len = !rstreq(relocations[j].oldPath, "/")
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

	    if (!rstreqn(relocations[j].oldPath, fn, len))
		continue;
	    break;
	}
	if (j < 0) continue;

	rpmtdSetIndex(&fmodes, i);
	ft = rpmfiWhatis(rpmtdGetNumber(&fmodes));

	/* On install, a relocate to NULL means skip the path. */
	if (relocations[j].newPath == NULL) {
	    if (ft == XDIR) {
		/* Start with the parent, looking for directory to exclude. */
		for (j = dirIndexes[i]; j < dirCount; j++) {
		    len = strlen(dirNames[j]) - 1;
		    while (len > 0 && dirNames[j][len-1] == '/') len--;
		    if (fnlen != len)
			continue;
		    if (!rstreqn(fn, dirNames[j], fnlen))
			continue;
		    break;
		}
	    }
	    rpmfsSetAction(fs, i, FA_SKIPNSTATE);
	    rpmlog(RPMLOG_DEBUG, "excluding %s %s\n",
		   ftstring(ft), fn);
	    continue;
	}

	/* Relocation on full paths only, please. */
	if (fnlen != len) continue;

	rpmlog(RPMLOG_DEBUG, "relocating %s to %s\n",
	       fn, relocations[j].newPath);
	nrelocated++;

	strcpy(fn, relocations[j].newPath);
	{   char * te = strrchr(fn, '/');
	    if (te) {
		if (te > fn) te++;	/* root is special */
		fnlen = te - fn;
	    } else
		te = fn + strlen(fn);
	    if (!rstreq(baseNames[i], te)) { /* basename changed too? */
		if (!haveRelocatedBase) {
		    /* XXX TODO: use rpmtdDup() instead */
		    bnames.data = baseNames = duparray(baseNames, fileCount);
		    bnames.flags |= RPMTD_PTR_ALLOCED;
		    haveRelocatedBase = 1;
		}
		free(baseNames[i]);
		baseNames[i] = xstrdup(te);
	    }
	    *te = '\0';			/* terminate new directory name */
	}

	/* Does this directory already exist in the directory list? */
	for (j = 0; j < dirCount; j++) {
	    if (fnlen != strlen(dirNames[j]))
		continue;
	    if (!rstreqn(fn, dirNames[j], fnlen))
		continue;
	    break;
	}
	
	if (j < dirCount) {
	    dirIndexes[i] = j;
	    continue;
	}

	/* Creating new paths is a pita */
	dirNames = dnames.data = xrealloc(dnames.data, 
			       sizeof(*dirNames) * (dirCount + 1));

	dirNames[dirCount] = xstrdup(fn);
	dirIndexes[i] = dirCount;
	dirCount++;
	dnames.count++;
    }

    /* Finish off by relocating directories. */
    for (i = dirCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {

	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		continue;
	    size_t len = !rstreq(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (len && !rstreqn(relocations[j].oldPath, dirNames[i], len))
		continue;

	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (dirNames[i][len] != '/')
		continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		char *t = NULL;
		rstrscat(&t, relocations[j].newPath, (dirNames[i] + len), NULL);
		/* Unfortunately rpmCleanPath strips the trailing slash.. */
		(void) rpmCleanPath(t);
		rstrcat(&t, "/");

		rpmlog(RPMLOG_DEBUG,
		       "relocating directory %s to %s\n", dirNames[i], t);
		free(dirNames[i]);
		dirNames[i] = t;
		nrelocated++;
	    }
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (nrelocated) {
	saveRelocs(h, &bnames, &dnames, &dindexes);
    }

    rpmtdFreeData(&bnames);
    rpmtdFreeData(&dnames);
    rpmtdFreeData(&dindexes);
    rpmtdFreeData(&fmodes);
    free(fn);
}

rpmfi rpmfiFree(rpmfi fi)
{
    if (fi == NULL) return NULL;

    if (fi->nrefs > 1)
	return rpmfiUnlink(fi);

    if (fi->fc > 0) {
	fi->bnid = _free(fi->bnid);
	fi->dnid = _free(fi->dnid);
	fi->dil = _free(fi->dil);

	fi->flinks = _free(fi->flinks);
	fi->flangs = _free(fi->flangs);
	fi->digests = _free(fi->digests);
	fi->fcaps = _free(fi->fcaps);

	fi->cdict = _free(fi->cdict);

	fi->fuser = _free(fi->fuser);
	fi->fgroup = _free(fi->fgroup);

	fi->fstates = _free(fi->fstates);
	fi->fps = _free(fi->fps);

	fi->pool = rpmstrPoolFree(fi->pool);

	/* these point to header memory if KEEPHEADER is used, dont free */
	if (!(fi->fiflags & RPMFI_KEEPHEADER) && fi->h == NULL) {
	    fi->fmtimes = _free(fi->fmtimes);
	    fi->fmodes = _free(fi->fmodes);
	    fi->fflags = _free(fi->fflags);
	    fi->vflags = _free(fi->vflags);
	    fi->fsizes = _free(fi->fsizes);
	    fi->frdevs = _free(fi->frdevs);
	    fi->finodes = _free(fi->finodes);

	    fi->fcolors = _free(fi->fcolors);
	    fi->fcdictx = _free(fi->fcdictx);
	    fi->ddict = _free(fi->ddict);
	    fi->fddictx = _free(fi->fddictx);
	    fi->fddictn = _free(fi->fddictn);

	}
    }

    fi->fn = _free(fi->fn);
    fi->apath = _free(fi->apath);

    fi->replacedSizes = _free(fi->replacedSizes);

    fi->h = headerFree(fi->h);

    (void) rpmfiUnlink(fi);
    memset(fi, 0, sizeof(*fi));		/* XXX trash and burn */
    fi = _free(fi);

    return NULL;
}

static rpmsid * tag2pool(rpmstrPool pool, Header h, rpmTag tag)
{
    rpmsid *sids = NULL;
    struct rpmtd_s td;
    if (headerGet(h, tag, &td, HEADERGET_MINMEM)) {
	sids = rpmtdToPool(&td, pool);
	rpmtdFreeData(&td);
    }
    return sids;
}

/* validate a indexed tag data triplet (such as file bn/dn/dx) */
static int indexSane(rpmtd xd, rpmtd yd, rpmtd zd)
{
    int sane = 0;
    uint32_t xc = rpmtdCount(xd);
    uint32_t yc = rpmtdCount(yd);
    uint32_t zc = rpmtdCount(zd);

    /* check that the amount of data in each is sane */
    if (xc > 0 && yc > 0 && yc <= xc && zc == xc) {
	uint32_t * i;
	/* ...and that the indexes are within bounds */
	while ((i = rpmtdNextUint32(zd))) {
	    if (*i >= yc)
		break;
	}
	/* unless the loop runs to finish, the data is broken */
	sane = (i == NULL);
    }
    return sane;
}

#define _hgfi(_h, _tag, _td, _flags, _data) \
    if (headerGet((_h), (_tag), (_td), (_flags))) \
	_data = (td.data)

static int rpmfiPopulate(rpmfi fi, Header h, rpmfiFlags flags)
{
    headerGetFlags scareFlags = (flags & RPMFI_KEEPHEADER) ? 
				HEADERGET_MINMEM : HEADERGET_ALLOC;
    headerGetFlags defFlags = HEADERGET_ALLOC;
    struct rpmtd_s fdigests, digalgo, td;
    unsigned char * t;

    /* XXX TODO: all these should be sanity checked, ugh... */
    if (!(flags & RPMFI_NOFILEMODES))
	_hgfi(h, RPMTAG_FILEMODES, &td, scareFlags, fi->fmodes);
    if (!(flags & RPMFI_NOFILEFLAGS))
	_hgfi(h, RPMTAG_FILEFLAGS, &td, scareFlags, fi->fflags);
    if (!(flags & RPMFI_NOFILEVERIFYFLAGS))
	_hgfi(h, RPMTAG_FILEVERIFYFLAGS, &td, scareFlags, fi->vflags);
    if (!(flags & RPMFI_NOFILESIZES))
	_hgfi(h, RPMTAG_FILESIZES, &td, scareFlags, fi->fsizes);

    if (!(flags & RPMFI_NOFILECOLORS))
	_hgfi(h, RPMTAG_FILECOLORS, &td, scareFlags, fi->fcolors);

    if (!(flags & RPMFI_NOFILECLASS)) {
	_hgfi(h, RPMTAG_CLASSDICT, &td, scareFlags, fi->cdict);
	fi->ncdict = rpmtdCount(&td);
	_hgfi(h, RPMTAG_FILECLASS, &td, scareFlags, fi->fcdictx);
    }
    if (!(flags & RPMFI_NOFILEDEPS)) {
	_hgfi(h, RPMTAG_DEPENDSDICT, &td, scareFlags, fi->ddict);
	fi->nddict = rpmtdCount(&td);
	_hgfi(h, RPMTAG_FILEDEPENDSX, &td, scareFlags, fi->fddictx);
	_hgfi(h, RPMTAG_FILEDEPENDSN, &td, scareFlags, fi->fddictn);
    }

    if (!(flags & RPMFI_NOFILESTATES))
	_hgfi(h, RPMTAG_FILESTATES, &td, defFlags, fi->fstates);

    if (!(flags & RPMFI_NOFILECAPS))
	_hgfi(h, RPMTAG_FILECAPS, &td, defFlags, fi->fcaps);

    if (!(flags & RPMFI_NOFILELINKTOS))
	fi->flinks = tag2pool(fi->pool, h, RPMTAG_FILELINKTOS);
    /* FILELANGS are only interesting when installing */
    if ((headerGetInstance(h) == 0) && !(flags & RPMFI_NOFILELANGS))
	fi->flangs = tag2pool(fi->pool, h, RPMTAG_FILELANGS);

    /* See if the package has non-md5 file digests */
    fi->digestalgo = PGPHASHALGO_MD5;
    if (headerGet(h, RPMTAG_FILEDIGESTALGO, &digalgo, HEADERGET_MINMEM)) {
	uint32_t *algo = rpmtdGetUint32(&digalgo);
	/* Hmm, what to do with unknown digest algorithms? */
	if (algo && rpmDigestLength(*algo) != 0) {
	    fi->digestalgo = *algo;
	}
    }

    fi->digests = NULL;
    /* grab hex digests from header and store in binary format */
    if (!(flags & RPMFI_NOFILEDIGESTS) &&
	headerGet(h, RPMTAG_FILEDIGESTS, &fdigests, HEADERGET_MINMEM)) {
	const char *fdigest;
	size_t diglen = rpmDigestLength(fi->digestalgo);
	fi->digests = t = xmalloc(rpmtdCount(&fdigests) * diglen);

	while ((fdigest = rpmtdNextString(&fdigests))) {
	    if (!(fdigest && *fdigest != '\0')) {
		memset(t, 0, diglen);
		t += diglen;
		continue;
	    }
	    for (int j = 0; j < diglen; j++, t++, fdigest += 2)
		*t = (rnibble(fdigest[0]) << 4) | rnibble(fdigest[1]);
	}
	rpmtdFreeData(&fdigests);
    }

    /* XXX TR_REMOVED doesn;t need fmtimes, frdevs, finodes */
    if (!(flags & RPMFI_NOFILEMTIMES))
	_hgfi(h, RPMTAG_FILEMTIMES, &td, scareFlags, fi->fmtimes);
    if (!(flags & RPMFI_NOFILERDEVS))
	_hgfi(h, RPMTAG_FILERDEVS, &td, scareFlags, fi->frdevs);
    if (!(flags & RPMFI_NOFILEINODES))
	_hgfi(h, RPMTAG_FILEINODES, &td, scareFlags, fi->finodes);

    if (!(flags & RPMFI_NOFILEUSER)) 
	fi->fuser = tag2pool(fi->pool, h, RPMTAG_FILEUSERNAME);
    if (!(flags & RPMFI_NOFILEGROUP)) 
	fi->fgroup = tag2pool(fi->pool, h, RPMTAG_FILEGROUPNAME);

    /* TODO: validate and return a real error */
    return 0;
}

rpmfi rpmfiNewPool(rpmstrPool pool, Header h, rpmTagVal tagN, rpmfiFlags flags)
{
    rpmfi fi = xcalloc(1, sizeof(*fi)); 
    struct rpmtd_s bn, dn, dx;

    fi->magic = RPMFIMAGIC;
    fi->i = -1;
    fi->fiflags = flags;

    /*
     * Grab and validate file triplet data. Headers with no files simply
     * fall through here and an empty file set is returned.
     */
    if (headerGet(h, RPMTAG_BASENAMES, &bn, HEADERGET_MINMEM)) {
	headerGet(h, RPMTAG_DIRNAMES, &dn, HEADERGET_MINMEM);
	headerGet(h, RPMTAG_DIRINDEXES, &dx, HEADERGET_ALLOC);

	if (indexSane(&bn, &dn, &dx)) {
	    /* private or shared pool? */
	    fi->pool = (pool != NULL) ? rpmstrPoolLink(pool) :
					rpmstrPoolCreate();

	    /* init the file triplet data */
	    fi->fc = rpmtdCount(&bn);
	    fi->dc = rpmtdCount(&dn);
	    fi->bnid = rpmtdToPool(&bn, fi->pool);
	    fi->dnid = rpmtdToPool(&dn, fi->pool);
	    /* steal index data from the td (pooh...) */
	    fi->dil = dx.data;
	    dx.data = NULL;

	    /* populate the rest of the stuff */
	    rpmfiPopulate(fi, h, flags);

	    /* freeze the pool to save memory, but only if private pool */
	    if (fi->pool != pool)
		rpmstrPoolFreeze(fi->pool, 0);

	    fi->h = (fi->fiflags & RPMFI_KEEPHEADER) ? headerLink(h) : NULL;
	} else {
	    /* broken data, free and return NULL */
	    fi = _free(fi);
	}
	rpmtdFreeData(&bn);
	rpmtdFreeData(&dn);
	rpmtdFreeData(&dx);
    }

    return rpmfiLink(fi);
}

rpmfi rpmfiNew(const rpmts ts, Header h, rpmTagVal tagN, rpmfiFlags flags)
{
    return rpmfiNewPool(NULL, h, tagN, flags);
}

void rpmfiSetFReplacedSizeIndex(rpmfi fi, int ix, rpm_loff_t newsize)
{
    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->replacedSizes == NULL) {
	    fi->replacedSizes = xcalloc(fi->fc, sizeof(*fi->replacedSizes));
	}
	/* XXX watch out, replacedSizes is not rpm_loff_t (yet) */
	fi->replacedSizes[ix] = (rpm_off_t) newsize;
    }
}

rpm_loff_t rpmfiFReplacedSizeIndex(rpmfi fi, int ix)
{
    rpm_loff_t rsize = 0;
    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->replacedSizes) {
	    rsize = fi->replacedSizes[ix];
	}
    }
    return rsize;
}

void rpmfiFpLookup(rpmfi fi, fingerPrintCache fpc)
{
    /* This can get called twice (eg yum), scratch former results and redo */
    if (fi->fc > 0) {
	if (fi->fps)
	    free(fi->fps);
	fi->fps = fpLookupList(fpc, fi->pool,
			       fi->dnid, fi->bnid, fi->dil, fi->fc);
    }
}

/* 
 * Generate iterator accessors function wrappers, these do nothing but
 * call the corresponding rpmfiFooIndex(fi, fi->[ij])
 */

#define RPMFI_ITERFUNC(TYPE, NAME, IXV) \
    TYPE rpmfi ## NAME(rpmfi fi) { return rpmfi ## NAME ## Index(fi, fi ? fi->IXV : -1); }

RPMFI_ITERFUNC(rpmsid, BNId, i)
RPMFI_ITERFUNC(rpmsid, DNId, j)
RPMFI_ITERFUNC(const char *, BN, i)
RPMFI_ITERFUNC(const char *, DN, j)
RPMFI_ITERFUNC(const char *, FLink, i)
RPMFI_ITERFUNC(const char *, FUser, i)
RPMFI_ITERFUNC(const char *, FGroup, i)
RPMFI_ITERFUNC(const char *, FCaps, i)
RPMFI_ITERFUNC(const char *, FLangs, i)
RPMFI_ITERFUNC(const char *, FClass, i)
RPMFI_ITERFUNC(rpmfileState, FState, i)
RPMFI_ITERFUNC(rpmfileAttrs, FFlags, i)
RPMFI_ITERFUNC(rpmVerifyAttrs, VFlags, i)
RPMFI_ITERFUNC(rpm_mode_t, FMode, i)
RPMFI_ITERFUNC(rpm_rdev_t, FRdev, i)
RPMFI_ITERFUNC(rpm_time_t, FMtime, i)
RPMFI_ITERFUNC(rpm_ino_t, FInode, i)
RPMFI_ITERFUNC(rpm_loff_t, FSize, i)
RPMFI_ITERFUNC(rpm_color_t, FColor, i)
RPMFI_ITERFUNC(uint32_t, FNlink, i)

const char * rpmfiFN(rpmfi fi)
{
    const char *fn = ""; /* preserve behavior on errors */
    if (fi != NULL) {
	free(fi->fn);
	fi->fn = rpmfiFNIndex(fi, fi->i);
	if (fi->fn != NULL)
	    fn = fi->fn;
    }
    return fn;
}

const unsigned char * rpmfiFDigest(rpmfi fi, int *algo, size_t *len)
{
    return rpmfiFDigestIndex(fi, fi ? fi->i : -1, algo, len);
}

uint32_t rpmfiFDepends(rpmfi fi, const uint32_t ** fddictp)
{
    return rpmfiFDependsIndex(fi,  fi ? fi->i : -1, fddictp);
}

int rpmfiCompare(const rpmfi afi, const rpmfi bfi)
{
    return rpmfiCompareIndex(afi, afi ? afi->i : -1, bfi, bfi ? bfi->i : -1);
}

rpmFileAction rpmfiDecideFate(const rpmfi ofi, rpmfi nfi, int skipMissing)
{
    return rpmfiDecideFateIndex(ofi, ofi ? ofi->i : -1,
				nfi, nfi ? nfi->i : -1,
				skipMissing);
}

int rpmfiConfigConflict(const rpmfi fi)
{
    return rpmfiConfigConflictIndex(fi, fi ? fi->i : -1);
}

rpmstrPool rpmfiPool(rpmfi fi)
{
    return (fi != NULL) ? fi->pool : NULL;
}
