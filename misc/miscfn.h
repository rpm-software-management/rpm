#ifndef H_MISCFN
#define H_MISCFN

#include "config.h"

#if HAVE_FNMATCH_H
#include <fnmatch.h>
#else
#include "misc-fnmatch.h"
#endif

#if HAVE_GLOB_H
#include <glob.h>
#else
#include "misc-glob.h"
#endif

#if ! HAVE_REALPATH
char *realpath(char *path, char resolved_path[]);
#endif

#if ! HAVE_S_IFSOCK
#define S_IFSOCK (0)
#endif

#if ! HAVE_S_ISLNK
#define S_ISLNK(mode) ((mode) & S_IFLNK)
#endif

#if ! HAVE_S_ISSOCK
#define S_ISSOCK(mode) ((mode) & S_IFSOCK)
#endif

#if NEED_STRINGS_H
#include <strings.h>
#endif

#if ! HAVE_REALPATH
char *realpath(const char *path, char resolved_path []);
#endif

#if NEED_TIMEZONE
#include <sys/stdtypes.h>
extern time_t timezone;
#endif

#if NEED_MYREALLOC
#include <sys/stdtypes.h>
#define realloc(ptr,size) myrealloc(ptr,size)
extern void *myrealloc(void *, size_t);
#endif

#endif
