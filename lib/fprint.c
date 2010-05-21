/**
 * \file lib/fprint.c
 */

#include "system.h"

#include <rpm/rpmfileutil.h>	/* for rpmCleanPath */

#include "lib/rpmdb_internal.h"
#include "lib/rpmfi_internal.h"
#include "lib/fprint.h"
#include "lib/misc.h"
#include "debug.h"
#include <libgen.h>

/* Create new hash table type rpmFpEntryHash */
#include "lib/rpmhash.C"

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE rpmFpEntryHash
#define HTKEYTYPE const char *
#define HTDATATYPE const struct fprintCacheEntry_s *
#include "lib/rpmhash.C"

fingerPrintCache fpCacheCreate(int sizeHint)
{
    fingerPrintCache fpc;

    fpc = xmalloc(sizeof(*fpc));
    fpc->ht = rpmFpEntryHashCreate(sizeHint, hashFunctionString, strcmp,
				   (rpmFpEntryHashFreeKey)free,
				   (rpmFpEntryHashFreeData)free);
    return fpc;
}

fingerPrintCache fpCacheFree(fingerPrintCache cache)
{
    if (cache) {
	cache->ht = rpmFpEntryHashFree(cache->ht);
	free(cache);
    }
    return NULL;
}

/**
 * Find directory name entry in cache.
 * @param cache		pointer to fingerprint cache
 * @param dirName	string to locate in cache
 * @return pointer to directory name entry (or NULL if not found).
 */
static const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache,
			    const char * dirName)
{
    const struct fprintCacheEntry_s ** data;

    if (rpmFpEntryHashGetEntry(cache->ht, dirName, &data, NULL, NULL))
	return data[0];
    return NULL;
}

/**
 * Return finger print of a file path.
 * @param cache		pointer to fingerprint cache
 * @param dirName	leading directory name of path
 * @param baseName	file name of path
 * @param scareMemory
 * @return pointer to the finger print associated with a file path.
 */
fingerPrint fpLookup(fingerPrintCache cache,
		const char * dirName, const char * baseName, int scareMemory)
{
    char dir[PATH_MAX];
    const char * cleanDirName;
    size_t cdnl;
    char * end;		    /* points to the '\0' at the end of "buf" */
    fingerPrint fp;
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
    fp.entry = NULL;
    fp.subDir = NULL;
    fp.baseName = NULL;
    if (cleanDirName == NULL) goto exit; /* XXX can't happen */

    buf = xstrdup(cleanDirName);
    end = buf + cdnl;

    /* no need to pay attention to that extra little / at the end of dirName */
    if (buf[1] && end[-1] == '/') {
	end--;
	*end = '\0';
    }

    while (1) {

	/* as we're stating paths here, we want to follow symlinks */

	cacheHit = cacheContainsDirectory(cache, (*buf != '\0' ? buf : "/"));
	if (cacheHit != NULL) {
	    fp.entry = cacheHit;
	} else if (!stat((*buf != '\0' ? buf : "/"), &sb)) {
	    struct fprintCacheEntry_s * newEntry = xmalloc(sizeof(* newEntry));

	    newEntry->ino = sb.st_ino;
	    newEntry->dev = sb.st_dev;
	    newEntry->dirName = xstrdup((*buf != '\0' ? buf : "/"));
	    fp.entry = newEntry;

	    rpmFpEntryHashAddEntry(cache->ht, newEntry->dirName, fp.entry);
	}

        if (fp.entry) {
	    fp.subDir = cleanDirName + (end - buf);
	    if (fp.subDir[0] == '/' && fp.subDir[1] != '\0')
		fp.subDir++;
	    if (fp.subDir[0] == '\0' ||
	    /* XXX don't bother saving '/' as subdir */
	       (fp.subDir[0] == '/' && fp.subDir[1] == '\0'))
		fp.subDir = NULL;
	    fp.baseName = baseName;
	    if (!scareMemory && fp.subDir != NULL)
		fp.subDir = xstrdup(fp.subDir);
	/* FIX: fp.entry.{dirName,dev,ino} undef @*/
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
    /* FIX: fp.entry.{dirName,dev,ino} undef @*/
    return fp;
}

unsigned int fpHashFunction(const fingerPrint * fp)
{
    unsigned int hash = 0;
    int j;

    hash = hashFunctionString(fp->baseName);
    if (fp->subDir) hash ^= hashFunctionString(fp->subDir);

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

void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const uint32_t * dirIndexes, 
		  int fileCount, fingerPrint * fpList)
{
    int i;

    for (i = 0; i < fileCount; i++) {
	/* If this is in the same directory as the last file, don't bother
	   redoing all of this work */
	if (i > 0 && dirIndexes[i - 1] == dirIndexes[i]) {
	    fpList[i].entry = fpList[i - 1].entry;
	    fpList[i].subDir = fpList[i - 1].subDir;
	    fpList[i].baseName = baseNames[i];
	} else {
	    fpList[i] = fpLookup(cache, dirNames[dirIndexes[i]], baseNames[i],
				 1);
	}
    }
}

void fpLookupSubdir(rpmFpHash symlinks, rpmFpHash fphash, fingerPrintCache fpc, rpmte p, int filenr)
{
    rpmfi fi = rpmteFI(p);
    struct fingerPrint_s current_fp;
    char *endsubdir, *endbasename, *currentsubdir;
    size_t lensubDir;

    struct rpmffi_s * recs;
    int numRecs;
    int i, fiFX;
    fingerPrint *fp = rpmfiFpsIndex(fi, filenr);
    int symlinkcount = 0;
    struct rpmffi_s ffi = { p, filenr};

    if (fp->subDir == NULL) {
	 rpmFpHashAddEntry(fphash, fp, ffi);
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
	      rpmfi foundfi;
	      int filenr;
	      char const *linktarget;
	      char *link;

	      foundfi =  rpmteFI(recs[i].p);
	      fiFX = rpmfiFX(foundfi);

	      filenr = recs[i].fileno;
	      rpmfiSetFX(foundfi, filenr);
	      linktarget = rpmfiFLink(foundfi);

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

		   *fp = fpLookup(fpc, link, fp->baseName, 0);

		   free(link);
		   free(currentsubdir);
		   symlinkcount++;

		   /* setup current_fp for the new path */
		   found = 1;
		   current_fp = *fp;
		   if (fp->subDir == NULL) {
		     /* directory exists - no need to look for symlinks */
		     rpmFpHashAddEntry(fphash, fp, ffi);
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
	      rpmfiSetFX(foundfi, fiFX);
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
    rpmFpHashAddEntry(fphash, fp, ffi);

}
