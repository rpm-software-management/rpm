#ifndef _H_MYFTW_
#define _H_MYFTW_

/** \ingroup rpmbuild
 * \file build/myftw.h
 * Portable ftw(3) using lstat() instead of stat().
 */

#include <sys/stat.h>

/* The FLAG argument to the user function passed to ftw.  */
#define MYFTW_F		0		/* Regular file.  */
#define MYFTW_D		1		/* Directory.  */
#define MYFTW_DNR	2		/* Unreadable directory.  */
#define MYFTW_NS	3		/* Unstatable file.  */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*myftwFunc) (void *fl, const char *name, struct stat *statp)
	/*@*/;

int myftw (const char *dir, int descriptors, myftwFunc func, void *fl)
	/*@modifies *fl, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_MYFTW_ */
