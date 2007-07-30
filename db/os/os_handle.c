/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998,2007 Oracle.  All rights reserved.
 *
 * $Id: os_handle.c,v 12.15 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_openhandle --
 *	Open a file, using POSIX 1003.1 open flags.
 *
 * PUBLIC: int __os_openhandle
 * PUBLIC:     __P((DB_ENV *, const char *, int, int, DB_FH **));
 */
int
__os_openhandle(dbenv, name, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	int flags, mode;
	DB_FH **fhpp;
{
	DB_FH *fhp;
	u_int nrepeat, retries;
	int ret;
#ifdef HAVE_VXWORKS
	int newflags;
#endif
	/*
	 * Allocate the file handle and copy the file name.  We generally only
	 * use the name for verbose or error messages, but on systems where we
	 * can't unlink temporary files immediately, we use the name to unlink
	 * the temporary file when the file handle is closed.
	 *
	 * Lock the DB_ENV handle and insert the new file handle on the list.
	 */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &fhp)) != 0)
		return (ret);
	if ((ret = __os_strdup(dbenv, name, &fhp->name)) != 0)
		goto err;
	if (dbenv != NULL) {
		MUTEX_LOCK(dbenv, dbenv->mtx_env);
		TAILQ_INSERT_TAIL(&dbenv->fdlist, fhp, q);
		MUTEX_UNLOCK(dbenv, dbenv->mtx_env);
		F_SET(fhp, DB_FH_ENVLINK);
	}

#ifdef HAVE_QNX
	if (LF_ISSET(DB_OSO_REGION)) {
		if ((ret =
		    __os_qnx_region_open(dbenv, name, oflags, mode, fhp)) != 0)
			goto err;
		goto done;
	}
#endif

	/* If the application specified an interface, use it. */
	if (DB_GLOBAL(j_open) != NULL) {
		if ((fhp->fd = DB_GLOBAL(j_open)(name, flags, mode)) == -1) {
			ret = __os_posix_err(__os_get_syserr());
			goto err;
		}
		goto done;
	}

	retries = 0;
	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		ret = 0;
#ifdef	HAVE_VXWORKS
		/*
		 * VxWorks does not support O_CREAT on open, you have to use
		 * creat() instead.  (It does not support O_EXCL or O_TRUNC
		 * either, even though they are defined "for future support".)
		 * We really want the POSIX behavior that if O_CREAT is set,
		 * we open if it exists, or create it if it doesn't exist.
		 * If O_CREAT is specified, single thread and try to open the
		 * file.  If successful, and O_EXCL return EEXIST.  If
		 * unsuccessful call creat and then end single threading.
		 */
		if (LF_ISSET(O_CREAT)) {
			DB_BEGIN_SINGLE_THREAD;
			newflags = flags & ~(O_CREAT | O_EXCL);
			if ((fhp->fd = open(name, newflags, mode)) != -1) {
				/*
				 * We need to mark the file opened at this
				 * point so that if we get any error below
				 * we will properly close the fd we just
				 * opened on the error path.
				 */
				F_SET(fhp, DB_FH_OPENED);
				if (LF_ISSET(O_EXCL)) {
					/*
					 * If we get here, want O_EXCL create,
					 * and the file exists.  Close and
					 * return EEXISTS.
					 */
					DB_END_SINGLE_THREAD;
					ret = EEXIST;
					goto err;
				}
				/*
				 * XXX
				 * Assume any error means non-existence.
				 * Unfortunately return values (even for
				 * non-existence) are driver specific so
				 * there is no single error we can use to
				 * verify we truly got the equivalent of
				 * ENOENT.
				 */
			} else
				fhp->fd = creat(name, newflags);
			DB_END_SINGLE_THREAD;
		} else
		/* FALLTHROUGH */
#endif
#ifdef __VMS
		/*
		 * !!!
		 * Open with full sharing on VMS.
		 *
		 * We use these flags because they are the ones set by the VMS
		 * CRTL mmap() call when it opens a file, and we have to be
		 * able to open files that mmap() has previously opened, e.g.,
		 * when we're joining already existing DB regions.
		 */
		fhp->fd = open(name, flags, mode, "shr=get,put,upd,del,upi");
#else
		fhp->fd = open(name, flags, mode);
#endif
		if (fhp->fd != -1) {
			ret = 0;
			break;
		}

		switch (ret = __os_posix_err(__os_get_syserr())) {
		case EMFILE:
		case ENFILE:
		case ENOSPC:
			/*
			 * If it's a "temporary" error, we retry up to 3 times,
			 * waiting up to 12 seconds.  While it's not a problem
			 * if we can't open a database, an inability to open a
			 * log file is cause for serious dismay.
			 */
			__os_sleep(dbenv, nrepeat * 2, 0);
			break;
		case EAGAIN:
		case EBUSY:
		case EINTR:
			/*
			 * If an EAGAIN, EBUSY or EINTR, retry immediately for
			 * DB_RETRY times.
			 */
			if (++retries < DB_RETRY)
				--nrepeat;
			break;
		default:
			/* Open is silent on error. */
			goto err;
		}
	}

	if (ret == 0) {
#if defined(HAVE_FCNTL_F_SETFD)
		/* Deny file descriptor access to any child process. */
		if (fcntl(fhp->fd, F_SETFD, 1) == -1) {
			ret = __os_get_syserr();
			__db_syserr(dbenv, ret, "fcntl(F_SETFD)");
			ret = __os_posix_err(ret);
			goto err;
		}
#endif

done:		F_SET(fhp, DB_FH_OPENED);
		*fhpp = fhp;
		return (0);
	}

err:	(void)__os_closehandle(dbenv, fhp);
	return (ret);
}

/*
 * __os_closehandle --
 *	Close a file.
 *
 * PUBLIC: int __os_closehandle __P((DB_ENV *, DB_FH *));
 */
int
__os_closehandle(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	int ret;

	ret = 0;

	/*
	 * If we linked the DB_FH handle into the DB_ENV, it needs to be
	 * unlinked.
	 */
	DB_ASSERT(dbenv, dbenv != NULL || !F_ISSET(fhp, DB_FH_ENVLINK));

	if (dbenv != NULL) {
		if (fhp->name != NULL && FLD_ISSET(
		    dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
			__db_msg(dbenv, "fileops: close %s", fhp->name);

		if (F_ISSET(fhp, DB_FH_ENVLINK)) {
			/*
			 * Lock the DB_ENV handle and remove this file
			 * handle from the list.
			 */
			MUTEX_LOCK(dbenv, dbenv->mtx_env);
			TAILQ_REMOVE(&dbenv->fdlist, fhp, q);
			MUTEX_UNLOCK(dbenv, dbenv->mtx_env);
		}
	}

	/* Discard any underlying system file reference. */
	if (F_ISSET(fhp, DB_FH_OPENED)) {
		if (DB_GLOBAL(j_close) != NULL)
			ret = DB_GLOBAL(j_close)(fhp->fd);
		else
			RETRY_CHK((close(fhp->fd)), ret);
		if (ret != 0) {
			__db_syserr(dbenv, ret, "close");
			ret = __os_posix_err(ret);
		}
	}

	/* Unlink the file if we haven't already done so. */
	if (F_ISSET(fhp, DB_FH_UNLINK))
		(void)__os_unlink(dbenv, fhp->name);

	if (fhp->name != NULL)
		__os_free(dbenv, fhp->name);
	__os_free(dbenv, fhp);

	return (ret);
}
