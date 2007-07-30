/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_fsync.c,v 12.13 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#ifdef	HAVE_VXWORKS
#include "ioLib.h"

#define	fsync(fd)	__vx_fsync(fd)

int
__vx_fsync(fd)
	int fd;
{
	int ret;

	/*
	 * The results of ioctl are driver dependent.  Some will return the
	 * number of bytes sync'ed.  Only if it returns 'ERROR' should we
	 * flag it.
	 */
	if ((ret = ioctl(fd, FIOSYNC, 0)) != ERROR)
		return (0);
	return (ret);
}
#endif

#ifdef __hp3000s900
#define	fsync(fd)	__mpe_fsync(fd)

int
__mpe_fsync(fd)
	int fd;
{
	extern FCONTROL(short, short, void *);

	FCONTROL(_MPE_FILENO(fd), 2, NULL);	/* Flush the buffers */
	FCONTROL(_MPE_FILENO(fd), 6, NULL);	/* Write the EOF */
	return (0);
}
#endif

/*
 * __os_fsync --
 *	Flush a file descriptor.
 *
 * PUBLIC: int __os_fsync __P((DB_ENV *, DB_FH *));
 */
int
__os_fsync(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	int ret;

	DB_ASSERT(dbenv, F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

	/*
	 * Do nothing if the file descriptor has been marked as not requiring
	 * any sync to disk.
	 */
	if (F_ISSET(fhp, DB_FH_NOSYNC))
		return (0);

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: flush %s", fhp->name);

	if (DB_GLOBAL(j_fsync) != NULL)
		ret = DB_GLOBAL(j_fsync)(fhp->fd);
	else {
#if defined(F_FULLFSYNC)
		RETRY_CHK((fcntl(fhp->fd, F_FULLFSYNC, 0)), ret);
		/*
		 * On OS X, F_FULLSYNC only works on HFS+, so we need to fall
		 * back to regular fsync on other filesystems.
		 */
		if (ret == ENOTSUP)
			RETRY_CHK((fsync(fhp->fd)), ret);
#elif defined(HAVE_FDATASYNC)
		RETRY_CHK((fdatasync(fhp->fd)), ret);
#else
		RETRY_CHK((fsync(fhp->fd)), ret);
#endif
	}

	if (ret != 0) {
		__db_syserr(dbenv, ret, "fsync");
		ret = __os_posix_err(ret);
	}
	return (ret);
}
