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
#include "lib/fsm.h"	/* XXX newFSM() */

#include "debug.h"

/*
 * Simple and stupid string "cache."
 * Store each unique string just once, retrieve by index value. 
 * For data where number of unique names is typically very low,
 * the dumb linear lookup appears to be fast enough and hash table seems
 * like an overkill.
 */
struct strcache_s {
    char **uniq;
    scidx_t num;
};

static struct strcache_s _ugcache = { NULL, 0 };
static strcache ugcache = &_ugcache;
static struct strcache_s _langcache = { NULL, 0 };
static strcache langcache = &_langcache;

static scidx_t strcachePut(strcache cache, const char *str)
{
    int found = 0;
    scidx_t ret;

    for (scidx_t i = 0; i < cache->num; i++) {
	if (strcmp(str, cache->uniq[i]) == 0) {
	    ret = i;
	    found = 1;
	    break;
	}
    }
    if (!found) {
	/* blow up on index wraparound */
	assert((scidx_t)(cache->num + 1) > cache->num);
	cache->uniq = xrealloc(cache->uniq, 
				sizeof(cache->uniq) * (cache->num+1));
	cache->uniq[cache->num] = xstrdup(str);
	ret = cache->num;
	cache->num++;
    }
    return ret;
}

static const char *strcacheGet(strcache cache, scidx_t idx)
{
    const char *name = NULL;
    if (idx >= 0 && idx < cache->num && cache->uniq != NULL)
	name = cache->uniq[idx];
    return name;
}
    
static strcache strcacheNew(void)
{
    strcache cache = xcalloc(1, sizeof(*cache));
    return cache;
}

static strcache strcacheFree(strcache cache)
{
    if (cache != NULL) {
	for (scidx_t i = 0; i < cache->num; i++) {
	    free(cache->uniq[i]);
	}
	cache->uniq = _free(cache->uniq);
	free(cache);
    }
    return NULL;
} 


int _rpmfi_debug = 0;

rpmfi rpmfiUnlink(rpmfi fi, const char * msg)
{
    if (fi == NULL) return NULL;
if (_rpmfi_debug && msg != NULL)
fprintf(stderr, "--> fi %p -- %d %s\n", fi, fi->nrefs, msg);
    fi->nrefs--;
    return NULL;
}

rpmfi rpmfiLink(rpmfi fi, const char * msg)
{
    if (fi == NULL) return NULL;
    fi->nrefs++;
if (_rpmfi_debug && msg != NULL)
fprintf(stderr, "--> fi %p ++ %d %s\n", fi, fi->nrefs, msg);
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

const char * rpmfiBNIndex(rpmfi fi, int ix)
{
    const char * BN = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->bnl != NULL)
	    BN = fi->bnl[ix];
    }
    return BN;
}

const char * rpmfiDNIndex(rpmfi fi, int jx)
{
    const char * DN = NULL;

    if (fi != NULL && jx >= 0 && jx < fi->dc) {
	if (fi->dnl != NULL)
	    DN = fi->dnl[jx];
    }
    return DN;
}

const char * rpmfiFNIndex(rpmfi fi, int ix)
{
    const char * FN = "";

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	char * t;
	if (fi->fn == NULL) {
	    size_t dnlmax = 0, bnlmax = 0, len;
	    for (int i = 0; i < fi->dc; i++) {
		if ((len = strlen(fi->dnl[i])) > dnlmax)
		    dnlmax = len;
	    }
	    for (int i = 0; i < fi->fc; i++) {
		if ((len = strlen(fi->bnl[i])) > bnlmax)
		    bnlmax = len;
	    }
	    fi->fn = xmalloc(dnlmax + bnlmax + 1);
	}
	FN = t = fi->fn;
	*t = '\0';
	t = stpcpy(t, fi->dnl[fi->dil[ix]]);
	t = stpcpy(t, fi->bnl[ix]);
    }
    return FN;
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
    pgpHashAlgo algo = 0;

    digest = rpmfiFDigest(fi, &algo, NULL);
    return (algo == PGPHASHALGO_MD5) ? digest : NULL;
}

pgpHashAlgo rpmfiDigestAlgo(rpmfi fi)
{
    return fi ? fi->digestalgo : 0;
}

const unsigned char * rpmfiFDigestIndex(rpmfi fi, int ix, pgpHashAlgo *algo, size_t *len)
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

char * rpmfiFDigestHex(rpmfi fi, pgpHashAlgo *algo)
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
	    flink = strcacheGet(fi->flinkcache, fi->flinks[ix]);
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
	if (fi->finodes && fi->frdevs) {
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
	    fuser = strcacheGet(ugcache, fi->fuser[ix]);
    }
    return fuser;
}

const char * rpmfiFGroupIndex(rpmfi fi, int ix)
{
    const char * fgroup = NULL;

    if (fi != NULL && ix >= 0 && ix < fi->fc) {
	if (fi->fgroup != NULL)
	    fgroup = strcacheGet(ugcache, fi->fgroup[ix]);
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
	flangs = strcacheGet(langcache, fi->flangs[ix]);
    }
    return flangs;
}

struct fingerPrint_s *rpmfiFpsIndex(rpmfi fi, int ix)
{
    struct fingerPrint_s * fps = NULL;
    if (fi != NULL && fi->fps != NULL && ix >= 0 && ix < fi->fc) {
	fps = fi->fps + ix;
    }
    return fps;
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

if (_rpmfi_debug  < 0 && i != -1)
fprintf(stderr, "*** fi %p\t%s[%d] %s%s\n", fi, (fi->Type ? fi->Type : "?Type?"), i, (i >= 0 ? fi->dnl[fi->j] : ""), (i >= 0 ? fi->bnl[fi->i] : ""));

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

if (_rpmfi_debug  < 0 && j != -1)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, (fi->Type ? fi->Type : "?Type?"), j);

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

int rpmfiCompare(const rpmfi afi, const rpmfi bfi)
{
    rpmFileTypes awhat = rpmfiWhatis(rpmfiFMode(afi));
    rpmFileTypes bwhat = rpmfiWhatis(rpmfiFMode(bfi));

    if ((rpmfiFFlags(afi) & RPMFILE_GHOST) ||
	(rpmfiFFlags(bfi) & RPMFILE_GHOST)) return 0;

    if (awhat != bwhat) return 1;

    if (awhat == LINK) {
	const char * alink = rpmfiFLink(afi);
	const char * blink = rpmfiFLink(bfi);
	if (alink == blink) return 0;
	if (alink == NULL) return 1;
	if (blink == NULL) return -1;
	return strcmp(alink, blink);
    } else if (awhat == REG) {
	size_t adiglen, bdiglen;
	pgpHashAlgo aalgo, balgo;
	const unsigned char * adigest = rpmfiFDigest(afi, &aalgo, &adiglen);
	const unsigned char * bdigest = rpmfiFDigest(bfi, &balgo, &bdiglen);
	if (adigest == bdigest) return 0;
	if (adigest == NULL) return 1;
	if (bdigest == NULL) return -1;
	/* can't meaningfully compare different hash types */
	if (aalgo != balgo || adiglen != bdiglen) return -1;
	return memcmp(adigest, bdigest, adiglen);
    }

    return 0;
}

rpmFileAction rpmfiDecideFate(const rpmfi ofi, rpmfi nfi, int skipMissing)
{
    const char * fn = rpmfiFN(nfi);
    rpmfileAttrs newFlags = rpmfiFFlags(nfi);
    char buffer[1024];
    rpmFileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int save = (newFlags & RPMFILE_NOREPLACE) ? FA_ALTNAME : FA_SAVE;

    if (lstat(fn, &sb)) {
	/*
	 * The file doesn't exist on the disk. Create it unless the new
	 * package has marked it as missingok, or allfiles is requested.
	 */
	if (skipMissing && (newFlags & RPMFILE_MISSINGOK)) {
	    rpmlog(RPMLOG_DEBUG, "%s skipped due to missingok flag\n",
			fn);
	    return FA_SKIP;
	} else {
	    return FA_CREATE;
	}
    }

    diskWhat = rpmfiWhatis((rpm_mode_t)sb.st_mode);
    dbWhat = rpmfiWhatis(rpmfiFMode(ofi));
    newWhat = rpmfiWhatis(rpmfiFMode(nfi));

    /*
     * RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
     * them in older packages as well.
     */
    if (newWhat == XDIR)
	return FA_CREATE;

    if (diskWhat != newWhat && dbWhat != REG && dbWhat != LINK)
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
    memset(buffer, 0, sizeof(buffer));
    if (dbWhat == REG) {
	pgpHashAlgo oalgo, nalgo;
	size_t odiglen, ndiglen;
	const unsigned char * odigest, * ndigest;
	odigest = rpmfiFDigest(ofi, &oalgo, &odiglen);
	if (diskWhat == REG) {
	    if (rpmDoDigest(oalgo, fn, 0, 
		(unsigned char *)buffer, NULL))
	        return FA_CREATE;	/* assume file has been removed */
	    if (odigest && !memcmp(odigest, buffer, odiglen))
	        return FA_CREATE;	/* unmodified config file, replace. */
	}
	ndigest = rpmfiFDigest(nfi, &nalgo, &ndiglen);
	/* XXX can't compare different hash types, what should we do here? */
	if (oalgo != nalgo || odiglen != ndiglen)
	    return FA_CREATE;
	if (odigest && ndigest && !memcmp(odigest, ndigest, odiglen))
	    return FA_SKIP;	/* identical file, don't bother. */
    } else /* dbWhat == LINK */ {
	const char * oFLink, * nFLink;
	oFLink = rpmfiFLink(ofi);
	if (diskWhat == LINK) {
	    if (readlink(fn, buffer, sizeof(buffer) - 1) == -1)
		return FA_CREATE;	/* assume file has been removed */
	    if (oFLink && !strcmp(oFLink, buffer))
		return FA_CREATE;	/* unmodified config file, replace. */
	}
	nFLink = rpmfiFLink(nfi);
	if (oFLink && nFLink && !strcmp(oFLink, nFLink))
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

int rpmfiConfigConflict(const rpmfi fi)
{
    const char * fn = rpmfiFN(fi);
    rpmfileAttrs flags = rpmfiFFlags(fi);
    char buffer[1024];
    rpmFileTypes newWhat, diskWhat;
    struct stat sb;

    if (!(flags & RPMFILE_CONFIG) || lstat(fn, &sb)) {
	return 0;
    }

    diskWhat = rpmfiWhatis((rpm_mode_t)sb.st_mode);
    newWhat = rpmfiWhatis(rpmfiFMode(fi));

    if (newWhat != LINK && newWhat != REG)
	return 1;

    if (diskWhat != newWhat)
	return 1;
    
    memset(buffer, 0, sizeof(buffer));
    if (newWhat == REG) {
	pgpHashAlgo algo;
	size_t diglen;
	const unsigned char *ndigest = rpmfiFDigest(fi, &algo, &diglen);
	if (rpmDoDigest(algo, fn, 0, (unsigned char *)buffer, NULL))
	    return 0;	/* assume file has been removed */
	if (ndigest && !memcmp(ndigest, buffer, diglen))
	    return 0;	/* unmodified config file */
    } else /* newWhat == LINK */ {
	const char * nFLink;
	if (readlink(fn, buffer, sizeof(buffer) - 1) == -1)
	    return 0;	/* assume file has been removed */
	nFLink = rpmfiFLink(fi);
	if (nFLink && !strcmp(nFLink, buffer))
	    return 0;	/* unmodified config file */
    }

    return 1;
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

/**
 * Relocate files in header.
 * @todo multilib file dispositions need to be checked.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param origH		package header
 * @return		header with relocated files
 */
static
Header relocateFileList(const rpmts ts, rpmfi fi,
		Header origH)
{
    rpmte p = rpmtsRelocateElement(ts);
    static int _printed = 0;
    int allowBadRelocate = (rpmtsFilterFlags(ts) & RPMPROB_FILTER_FORCERELOCATE);
    rpmRelocation * relocations = NULL;
    int numRelocations;
    char ** baseNames;
    char ** dirNames;
    uint32_t * dirIndexes;
    uint32_t * newDirIndexes;
    rpm_count_t fileCount, dirCount, numValid = 0;
    rpm_color_t * fColors = NULL;
    rpm_color_t * dColors = NULL;
    rpm_mode_t * fModes = NULL;
    Header h;
    int nrelocated = 0;
    int fileAlloced = 0;
    char * fn = NULL;
    int haveRelocatedBase = 0;
    int reldel = 0;
    int len;
    int i, j;
    struct rpmtd_s validRelocs;
    struct rpmtd_s bnames, dnames, dindexes, fcolors, fmodes;

    
    if (headerGet(origH, RPMTAG_PREFIXES, &validRelocs, HEADERGET_MINMEM)) 
	numValid = rpmtdCount(&validRelocs);

assert(p != NULL);
    numRelocations = 0;
    if (p->relocs)
	while (p->relocs[numRelocations].newPath ||
	       p->relocs[numRelocations].oldPath)
	    numRelocations++;

    /*
     * If no relocations are specified (usually the case), then return the
     * original header. If there are prefixes, however, then INSTPREFIXES
     * should be added, but, since relocateFileList() can be called more
     * than once for the same header, don't bother if already present.
     */
    if (p->relocs == NULL || numRelocations == 0) {
	if (numValid) {
	    if (!headerIsEntry(origH, RPMTAG_INSTPREFIXES)) {
		rpmtdSetTag(&validRelocs, RPMTAG_INSTPREFIXES);
		headerPut(origH, &validRelocs, HEADERPUT_DEFAULT);
	    }
	    rpmtdFreeData(&validRelocs);
	}
	/* XXX FIXME multilib file actions need to be checked. */
	return headerLink(origH);
    }

    h = headerLink(origH);

    relocations = xmalloc(sizeof(*relocations) * numRelocations);

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < numRelocations; i++) {
	char * t;

	/*
	 * Default relocations (oldPath == NULL) are handled in the UI,
	 * not rpmlib.
	 */
	if (p->relocs[i].oldPath == NULL) continue; /* XXX can't happen */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	t = xstrdup(p->relocs[i].oldPath);
	relocations[i].oldPath = (t[0] == '/' && t[1] == '\0')
	    ? t
	    : stripTrailingChar(t, '/');

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (p->relocs[i].newPath) {
	    int del;
	    int valid = 0;
	    const char *validprefix;

	    t = xstrdup(p->relocs[i].newPath);
	    relocations[i].newPath = (t[0] == '/' && t[1] == '\0')
		? t
		: stripTrailingChar(t, '/');

	   	/* FIX:  relocations[i].oldPath == NULL */
	    /* Verify that the relocation's old path is in the header. */
	    rpmtdInit(&validRelocs);
	    while ((validprefix = rpmtdNextString(&validRelocs))) {
		if (strcmp(validprefix, relocations[i].oldPath) == 0) {
		    valid = 1;
		    break;
		}
	    }

	    if (!valid && !allowBadRelocate) {
		rpmps ps = rpmtsProblems(ts);
		rpmpsAppend(ps, RPMPROB_BADRELOCATE,
			rpmteNEVRA(p), rpmteKey(p),
			relocations[i].oldPath, NULL, NULL, 0);
		ps = rpmpsFree(ps);
	    }
	    del =
		strlen(relocations[i].newPath) - strlen(relocations[i].oldPath);

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
	    /* LCL: ??? */
	    tmpReloc = relocations[j - 1];
	    relocations[j - 1] = relocations[j];
	    relocations[j] = tmpReloc;
	    madeSwap = 1;
	}
	if (!madeSwap) break;
    }

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

    /* Add relocation values to the header */
    if (numValid) {
	const char *validprefix;
	const char ** actualRelocations;
	rpm_count_t numActual;

	actualRelocations = xmalloc(numValid * sizeof(*actualRelocations));
	numActual = 0;
	rpmtdInit(&validRelocs);
	while ((validprefix = rpmtdNextString(&validRelocs))) {
	    for (j = 0; j < numRelocations; j++) {
		if (relocations[j].oldPath == NULL || /* XXX can't happen */
		    strcmp(validprefix, relocations[j].oldPath))
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

	if (numActual) {
	    headerPutStringArray(h, RPMTAG_INSTPREFIXES,
				     actualRelocations, numActual);
	}

	actualRelocations = _free(actualRelocations);
	rpmtdFreeData(&validRelocs);
    }

    headerGet(h, RPMTAG_BASENAMES, &bnames, fi->scareFlags);
    headerGet(h, RPMTAG_DIRINDEXES, &dindexes, fi->scareFlags);
    headerGet(h, RPMTAG_DIRNAMES, &dnames, fi->scareFlags);
    headerGet(h, RPMTAG_FILECOLORS, &fcolors, fi->scareFlags);
    headerGet(h, RPMTAG_FILEMODES, &fmodes, fi->scareFlags);
    /* TODO XXX ugh.. use rpmtd iterators & friends instead */
    baseNames = bnames.data;
    dirIndexes = dindexes.data;
    fColors = fcolors.data;
    fModes = fmodes.data;
    fileCount = rpmtdCount(&bnames);
    dirCount = rpmtdCount(&dnames);
    /* XXX TODO: use rpmtdDup() instead */
    dirNames = dnames.data = duparray(dnames.data, dirCount);
    dnames.flags |= RPMTD_PTR_ALLOCED;

    dColors = xcalloc(dirCount, sizeof(*dColors));

    newDirIndexes = xmalloc(sizeof(*newDirIndexes) * fileCount);
    memcpy(newDirIndexes, dirIndexes, sizeof(*newDirIndexes) * fileCount);
    dirIndexes = newDirIndexes;

    /*
     * For all relocations, we go through sorted file/relocation lists 
     * backwards so that /usr/local relocations take precedence over /usr 
     * ones.
     */

    /* Relocate individual paths. */

    for (i = fileCount - 1; i >= 0; i--) {
	rpmFileTypes ft;
	int fnlen;

	len = reldel +
		strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}

assert(fn != NULL);		/* XXX can't happen */
	*fn = '\0';
	fnlen = stpcpy( stpcpy(fn, dirNames[dirIndexes[i]]), baseNames[i]) - fn;

if (fColors != NULL) {
/* XXX pkgs may not have unique dirNames, so color all dirNames that match. */
for (j = 0; j < dirCount; j++) {
if (strcmp(dirNames[dirIndexes[i]], dirNames[j])) continue;
dColors[j] |= fColors[i];
}
}

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
	    break;
	}
	if (j < 0) continue;

/* FIX: fModes may be NULL */
	ft = rpmfiWhatis(fModes[i]);

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
		    break;
		}
	    }
	    rpmfsSetAction(rpmteGetFileStates(p), i, FA_SKIPNSTATE);
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
	    if (strcmp(baseNames[i], te)) { /* basename changed too? */
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
	    if (strncmp(fn, dirNames[j], fnlen))
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
		char *t = NULL;
		rstrscat(&t, relocations[j].newPath, (dirNames[i] + len), NULL);
		/* Unfortunatly rpmCleanPath strips the trailing slash.. */
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

	if (rpmtdFromStringArray(&td, RPMTAG_BASENAMES, (const char**) baseNames, fileCount)) {
	    headerMod(h, &td);
	}
	free(fi->bnl);
	headerGet(h, RPMTAG_BASENAMES, &td, fi->scareFlags);
	fi->fc = rpmtdCount(&td);
	fi->bnl = td.data;

	if (rpmtdFromStringArray(&td, RPMTAG_DIRNAMES, (const char**) dirNames, dirCount)) {
	    headerMod(h, &td);
	}
	free(fi->dnl);
	headerGet(h, RPMTAG_DIRNAMES, &td, fi->scareFlags);
	fi->dc = rpmtdCount(&td);
	fi->dnl = td.data;

	if (rpmtdFromUint32(&td, RPMTAG_DIRINDEXES, dirIndexes, fileCount)) {
	    headerMod(h, &td);
	}
	headerGet(h, RPMTAG_DIRINDEXES, &td, fi->scareFlags);
	/* Ugh, nasty games with how dil is alloced depending on scareMem */
	if (fi->scareFlags & HEADERGET_ALLOC)
	    free(fi->dil);
	fi->dil = td.data;
    }

    rpmtdFreeData(&bnames);
    rpmtdFreeData(&dnames);
    rpmtdFreeData(&dindexes);
    rpmtdFreeData(&fcolors);
    rpmtdFreeData(&fmodes);
    free(fn);
    for (i = 0; i < numRelocations; i++) {
	free(relocations[i].oldPath);
	free(relocations[i].newPath);
    }
    free(relocations);
    free(newDirIndexes);
    free(dColors);

    return h;
}

rpmfi rpmfiFree(rpmfi fi)
{
    if (fi == NULL) return NULL;

    if (fi->nrefs > 1)
	return rpmfiUnlink(fi, fi->Type);

if (_rpmfi_debug < 0)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, fi->Type, fi->fc);

    if (fi->fc > 0) {
	fi->bnl = _free(fi->bnl);
	fi->dnl = _free(fi->dnl);

	fi->flinkcache = strcacheFree(fi->flinkcache);
	fi->flinks = _free(fi->flinks);
	fi->flangs = _free(fi->flangs);
	fi->digests = _free(fi->digests);
	fi->fcaps = _free(fi->fcaps);

	fi->cdict = _free(fi->cdict);

	fi->fuser = _free(fi->fuser);
	fi->fgroup = _free(fi->fgroup);

	fi->fstates = _free(fi->fstates);
	fi->fps = _free(fi->fps);

	if (!(fi->fiflags & RPMFI_KEEPHEADER) && fi->h == NULL) {
	    fi->fmtimes = _constfree(fi->fmtimes);
	    fi->fmodes = _free(fi->fmodes);
	    fi->fflags = _constfree(fi->fflags);
	    fi->vflags = _constfree(fi->vflags);
	    fi->fsizes = _constfree(fi->fsizes);
	    fi->frdevs = _constfree(fi->frdevs);
	    fi->finodes = _constfree(fi->finodes);
	    fi->dil = _free(fi->dil);

	    fi->fcolors = _constfree(fi->fcolors);
	    fi->fcdictx = _constfree(fi->fcdictx);
	    fi->ddict = _constfree(fi->ddict);
	    fi->fddictx = _constfree(fi->fddictx);
	    fi->fddictn = _constfree(fi->fddictn);

	}
    }

    fi->fsm = freeFSM(fi->fsm);

    fi->fn = _free(fi->fn);
    fi->apath = _free(fi->apath);

    fi->replacedSizes = _free(fi->replacedSizes);

    fi->h = headerFree(fi->h);

    (void) rpmfiUnlink(fi, fi->Type);
    memset(fi, 0, sizeof(*fi));		/* XXX trash and burn */
    fi = _free(fi);

    return NULL;
}

/* Helper to push header tag data into a string cache */
static scidx_t *cacheTag(strcache cache, Header h, rpmTag tag)
{
    scidx_t *idx = NULL;
    struct rpmtd_s td;
    if (headerGet(h, tag, &td, HEADERGET_MINMEM)) {
       idx = xmalloc(sizeof(*idx) * rpmtdCount(&td));
       int i = 0;
       const char *str;
       while ((str = rpmtdNextString(&td))) {
	   idx[i++] = strcachePut(cache, str);
       }
       rpmtdFreeData(&td);
    }
    return idx;
}

#define _hgfi(_h, _tag, _td, _flags, _data) \
    if (headerGet((_h), (_tag), (_td), (_flags))) \
	_data = (td.data)

rpmfi rpmfiNew(const rpmts ts, Header h, rpmTag tagN, rpmfiFlags flags)
{
    rpmte p;
    rpmfi fi = NULL;
    const char * Type;
    rpm_loff_t *asize = NULL;
    unsigned char * t;
    int isBuild, isSource;
    struct rpmtd_s fdigests, digalgo;
    struct rpmtd_s td;
    headerGetFlags scareFlags = (flags & RPMFI_KEEPHEADER) ? 
				HEADERGET_MINMEM : HEADERGET_ALLOC;
    headerGetFlags defFlags = HEADERGET_ALLOC;

    if (tagN == RPMTAG_BASENAMES) {
	Type = "Files";
    } else {
	Type = "?Type?";
	goto exit;
    }

    fi = xcalloc(1, sizeof(*fi));
    if (fi == NULL)	/* XXX can't happen */
	goto exit;

    fi->magic = RPMFIMAGIC;
    fi->Type = Type;
    fi->i = -1;
    fi->tagN = tagN;

    fi->fiflags = flags;
    fi->scareFlags = scareFlags;

    fi->h = (fi->fiflags & RPMFI_KEEPHEADER) ? headerLink(h) : NULL;

    if (headerGet(h, RPMTAG_LONGARCHIVESIZE, &td, HEADERGET_EXT)) {
	asize = rpmtdGetUint64(&td);
    }
    /* 0 means unknown */
    fi->archiveSize = asize ? *asize : 0;
    rpmtdFreeData(&td);
    
    /* Archive size is not set when this gets called from build */
    isBuild = (asize == NULL);
    isSource = headerIsSource(h);
    if (isBuild) fi->fiflags |= RPMFI_ISBUILD;
    if (isSource) fi->fiflags |= RPMFI_ISSOURCE;

    _hgfi(h, RPMTAG_BASENAMES, &td, defFlags, fi->bnl);
    fi->fc = rpmtdCount(&td);
    if (fi->fc == 0) {
	goto exit;
    }

    _hgfi(h, RPMTAG_DIRNAMES, &td, defFlags, fi->dnl);
    fi->dc = rpmtdCount(&td);
    _hgfi(h, RPMTAG_DIRINDEXES, &td, scareFlags, fi->dil);
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

    if (!(flags & RPMFI_NOFILELINKTOS)) {
	fi->flinkcache = strcacheNew();
	fi->flinks = cacheTag(fi->flinkcache, h, RPMTAG_FILELINKTOS);
    }
    /* FILELANGS are only interesting when installing */
    if ((headerGetInstance(h) == 0) && !(flags & RPMFI_NOFILELANGS))
	fi->flangs = cacheTag(langcache, h, RPMTAG_FILELANGS);

    /* See if the package has non-md5 file digests */
    fi->digestalgo = PGPHASHALGO_MD5;
    if (headerGet(h, RPMTAG_FILEDIGESTALGO, &digalgo, HEADERGET_MINMEM)) {
	pgpHashAlgo *algo = rpmtdGetUint32(&digalgo);
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
	fi->fuser = cacheTag(ugcache, h, RPMTAG_FILEUSERNAME);
    if (!(flags & RPMFI_NOFILEGROUP)) 
	fi->fgroup = cacheTag(ugcache, h, RPMTAG_FILEGROUPNAME);

    if (ts != NULL)
    if (fi != NULL)
    if ((p = rpmtsRelocateElement(ts)) != NULL && rpmteType(p) == TR_ADDED
     && !headerIsSource(h)
     && !headerIsEntry(h, RPMTAG_ORIGBASENAMES))
    {
	Header foo;

	/* FIX: fi-digests undefined */
	foo = relocateFileList(ts, fi, h);
	fi->h = headerFree(fi->h);
	fi->h = headerLink(foo);
	foo = headerFree(foo);
    }

    if (!(fi->fiflags & RPMFI_KEEPHEADER)) {
	fi->h = headerFree(fi->h);
    }

    /* lazily alloced from rpmfiFN() */
    fi->fn = NULL;

exit:
if (_rpmfi_debug < 0)
fprintf(stderr, "*** fi %p\t%s[%d]\n", fi, Type, (fi ? fi->fc : 0));

    /* FIX: rpmfi null annotations */
    return rpmfiLink(fi, (fi ? fi->Type : NULL));
}

void rpmfiSetFReplacedSize(rpmfi fi, rpm_loff_t newsize)
{
    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
	if (fi->replacedSizes == NULL) {
	    fi->replacedSizes = xcalloc(fi->fc, sizeof(*fi->replacedSizes));
	}
	/* XXX watch out, replacedSizes is not rpm_loff_t (yet) */
	fi->replacedSizes[fi->i] = (rpm_off_t) newsize;
    }
}

rpm_loff_t rpmfiFReplacedSize(rpmfi fi)
{
    rpm_loff_t rsize = 0;
    if (fi != NULL && fi->i >= 0 && fi->i < fi->fc) {
	if (fi->replacedSizes) {
	    rsize = fi->replacedSizes[fi->i];
	}
    }
    return rsize;
}

void rpmfiFpLookup(rpmfi fi, fingerPrintCache fpc)
{
    if (fi->fc > 0 && fi->fps == NULL) {
	fi->fps = xcalloc(fi->fc, sizeof(*fi->fps));
    }
    fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fi->fc, fi->fps);
}

FSM_t rpmfiFSM(rpmfi fi)
{
    if (fi != NULL && fi->fsm == NULL) {
    	cpioMapFlags mapflags;
	/* Figure out mapflags: 
	 * - path, mode, uid and gid are used by everything
	 * - all binary packages get SBIT_CHECK set
	 * - if archive size is not known, we're only building this package,
	 *   different rules apply 
	 */
	mapflags = CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;
	if (fi->fiflags & RPMFI_ISBUILD) {
	    mapflags |= CPIO_MAP_TYPE;
	    if (fi->fiflags & RPMFI_ISSOURCE) mapflags |= CPIO_FOLLOW_SYMLINKS;
	} else {
	    if (!(fi->fiflags & RPMFI_ISSOURCE)) mapflags |= CPIO_SBIT_CHECK;
	}
	fi->fsm = newFSM(mapflags);
    }
    return (fi != NULL) ? fi->fsm : NULL;
}

/* 
 * Generate iterator accessors function wrappers, these do nothing but
 * call the corresponding rpmfiFooIndex(fi, fi->[ij])
 */

#define RPMFI_ITERFUNC(TYPE, NAME, IXV) \
    TYPE rpmfi ## NAME(rpmfi fi) { return rpmfi ## NAME ## Index(fi, fi ? fi->IXV : -1); }

RPMFI_ITERFUNC(const char *, BN, i)
RPMFI_ITERFUNC(const char *, DN, j)
RPMFI_ITERFUNC(const char *, FN, i)
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

const unsigned char * rpmfiFDigest(rpmfi fi, pgpHashAlgo *algo, size_t *len)
{
    return rpmfiFDigestIndex(fi, fi ? fi->i : -1, algo, len);
}

uint32_t rpmfiFDepends(rpmfi fi, const uint32_t ** fddictp)
{
    return rpmfiFDependsIndex(fi,  fi ? fi->i : -1, fddictp);
}
