#include "system.h"

#include "rpmlib.h"

#include "fprint.h"

fingerPrint fpLookup(char * fullName, int scareMemory) {
    char dir[PATH_MAX];
    char * chptr1, * end;
    fingerPrint fp;
    struct stat sb;
    char * buf;

    /* assert(*fullName == '/' || !scareMemory); */

    /* FIXME: a directory stat cache could *really* speed things up. we'd
       have to be sure to flush it, but... */

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
	    chptr1 = alloca(strlen(dir) + strlen(fullName) + 2);
	    sprintf(chptr1, "%s/%s", dir, fullName);
	    fullName = chptr1;
	}
    }

    /* FIXME: perhaps we should collapse //, /./, and /../ stuff if
       !scareMemory?? */

    buf = alloca(strlen(fullName) + 1);
    strcpy(buf, fullName);
    end = strrchr(buf, '/');
    while (*buf) {
	*end = '\0';

	/* as we're stating paths here, we want to follow symlinks */
	if (!stat(buf, &sb)) {
	    chptr1 = fullName + (end - buf) + 1;
	    if (scareMemory)
		fp.basename = strdup(chptr1);
	    else
		fp.basename = chptr1;
	    fp.ino = sb.st_ino;
	    fp.dev = sb.st_dev;
	    return fp;
	}

	buf--;
	while ((end > buf) && *end != '/') end--;
    }

    /* this can't happen, or stat('/') just failed! */
    fp.basename = 0;
    fp.ino = fp.dev = 0;

    return fp;
}

unsigned int fpHashFunction(const void * key) {
    const fingerPrint * fp = key;
    unsigned int hash = 0;
    char ch;
    char * chptr;

    ch = 0;
    chptr = fp->basename;
    while (*chptr) ch ^= *chptr++;

    hash |= ch << 24;
    hash |= (((fp->dev >> 8) ^ fp->dev) & 0xFF) << 16;
    hash |= fp->ino & 0xFFFF;
    
    return hash;
}

int fpEqual(const void * key1, const void * key2) {
    return FP_EQUAL(*((const fingerPrint *) key1), *((fingerPrint *) key2));
}
