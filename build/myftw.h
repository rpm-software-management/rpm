/* myftw.h -- ftw() using lstat() instead of stat() */

#ifndef _MYFTW_H_
#define _MYFTW_H_

int myftw (const char *dir, __ftw_func_t func, int descriptors);

#endif _MYFTW_H_
