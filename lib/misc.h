#ifndef H_MISC
#define H_MISC

#include <unistd.h>
#include <sys/types.h>

#include "header.h"

#ifdef __cplusplus
extern "C" {
#endif

/*@only@*/ char ** splitString(const char * str, int length, char sep);
void	freeSplitString( /*@only@*/ char ** list);
void	stripTrailingSlashes(char * str);

int	rpmfileexists(const char * filespec);

int	rpmvercmp(const char * one, const char * two);

/* these are like the normal functions, but they malloc() the space which
   is needed */
int	dosetenv(const char *name, const char *value, int overwrite);
int	doputenv(const char * str);

/* These may be called w/ a NULL argument to flush the cache -- they return
   -1 if the user can't be found */
int	unameToUid(const char * thisUname, /*@out@*/ uid_t * uid);
int	gnameToGid(const char * thisGname, /*@out@*/ gid_t * gid);

/* Call w/ -1 to flush the cache, returns NULL if the user can't be found */
/*@observer@*/ /*@null@*/ char * uidToUname(uid_t uid);
/*@observer@*/ /*@null@*/ char * gidToGname(gid_t gid);

int	makeTempFile(const char * prefix, /*@out@*/ const char ** fnptr,
			/*@out@*/ FD_t * fdptr);
char *	currentDirectory(void);		/* result needs to be freed */
void	compressFilelist(Header h);
void	expandFilelist(Header h);
void	buildOrigFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr);

int myGlobPatternP (const char *patternURL);
int rpmGlob(const char * patterns, int * argcPtr, const char *** argvPtr);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
