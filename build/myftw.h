/* myftw.h -- ftw() using lstat() instead of stat() */

#ifndef _MYFTW_H_
#define _MYFTW_H_

int myftw (const char *dir,
	   int (*func) (const char *file,
			struct stat *status,
			int flag),
	   int descriptors);

#endif _MYFTW_H_
