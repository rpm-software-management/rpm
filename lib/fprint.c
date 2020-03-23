/**
 * \file lib/fprint.c
 */

#include "system.h"

#include <rpm/rpmfileutil.h>	/* for rpmCleanPath */
#include <rpm/rpmts.h>
#include <rpm/rpmsq.h>

#include "lib/rpmdb_internal.h"
#include "lib/rpmfi_internal.h"
#include "lib/rpmte_internal.h"
#include "lib/fprint.h"
#include "lib/misc.h"
#include "debug.h"
#include <libgen.h>

/* Create new hash table type rpmFpEntryHash */
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE rpmFpEntryHash
#define HTKEYTYPE rpmsid
#define HTDATATYPE const struct fprintCacheEntry_s *
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/* Create by-fingerprint hash table */
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE rpmFpHash
#define HTKEYTYPE const fingerPrint *
#define HTDATATYPE struct rpmffi_s
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

static unsigned int sidHash(rpmsid sid)
{
    return sid;
}

static int sidCmp(rpmsid a, rpmsid b)
{
    return (a != b);
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
struct fingerPrint_s {
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
    fingerPrintCache fpc;

    fpc = xcalloc(1, sizeof(*fpc));
    fpc->ht = rpmFpEntryHashCreate(sizeHint, sidHash, sidCmp,
				   NULL, (rpmFpEntryHashFreeData)free);
    fpc->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();
    return fpc;
}

fingerPrintCache fpCacheFree(fingerPrintCache cache)
{
    if (cache) {
	cache->ht = rpmFpEntryHashFree(cache->ht);
	cache->fp = rpmFpHashFree(cache->fp);
	cache->pool = rpmstrPoolFree(cache->pool);
	free(cache);
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
    const struct fprintCacheEntry_s ** data;

    if (rpmFpEntryHashGetEntry(cache->ht, dirId, &data, NULL, NULL))
	return data[0];
    return NULL;
}

static char * canonDir(rpmstrPool pool, rpmsid dirNameId)
{
    const char * dirName = rpmstrPoolStr(pool, dirNameId);
    size_t cdnl = rpmstrPoolStrlen(pool, dirNameId);;
    char *cdnbuf = NULL;

    if (*dirName == '/') {
	cdnbuf = xstrdup(dirName);
	cdnbuf = rpmCleanPath(cdnbuf);
	/* leave my trailing slashes along you b**** */
	if (cdnl > 1)
	    cdnbuf = rstrcat(&cdnbuf, "/");
    } else {
	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we 
	 * do instead is use realpath() on `.' and then append arg to
	 * the result.
	 */

	/* if the current directory doesn't exist, we might fail. 
	   oh well. likewise if it's too long.  */

	/* XXX we should let realpath() allocate if it can */
	cdnbuf = xmalloc(PATH_MAX);
	cdnbuf[0] = '\0';
	if (realpath(".", cdnbuf) != NULL) {
	    char *end = cdnbuf + strlen(cdnbuf);
	    if (end[-1] != '/')	*end++ = '/';
	    end = stpncpy(end, dirName, PATH_MAX - (end - cdnbuf));
	    *end = '\0';
	    (void)rpmCleanPath(cdnbuf); /* XXX possible /../ from concatenation */
	    end = cdnbuf + strlen(cdnbuf);
	    if (end[-1] != '/')	*end++ = '/';
	    *end = '\0';
	} else {
	    cdnbuf = _free(cdnbuf);
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
	    struct fprintCacheEntry_s * newEntry = xmalloc(sizeof(* newEntry));

	    newEntry->ino = sb.st_ino;
	    newEntry->dev = sb.st_dev;
	    newEntry->dirId = fpId;
	    fp->entry = newEntry;

	    rpmFpEntryHashAddEntry(cache->ht, fpId, fp->entry);
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
	*fp = xcalloc(1, sizeof(**fp));
    return doLookup(cache, dirName, baseName, *fp);
}

/**
 * Return hash value for a finger print.
 * Hash based on dev and inode only!
 * @param fp		pointer to finger print entry
 * @return hash value
 */
static unsigned int fpHashFunction(const fingerPrint * fp)
{
    unsigned int hash = 0;
    int j;

    hash = sidHash(fp->baseNameId);
    if (fp->subDirId) hash ^= sidHash(fp->subDirId);

    hash ^= ((unsigned)fp->entry->dev);
    for (j=0; j<4; j++) hash ^= ((fp->entry->ino >> (8*j)) & 0xFF) << ((3-j)*8);
    
    return hash;
}

int fpEqual(const fingerPrint * k1, const fingerPrint * k2)
{
    /* If the addresses are the same, so are the values. */
    if (k1 == k2)
	return 0;

    /* Otherwise, compare fingerprints by value. */
    if (FP_EQUAL(*k1, *k2))
	return 0;
    return 1;

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
    struct fingerPrint_s ofp;
    doLookup(cache, dirName, baseName, &ofp);
    return FP_EQUAL(*fp, ofp);
}

fingerPrint * fpLookupList(fingerPrintCache cache, rpmstrPool pool,
		  rpmsid * dirNames, rpmsid * baseNames,
		  const uint32_t * dirIndexes, 
		  int fileCount)
{
    fingerPrint * fps = xmalloc(fileCount * sizeof(*fps));
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
static void fpLookupSubdir(rpmFpHash symlinks, fingerPrintCache fpc, rpmte p, int filenr)
{
    rpmfiles fi = rpmteFiles(p);
    struct fingerPrint_s current_fp;
    const char *currentsubdir;
    size_t lensubDir, bnStart, bnEnd;

    struct rpmffi_s * recs;
    int numRecs;
    int i;
    fingerPrint *fp = rpmfilesFps(fi) + filenr;
    int symlinkcount = 0;
    struct rpmffi_s ffi = { p, filenr};

    if (fp->subDirId == 0) {
	rpmFpHashAddEntry(fpc->fp, fp, ffi);
	goto exit;
    }

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
	char found = 0;

	current_fp.baseNameId = rpmstrPoolIdn(fpc->pool,
						currentsubdir + bnStart,
						bnEnd - bnStart, 1);

	rpmFpHashGetEntry(symlinks, &current_fp, &recs, &numRecs, NULL);

	for (i = 0; i < numRecs; i++) {
	    rpmfiles foundfi = rpmteFiles(recs[i].p);
	    char const *linktarget = rpmfilesFLink(foundfi, recs[i].fileno);
	    char *link;

	    /* Ignore already removed (by eg %pretrans) links */
	    if (linktarget && rpmteType(recs[i].p) == TR_REMOVED) {
		char *path = rpmfilesFN(foundfi, recs[i].fileno);
		struct stat sb;
		if (lstat(path, &sb) == -1)
		    linktarget = NULL;
		free(path);
	    }

	    foundfi = rpmfilesFree(foundfi);

	    if (linktarget && *linktarget != '\0') {
		const char *bn;
		/* this "directory" is a symlink */
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
		if (strlen(currentsubdir + bnEnd)) {
		    rstrscat(&link, currentsubdir + bnEnd, NULL);
		}

		bn = rpmstrPoolStr(fpc->pool, fp->baseNameId);
		doLookup(fpc, link, bn, fp);

		free(link);
		symlinkcount++;

		/* setup current_fp for the new path */
		found = 1;
		current_fp = *fp;
		if (fp->subDirId == 0) {
		    /* directory exists - no need to look for symlinks */
		    rpmFpHashAddEntry(fpc->fp, fp, ffi);
		    goto exit;
		}
		currentsubdir = rpmstrPoolStr(fpc->pool, fp->subDirId);
		lensubDir = rpmstrPoolStrlen(fpc->pool, fp->subDirId);
		/* no subDir for now */
		current_fp.subDirId = 0;

		/* Set baseName to the upper most dir */
		bnStart = bnEnd = 1;
		while (bnEnd < lensubDir && currentsubdir[bnEnd] != '/')
		    bnEnd++;
		break;

	    }
	}
	if (symlinkcount > 50) {
	    // found too many symlinks in the path
	    // most likley a symlink cicle
	    // giving up
	    // TODO warning/error
	    break;
	}
	if (found) {
	    continue; // restart loop after symlink
	}

	/* Set former baseName as subDir */
	bnEnd++;
	current_fp.subDirId = rpmstrPoolIdn(fpc->pool, currentsubdir, bnEnd, 1);

	/* set baseName to the next lower dir */
	bnStart = bnEnd;
	while (bnEnd < lensubDir && currentsubdir[bnEnd] != '/')
	    bnEnd++;
    }
    rpmFpHashAddEntry(fpc->fp, fp, ffi);

exit:
    rpmfilesFree(fi);
}

fingerPrint * fpCacheGetByFp(fingerPrintCache cache,
			     struct fingerPrint_s * fp, int ix,
			     struct rpmffi_s ** recs, int * numRecs)
{
    if (rpmFpHashGetEntry(cache->fp, fp + ix, recs, numRecs, NULL))
	return fp + ix;
    else
	return NULL;
}

void fpCachePopulate(fingerPrintCache fpc, rpmts ts, int fileCount)
{
    rpmtsi pi;
    rpmte p;
    rpmfs fs;
    rpmfiles fi;
    int i, fc;

    if (fpc->fp == NULL)
	fpc->fp = rpmFpHashCreate(fileCount/2 + 10001, fpHashFunction, fpEqual,
				  NULL, NULL);

    rpmFpHash symlinks = rpmFpHashCreate(fileCount/16+16, fpHashFunction, fpEqual, NULL, NULL);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	fingerPrint *fpList;
	(void) rpmsqPoll();

	if ((fi = rpmteFiles(p)) == NULL)
	    continue;

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfilesFpLookup(fi, fpc);
	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);
	fpList = rpmfilesFps(fi);
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
	    rpmFpHashAddEntry(symlinks, fpList + i, ffi);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);
	rpmfilesFree(fi);
    }
    rpmtsiFree(pi);

    /* ===============================================
     * Check fingerprints if they contain symlinks
     * and add them to the hash table
     */

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmsqPoll();

	if ((fi = rpmteFiles(p)) == NULL)
	    continue;

	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	for (i = 0; i < fc; i++) {
	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		continue;
	    fpLookupSubdir(symlinks, fpc, p, i);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfilesFree(fi);
    }
    rpmtsiFree(pi);

    rpmFpHashFree(symlinks);
}

