/** \ingroup rpmio
 * \file rpmio/rpmdav.c
 */

#include "system.h"

#if defined(HAVE_PTHREAD_H) && !defined(__LCLINT__)
#include <pthread.h>
#endif

#include <rpmio_internal.h>

#define _RPMDAV_INTERNAL
#include <rpmdav.h>
                                                                                
#include "ugid.h"
#include "debug.h"

/*@access DIR @*/
/*@access FD_t @*/
/*@access urlinfo @*/

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/* =============================================================== */
#ifdef	NOTYET
static int davMkdir(const char * path, /*@unused@*/ mode_t mode)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc;
    if ((rc = davCmd("MKD", path, NULL)) != 0)
	return rc;
#if NOTYET
    {	char buf[20];
	sprintf(buf, " 0%o", mode);
	(void) davCmd("SITE CHMOD", path, buf);
    }
#endif
    return rc;
}

static int davChdir(const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    return davCmd("CWD", path, NULL);
}

static int davRmdir(const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    return davCmd("RMD", path, NULL);
}

static int davRename(const char * oldpath, const char * newpath)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc;
    if ((rc = davCmd("RNFR", oldpath, NULL)) != 0)
	return rc;
    return davCmd("RNTO", newpath, NULL);
}

static int davUnlink(const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    return davCmd("DELE", path, NULL);
}
#endif	/* NOTYET */

/* =============================================================== */
	
#ifdef	NOTYET
/*@unchecked@*/
static int dav_st_ino = 0xdead0000;

static int davStat(const char * path, /*@out@*/ struct stat *st)
	/*@globals dav_st_ino, h_errno, fileSystem, internalState @*/
	/*@modifies *st, dav_st_ino, fileSystem, internalState @*/
{
    char buf[1024];
    int rc;
    rc = davNLST(path, DO_FTP_STAT, st, NULL, 0);
    /* XXX fts(3) needs/uses st_ino, make something up for now. */
    if (st->st_ino == 0)
	st->st_ino = dav_st_ino++;
if (_dav_debug)
fprintf(stderr, "*** davStat(%s) rc %d\n%s", path, rc, statstr(st, buf));
    return rc;
}

static int davLstat(const char * path, /*@out@*/ struct stat *st)
	/*@globals dav_st_ino, h_errno, fileSystem, internalState @*/
	/*@modifies *st, dav_st_ino, fileSystem, internalState @*/
{
    char buf[1024];
    int rc;
    rc = davNLST(path, DO_FTP_LSTAT, st, NULL, 0);
    /* XXX fts(3) needs/uses st_ino, make something up for now. */
    if (st->st_ino == 0)
	st->st_ino = dav_st_ino++;
if (_dav_debug)
fprintf(stderr, "*** davLstat(%s) rc %d\n%s\n", path, rc, statstr(st, buf));
    return rc;
}

static int davReadlink(const char * path, /*@out@*/ char * buf, size_t bufsiz)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    int rc;
    rc = davNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
if (_dav_debug)
fprintf(stderr, "*** davReadlink(%s) rc %d\n", path, rc);
    return rc;
}
#endif	/* NOTYET */

/*@unchecked@*/
int avmagicdir = 0x3607113;

int avClosedir(/*@only@*/ DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;

if (_av_debug)
fprintf(stderr, "*** avClosedir(%p)\n", avdir);

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon @*/
    (void) pthread_mutex_destroy(&avdir->lock);
/*@=moduncon =noeffectuncon @*/
#endif

    avdir = _free(avdir);
    return 0;
}

struct dirent * avReaddir(DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;
    struct dirent * dp;
    const char ** av;
    unsigned char * dt;
    int ac;
    int i;

    if (avdir == NULL || !ISAVMAGIC(avdir) || avdir->data == NULL) {
	/* XXX TODO: EBADF errno. */
	return NULL;
    }

    dp = (struct dirent *) avdir->data;
    av = (const char **) (dp + 1);
    ac = avdir->size;
    dt = (char *) (av + (ac + 1));
    i = avdir->offset + 1;

/*@-boundsread@*/
    if (i < 0 || i >= ac || av[i] == NULL)
	return NULL;
/*@=boundsread@*/

    avdir->offset = i;

    /* XXX glob(3) uses REAL_DIR_ENTRY(dp) test on d_ino */
/*@-type@*/
    dp->d_ino = i + 1;		/* W2DO? */
    dp->d_reclen = 0;		/* W2DO? */

#if !defined(hpux) && !defined(sun)
    dp->d_off = 0;		/* W2DO? */
/*@-boundsread@*/
    dp->d_type = dt[i];
/*@=boundsread@*/
#endif
/*@=type@*/

    strncpy(dp->d_name, av[i], sizeof(dp->d_name));
if (_av_debug)
fprintf(stderr, "*** avReaddir(%p) %p \"%s\"\n", (void *)avdir, dp, dp->d_name);
    
    return dp;
}

/*@-boundswrite@*/
DIR * avOpendir(const char * path)
{
    AVDIR avdir;
    struct dirent * dp;
    size_t nb = 0;
    const char ** av;
    unsigned char * dt;
    char * t;
    int ac;

if (_av_debug)
fprintf(stderr, "*** avOpendir(%s)\n", path);
    nb = sizeof(".") + sizeof("..");
    ac = 2;

    nb += sizeof(*avdir) + sizeof(*dp) + ((ac + 1) * sizeof(*av)) + (ac + 1);
    avdir = xcalloc(1, nb);
/*@-abstract@*/
    dp = (struct dirent *) (avdir + 1);
    av = (const char **) (dp + 1);
    dt = (char *) (av + (ac + 1));
    t = (char *) (dt + ac + 1);
/*@=abstract@*/

    avdir->fd = avmagicdir;
/*@-usereleased@*/
    avdir->data = (char *) dp;
/*@=usereleased@*/
    avdir->allocation = nb;
    avdir->size = ac;
    avdir->offset = -1;
    avdir->filepos = 0;

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon -nullpass @*/
    (void) pthread_mutex_init(&avdir->lock, NULL);
/*@=moduncon =noeffectuncon =nullpass @*/
#endif

    ac = 0;
    /*@-dependenttrans -unrecog@*/
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, ".");	t++;
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, "..");	t++;
    /*@=dependenttrans =unrecog@*/

    av[ac] = NULL;

/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}
/*@=boundswrite@*/

/*@-boundswrite@*/
DIR * davOpendir(const char * path)
{
    AVDIR avdir;
    struct dirent * dp;
    size_t nb;
    const char ** av;
    unsigned char * dt;
    char * t;
    int ac;

if (_dav_debug)
fprintf(stderr, "*** davOpendir(%s)\n", path);

/* XXX read dav collection. */

    nb = sizeof(".") + sizeof("..");
    ac = 2;

    nb += sizeof(*avdir) + sizeof(*dp) + ((ac + 1) * sizeof(*av)) + (ac + 1);
    avdir = xcalloc(1, nb);
    /*@-abstract@*/
    dp = (struct dirent *) (avdir + 1);
    av = (const char **) (dp + 1);
    dt = (char *) (av + (ac + 1));
    t = (char *) (dt + ac + 1);
    /*@=abstract@*/

    avdir->fd = avmagicdir;
/*@-usereleased@*/
    avdir->data = (char *) dp;
/*@=usereleased@*/
    avdir->allocation = nb;
    avdir->size = ac;
    avdir->offset = -1;
    avdir->filepos = 0;

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon -nullpass @*/
    (void) pthread_mutex_init(&avdir->lock, NULL);
/*@=moduncon =noeffectuncon =nullpass @*/
#endif

    ac = 0;
    /*@-dependenttrans -unrecog@*/
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, ".");	t++;
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, "..");	t++;
    /*@=dependenttrans =unrecog@*/

/* XXX load dav collection. */

    av[ac] = NULL;

/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}
/*@=boundswrite@*/
