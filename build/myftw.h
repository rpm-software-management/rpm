/* myftw.h -- ftw() using lstat() instead of stat() */

int myftw (const char *dir, __ftw_func_t func, int descriptors);
