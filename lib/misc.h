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

/**
 */
typedef int (*md5func) (const char * fn, /*@out@*/ unsigned char * digest)
	/*@globals fileSystem@*/
	/*@modifies *digest, fileSystem @*/;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate MD5 sum for file.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @return		0 on success, 1 on error
 */
/*@-exportlocal@*/
int domd5(const char * fn, /*@out@*/ unsigned char * digest, int asAscii)
	/*@globals fileSystem@*/
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
	/*@globals fileSystem@*/
	/*@modifies digest, fileSystem @*/
{
    return domd5(fn, digest, 1);
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
	/*@globals fileSystem@*/
	/*@modifies bindigest, fileSystem @*/
{
    return domd5(fn, bindigest, 0);
}

/**
 * Split string into fields separated by a character.
 * @param str		string
 * @param length	length of string
 * @param sep		separator character
 * @return		(malloc'd) argv array
 */
/*@only@*/ char ** splitString(const char * str, int length, char sep)
	/*@*/;

/**
 * Free split string argv array.
 * @param list		argv array
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
 * Check if file esists using stat(2).
 * @param urlfn		file name (may be URL)
 * @return		1 if file exists, 0 if not
 */
int rpmfileexists(const char * urlfn)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/;

/**
 * Like the libc function, but malloc()'s the space needed.
 * @param name		variable name
 * @param value		variable value
 * @param overwrte	should an existing variable be changed?
 * @return		0 on success
 */
int dosetenv(const char * name, const char * value, int overwrite)
	/*@globals environ@*/
	/*@modifies *environ @*/;

/**
 * Like the libc function, but malloc()'s the space needed.
 * @param str		"name=value" string
 * @return		0 on success
 */
int doputenv(const char * str)
	/*@globals environ@*/
	/*@modifies *environ @*/;

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
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *fnptr, *fdptr, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/**
 * Return (malloc'd) current working directory.
 * @return		current working directory (malloc'ed)
 */
/*@only@*/ char * currentDirectory(void)
	/*@*/;

/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h		header
 */
void compressFilelist(Header h)
	/*@modifies h @*/;

/**
 * Convert (dirname,basename,dirindex) tags to absolute path tag.
 * @param h		header
 */
void expandFilelist(Header h)
	/*@modifies h @*/;

/**
 * @param h		header
 */
void buildOrigFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr)
	/*@modifies *fileListPtr, *fileCountPtr @*/;

/**
 */
/*@-exportlocal@*/
int myGlobPatternP (const char *patternURL)	/*@*/;
/*@=exportlocal@*/

/**
 */
int rpmGlob(const char * patterns, /*@out@*/ int * argcPtr,
		/*@out@*/ const char *** argvPtr)
	/*@globals fileSystem@*/
	/*@modifies *argcPtr, *argvPtr, fileSystem @*/;

/**
 * Retrofit a Provides: name = version-release dependency into legacy
 * packages.
 * @param h		header
 */
void providePackageNVR(Header h)
	/*@modifies h @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
