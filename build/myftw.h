/* myftw.h -- ftw() using lstat() instead of stat() */

#ifndef _MYFTW_H_
#define _MYFTW_H_

#include <sys/stat.h>

/* The FLAG argument to the user function passed to ftw.  */
#define MYFTW_F		0		/* Regular file.  */
#define MYFTW_D		1		/* Directory.  */
#define MYFTW_DNR	2		/* Unreadable directory.  */
#define MYFTW_NS	3		/* Unstatable file.  */

int myftw (const char *dir,
	   int descriptors,
	   int (*func) (void *fl, char *name, struct stat *statp),
	   void *fl);

#endif _MYFTW_H_
