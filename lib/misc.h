#ifndef H_MISC
#define H_MISC

/**
 * \file lib/misc.h
 *
 */

#include <unistd.h>
#include <sys/types.h>

#include "header.h"
#include "ugid.h"

typedef unsigned char byte;
typedef unsigned int uint32;
typedef int (*md5func)(const char * fn, /*@out@*/ byte * digest);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate MD5 sum for file.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @param brokenEndian	calculate broken MD5 sum?
 * @return		0 on success, 1 on error
 */
/*@-exportlocal@*/
int domd5(const char * fn, /*@out@*/ unsigned char * digest, int asAscii,
		 int brokenEndian)
	/*@modifies digest, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Return MD5 sum of file as ASCII string.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdfile(const char * fn, /*@out@*/ unsigned char * digest)
	/*@modifies digest, fileSystem @*/
{
    return domd5(fn, digest, 1, 0);
}

/**
 * Return MD5 sum of file as binary data.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdbinfile(const char * fn, /*@out@*/ unsigned char * bindigest)
	/*@modifies bindigest, fileSystem @*/
{
    return domd5(fn, bindigest, 0, 0);
}

/**
 * Return (broken!) MD5 sum of file as ASCII string.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdfileBroken(const char * fn, /*@out@*/ unsigned char * digest)
	/*@modifies digest, fileSystem @*/
{
    return domd5(fn, digest, 1, 1);
}

/**
 * Return (broken!) MD5 sum of file as binary data.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdbinfileBroken(const char * fn, /*@out@*/ unsigned char * bindigest)
	/*@modifies bindigest, fileSystem @*/
{
    return domd5(fn, bindigest, 0, 1);
}

/**
 */
/*@only@*/ char ** splitString(const char * str, int length, char sep)
	/*@*/;

/**
 */
void freeSplitString( /*@only@*/ char ** list)
	/*@modifies list @*/;

/**
 * Remove occurences of trailing character from string.
 * @param s		string
 * @param c		character to strip
 * @return 		string
 */
/*@unused@*/ static inline
/*@only@*/ char * stripTrailingChar(/*@only@*/ char * s, char c)
	/*@modifies *s */
{
    char * t;
    for (t = s + strlen(s) - 1; *t == c && t >= s; t--)
	*t = '\0';
    return s;
}

/**
 */
int rpmfileexists(const char * urlfn)
	/*@modifies fileSystem @*/;

/*
 * These are like the libc functions, but they malloc() the space which
 * is needed.
 */

/**
 */
int dosetenv(const char * name, const char * value, int overwrite)
	/*@modifies fileSystem @*/;

/**
 */
int doputenv(const char * str)
	/*@modifies fileSystem @*/;

/**
 * Return file handle for a temporaray file.
 * A unique temporaray file path will be generated using
 *	rpmGenPath(prefix, "%{_tmppath}/", "rpm-tmp.XXXXX")
 * where "XXXXXX" is filled in using rand(3). The file is opened, and
 * the link count and (dev,ino) location are verified after opening.
 * The file name and the open file handle are returned.
 *
 * @param prefix	leading part of temp file path
 * @retval fnptr	temp file name (or NULL)
 * @retval fdptr	temp file handle
 * @return		0 on success
 */
int makeTempFile(/*@null@*/ const char * prefix,
		/*@null@*/ /*@out@*/ const char ** fnptr,
		/*@out@*/ FD_t * fdptr)
	/*@modifies *fnptr, *fdptr, fileSystem @*/;

/**
 * Return (malloc'd) current working directory.
 * @return		current working directory (malloc'ed)
 */
/*@only@*/ char * currentDirectory(void)
	/*@modifies fileSystem @*/;

/**
 */
void compressFilelist(Header h)
	/*@modifies h @*/;

/**
 */
void expandFilelist(Header h)
	/*@modifies h @*/;

/**
 */
void buildOrigFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr)
	/*@modifies *fileListPtr, *fileCountPtr @*/;

/**
 */
int myGlobPatternP (const char *patternURL)	/*@*/;

/**
 */
int rpmGlob(const char * patterns, /*@out@*/ int * argcPtr,
	/*@out@*/ const char *** argvPtr)
		/*@modifies *argcPtr, *argvPtr, fileSystem @*/;

/**
 */
void providePackageNVR(Header h)
	/*@modifies h @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
