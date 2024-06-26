/**
 * \file lib/fprint.c
 */

#include "system.h"

#include <unordered_map>

#include <rpm/rpmfileutil.h>	/* for rpmCleanPath */
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>
#include <rpm/rpmsq.h>

#include "rpmdb_internal.h"
#include "rpmfi_internal.h"
#include "rpmte_internal.h"
#include "fprint.h"
#include "misc.h"
#include "debug.h"
#include <libgen.h>


static unsigned int sidHash(rpmsid sid)
{
    return sid;
}


/**
 * Finger print cache entry.
 * This is really a directory and symlink cache. We don't differentiate between
 * the two. We can prepopulate it, which allows us to easily conduct "fake"
 * installs of a system w/o actually mounting filesystems.
 */
struct fprintCacheEntry_s {
    rpmsid dirId;			/*!< path to existing directory */
    dev_t dev;				/*!< stat(2) device number */
    ino_t ino;				/*!< stat(2) inode number */
};

/**
 * Associates a trailing sub-directory and final base name with an existing
 * directory finger print.
 */
struct fingerPrint {
    /*! directory finger print entry (the directory path is stat(2)-able */
    const struct fprintCacheEntry_s * entry;
    /*! trailing sub-directory path (directories that are not stat(2)-able */
    rpmsid subDirId;
    rpmsid baseNameId;	/*!< file base name id */
};

#define	FP_ENTRY_EQUAL(a, b) (((a)->dev == (b)->dev) && ((a)->ino == (b)->ino))

#define FP_EQUAL(a, b) ( \
	FP_ENTRY_EQUAL((a).entry, (b).entry) && \
	    ((a).baseNameId == (b).baseNameId) && \
	    ((a).subDirId == (b).subDirId) \
    )

/* Hash value for a finger print pointer, based on dev and inode only! */
struct fpHash
{
    size_t operator() (const fingerPrint *fp) const
    {
	unsigned int hash = 0;
	int j;

	hash = sidHash(fp->baseNameId);
	if (fp->subDirId) hash ^= sidHash(fp->subDirId);

	hash ^= ((unsigned)fp->entry->dev);
	for (j=0; j<4; j++) hash ^= ((fp->entry->ino >> (8*j)) & 0xFF) << ((3-j)*8);

	return hash;
    }
};

struct fpEqual
{
    bool operator()(const fingerPrint * k1, const fingerPrint * k2) const
    {
	/* If the addresses are the same, so are the values. */
	if (k1 == k2)
	    return true;

	/* Otherwise, compare fingerprints by value. */
	if (FP_EQUAL(*k1, *k2))
	    return true;
	return false;
    }
};

using rpmFpEntryHash = std::unordered_multimap<rpmsid,fprintCacheEntry_s>;
using rpmFpHash = std::unordered_map<fingerPrint *,rpmffi_s,fpHash,fpEqual>;

/**
 * Finger print cache.
 */
struct fprintCache_s {
    rpmFpEntryHash ht;			/*!< hashed by dirName */
    rpmFpHash fp;			/*!< hashed by fingerprint */
    rpmstrPool pool;			/*!< string pool */
};

fingerPrintCache fpCacheCreate(int sizeHint, rpmstrPool pool)
{
    fingerPrintCache fpc = new fprintCache_s {};
    fpc->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();
    return fpc;
}

fingerPrintCache fpCacheFree(fingerPrintCache cache)
{
    if (cache) {
	cache->pool = rpmstrPoolFree(cache->pool);
	delete cache;
    }
    return NULL;
}

/**
 * Find directory name entry in cache.
 * @param cache		pointer to fingerprint cache
 * @param dirId		string id to locate in cache
 * @return pointer to directory name entry (or NULL if not found).
 */
static const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache, rpmsid dirId)
{
    auto entry = cache->ht.find(dirId);
    if (entry != cache->ht.end())
	return &entry->second;
    return NULL;
}

static char * canonDir(rpmstrPool pool, rpmsid dirNameId)
{
    const char * dirName = rpmstrPoolStr(pool, dirNameId);
    char *cdnbuf = NULL;

    if (*dirName == '/') {
	cdnbuf = xstrdup(dirName);
    } else {
	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we 
	 * do instead is use realpath() on `.' and then append arg to
	 * the result.
	 */

	/* if the current directory doesn't exist, we might fail. oh well. */
	if ((cdnbuf = realpath(".", NULL)) != NULL)
	    cdnbuf = rstrscat(&cdnbuf, "/", dirName, NULL);
    }

    /* ensure canonical path with a trailing slash */
    if (cdnbuf) {
	size_t cdnl = strlen(cdnbuf);
	if (cdnl > 1) {
	    cdnbuf = rpmCleanPath(cdnbuf);
	    cdnbuf = rstrcat(&cdnbuf, "/");
	}
    }
    return cdnbuf;
}


static int doLookupId(fingerPrintCache cache,
	     rpmsid dirNameId, rpmsid baseNameId,
	     fingerPrint *fp)
{
    struct stat sb;
    const struct fprintCacheEntry_s * cacheHit;
    char *cdn = canonDir(cache->pool, dirNameId);
    rpmsid fpId;
    size_t fpLen;
    
    if (cdn == NULL) goto exit; /* XXX only if realpath() above fails */

    memset(fp, 0, sizeof(*fp));
    fpId = rpmstrPoolId(cache->pool, cdn, 1);
    fpLen = rpmstrPoolStrlen(cache->pool, fpId);;

    while (1) {
	/* as we're stating paths here, we want to follow symlinks */
	cacheHit = cacheContainsDirectory(cache, fpId);
	if (cacheHit != NULL) {
	    fp->entry = cacheHit;
	} else if (!stat(rpmstrPoolStr(cache->pool, fpId), &sb)) {
	    struct fprintCacheEntry_s newEntry = {
		.dirId = fpId,
		.dev = sb.st_dev,
		.ino = sb.st_ino,
	    };
	    auto ret = cache->ht.insert({fpId, newEntry});
	    fp->entry = &ret->second;
	}

        if (fp->entry) {
	    const char * subDir = cdn + fpLen - 1;
	    /* XXX don't bother saving '/' as subdir */
	    if (subDir[0] == '\0' || (subDir[0] == '/' && subDir[1] == '\0'))
		subDir = NULL;
	    fp->baseNameId = baseNameId;
	    if (subDir != NULL)
		fp->subDirId = rpmstrPoolId(cache->pool, subDir, 1);
	    goto exit;
	}

        /* stat of '/' just failed! */
	if (fpLen == 1)
	    abort();

	/* Find the parent directory and its id for the next round */
	fpLen--;
	while (fpLen > 1 && cdn[fpLen-1] != '/')
	    fpLen--;
	fpId = rpmstrPoolIdn(cache->pool, cdn, fpLen, 1);
    }

exit:
    free(cdn);
    /* XXX TODO: failure from eg realpath() should be returned and handled */
    return 0;
}

static int doLookup(fingerPrintCache cache,
		    const char * dirName, const char * baseName,
		    fingerPrint *fp)
{
    rpmsid dirNameId = rpmstrPoolId(cache->pool, dirName, 1);
    rpmsid baseNameId = rpmstrPoolId(cache->pool, baseName, 1);
    return doLookupId(cache, dirNameId, baseNameId, fp);
}

int fpLookup(fingerPrintCache cache,
             const char * dirName, const char * baseName,
             fingerPrint **fp)
{
    if (*fp == NULL)
	*fp = (fingerPrint *)xcalloc(1, sizeof(**fp));
    return doLookup(cache, dirName, baseName, *fp);
}

int fpLookupId(fingerPrintCache cache,
               rpmsid dirNameId, rpmsid baseNameId,
               fingerPrint **fp)
{
    if (*fp == NULL)
	*fp = (fingerPrint *)xcalloc(1, sizeof(**fp));
    return doLookupId(cache, dirNameId, baseNameId, *fp);
}

const char * fpEntryDir(fingerPrintCache cache, fingerPrint *fp)
{
    const char * dn = NULL;
    if (fp && fp->entry)
	dn = rpmstrPoolStr(cache->pool, fp->entry->dirId);
    return dn;
}

dev_t fpEntryDev(fingerPrintCache cache, fingerPrint *fp)
{
    return (fp && fp->entry) ? fp->entry->dev : 0;
}

int fpLookupEquals(fingerPrintCache cache, fingerPrint *fp,
	          const char * dirName, const char * baseName)
{
    struct fingerPrint ofp;
    doLookup(cache, dirName, baseName, &ofp);
    return FP_EQUAL(*fp, ofp);
}

int fpLookupEqualsId(fingerPrintCache cache, fingerPrint *fp,
	          rpmsid dirNameId, rpmsid baseNameId)
{
    struct fingerPrint ofp;
    doLookupId(cache, dirNameId, baseNameId, &ofp);
    return FP_EQUAL(*fp, ofp);
}

fingerPrint * fpLookupList(fingerPrintCache cache, rpmstrPool pool,
		  rpmsid * dirNames, rpmsid * baseNames,
		  const uint32_t * dirIndexes, 
		  int fileCount)
{
    fingerPrint * fps = (fingerPrint *)xmalloc(fileCount * sizeof(*fps));
    int i;

    /*
     * We could handle different pools easily enough, but there should be
     * no need for that. Make sure we catch any oddball cases there might be
     * lurking about.
     */
    assert(cache->pool == pool);

    for (i = 0; i < fileCount; i++) {
	/* If this is in the same directory as the last file, don't bother
	   redoing all of this work */
	if (i > 0 && dirIndexes[i - 1] == dirIndexes[i]) {
	    fps[i].entry = fps[i - 1].entry;
	    fps[i].subDirId = fps[i - 1].subDirId;
	    /* XXX if the pools are different, copy would be needed */
	    fps[i].baseNameId = baseNames[i];
	} else {
	    doLookupId(cache, dirNames[dirIndexes[i]], baseNames[i], &fps[i]);
	}
    }
    return fps;
}

/* Check file for to be installed symlinks in their path and correct their fp */
static void fpLookupSubdir(rpmFpHash symlinks, fingerPrintCache fpc, fingerPrint *fp)
{
    struct fingerPrint current_fp;
    const char *currentsubdir;
    size_t lensubDir, bnStart, bnEnd;
    int symlinkcount = 0;

    for (;;) {
	int found = 0;

	if (fp->subDirId == 0)
	    break;	/* directory exists - no need to look for symlinks */

	currentsubdir = rpmstrPoolStr(fpc->pool, fp->subDirId);
	lensubDir = rpmstrPoolStrlen(fpc->pool, fp->subDirId);
	current_fp = *fp;

	/* Set baseName to the upper most dir */
	bnStart = bnEnd = 1;
	while (bnEnd < lensubDir && currentsubdir[bnEnd] != '/')
	    bnEnd++;
	/* no subDir for now */
	current_fp.subDirId = 0;

	while (bnEnd < lensubDir) {
	    current_fp.baseNameId = rpmstrPoolIdn(fpc->pool,
						    currentsubdir + bnStart,
						    bnEnd - bnStart, 1);

	    auto range = symlinks.equal_range(&current_fp);
	    for (auto it = range.first; it != range.second; ++it) {
		const rpmffi_s & rec = it->second;
		rpmfiles foundfi = rpmteFiles(rec.p);
		char const *linktarget = rpmfilesFLink(foundfi, rec.fileno);
		char *link;
		rpmsid linkId;

		foundfi = rpmfilesFree(foundfi);

		if (!linktarget || *linktarget == '\0')
		    continue;

		/* this "directory" will be symlink */
		link = NULL;
		if (*linktarget != '/') {
		    const char *dn, *subDir = NULL;
		    dn = rpmstrPoolStr(fpc->pool, current_fp.entry->dirId);
		    if (current_fp.subDirId) {
			subDir = rpmstrPoolStr(fpc->pool, current_fp.subDirId);
		    }
		    rstrscat(&link, dn, subDir ? subDir : "", "/", NULL);
		}
		rstrscat(&link, linktarget, "/", NULL);
		if (currentsubdir[bnEnd])
		    rstrscat(&link, currentsubdir + bnEnd, NULL);
		linkId = rpmstrPoolId(fpc->pool, link, 1);
		free(link);

		/* this modifies the fingerprint! */
		doLookupId(fpc, linkId, fp->baseNameId, fp);
		found = 1;
		break;
	    }

	    if (found)
		break;

	    /* Set former baseName as subDir */
	    bnEnd++;
	    current_fp.subDirId = rpmstrPoolIdn(fpc->pool, currentsubdir, bnEnd, 1);

	    /* set baseName to the next lower dir */
	    bnStart = bnEnd;
	    while (bnEnd < lensubDir && currentsubdir[bnEnd] != '/')
		bnEnd++;
	}

	if (!found)
	    break;	/* no symlink found, we are done */

	if (++symlinkcount > 50) {
	    /* we followed too many symlinks, there is most likely a cycle */
	    /* TODO: warning/error */
	    break;
	}
    }
}

fingerPrint * fpCacheGetByFp(fingerPrintCache cache,
			     struct fingerPrint * fp, int ix,
			     std::vector<struct rpmffi_s> & recs)
{
    auto range = cache->fp.equal_range(fp + ix);
    if (range.first != range.second) {
	for (auto it = range.first; it != range.second; ++it)
	    recs.push_back(it->second);
	return fp + ix;
    } else
	return NULL;
}

void fpCachePopulate(fingerPrintCache fpc, rpmts ts, int fileCount)
{
    rpmtsi pi;
    rpmte p;
    rpmfs fs;
    rpmfiles fi;
    int i, fc;
    int havesymlinks = 0;

    rpmFpHash symlinks;

    /* populate the fingerprints of all packages in the transaction */
    /* also create a hash of all symlinks in the new packages */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmteFiles(p)) == NULL)
	    continue;

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfilesFpLookup(fi, fpc);
	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);

	if (rpmteType(p) != TR_REMOVED) {
	    fingerPrint *fpList = rpmfilesFps(fi);
	    /* collect symbolic links */
	    for (i = 0; i < fc; i++) {
		struct rpmffi_s ffi;
		char const *linktarget;
		if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		    continue;
		linktarget = rpmfilesFLink(fi, i);
		if (!(linktarget && *linktarget != '\0'))
		    continue;
		ffi.p = p;
		ffi.fileno = i;
		symlinks.insert({fpList + i, ffi});
		havesymlinks = 1;
	    }
	}

	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);
	rpmfilesFree(fi);
    }
    rpmtsiFree(pi);

    /* ===============================================
     * Create the fingerprint -> (p, fileno) hash table
     * Also adapt the fingerprint if we have symlinks
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	fingerPrint *fpList, *lastfp = NULL;

	if ((fi = rpmteFiles(p)) == NULL)
	    continue;

	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);
	fpList = rpmfilesFps(fi);
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	for (i = 0; i < fc; i++) {
	    struct rpmffi_s ffi;
	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		continue;
	    if (havesymlinks) {
		/* if the entry/subdirid matches the one from the
		 * last entry we do not need to call fpLookupSubdir */
		if (!lastfp || lastfp->entry != fpList[i].entry ||
			lastfp->subDirId != fpList[i].subDirId)
		    fpLookupSubdir(symlinks, fpc, fpList + i);
	    }
	    ffi.p = p;
	    ffi.fileno = i;
	    fpc->fp.insert({fpList + i, ffi});
	    lastfp = fpList + i;
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfilesFree(fi);
    }
    rpmtsiFree(pi);
}

