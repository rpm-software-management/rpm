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
#include "lib/rpmug.h"
#include "rpmio/rpmio_internal.h"       /* fdInit/FiniDigest */

#include "debug.h"

struct hardlinks_s {
    int nlink;
    int files[];
};

typedef struct hardlinks_s * hardlinks_t;

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE nlinkHash
#define HTKEYTYPE int
#define HTDATATYPE struct hardlinks_s *
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

typedef int (*iterfunc)(rpmfi fi);

struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */
    iterfunc next;		/*!< Iterator function. */
    char * fn;			/*!< File name buffer. */

    rpmfiles files;		/*!< File info set */
    rpmcpio_t archive;		/*!< Archive with payload */
    char ** apath;
    int nrefs;			/*!< Reference count */
};

struct rpmfn_s {
    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

    rpmsid * bnid;		/*!< Index to base name(s) (pool) */
    rpmsid * dnid;		/*!< Index to directory name(s) (pool) */
    uint32_t * dil;		/*!< Directory indice(s) (from header) */
};

typedef struct rpmfn_s * rpmfn;

/**
 * A package filename set.
 */
struct rpmfiles_s {
    Header h;			/*!< Header for file info set (or NULL) */
    rpmstrPool pool;		/*!< String pool of this file info set */

    struct rpmfn_s fndata;	/*!< File name data */

    rpmsid * flinks;		/*!< Index to file link(s) (pool) */

    rpm_flag_t * fflags;	/*!< File flag(s) (from header) */
    rpm_off_t * fsizes;		/*!< File size(s) (from header) */
    rpm_loff_t * lfsizes;	/*!< File size(s) (from header) */
    rpm_time_t * fmtimes;	/*!< File modification time(s) (from header) */
    rpm_mode_t * fmodes;	/*!< File mode(s) (from header) */
    rpm_rdev_t * frdevs;	/*!< File rdev(s) (from header) */
    rpm_ino_t * finodes;	/*!< File inodes(s) (from header) */

    rpmsid * fuser;		/*!< Index to file owner(s) (misc pool) */
    rpmsid * fgroup;		/*!< Index to file group(s) (misc pool) */
    rpmsid * flangs;		/*!< Index to file lang(s) (misc pool) */

    char * fstates;		/*!< File state(s) (from header) */

    rpm_color_t * fcolors;	/*!< File color bits (header) */
    char ** fcaps;		/*!< File capability strings (header) */

    char ** cdict;		/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    uint32_t * fcdictx;		/*!< File class dictionary index (header) */

    uint32_t * ddict;		/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    uint32_t * fddictx;		/*!< File depends dictionary start (header) */
    uint32_t * fddictn;		/*!< File depends dictionary count (header) */
    rpm_flag_t * vflags;	/*!< File verify flag(s) (from header) */

    rpmfiFlags fiflags;		/*!< file info set control flags */

    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */

    int digestalgo;		/*!< File digest algorithm */
    unsigned char * digests;	/*!< File digests in binary. */

    struct nlinkHash_s * nlinks;/*!< Files connected by hardlinks */
    rpm_off_t * replacedSizes;	/*!< (TR_ADDED) */
    rpm_loff_t * replacedLSizes;/*!< (TR_ADDED) */
    int magic;
    int nrefs;		/*!< Reference count. */
};

static int indexSane(rpmtd xd, rpmtd yd, rpmtd zd);
static int cmpPoolFn(rpmstrPool pool, rpmfn files, int ix, const char * fn);

static rpmfiles rpmfilesUnlink(rpmfiles fi)
{
    if (fi)
	fi->nrefs--;
    return NULL;
}

rpmfiles rpmfilesLink(rpmfiles fi)
{
    if (fi)
	fi->nrefs++;
    return fi;
}

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

/*
 * Collect and validate file path data from header.
 * Return the number of files found (could be none) or -1 on error.
 */
static int rpmfnInit(rpmfn fndata, Header h, rpmstrPool pool)
{
    struct rpmtd_s bn, dn, dx;
    int rc = 0;

    /* Grab and validate file triplet data (if there is any) */
    if (headerGet(h, RPMTAG_BASENAMES, &bn, HEADERGET_MINMEM)) {
	headerGet(h, RPMTAG_DIRNAMES, &dn, HEADERGET_MINMEM);
	headerGet(h, RPMTAG_DIRINDEXES, &dx, HEADERGET_ALLOC);

	if (indexSane(&bn, &dn, &dx)) {
	    /* Init the file triplet data */
	    fndata->fc = rpmtdCount(&bn);
	    fndata->dc = rpmtdCount(&dn);
	    fndata->bnid = rpmtdToPool(&bn, pool);
	    fndata->dnid = rpmtdToPool(&dn, pool);
	    /* Steal index data from the td (pooh...) */
	    fndata->dil = dx.data;
	    dx.data = NULL;
	    rc = fndata->fc;
	} else {
	    memset(fndata, 0, sizeof(*fndata));
	    rc = -1;
	}
	rpmtdFreeData(&bn);
	rpmtdFreeData(&dn);
	rpmtdFreeData(&dx);
    }

    return rc;
}

static void rpmfnClear(rpmfn fndata)
{
    if (fndata) {
	free(fndata->bnid);
	free(fndata->dnid);
	free(fndata->dil);
	memset(fndata, 0, sizeof(*fndata));
    }
}

static rpm_count_t rpmfnFC(rpmfn fndata)
{
    return (fndata != NULL) ? fndata->fc :0;
}

static rpm_count_t rpmfnDC(rpmfn fndata)
{
    return (fndata != NULL) ? fndata->dc : 0;
}

static int rpmfnDI(rpmfn fndata, int dx)
{
    int j = -1;
    if (dx >= 0 && dx < rpmfnFC(fndata)) {
	if (fndata->dil != NULL)
	    j = fndata->dil[dx];
    }
    return j;
}

static rpmsid rpmfnBNId(rpmfn fndata, int ix)
{
    rpmsid id = 0;
    if (ix >= 0 && ix < rpmfnFC(fndata)) {
	if (fndata->bnid != NULL)
	    id = fndata->bnid[ix];
    }
    return id;
}

static rpmsid rpmfnDNId(rpmfn fndata, int ix)
{
    rpmsid id = 0;
    if (ix >= 0 && ix < rpmfnDC(fndata)) {
	if (fndata->dnid != NULL)
	    id = fndata->dnid[ix];
    }
    return id;
}

static const char * rpmfnBN(rpmstrPool pool, rpmfn fndata, int ix)
{
    return rpmstrPoolStr(pool, rpmfnBNId(fndata, ix));
}

static const char * rpmfnDN(rpmstrPool pool, rpmfn fndata, int ix)
{
    return rpmstrPoolStr(pool, rpmfnDNId(fndata, ix));
}

static char * rpmfnFN(rpmstrPool pool, rpmfn fndata, int ix)
{
    char *fn = NULL;
    if (ix >= 0 && ix < rpmfnFC(fndata)) {
	fn = rstrscat(NULL, rpmfnDN(pool, fndata, rpmfnDI(fndata, ix)),
			    rpmfnBN(pool, fndata, ix), NULL);
    }
    return fn;
}

rpm_count_t rpmfilesFC(rpmfiles fi)
{
    return (fi != NULL ? rpmfnFC(&fi->fndata) : 0);
}

rpm_count_t rpmfilesDC(rpmfiles fi)
{
    return (fi != NULL ? rpmfnDC(&fi->fndata) : 0);
}

int rpmfilesDigestAlgo(rpmfiles fi)
{
    return (fi != NULL) ? fi->digestalgo : 0;
}

rpm_count_t rpmfiFC(rpmfi fi)
{
    return (fi != NULL ? rpmfilesFC(fi->files) : 0);
}

rpm_count_t rpmfiDC(rpmfi fi)
{
    return (fi != NULL ? rpmfilesDC(fi->files) : 0);
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

    if (fi != NULL && fx >= 0 && fx < rpmfilesFC(fi->files)) {
	i = fi->i;
	fi->i = fx;
	fi->j = rpmfilesDI(fi->files, fi->i);
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

    if (fi != NULL && dx >= 0 && dx < rpmfiDC(fi)) {
	j = fi->j;
	fi->j = dx;
    }
    return j;
}

int rpmfilesDI(rpmfiles fi, int dx)
{
    return (fi != NULL) ? rpmfnDI(&fi->fndata, dx) : -1;
}

rpmsid rpmfilesBNId(rpmfiles fi, int ix)
{
    return (fi != NULL) ? rpmfnBNId(&fi->fndata, ix) : 0;
}

rpmsid rpmfilesDNId(rpmfiles fi, int jx)
{
    return (fi != NULL) ? rpmfnDNId(&fi->fndata, jx) : 0;
}

const char * rpmfilesBN(rpmfiles fi, int ix)
{
    return (fi != NULL) ? rpmfnBN(fi->pool, &fi->fndata, ix) : NULL;
}

const char * rpmfilesDN(rpmfiles fi, int jx)
{
    return (fi != NULL) ? rpmfnDN(fi->pool, &fi->fndata, jx) : NULL;
}

char * rpmfilesFN(rpmfiles fi, int ix)
{
    return (fi != NULL) ? rpmfnFN(fi->pool, &fi->fndata, ix) : NULL;
}

static int cmpPoolFn(rpmstrPool pool, rpmfn files, int ix, const char * fn)
{
    rpmsid dnid = rpmfnDNId(files, rpmfnDI(files, ix));
    size_t l = rpmstrPoolStrlen(pool, dnid);
    int cmp = strncmp(rpmstrPoolStr(pool, dnid), fn, l);
    if (cmp == 0)
	cmp = strcmp(rpmfnBN(pool, files, ix), fn + l);
    return cmp;
}

static int rpmfnFindFN(rpmstrPool pool, rpmfn files, const char * fn)
{
    int fc = rpmfnFC(files);

    if (fn[0] == '.' && fn[1] == '/') {
	fn++;
    }

    /* try binary search */

    int lo = 0;
    int hi = fc;
    int mid, cmp;

    while (hi > lo) {
	mid = (hi + lo) / 2 ;
	cmp = cmpPoolFn(pool, files, mid, fn);
	if (cmp < 0) {
	    lo = mid+1;
	} else if (cmp > 0) {
	    hi = mid;
	} else {
	    return mid;
	}
    }

    /* not found: try linear search */
    for (int i=0; i < fc; i++) {
	if (cmpPoolFn(pool, files, mid, fn) == 0)
	    return i;
    }
    return -1;
}

int rpmfilesFindFN(rpmfiles files, const char * fn)
{
    return (files && fn) ? rpmfnFindFN(files->pool, &files->fndata, fn) : -1;
}

int rpmfiFindFN(rpmfi fi, const char * fn)
{
    int ix = -1;

    if (fi != NULL) {
	ix = rpmfilesFindFN(fi->files, fn);
    }
    return ix;
}

rpmfileAttrs rpmfilesFFlags(rpmfiles fi, int ix)
{
    rpmfileAttrs FFlags = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fflags != NULL)
	    FFlags = fi->fflags[ix];
    }
    return FFlags;
}

rpmVerifyAttrs rpmfilesVFlags(rpmfiles fi, int ix)
{
    rpmVerifyAttrs VFlags = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->vflags != NULL)
	    VFlags = fi->vflags[ix];
    }
    return VFlags;
}

rpm_mode_t rpmfilesFMode(rpmfiles fi, int ix)
{
    rpm_mode_t fmode = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fmodes != NULL)
	    fmode = fi->fmodes[ix];
    }
    return fmode;
}

rpmfileState rpmfilesFState(rpmfiles fi, int ix)
{
    rpmfileState fstate = RPMFILE_STATE_MISSING;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
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
    return fi ? rpmfilesDigestAlgo(fi->files) : 0;
}

const unsigned char * rpmfilesFDigest(rpmfiles fi, int ix, int *algo, size_t *len)
{
    const unsigned char *digest = NULL;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
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

const char * rpmfilesFLink(rpmfiles fi, int ix)
{
    const char * flink = NULL;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->flinks != NULL)
	    flink = rpmstrPoolStr(fi->pool, fi->flinks[ix]);
    }
    return flink;
}

rpm_loff_t rpmfilesFSize(rpmfiles fi, int ix)
{
    rpm_loff_t fsize = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fsizes != NULL)
	    fsize = fi->fsizes[ix];
	else if (fi->lfsizes != NULL)
	    fsize = fi->lfsizes[ix];
    }
    return fsize;
}

rpm_rdev_t rpmfilesFRdev(rpmfiles fi, int ix)
{
    rpm_rdev_t frdev = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->frdevs != NULL)
	    frdev = fi->frdevs[ix];
    }
    return frdev;
}

rpm_ino_t rpmfilesFInode(rpmfiles fi, int ix)
{
    rpm_ino_t finode = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->finodes != NULL)
	    finode = fi->finodes[ix];
    }
    return finode;
}

rpm_color_t rpmfilesColor(rpmfiles files)
{
    rpm_color_t color = 0;

    if (files != NULL && files->fcolors != NULL) {
	int fc = rpmfilesFC(files);
	for (int i = 0; i < fc; i++)
	    color |= files->fcolors[i];
	/* XXX ignore all but lsnibble for now. */
	color &= 0xf;
    }
    return color;
}

rpm_color_t rpmfiColor(rpmfi fi)
{
    return (fi != NULL) ? rpmfilesColor(fi->files) : 0;
}

rpm_color_t rpmfilesFColor(rpmfiles fi, int ix)
{
    rpm_color_t fcolor = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fcolors != NULL)
	    /* XXX ignore all but lsnibble for now. */
	    fcolor = (fi->fcolors[ix] & 0x0f);
    }
    return fcolor;
}

const char * rpmfilesFClass(rpmfiles fi, int ix)
{
    const char * fclass = NULL;
    int cdictx;

    if (fi != NULL && fi->fcdictx != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	cdictx = fi->fcdictx[ix];
	if (fi->cdict != NULL && cdictx >= 0 && cdictx < fi->ncdict)
	    fclass = fi->cdict[cdictx];
    }
    return fclass;
}

uint32_t rpmfilesFDepends(rpmfiles fi, int ix, const uint32_t ** fddictp)
{
    int fddictx = -1;
    int fddictn = 0;
    const uint32_t * fddict = NULL;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
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

uint32_t rpmfilesFLinks(rpmfiles fi, int ix, const int ** files)
{
    uint32_t nlink = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	nlink = 1;
	if (fi->nlinks) {
	    struct hardlinks_s ** hardlinks = NULL;
	    nlinkHashGetEntry(fi->nlinks, ix, &hardlinks, NULL, NULL);
	    if (hardlinks) {
		nlink = hardlinks[0]->nlink;
		if (files) {
		    *files = hardlinks[0]->files;
		}
	    } else if (files){
		*files = NULL;
	    }
	}
    }
    return nlink;
}

uint32_t rpmfiFLinks(rpmfi fi, const int ** files)
{
    return rpmfilesFLinks(fi->files, fi ? fi->i : -1, files);
}

uint32_t rpmfilesFNlink(rpmfiles fi, int ix)
{
    return rpmfilesFLinks(fi, ix, NULL);
}

rpm_time_t rpmfilesFMtime(rpmfiles fi, int ix)
{
    rpm_time_t fmtime = 0;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fmtimes != NULL)
	    fmtime = fi->fmtimes[ix];
    }
    return fmtime;
}

const char * rpmfilesFUser(rpmfiles fi, int ix)
{
    const char * fuser = NULL;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fuser != NULL)
	    fuser = rpmstrPoolStr(fi->pool, fi->fuser[ix]);
    }
    return fuser;
}

const char * rpmfilesFGroup(rpmfiles fi, int ix)
{
    const char * fgroup = NULL;

    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->fgroup != NULL)
	    fgroup = rpmstrPoolStr(fi->pool, fi->fgroup[ix]);
    }
    return fgroup;
}

const char * rpmfilesFCaps(rpmfiles fi, int ix)
{
    const char *fcaps = NULL;
    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	fcaps = fi->fcaps ? fi->fcaps[ix] : "";
    }
    return fcaps;
}

const char * rpmfilesFLangs(rpmfiles fi, int ix)
{
    const char *flangs = NULL;
    if (fi != NULL && fi->flangs != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	flangs = rpmstrPoolStr(fi->pool, fi->flangs[ix]);
    }
    return flangs;
}

struct fingerPrint_s *rpmfilesFps(rpmfiles fi)
{
    return (fi != NULL) ? fi->fps : NULL;
}

static int iterFwd(rpmfi fi)
{
    return fi->i + 1;
}

static int iterBack(rpmfi fi)
{
    return fi->i - 1;
}

int rpmfiNext(rpmfi fi)
{
    int i = -1;
    if (fi != NULL) {
	int next = fi->next(fi);
	if (next >= 0 && next < rpmfilesFC(fi->files)) {
	    fi->i = next;
	    fi->j = rpmfilesDI(fi->files, fi->i);
	} else {
	    fi->i = -1;
	}
	i = fi->i;
    }
    return i;
}

rpmfi rpmfiInit(rpmfi fi, int fx)
{
    if (fi != NULL) {
	if (fx >= 0 && fx < rpmfilesFC(fi->files)) {
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
	if (fi->j < rpmfilesDC(fi->files))
	    j = fi->j;
	else
	    fi->j = -1;
    }

    return j;
}

rpmfi rpmfiInitD(rpmfi fi, int dx)
{
    if (fi != NULL) {
	if (dx >= 0 && dx < rpmfilesFC(fi->files))
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

int rpmfilesCompare(rpmfiles afi, int aix, rpmfiles bfi, int bix)
{
    mode_t amode = rpmfilesFMode(afi, aix);
    mode_t bmode = rpmfilesFMode(bfi, bix);
    rpmFileTypes awhat = rpmfiWhatis(amode);

    if ((rpmfilesFFlags(afi, aix) & RPMFILE_GHOST) ||
	(rpmfilesFFlags(bfi, bix) & RPMFILE_GHOST)) return 0;

    /* Mode difference is a conflict, except for symlinks */
    if (!(awhat == LINK && rpmfiWhatis(bmode) == LINK) && amode != bmode)
	return 1;

    if (awhat == LINK || awhat == REG) {
	if (rpmfilesFSize(afi, aix) != rpmfilesFSize(bfi, bix))
	    return 1;
    }

    if (!rstreq(rpmfilesFUser(afi, aix), rpmfilesFUser(bfi, bix)))
	return 1;
    if (!rstreq(rpmfilesFGroup(afi, aix), rpmfilesFGroup(bfi, bix)))
	return 1;

    if (awhat == LINK) {
	const char * alink = rpmfilesFLink(afi, aix);
	const char * blink = rpmfilesFLink(bfi, bix);
	if (alink == blink) return 0;
	if (alink == NULL) return 1;
	if (blink == NULL) return -1;
	return strcmp(alink, blink);
    } else if (awhat == REG) {
	size_t adiglen, bdiglen;
	int aalgo, balgo;
	const unsigned char * adigest, * bdigest;
	adigest = rpmfilesFDigest(afi, aix, &aalgo, &adiglen);
	bdigest = rpmfilesFDigest(bfi, bix, &balgo, &bdiglen);
	if (adigest == bdigest) return 0;
	if (adigest == NULL) return 1;
	if (bdigest == NULL) return -1;
	/* can't meaningfully compare different hash types */
	if (aalgo != balgo || adiglen != bdiglen) return -1;
	return memcmp(adigest, bdigest, adiglen);
    } else if (awhat == CDEV || awhat == BDEV) {
	if (rpmfilesFRdev(afi, aix) != rpmfilesFRdev(bfi, bix))
	    return 1;
    }

    return 0;
}

rpmFileAction rpmfilesDecideFate(rpmfiles ofi, int oix,
				   rpmfiles nfi, int nix,
				   int skipMissing)
{
    char * fn = rpmfilesFN(nfi, nix);
    rpmfileAttrs newFlags = rpmfilesFFlags(nfi, nix);
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
    dbWhat = rpmfiWhatis(rpmfilesFMode(ofi, oix));
    newWhat = rpmfiWhatis(rpmfilesFMode(nfi, nix));

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
	odigest = rpmfilesFDigest(ofi, oix, &oalgo, &odiglen);
	if (diskWhat == REG) {
	    if (rpmDoDigest(oalgo, fn, 0, (unsigned char *)buffer, NULL))
	        goto exit;	/* assume file has been removed */
	    if (odigest && memcmp(odigest, buffer, odiglen) == 0)
	        goto exit;	/* unmodified config file, replace. */
	}

	/* See if the file on disk is identical to the one in new pkg */
	ndigest = rpmfilesFDigest(nfi, nix, &nalgo, &ndiglen);
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
	oFLink = rpmfilesFLink(ofi, oix);
	if (diskWhat == LINK) {
	    ssize_t link_len = readlink(fn, buffer, sizeof(buffer) - 1);
	    if (link_len == -1)
		goto exit;		/* assume file has been removed */
	    buffer[link_len] = '\0';
	    if (oFLink && rstreq(oFLink, buffer))
		goto exit;		/* unmodified config file, replace. */
	}

	/* See if the link on disk is identical to the one in new pkg */
	nFLink = rpmfilesFLink(nfi, nix);
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

int rpmfilesConfigConflict(rpmfiles fi, int ix)
{
    char * fn = NULL;
    rpmfileAttrs flags = rpmfilesFFlags(fi, ix);
    char buffer[1024];
    rpmFileTypes newWhat, diskWhat;
    struct stat sb;
    int rc = 0;

    /* Non-configs are not config conflicts. */
    if (!(flags & RPMFILE_CONFIG))
	return 0;

    /* Only links and regular files can be %config, this is kinda moot */
    /* XXX: Why are we returning 1 here? */
    newWhat = rpmfiWhatis(rpmfilesFMode(fi, ix));
    if (newWhat != LINK && newWhat != REG)
	return 1;

    /* If it's not on disk, there's nothing to be saved */
    fn = rpmfilesFN(fi, ix);
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
    if (rpmfilesFSize(fi, ix) != sb.st_size) {
	rc = 1;
	goto exit;
    }
    
    memset(buffer, 0, sizeof(buffer));
    if (newWhat == REG) {
	int algo;
	size_t diglen;
	const unsigned char *ndigest = rpmfilesFDigest(fi,ix, &algo, &diglen);
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
	nFLink = rpmfilesFLink(fi, ix);
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
    /* When any relocations are present there'll be more work to do */
    return 1;
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

    if (!addPrefixes(h, relocations, numRelocations))
	return;

    if (rpmIsDebug()) {
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

rpmfiles rpmfilesFree(rpmfiles fi)
{
    if (fi == NULL) return NULL;

    if (fi->nrefs > 1)
	return rpmfilesUnlink(fi);

    if (rpmfilesFC(fi) > 0) {
	rpmfnClear(&fi->fndata);

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
	    fi->lfsizes = _free(fi->lfsizes);
	    fi->frdevs = _free(fi->frdevs);
	    fi->finodes = _free(fi->finodes);

	    fi->fcolors = _free(fi->fcolors);
	    fi->fcdictx = _free(fi->fcdictx);
	    fi->ddict = _free(fi->ddict);
	    fi->fddictx = _free(fi->fddictx);
	    fi->fddictn = _free(fi->fddictn);

	}
    }

    fi->replacedSizes = _free(fi->replacedSizes);
    fi->replacedLSizes = _free(fi->replacedLSizes);

    fi->h = headerFree(fi->h);

    fi->nlinks = nlinkHashFree(fi->nlinks);

    (void) rpmfilesUnlink(fi);
    memset(fi, 0, sizeof(*fi));		/* XXX trash and burn */
    fi = _free(fi);

    return NULL;
}

rpmfi rpmfiFree(rpmfi fi)
{
    if (fi == NULL) return NULL;

    if (fi->nrefs > 1)
	return rpmfiUnlink(fi);

    fi->files = rpmfilesFree(fi->files);
    fi->fn = _free(fi->fn);
    fi->apath = _free(fi->apath);

    free(fi);
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
    /* normally yc <= xc but larger values are not fatal (RhBug:1001553) */
    if (xc > 0 && yc > 0 && zc == xc) {
	uint32_t * i, nvalid = 0;
	/* ...and that the indexes are within bounds */
	while ((i = rpmtdNextUint32(zd))) {
	    if (*i >= yc)
		break;
	    nvalid++;
	}
	/* unless the loop runs to finish, the data is broken */
	sane = (nvalid == zc);
    }
    return sane;
}

#define _hgfi(_h, _tag, _td, _flags, _data) \
    if (headerGet((_h), (_tag), (_td), (_flags))) \
	_data = (td.data)

/*** Hard link handling ***/

struct fileid_s {
    rpm_dev_t id_dev;
    rpm_ino_t id_ino;
};

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE fileidHash
#define HTKEYTYPE struct fileid_s
#define HTDATATYPE int
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE


static unsigned int fidHashFunc(struct fileid_s a)
{
	return  a.id_ino + (a.id_dev<<16) + (a.id_dev>>16);
}

static int fidCmp(struct fileid_s a, struct fileid_s b)
{
	return  !((a.id_dev == b.id_dev) && (a.id_ino == b.id_ino));
}

static unsigned int intHash(int a)
{
	return a < 0 ? UINT_MAX-a : a;
}

static int intCmp(int a, int b)
{
	return a != b;
}

static struct hardlinks_s * freeNLinks(struct hardlinks_s * nlinks)
{
	nlinks->nlink--;
	if (!nlinks->nlink) {
		nlinks = _free(nlinks);
	}
	return nlinks;
}

static void rpmfilesBuildNLink(rpmfiles fi, Header h)
{
	struct fileid_s f_id;
	fileidHash files;
	rpm_dev_t * fdevs = NULL;
	struct rpmtd_s td;
	int fc = 0;
	int totalfc;

	if (!fi->finodes)
	return;

	_hgfi(h, RPMTAG_FILEDEVICES, &td, HEADERGET_ALLOC, fdevs);
	if (!fdevs)
	return;

	totalfc = rpmfilesFC(fi);
	files = fileidHashCreate(totalfc, fidHashFunc, fidCmp, NULL, NULL);
	for (int i=0; i < totalfc; i++) {
		if (!S_ISREG(rpmfilesFMode(fi, i)) ||
			(rpmfilesFFlags(fi, i) & RPMFILE_GHOST) ||
			fi->finodes[i] <= 0) {
			continue;
		}
		fc++;
		f_id.id_dev = fdevs[i];
		f_id.id_ino = fi->finodes[i];
		fileidHashAddEntry(files, f_id, i);
	}
	if (fileidHashNumKeys(files) != fc) {
	/* Hard links */
	fi->nlinks = nlinkHashCreate(2*(totalfc - fileidHashNumKeys(files)),
					 intHash, intCmp, NULL, freeNLinks);
	for (int i=0; i < totalfc; i++) {
		int fcnt;
		int * data;
		if (!S_ISREG(rpmfilesFMode(fi, i)) ||
		(rpmfilesFFlags(fi, i) & RPMFILE_GHOST)) {
		continue;
		}
		f_id.id_dev = fdevs[i];
		f_id.id_ino = fi->finodes[i];
		fileidHashGetEntry(files, f_id, &data, &fcnt, NULL);
		if (fcnt > 1 && !nlinkHashHasEntry(fi->nlinks, i)) {
		struct hardlinks_s * hlinks;
		hlinks = xmalloc(sizeof(struct hardlinks_s)+
				 fcnt*sizeof(hlinks->files[0]));
		hlinks->nlink = fcnt;
		for (int j=0; j<fcnt; j++) {
			hlinks->files[j] = data[j];
			nlinkHashAddEntry(fi->nlinks, data[j], hlinks);
		}
		}
	}
	}
	_free(fdevs);
	files = fileidHashFree(files);
}

static int rpmfilesPopulate(rpmfiles fi, Header h, rpmfiFlags flags)
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
    if (!(flags & RPMFI_NOFILESIZES)) {
	_hgfi(h, RPMTAG_FILESIZES, &td, scareFlags, fi->fsizes);
	_hgfi(h, RPMTAG_LONGFILESIZES, &td, scareFlags, fi->lfsizes);
    }
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
    if (!(flags & RPMFI_NOFILEINODES)) {
	_hgfi(h, RPMTAG_FILEINODES, &td, scareFlags, fi->finodes);
	rpmfilesBuildNLink(fi, h);
    }
    if (!(flags & RPMFI_NOFILEUSER)) 
	fi->fuser = tag2pool(fi->pool, h, RPMTAG_FILEUSERNAME);
    if (!(flags & RPMFI_NOFILEGROUP)) 
	fi->fgroup = tag2pool(fi->pool, h, RPMTAG_FILEGROUPNAME);

    /* TODO: validate and return a real error */
    return 0;
}

rpmfiles rpmfilesNew(rpmstrPool pool, Header h, rpmTagVal tagN, rpmfiFlags flags)
{
    rpmfiles fi = xcalloc(1, sizeof(*fi)); 
    int fc;

    fi->magic = RPMFIMAGIC;
    fi->fiflags = flags;
    /* private or shared pool? */
    fi->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();

    /*
     * Grab and validate file triplet data. Headers with no files simply
     * fall through here and an empty file set is returned.
     */
    fc = rpmfnInit(&fi->fndata, h, fi->pool);

    /* Broken data, bail out */
    if (fc < 0)
	goto err;

    /* populate the rest of the stuff if we have files */
    if (fc > 0)
	rpmfilesPopulate(fi, h, flags);

    /* freeze the pool to save memory, but only if private pool */
    if (fi->pool != pool)
	rpmstrPoolFreeze(fi->pool, 0);

    fi->h = (fi->fiflags & RPMFI_KEEPHEADER) ? headerLink(h) : NULL;

    return rpmfilesLink(fi);

err:
    rpmfilesFree(fi);
    return NULL;
}

static rpmfi initIter(rpmfiles files, int itype, int link)
{
    rpmfi fi = NULL;

    if (files) {
	fi = xcalloc(1, sizeof(*fi)); 
	fi->i = -1;
	fi->files = link ? rpmfilesLink(files) : files;
	switch (itype) {
	case RPMFI_ITER_BACK:
	    fi->i = rpmfilesFC(fi->files);
	    fi->next = iterBack;
	    break;
	case RPMFI_ITER_FWD:
	default:
	    fi->i = -1;
	    fi->next = iterFwd;
	    break;
	};
	rpmfiLink(fi);
    }
    return fi;
}

rpmfi rpmfilesIter(rpmfiles files, int itype)
{
    /* standalone iterators need to bump our refcount */
    return initIter(files, itype, 1);
}

rpmfi rpmfiNewPool(rpmstrPool pool, Header h, rpmTagVal tagN, rpmfiFlags flags)
{
    rpmfiles files = rpmfilesNew(pool, h, tagN, flags);
    /* we already own rpmfiles, avoid extra refcount on it */
    return initIter(files, RPMFI_ITER_FWD, 0);
}

rpmfi rpmfiNew(const rpmts ts, Header h, rpmTagVal tagN, rpmfiFlags flags)
{
    return rpmfiNewPool(NULL, h, tagN, flags);
}

void rpmfilesSetFReplacedSize(rpmfiles fi, int ix, rpm_loff_t newsize)
{
    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	/* Switch over to 64 bit variant */
	int fc = rpmfilesFC(fi);
	if (newsize > UINT32_MAX && fi->replacedLSizes == NULL) {
	    fi->replacedLSizes = xcalloc(fc, sizeof(*fi->replacedLSizes));
	    /* copy 32 bit data */
	    if (fi->replacedSizes) {
		for (int i=0; i < fc; i++)
		    fi->replacedLSizes[i] = fi->replacedSizes[i];
		fi->replacedSizes = _free(fi->replacedSizes);
	    }
	}
	if (fi->replacedLSizes != NULL) {
	    fi->replacedLSizes[ix] = newsize;
	} else {
	    if (fi->replacedSizes == NULL)
		fi->replacedSizes = xcalloc(fc, sizeof(*fi->replacedSizes));
	    fi->replacedSizes[ix] = (rpm_off_t) newsize;
	}
    }
}

rpm_loff_t rpmfilesFReplacedSize(rpmfiles fi, int ix)
{
    rpm_loff_t rsize = 0;
    if (fi != NULL && ix >= 0 && ix < rpmfilesFC(fi)) {
	if (fi->replacedSizes) {
	    rsize = fi->replacedSizes[ix];
	} else if (fi->replacedLSizes) {
	    rsize = fi->replacedLSizes[ix];
	}
    }
    return rsize;
}

void rpmfilesFpLookup(rpmfiles fi, fingerPrintCache fpc)
{
    /* This can get called twice (eg yum), scratch former results and redo */
    if (rpmfilesFC(fi) > 0) {
	rpmfn fn = &fi->fndata;
	if (fi->fps)
	    free(fi->fps);
	fi->fps = fpLookupList(fpc, fi->pool,
			       fn->dnid, fn->bnid, fn->dil, fn->fc);
    }
}

void rpmfiSetApath(rpmfi fi, char **apath)
{
    if (fi != NULL) {
	fi->apath = _free(fi->apath);
	if (apath) 
	    fi->apath = apath;
    }
}

/* 
 * Generate iterator accessors function wrappers, these do nothing but
 * call the corresponding rpmfiFooIndex(fi, fi->[ij])
 */

#define RPMFI_ITERFUNC(TYPE, NAME, IXV) \
    TYPE rpmfi ## NAME(rpmfi fi) { return rpmfiles ## NAME(fi->files, fi ? fi->IXV : -1); }

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
	fi->fn = rpmfilesFN(fi->files, fi->i);
	if (fi->fn != NULL)
	    fn = fi->fn;
    }
    return fn;
}

const unsigned char * rpmfiFDigest(rpmfi fi, int *algo, size_t *len)
{
    return rpmfilesFDigest(fi->files, fi ? fi->i : -1, algo, len);
}

uint32_t rpmfiFDepends(rpmfi fi, const uint32_t ** fddictp)
{
    return rpmfilesFDepends(fi->files,  fi ? fi->i : -1, fddictp);
}

int rpmfiCompare(const rpmfi afi, const rpmfi bfi)
{
    return rpmfilesCompare(afi->files, afi ? afi->i : -1, bfi->files, bfi ? bfi->i : -1);
}

rpmFileAction rpmfiDecideFate(const rpmfi ofi, rpmfi nfi, int skipMissing)
{
    return rpmfilesDecideFate(ofi->files, ofi ? ofi->i : -1,
				nfi->files, nfi ? nfi->i : -1,
				skipMissing);
}

int rpmfiConfigConflict(const rpmfi fi)
{
    return rpmfilesConfigConflict(fi->files, fi ? fi->i : -1);
}

rpmstrPool rpmfilesPool(rpmfiles fi)
{
    return (fi != NULL) ? fi->pool : NULL;
}

rpmfiles rpmfiFiles(rpmfi fi)
{
    return (fi != NULL) ? fi->files : NULL;
}

/******************************************************/
/*** Archive handling *********************************/
/******************************************************/

int rpmfiAttachArchive(rpmfi fi, FD_t fd, char mode)
{
    if (fi == NULL || fd == NULL)
	return -1;
    fi->archive = rpmcpioOpen(fd, mode);
    return (fi->archive == NULL);
}

int rpmfiArchiveClose(rpmfi fi)
{
    if (fi == NULL)
	return -1;
    int rc = rpmcpioClose(fi->archive);
    fi->archive = rpmcpioFree(fi->archive);
    return rc;
}

rpm_loff_t rpmfiArchiveTell(rpmfi fi)
{
    if (fi == NULL || fi->archive == NULL)
	return 0;
    return (rpm_loff_t) rpmcpioTell(fi->archive);
}

int rpmfiArchiveWriteHeader(rpmfi fi)
{
    int rc;

    if (fi == NULL)
	return -1;

    rpm_loff_t fsize = rpmfiFSize(fi);
    mode_t finalMode = rpmfiFMode(fi);
    rpmfiles files = fi->files;

    const int * hardlinks = NULL;
    int numHardlinks;
    numHardlinks = rpmfiFLinks(fi, &hardlinks);

    if (S_ISDIR(finalMode)) {
	fsize = 0;
    } else if (S_ISLNK(finalMode)) {
	fsize = strlen(rpmfiFLink(fi)); // XXX ugly!!!
    } else {
	/* Set fsize to 0 for all but the very last hardlinked file */
	const int * hardlinks = NULL;
	int numHardlinks;
	numHardlinks = rpmfiFLinks(fi, &hardlinks);
	if (numHardlinks > 1 && hardlinks[numHardlinks-1]!=fi->i) {
	    fsize = 0;
	}
    }

    if (files->lfsizes) {
	return rpmcpioStrippedHeaderWrite(fi->archive, rpmfiFX(fi), fsize);
    } else {
	struct stat st;
	memset(&st, 0, sizeof(st));

	const char *user = rpmfiFUser(fi);
	const char *group = rpmfiFGroup(fi);
	uid_t uid = 0;
	gid_t gid = 0;

	if (user) rpmugUid(user, &uid);
	if (group) rpmugGid(group, &gid);

	st.st_mode = finalMode;
	st.st_ino = rpmfiFInode(fi);
	st.st_rdev = rpmfiFRdev(fi);
	st.st_mtime = rpmfiFMtime(fi);
	st.st_nlink = numHardlinks;
	st.st_uid = uid;
	st.st_gid = gid;
	st.st_size = fsize;

	if (fi->apath) {
	    rc = rpmcpioHeaderWrite(fi->archive, fi->apath[rpmfiFX(fi)], &st);
	} else {
	    char * path = rstrscat(NULL, ".", rpmfiDN(fi), rpmfiBN(fi), NULL);
	    rc = rpmcpioHeaderWrite(fi->archive, path, &st);
	    free(path);
	}
    }

    return rc;
}

size_t rpmfiArchiveWrite(rpmfi fi, const void * buf, size_t size)
{
    if (fi == NULL || fi->archive == NULL)
	return -1;
    return rpmcpioWrite(fi->archive, buf, size);
}

int rpmfiArchiveWriteFile(rpmfi fi, FD_t fd)
{
    rpm_loff_t left;
    int rc = 0;
    size_t len;
    char buf[BUFSIZ*4];

    if (fi == NULL || fi->archive == NULL || fd == NULL)
	return -1;

    left = rpmfiFSize(fi);

    while (left) {
	len = (left > sizeof(buf) ? sizeof(buf) : left);
	if (Fread(buf, sizeof(*buf), len, fd) != len || Ferror(fd)) {
	    rc = CPIOERR_READ_FAILED;
	    break;
	}

	if (rpmcpioWrite(fi->archive, buf, len) != len) {
	    rc = CPIOERR_WRITE_FAILED;
	    break;
	}
	left -= len;
    }
    return rc;
}


/** \ingroup payload
 */
static int cpioStrCmp(const void * a, const void * b)
{
    const char * afn = *(const char **)a;
    const char * bfn = *(const char **)b;

    /* Match rpm-4.0 payloads with ./ prefixes. */
    if (afn[0] == '.' && afn[1] == '/')	afn += 2;
    if (bfn[0] == '.' && bfn[1] == '/')	bfn += 2;

    /* If either path is absolute, make it relative. */
    if (afn[0] == '/')	afn += 1;
    if (bfn[0] == '/')	bfn += 1;

    return strcmp(afn, bfn);
}


int rpmfiArchiveNext(rpmfi fi)
{
    int rc;
    int fx = -1;
    char * path;

    if (fi == NULL || fi->archive == NULL)
	return -1;

    /* Read next payload header. */
    rc = rpmcpioHeaderRead(fi->archive, &path, &fx);

    if (rc) {
	return rc;
    }


    if (fx == -1) {
	/* Identify mapping index. */
	int fc = rpmfiFC(fi);
	if (fi && fc > 0 && fi->apath) {
	    char ** p = NULL;
	    if (fi->apath != NULL)
		p = bsearch(&path, fi->apath, fc, sizeof(path),
			    cpioStrCmp);
	    if (p) {
		fx = (p - fi->apath);
	    }
	} else if (fc > 0) {
	    fx = rpmfiFindFN(fi, path);
	}
	free(path);
	rpmfiSetFX(fi, fx);
    } else {
	rpmfiSetFX(fi, fx);

	uint32_t numlinks;
	const int * links;
	rpm_loff_t fsize = 0;
	rpm_mode_t mode = rpmfiFMode(fi);

	numlinks = rpmfiFLinks(fi, &links);
	if (S_ISREG(mode)) {
	    if (numlinks>1 && links[numlinks-1]!=fx) {
		fsize = 0;
	    } else {
		fsize = rpmfiFSize(fi);
	    }
	} else if (S_ISLNK(mode)) {
	    fsize = rpmfiFSize(fi);
	}
	rpmcpioSetExpectedFileSize(fi->archive, fsize);
    }
    /* Mapping error */
    if (fx < 0) {
	return CPIOERR_UNMAPPED_FILE;
    }
    return 0;
}

size_t rpmfiArchiveRead(rpmfi fi, void * buf, size_t size)
{
    if (fi == NULL || fi->archive == NULL)
	return -1;
    return rpmcpioRead(fi->archive, buf, size);
}

int rpmfiArchiveReadToFile(rpmfi fi, FD_t fd, char nodigest)
{
    if (fi == NULL || fi->archive == NULL || fd == NULL)
	return -1;

    rpm_loff_t left = rpmfiFSize(fi);
    const unsigned char * fidigest = NULL;
    pgpHashAlgo digestalgo = 0;
    int rc = 0;
    char buf[BUFSIZ*4];

    if (!nodigest) {
	digestalgo = rpmfiDigestAlgo(fi);
	fidigest = rpmfilesFDigest(fi->files, rpmfiFX(fi), NULL, NULL);
	fdInitDigest(fd, digestalgo, 0);
    }

    while (left) {
	size_t len;
	len = (left > sizeof(buf) ? sizeof(buf) : left);
	if (rpmcpioRead(fi->archive, buf, len) != len) {
	    rc = CPIOERR_READ_FAILED;
	    goto exit;
	}
	if ((Fwrite(buf, sizeof(*buf), len, fd) != len) || Ferror(fd)) {
	    rc = CPIOERR_WRITE_FAILED;
	    goto exit;
	}

	left -= len;
    }

    if (!nodigest) {
	void * digest = NULL;

	(void) Fflush(fd);
	fdFiniDigest(fd, digestalgo, &digest, NULL, 0);

	if (digest != NULL && fidigest != NULL) {
	    size_t diglen = rpmDigestLength(digestalgo);
	    if (memcmp(digest, fidigest, diglen)) {
		rc = CPIOERR_DIGEST_MISMATCH;
	    }
	} else {
	    rc = CPIOERR_DIGEST_MISMATCH;
	}
	free(digest);
    }

exit:
    return rc;
}
