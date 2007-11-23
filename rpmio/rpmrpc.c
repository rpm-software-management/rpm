/** \ingroup rpmio
 * \file rpmio/rpmrpc.c
 */

#include "system.h"

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#include <rpmurl.h>
#include <rpmstring.h>
#include "rpmio_internal.h"

#include "ugid.h"
#include "debug.h"

/* =============================================================== */
int Mkdir (const char * path, mode_t mode)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return mkdir(path, mode);
}

int Chdir (const char * path)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return chdir(path);
}

int Rmdir (const char * path)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return rmdir(path);
}

/* XXX rpmdb.c: analogue to rename(2). */

int Rename (const char * oldpath, const char * newpath)
{
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    /* XXX lib/install.c used to rely on this behavior. */
    if (!strcmp(oldpath, newpath)) return 0;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    default:
	return -2;
	break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_PATH:
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return rename(oldpath, newpath);
}

int Link (const char * oldpath, const char * newpath)
{
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    default:
	return -2;
	break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_PATH:
if (_rpmio_debug)
fprintf(stderr, "*** link old %*s new %*s\n", (int)(oe - oldpath), oldpath, (int)(ne - newpath), newpath);
	if (!(oldut == newut && oe && ne && (oe - oldpath) == (ne - newpath) &&
	    !xstrncasecmp(oldpath, newpath, (oe - oldpath))))
	    return -2;
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    default:
	return -2;
	break;
    }
    return link(oldpath, newpath);
}

/* XXX build/build.c: analogue to unlink(2). */

int Unlink(const char * path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return unlink(path);
}

int Stat(const char * path, struct stat * st)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Stat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return stat(path, st);
}

int Lstat(const char * path, struct stat * st)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Lstat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
    return lstat(path, st);
}

int Readlink(const char * path, char * buf, size_t bufsiz)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    default:
	return -2;
	break;
    }
/* FIX: *buf is undefined */
    return readlink(path, buf, bufsiz);
}

int Access(const char * path, int amode)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Access(%s,%d)\n", path, amode);
    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    default:
	return -2;
	break;
    }
    return access(path, amode);
}

/* glob_pattern_p() taken from bash
 * Copyright (C) 1985, 1988, 1989 Free Software Foundation, Inc.
 *
 * Return nonzero if PATTERN has any special globbing chars in it.
 */
int Glob_pattern_p (const char * pattern, int quote)
{
    const char *p;
    int open = 0;
    char c;
  
    (void) urlPath(pattern, &p);
    while ((c = *p++) != '\0')
	switch (c) {
	case '?':
	case '*':
	    return (1);
	case '\\':
	    if (quote && *p != '\0')
		p++;
	    continue;

	case '[':
	    open = 1;
	    continue;
	case ']':
	    if (open)
		return (1);
	    continue;

	case '+':
	case '@':
	case '!':
	    if (*p == '(')
		return (1);
	    continue;
	}

    return (0);
}

int Glob_error(const char * epath, int eerrno)
{
    return 1;
}

int Glob(const char *pattern, int flags,
	int errfunc(const char * epath, int eerrno), glob_t *pglob)
{
    const char * lpath;
    int ut = urlPath(pattern, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Glob(%s,0x%x,%p,%p)\n", pattern, (unsigned)flags, (void *)errfunc, pglob);
    switch (ut) {
    case URL_IS_PATH:
	pattern = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    default:
	return -2;
	break;
    }
    return glob(pattern, flags, errfunc, pglob);
}

void Globfree(glob_t *pglob)
{
if (_rpmio_debug)
fprintf(stderr, "*** Globfree(%p)\n", pglob);
    globfree(pglob);
}

DIR * Opendir(const char * path)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Opendir(%s)\n", path);
    switch (ut) {
    case URL_IS_PATH:
	path = lpath;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_HTTPS:	
    case URL_IS_HTTP:
    default:
	return NULL;
	break;
    }
    return opendir(path);
}

struct dirent * Readdir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Readdir(%p)\n", (void *)dir);
    if (dir == NULL)
	return NULL;
    return readdir(dir);
}

int Closedir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Closedir(%p)\n", (void *)dir);
    if (dir == NULL)
	return 0;
    return closedir(dir);
}
