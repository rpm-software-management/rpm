#include "system.h"

#include <rpmlib.h>
#include "fprint.h"

fingerPrintCache fpCacheCreate(int sizeHint) {
    fingerPrintCache fpc;

    fpc = malloc(sizeof(*fpc));
    fpc->ht = htCreate(sizeHint * 2, sizeof(char *), hashFunctionString,
		       hashEqualityString);
    return fpc;
}

void fpCacheFree(fingerPrintCache cache) {
}

static const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache,
			    const char * dirName) {
    const void ** data;
    int count;

    if (htGetEntry(cache->ht, dirName, &data, &count, NULL)) return NULL;
    return data[0];
}

static fingerPrint doLookup(fingerPrintCache cache, const char * dirName,
			    char * baseName, int scareMemory) {
    char dir[PATH_MAX];
    const char * chptr1;
    char * end, * bn;
    fingerPrint fp;
    struct stat sb;
    char * buf;
    int stripCount;
    const struct fprintCacheEntry_s * cacheHit;
    struct fprintCacheEntry_s * newEntry;

    /* assert(*dirName == '/' || !scareMemory); */

    if (*dirName != '/') {
	scareMemory = 0;

	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we 
	 * do instaed is use realpath() on `.' and then append arg to
	 * the result.
	 */

	/* if the current directory doesn't exist, we might fail. 
	   oh well. likewise if it's too long.  */
	dir[0] = '\0';
	if ( /*@-unrecog@*/ realpath(".", dir) /*@=unrecog@*/ != NULL) {
	    char *s = alloca(strlen(dir) + strlen(dirName) + 2);
	    sprintf(s, "%s/%s", dir, dirName);
	    dirName = chptr1 = s;
	}
    }

    /* FIXME: perhaps we should collapse //, /./, and /../ stuff if
       !scareMemory?? */

    buf = alloca(strlen(dirName) + 1);
    strcpy(buf, dirName);
    end = bn = strrchr(buf, '/');
    stripCount = 0;
    fp.entry = NULL;
    while (*buf) {
	*end = '\0';
	stripCount++;

	/* as we're stating paths here, we want to follow symlinks */

	if ((cacheHit = cacheContainsDirectory(cache, *buf ? buf : "/"))) {
	    fp.entry = cacheHit;
	} else if (!stat(*buf ? buf : "/", &sb)) {
	    newEntry = malloc(sizeof(*fp.entry));
	    newEntry->ino = sb.st_ino;
	    newEntry->dev = sb.st_dev;
	    newEntry->isFake = 0;
	    newEntry->dirName = strdup(*buf ? buf : "/");	    
	    fp.entry = newEntry;

	    /* XXX FIXME: memory leak */
	    htAddEntry(cache->ht, strdup(*buf ? buf : "/"), fp.entry);
	}

        if (fp.entry) {
	    chptr1 = dirName + (end - buf) + 1;
	    if (scareMemory)
		fp.subdir = chptr1;
	    else
		fp.subdir = xstrdup(chptr1);	/* XXX memory leak, but how
						   do we know we can free it? 
						   Using the (new) cache would
						   work if hash tables allowed
						   traversal. */
	    fp.basename = baseName;
	    return fp;
	}

	end--;
	while ((end > buf) && *end != '/') end--;
    }

    /* This can't happen, or stat('/') just failed! */
    abort();
    /*@notreached@*/

    return fp;
}

fingerPrint fpLookup(fingerPrintCache cache, const char * fullName, 
		     int scareMemory) {
    /* XXX FIXME */
    abort();
}

unsigned int fpHashFunction(const void * key)
{
    const fingerPrint * fp = key;
    unsigned int hash = 0;
    char ch;
    const char * chptr;

    ch = 0;
    chptr = fp->basename;
    while (*chptr) ch ^= *chptr++;

    hash |= ((unsigned)ch) << 24;
    hash |= (((((unsigned)fp->entry->dev) >> 8) ^ fp->entry->dev) & 0xFF) << 16;
    hash |= fp->entry->ino & 0xFFFF;
    
    return hash;
}

int fpEqual(const void * key1, const void * key2)
{
    return FP_EQUAL(*((const fingerPrint *) key1), *((fingerPrint *) key2));
}

void fpLookupList(fingerPrintCache cache, char ** dirNames, char ** baseNames,
		  int * dirIndexes, int fileCount, fingerPrint * fpList) {
    int i;

    for (i = 0; i < fileCount; i++) {
	/* If this is in the same directory as the last file, don't bother
	   redoing all of this work */
	if (i > 0 && dirIndexes[i - 1] == dirIndexes[i]) {
	    fpList[i].entry = fpList[i - 1].entry;
	    fpList[i].subdir = fpList[i - 1].subdir;
	    fpList[i].basename = baseNames[i];
	} else {
	    fpList[i] = doLookup(cache, dirNames[dirIndexes[i]], baseNames[i],
				 1);
	}
    }
}

void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList) {
    int fileCount;
    char ** baseNames, ** dirNames;
    int_32 * dirIndexes;

    if (!headerGetEntry(h, RPMTAG_COMPFILELIST, NULL, (void **) &baseNames,
			&fileCount)) return;
    headerGetEntry(h, RPMTAG_COMPDIRLIST, NULL, (void **) &dirNames, NULL);
    headerGetEntry(h, RPMTAG_COMPFILEDIRS, NULL, (void **) &dirIndexes, NULL);
			
    fpLookupList(cache, dirNames, baseNames, dirIndexes, fileCount, fpList);

    free(dirNames);
    free(baseNames);
}
