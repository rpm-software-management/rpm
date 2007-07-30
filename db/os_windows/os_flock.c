/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_flock.c,v 1.17 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_fdlock --
 *	Acquire/release a lock on a byte in a file.
 */
int
__os_fdlock(dbenv, fhp, offset, acquire, nowait)
	DB_ENV *dbenv;
	DB_FH *fhp;
	int acquire, nowait;
	off_t offset;
{
#ifdef DB_WINCE
	/*
	 * This functionality is not supported by WinCE, so just fail.
	 *
	 * Should only happen if an app attempts to open an environment
	 * with the DB_REGISTER flag.
	 */
	 __db_errx(dbenv, "fdlock API not implemented for WinCE, DB_REGISTER "
	     "environment flag not supported.");
	return (EFAULT);
#else
	int ret;
	DWORD low, high;
	OVERLAPPED over;

	DB_ASSERT(dbenv,
	    F_ISSET(fhp, DB_FH_OPENED) && fhp->handle != INVALID_HANDLE_VALUE);

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv,
		    "fileops: flock %s %s offset %lu",
		    fhp->name, acquire ? "acquire": "release", (u_long)offset);

	/*
	 * Windows file locking interferes with read/write operations, so we
	 * map the ranges to an area past the end of the file.
	 */
	DB_ASSERT(dbenv, offset < (u_int64_t)INT64_MAX);
	offset = UINT64_MAX - offset;
	low = (DWORD)offset;
	high = (DWORD)(offset >> 32);

	if (acquire) {
		if (nowait)
			RETRY_CHK_EINTR_ONLY(
			    !LockFile(fhp->handle, low, high, 1, 0), ret);
		else if (__os_is_winnt()) {
			memset(&over, 0, sizeof(over));
			over.Offset = low;
			over.OffsetHigh = high;
			RETRY_CHK_EINTR_ONLY(
			    !LockFileEx(fhp->handle, LOCKFILE_EXCLUSIVE_LOCK,
			    0, 1, 0, &over),
			    ret);
		} else {
			/* Windows 9x/ME doesn't support a blocking call. */
			for (;;) {
				RETRY_CHK_EINTR_ONLY(
				    !LockFile(fhp->handle, low, high, 1, 0),
				    ret);
				if (__os_posix_err(ret) != EAGAIN)
					break;
				__os_sleep(dbenv, 1, 0);
			}
		}
	} else
		RETRY_CHK_EINTR_ONLY(
		    !UnlockFile(fhp->handle, low, high, 1, 0), ret);

	return (__os_posix_err(ret));
#endif
}
