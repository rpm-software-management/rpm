#ifndef H_MISC
#define H_MISC

/**
 * \file lib/misc.h
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

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
 * Like the libc function, but malloc()'s the space needed.
 * @param name		variable name
 * @param value		variable value
 * @param overwrite	should an existing variable be changed?
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

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
