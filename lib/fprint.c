#include "system.h"

#include <rpmlib.h>
#include "fprint.h"

fingerPrintCache fpCacheCreate(int sizeHint)
{
    fingerPrintCache fpc;

    fpc = xmalloc(sizeof(*fpc));
    fpc->ht = htCreate(sizeHint * 2, 0, 1, hashFunctionString,
		       hashEqualityString);
    return fpc;
}

void fpCacheFree(fingerPrintCache cache)
{
    htFree(cache->ht);
    free(cache);
}

static const struct fprintCacheEntry_s * cacheContainsDirectory(
			    fingerPrintCache cache,
			    const char * dirName)
{
    const void ** data;

    if (htGetEntry(cache->ht, dirName, &data, NULL, NULL))
	return NULL;
    return data[0];
}

static fingerPrint doLookup(fingerPrintCache cache, const char * dirName,
			    const char * baseName, int scareMemory)
{
    char dir[PATH_MAX];
    char * end;
    fingerPrint fp;
    struct stat sb;
    char * buf;
    const struct fprintCacheEntry_s * cacheHit;

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
	    dirName = s;
	}
    }

    /* FIXME: perhaps we should collapse //, /./, and /../ stuff if
       !scareMemory?? */

    buf = alloca(strlen(dirName) + 1);
    strcpy(buf, dirName);
    end = buf + strlen(buf);
    fp.entry = NULL;
    while (*buf) {

	/* as we're stating paths here, we want to follow symlinks */

	if ((cacheHit = cacheContainsDirectory(cache, *buf ? buf : "/"))) {
	    fp.entry = cacheHit;
	} else if (!stat(*buf ? buf : "/", &sb)) {
	    size_t nb = sizeof(*fp.entry) + (*buf ? strlen(buf) : 1) + 1;
	    char * dn = xmalloc(nb);
	    struct fprintCacheEntry_s * newEntry = (void *)dn;
	    dn += sizeof(*newEntry);
	    strcpy(dn, (*buf ? buf : "/"));
	    newEntry->ino = sb.st_ino;
	    newEntry->dev = sb.st_dev;
	    newEntry->isFake = 0;
	    newEntry->dirName = dn;
	    fp.entry = newEntry;

	    htAddEntry(cache->ht, dn, fp.entry);
	}

        if (fp.entry) {
	    fp.subdir = dirName + (end - buf);
	    if (fp.subdir[0] == '/' && fp.subdir[1] != '\0')
		fp.subdir++;
	    else
		fp.subdir = "";
	    if (!scareMemory && fp.subdir != NULL)
		fp.subdir = xstrdup(fp.subdir);	/* XXX memory leak, but how
						   do we know we can free it? 
						   Using the (new) cache would
						   work if hash tables allowed
						   traversal. */
	    fp.basename = baseName;
	    return fp;
	}

	end--;
	while ((end > buf) && *end != '/') end--;
	*end = '\0';
    }

    /* This can't happen, or stat('/') just failed! */
    abort();
    /*@notreached@*/

    return fp;
}

fingerPrint fpLookup(fingerPrintCache cache, const char * fullName, 
		     int scareMemory)
{
    char *dn = strcpy(alloca(strlen(fullName)+1), fullName);
    char *bn = strrchr(dn, '/');

    if (bn)
	*bn++ = '\0';
    else
	bn = dn;

    return doLookup(cache, dn, bn, scareMemory);
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
    const fingerPrint *k1 = key1;
    const fingerPrint *k2 = key2;
    /* XXX negated to preserve strcmp return behavior in ht->eq */
    return (FP_EQUAL(*k1, *k2) ? 0 : 1);
}

void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const int * dirIndexes, 
		  int fileCount, fingerPrint * fpList)
{
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

void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList)
{
    int fileCount;
    const char ** baseNames, ** dirNames;
    int_32 * dirIndexes;

    if (!headerGetEntryMinMemory(h, RPMTAG_COMPFILELIST, NULL, 
			(void **) &baseNames, &fileCount)) return;
    headerGetEntryMinMemory(h, RPMTAG_COMPDIRLIST, NULL, (void **) &dirNames, 
			NULL);
    headerGetEntry(h, RPMTAG_COMPFILEDIRS, NULL, (void **) &dirIndexes, NULL);
			
    fpLookupList(cache, dirNames, baseNames, dirIndexes, fileCount, fpList);

    free(dirNames);
    free(baseNames);
}
