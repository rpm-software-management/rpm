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

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if ! HAVE_LCHOWN
#define lchown chown
#endif

#if HAVE_MNTENT_H
#include <mntent.h>
#define GETMNTENT_ONE 1
#define GETMNTENT_TWO 0
#define our_mntent struct mntent
#defeine our_mntdir mnt_dir
#elif HAVE_SYS_MNTTAB_H
#include <stdio.h>
#include <sys/mnttab.h>
#define GETMNTENT_ONE 0
#define GETMNTENT_TWO 1
#define our_mntdir mnt_mountp
#define our_mntent struct mnttab
#else
#error Neither mntent.h nor mnttab.h exists. I cannot build on this system.
#endif

#ifndef MOUNTED
#define MOUNTED "/etc/mnttab"
#endif

#endif
