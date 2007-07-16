/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_handle.c,v 12.10 2006/09/05 15:02:31 mjc Exp $
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
	DB_FH *fhp;
	int ret, nrepeat, retries;

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), fhpp)) != 0)
		return (ret);
	fhp = *fhpp;

	retries = 0;
	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		ret = 0;
		fhp->fd = _open(name, flags, mode);

		if (fhp->fd != -1) {
			F_SET(fhp, DB_FH_OPENED);
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
		}
	}

	if (ret != 0) {
		(void)__os_closehandle(dbenv, fhp);
		*fhpp = NULL;
	}

	return (ret);
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

	/*
	 * If we have a valid handle, close it and unlink any temporary
	 * file.
	 */
	if (F_ISSET(fhp, DB_FH_OPENED)) {
		if (fhp->handle != INVALID_HANDLE_VALUE)
			RETRY_CHK((!CloseHandle(fhp->handle)), ret);
		else
			RETRY_CHK((_close(fhp->fd)), ret);

		if (fhp->trunc_handle != INVALID_HANDLE_VALUE) {
			RETRY_CHK((!CloseHandle(fhp->trunc_handle)), t_ret);
			if (t_ret != 0 && ret == 0)
				ret = t_ret;
		}

		if (ret != 0) {
			__db_syserr(dbenv, ret, "CloseHandle");
			ret = __os_posix_err(ret);
		}

		/* Unlink the file if we haven't already done so. */
		if (F_ISSET(fhp, DB_FH_UNLINK)) {
			(void)__os_unlink(dbenv, fhp->name);
			__os_free(dbenv, fhp->name);
		}
	}

	__os_free(dbenv, fhp);

	return (ret);
}
