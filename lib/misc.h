#ifndef H_MISC
#define H_MISC

#include <unistd.h>
#include <sys/types.h>

#include "header.h"
#include "ugid.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
/*@only@*/ char ** splitString(const char * str, int length, char sep);

/**
 */
void	freeSplitString( /*@only@*/ char ** list);

/**
 */
void	stripTrailingSlashes(char * str)	/*@modifies *str @*/;

/**
 */
int	rpmfileexists(const char * filespec)	/*@*/;

/**
 */
int	rpmfileexists(const char * filespec);

/**
 */
int	rpmvercmp(const char * one, const char * two);

/* these are like the normal functions, but they malloc() the space which
   is needed */

/**
 */
int	rpmfileexists(const char * filespec);

/**
 */
int	dosetenv(const char *name, const char *value, int overwrite);

/**
 */
int	doputenv(const char * str);

/**
 */
int	makeTempFile(const char * prefix, /*@out@*/ const char ** fnptr,
			/*@out@*/ FD_t * fdptr);

/**
 * @return		cureent working directory (malloc'ed)
 */
/*@only@*/ char * currentDirectory(void);

/**
 */
void	compressFilelist(Header h);

/**
 */
void	expandFilelist(Header h);

/**
 */
void	buildOrigFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr);

/**
 */
int myGlobPatternP (const char *patternURL)	/*@*/;

/**
 */
int rpmGlob(const char * patterns, /*@out@*/ int * argcPtr,
	/*@out@*/ const char *** argvPtr)
		/*@modifies *argcPtr, *argvPtr @*/;

/**
 */
void providePackageNVR(Header h);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
