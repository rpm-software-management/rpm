/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998,2007 Oracle.  All rights reserved.
 *
 * $Id: os_handle.c,v 12.17 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_openhandle --
 *	Open a file, using POSIX 1003.1 open flags.
 */
int
__os_openhandle(dbenv, name, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	int flags, mode;
	DB_FH **fhpp;
{
#ifdef DB_WINCE
	/*
	 * __os_openhandle API is not implemented on WinCE.
	 * It is not currently called from within the Berkeley DB library,
	 * so don't log the failure via the __db_err mechanism.
	 */
	return (EFAULT);
#else
	DB_FH *fhp;
	int ret, nrepeat, retries;

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

	retries = 0;
	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		fhp->fd = _open(name, flags, mode);

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
		F_SET(fhp, DB_FH_OPENED);
		*fhpp = fhp;
		return (0);
	}

err:	(void)__os_closehandle(dbenv, fhp);
	return (ret);
#endif
}

/*
 * __os_closehandle --
 *	Close a file.
 */
int
__os_closehandle(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	int ret, t_ret;

	ret = 0;

	if (dbenv != NULL) {
		if (fhp->name != NULL && FLD_ISSET(
		    dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
			__db_msg(dbenv, "fileops: %s: close", fhp->name);

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
		if (fhp->handle != INVALID_HANDLE_VALUE)
			RETRY_CHK((!CloseHandle(fhp->handle)), ret);
		else
#ifdef DB_WINCE
			ret = EFAULT;
#else
			RETRY_CHK((_close(fhp->fd)), ret);
#endif

		if (fhp->trunc_handle != INVALID_HANDLE_VALUE) {
			RETRY_CHK((!CloseHandle(fhp->trunc_handle)), t_ret);
			if (t_ret != 0 && ret == 0)
				ret = t_ret;
		}

		if (ret != 0) {
			__db_syserr(dbenv, ret, "CloseHandle");
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
