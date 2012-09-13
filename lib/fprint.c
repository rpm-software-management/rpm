/**
 * \file lib/fprint.c
 */

#include "system.h"

#include <rpm/rpmfileutil.h>	/* for rpmCleanPath */
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>

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
#define HTKEYTYPE const char *
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

/**
 * Finger print cache.
 */
struct fprintCache_s {
    rpmFpEntryHash ht;			/*!< hashed by dirName */
    rpmFpHash fp;			/*!< hashed by fingerprint */
};

fingerPrintCache fpCacheCreate(int sizeHint)
{
    fingerPrintCache fpc;

    fpc = xcalloc(1, sizeof(*fpc));
    fpc->ht = rpmFpEntryHashCreate(sizeHint, rstrhash, strcmp,
				   (rpmFpEntryHashFreeKey)free,
				   (rpmFpEntryHashFreeData)free);
    return fpc;
}

fingerPrintCache fpCacheFree(fingerPrintCache cache)
{
    if (cache) {
	cache->ht = rpmFpEntryHashFree(cache->ht);
	cache->fp = rpmFpHashFree(cache->fp);
	free(cache);
    }
    return NULL;
}

/**
 * Find directory name entry in cache.
 * @param cache		pointer to fingerprint cache
 * @param dirName	string to locate in cache
 * @param dirHash	precalculated string hash
 * @return pointer to directory name entry (or NULL if not found).
 */
static const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache,
			    const char * dirName, unsigned int dirHash)
{
    const struct fprintCacheEntry_s ** data;

    if (rpmFpEntryHashGetHEntry(cache->ht, dirName, dirHash, &data, NULL, NULL))
	return data[0];
    return NULL;
}

int fpLookup(fingerPrintCache cache,
	     const char * dirName, const char * baseName, int scareMemory,
	     fingerPrint *fp)
{
    char dir[PATH_MAX];
    const char * cleanDirName;
    size_t cdnl;
    char * end;		    /* points to the '\0' at the end of "buf" */
    struct stat sb;
    char *buf = NULL;
    char *cdnbuf = NULL;
    const struct fprintCacheEntry_s * cacheHit;

    /* assert(*dirName == '/' || !scareMemory); */

    /* XXX WATCHOUT: fp.subDir is set below from relocated dirName arg */
    cleanDirName = dirName;
    cdnl = strlen(cleanDirName);

    if (*cleanDirName == '/') {
	if (!scareMemory) {
	    cdnbuf = xstrdup(dirName);
	    char trailingslash = (cdnbuf[strlen(cdnbuf)-1] == '/');
	    cdnbuf = rpmCleanPath(cdnbuf);
	    if (trailingslash) {
		cdnbuf = rstrcat(&cdnbuf, "/");
	    }
	    cleanDirName = cdnbuf;
	    cdnl = strlen(cleanDirName);
	}
    } else {
	scareMemory = 0;	/* XXX causes memory leak */

	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we 
	 * do instead is use realpath() on `.' and then append arg to
	 * the result.
	 */

	/* if the current directory doesn't exist, we might fail. 
	   oh well. likewise if it's too long.  */
	dir[0] = '\0';
	if (realpath(".", dir) != NULL) {
	    end = dir + strlen(dir);
	    if (end[-1] != '/')	*end++ = '/';
	    end = stpncpy(end, cleanDirName, sizeof(dir) - (end - dir));
	    *end = '\0';
	    (void)rpmCleanPath(dir); /* XXX possible /../ from concatenation */
	    end = dir + strlen(dir);
	    if (end[-1] != '/')	*end++ = '/';
	    *end = '\0';
	    cleanDirName = dir;
	    cdnl = end - dir;
	}
    }

    memset(fp, 0, sizeof(*fp));
    if (cleanDirName == NULL) goto exit; /* XXX can't happen */

    buf = xstrdup(cleanDirName);
    end = buf + cdnl;

    /* no need to pay attention to that extra little / at the end of dirName */
    if (buf[1] && end[-1] == '/') {
	end--;
	*end = '\0';
    }

    while (1) {
	/* buf contents change through end ptr, requiring rehash on each loop */
	const char *fpDir = (*buf != '\0') ? buf : "/";
	unsigned int fpHash = rpmFpEntryHashKeyHash(cache->ht, fpDir);

	/* as we're stating paths here, we want to follow symlinks */
	cacheHit = cacheContainsDirectory(cache, fpDir, fpHash);
	if (cacheHit != NULL) {
	    fp->entry = cacheHit;
	} else if (!stat(fpDir, &sb)) {
	    struct fprintCacheEntry_s * newEntry = xmalloc(sizeof(* newEntry));

	    newEntry->ino = sb.st_ino;
	    newEntry->dev = sb.st_dev;
	    newEntry->dirName = xstrdup(fpDir);
	    fp->entry = newEntry;

	    rpmFpEntryHashAddHEntry(cache->ht,
				    newEntry->dirName, fpHash, fp->entry);
	}

        if (fp->entry) {
	    fp->subDir = cleanDirName + (end - buf);
	    if (fp->subDir[0] == '/' && fp->subDir[1] != '\0')
		fp->subDir++;
	    if (fp->subDir[0] == '\0' ||
	    /* XXX don't bother saving '/' as subdir */
	       (fp->subDir[0] == '/' && fp->subDir[1] == '\0'))
		fp->subDir = NULL;
	    fp->baseName = baseName;
	    if (!scareMemory && fp->subDir != NULL)
		fp->subDir = xstrdup(fp->subDir);
	    goto exit;
	}

        /* stat of '/' just failed! */
	if (end == buf + 1)
	    abort();

	end--;
	while ((end > buf) && *end != '/') end--;
	if (end == buf)	    /* back to stat'ing just '/' */
	    end++;

	*end = '\0';
    }

exit:
    free(buf);
    free(cdnbuf);
    /* XXX TODO: failure from eg realpath() should be returned and handled */
    return 0;
}

/**
 * Return hash value for a finger print.
 * Hash based on dev and inode only!
 * @param key		pointer to finger print entry
 * @return hash value
 */
static unsigned int fpHashFunction(const fingerPrint * fp)
{
    unsigned int hash = 0;
    int j;

    hash = rstrhash(fp->baseName);
    if (fp->subDir) hash ^= rstrhash(fp->subDir);

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

void fpLookupList(fingerPrintCache cache, rpmstrPool pool,
		  rpmsid * dirNames, rpmsid * baseNames,
		  const uint32_t * dirIndexes, 
		  int fileCount, fingerPrint * fpList)
{
    int i;

    for (i = 0; i < fileCount; i++) {
	/* If this is in the same directory as the last file, don't bother
	   redoing all of this work */
	if (i > 0 && dirIndexes[i - 1] == dirIndexes[i]) {
	    fpList[i].entry = fpList[i - 1].entry;
	    fpList[i].subDir = fpList[i - 1].subDir;
	    fpList[i].baseName = rpmstrPoolStr(pool, baseNames[i]);
	} else {
	    fpLookup(cache,
		     rpmstrPoolStr(pool, dirNames[dirIndexes[i]]),
		     rpmstrPoolStr(pool, baseNames[i]),
		     1, &fpList[i]);
	}
    }
}

/* Check file for to be installed symlinks in their path and correct their fp */
static void fpLookupSubdir(rpmFpHash symlinks, fingerPrintCache fpc, rpmte p, int filenr)
{
    rpmfi fi = rpmteFI(p);
    struct fingerPrint_s current_fp;
    char *endsubdir, *endbasename, *currentsubdir;
    size_t lensubDir;

    struct rpmffi_s * recs;
    int numRecs;
    int i;
    fingerPrint *fp = rpmfiFpsIndex(fi, filenr);
    int symlinkcount = 0;
    struct rpmffi_s ffi = { p, filenr};

    if (fp->subDir == NULL) {
	 rpmFpHashAddEntry(fpc->fp, fp, ffi);
	 return;
    }

    lensubDir = strlen(fp->subDir);
    current_fp = *fp;
    currentsubdir = xstrdup(fp->subDir);

    /* Set baseName to the upper most dir */
    current_fp.baseName = endbasename = currentsubdir;
    while (*endbasename != '/' && endbasename < currentsubdir + lensubDir - 1)
	 endbasename++;
    *endbasename = '\0';

    current_fp.subDir = endsubdir = NULL; // no subDir for now

    while (endbasename < currentsubdir + lensubDir - 1) {
	 char found;
	 found = 0;

	 rpmFpHashGetEntry(symlinks, &current_fp,
		    &recs, &numRecs, NULL);

	 for (i=0; i<numRecs; i++) {
	      rpmfi foundfi = rpmteFI(recs[i].p);
	      char const *linktarget = rpmfiFLinkIndex(foundfi, recs[i].fileno);
	      char *link;

	      if (linktarget && *linktarget != '\0') {
		   /* this "directory" is a symlink */
		   link = NULL;
		   if (*linktarget != '/') {
			rstrscat(&link, current_fp.entry->dirName,
				 current_fp.subDir ? "/" : "",
				 current_fp.subDir ? current_fp.subDir : "",
				 "/", NULL);
		   }
		   rstrscat(&link, linktarget, "/", NULL);
		   if (strlen(endbasename+1)) {
			rstrscat(&link, endbasename+1, "/", NULL);
		   }

		   fpLookup(fpc, link, fp->baseName, 0, fp);

		   free(link);
		   free(currentsubdir);
		   symlinkcount++;

		   /* setup current_fp for the new path */
		   found = 1;
		   current_fp = *fp;
		   if (fp->subDir == NULL) {
		     /* directory exists - no need to look for symlinks */
		     rpmFpHashAddEntry(fpc->fp, fp, ffi);
		     return;
		   }
		   lensubDir = strlen(fp->subDir);
		   currentsubdir = xstrdup(fp->subDir);
		   current_fp.subDir = endsubdir = NULL; // no subDir for now

		   /* Set baseName to the upper most dir */
		   current_fp.baseName = currentsubdir;
		   endbasename = currentsubdir;
		   while (*endbasename != '/' &&
			  endbasename < currentsubdir + lensubDir - 1)
			endbasename++;
		   *endbasename = '\0';
		   break;

	      }
	 }
	 if (symlinkcount>50) {
	      // found too many symlinks in the path
	      // most likley a symlink cicle
	      // giving up
	      // TODO warning/error
	      break;
	 }
	 if (found) {
	      continue; // restart loop after symlink
	 }

	 if (current_fp.subDir == NULL) {
              /* after first round set former baseName as subDir */
	      current_fp.subDir = currentsubdir;
	 } else {
	      *endsubdir = '/'; // rejoin the former baseName with subDir
	 }
	 endsubdir = endbasename;

	 /* set baseName to the next lower dir */
	 endbasename++;
	 while (*endbasename != '\0' && *endbasename != '/')
	      endbasename++;
	 *endbasename = '\0';
	 current_fp.baseName = endsubdir+1;

    }
    free(currentsubdir);
    rpmFpHashAddEntry(fpc->fp, fp, ffi);

}

int fpCacheGetByFp(fingerPrintCache cache, struct fingerPrint_s * fp,
		   struct rpmffi_s ** recs, int * numRecs)
{
    return rpmFpHashGetEntry(cache->fp, fp, recs, numRecs, NULL);
}

void fpCachePopulate(fingerPrintCache fpc, rpmts ts, int fileCount)
{
    rpmtsi pi;
    rpmte p;
    rpmfs fs;
    rpmfi fi;
    int i, fc;

    if (fpc->fp == NULL)
	fpc->fp = rpmFpHashCreate(fileCount/2 + 10001, fpHashFunction, fpEqual,
				  NULL, NULL);

    rpmFpHash symlinks = rpmFpHashCreate(fileCount/16+16, fpHashFunction, fpEqual, NULL, NULL);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	if ((fi = rpmteFI(p)) == NULL)
	    continue;	/* XXX can't happen */

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	rpmfiFpLookup(fi, fpc);
	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);
	/* collect symbolic links */
	for (i = 0; i < fc; i++) {
	    struct rpmffi_s ffi;
	    char const *linktarget;
	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		continue;
	    linktarget = rpmfiFLinkIndex(fi, i);
	    if (!(linktarget && *linktarget != '\0'))
		continue;
	    ffi.p = p;
	    ffi.fileno = i;
	    rpmFpHashAddEntry(symlinks, rpmfiFpsIndex(fi, i), ffi);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);

    }
    rpmtsiFree(pi);

    /* ===============================================
     * Check fingerprints if they contain symlinks
     * and add them to the hash table
     */

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	(void) rpmdbCheckSignals();

	fs = rpmteGetFileStates(p);
	fc = rpmfsFC(fs);
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	for (i = 0; i < fc; i++) {
	    if (XFA_SKIPPING(rpmfsGetAction(fs, i)))
		continue;
	    fpLookupSubdir(symlinks, fpc, p, i);
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
    }
    rpmtsiFree(pi);

    rpmFpHashFree(symlinks);
}

