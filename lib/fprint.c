#include "system.h"

#include <rpmlib.h>
#include "fprint.h"

struct lookupCache {
    char * match;
    int pathsStripped;
    int matchLength;
    int stripLength;
    dev_t dev;
    ino_t ino;
};

static int strCompare(const void * a, const void * b)
{
    const char * const * one = a;
    const char * const * two = a;

    return strcmp(*one, *two);
}

static fingerPrint doLookup(const char * fullName, int scareMemory, 
			    struct lookupCache * cache)
{
    char dir[PATH_MAX];
    const char * chptr1;
    char * end, * bn;
    fingerPrint fp;
    struct stat sb;
    char * buf;
    int stripCount;

    /* assert(*fullName == '/' || !scareMemory); */

    /* FIXME: a better cache might speed things up? */

    if (cache && cache->pathsStripped && 
	!strncmp(fullName, cache->match, cache->matchLength)) {
	chptr1 = fullName + cache->matchLength;
	if (*chptr1 == '/') {
	    chptr1++;
	    stripCount = cache->pathsStripped - 1;
	    while (*chptr1) {
		if (*chptr1 == '/') stripCount--;
		chptr1++;
	    }

	    chptr1 = fullName + cache->matchLength + 1;
	    if (stripCount == 0 && !(cache->stripLength > 0 &&
		strncmp(cache->match+cache->matchLength+1, chptr1, cache->stripLength))) {
		if (scareMemory)
		    fp.basename = chptr1;
		else
		    fp.basename = strdup(chptr1);
		fp.ino = cache->ino;
		fp.dev = cache->dev;
		return fp;
	    }
	}
    }

    if (*fullName != '/') {
	scareMemory = 0;

	/* Using realpath on the arg isn't correct if the arg is a symlink,
	 * especially if the symlink is a dangling link.  What we should
	 * instead do is use realpath() on `.' and then append arg to
	 * it.
	 */

	/* if the current directory doesn't exist, we might fail. 
	   oh well. likewise if it's too long.  */
	if (realpath(".", dir) != NULL) {
	    char *s = alloca(strlen(dir) + strlen(fullName) + 2);
	    sprintf(s, "%s/%s", dir, fullName);
	    fullName = chptr1 = s;
	}
    }

    /* FIXME: perhaps we should collapse //, /./, and /../ stuff if
       !scareMemory?? */

    buf = alloca(strlen(fullName) + 1);
    strcpy(buf, fullName);
    end = bn = strrchr(buf, '/');
    stripCount = 0;
    while (*buf) {
	*end = '\0';
	stripCount++;

	/* as we're stating paths here, we want to follow symlinks */
	if (!stat(*buf ? buf : "/", &sb)) {
	    chptr1 = fullName + (end - buf) + 1;
	    if (scareMemory)
		fp.basename = chptr1;
	    else
		fp.basename = strdup(chptr1);
	    fp.ino = sb.st_ino;
	    fp.dev = sb.st_dev;

	    if (cache) {
		strcpy(cache->match, fullName);
		cache->match[strlen(fullName)+1] = '\0';
		cache->matchLength = (end-buf);
		cache->match[cache->matchLength] = '\0';
		cache->stripLength = (bn - (end+1));
		cache->match[bn-buf] = '\0';
		cache->pathsStripped = stripCount;
		cache->dev = sb.st_dev;
		cache->ino = sb.st_ino;
	    }

	    return fp;
	}

	end--;
	while ((end > buf) && *end != '/') end--;
    }

    /* this can't happen, or stat('/') just failed! */
    fp.basename = 0;
    fp.ino = fp.dev = 0;

    return fp;
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

    hash |= ch << 24;
    hash |= (((fp->dev >> 8) ^ fp->dev) & 0xFF) << 16;
    hash |= fp->ino & 0xFFFF;
    
    return hash;
}

int fpEqual(const void * key1, const void * key2)
{
    return FP_EQUAL(*((const fingerPrint *) key1), *((fingerPrint *) key2));
}

fingerPrint fpLookup(const char * fullName, int scareMemory)
{
    return doLookup(fullName, scareMemory, NULL);
}

void fpLookupList(const char ** fullNames, fingerPrint * fpList, int numItems,
		  int alreadySorted)
{
    int i, j;
    struct lookupCache cache;
    int maxLen = 0;

    if (!alreadySorted) {
	qsort(fullNames, numItems, sizeof(char *), strCompare);
    }

    for (i = 0; i < numItems; i++) {
	j = strlen(fullNames[i]);
	if (j > maxLen) maxLen = j;
    }

    cache.match = alloca(maxLen + 2);
    *cache.match = '\0';
    cache.matchLength = 0;
    cache.pathsStripped = 0;    
    cache.stripLength = 0;

    for (i = 0; i < numItems; i++) {
	fpList[i] = doLookup(fullNames[i], 1, &cache);
    }
}
