/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_handle.c,v 11.34 2003/04/24 16:17:06 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#endif

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

	/* If the application specified an interface, use it. */
	if (DB_GLOBAL(j_open) != NULL) {
		if ((fhp->fd = DB_GLOBAL(j_open)(name, flags, mode)) == -1) {
			ret = __os_get_errno();
			goto err;
		}
		F_SET(fhp, DB_FH_OPENED);
		return (0);
	}

	retries = 0;
	for (nrepeat = 1; nrepeat < 4; ++nrepeat) {
		ret = 0;
		fhp->fd = open(name, flags, mode);

		if (fhp->fd != -1) {
			F_SET(fhp, DB_FH_OPENED);
			break;
		}

		switch (ret = __os_get_errno()) {
		case EMFILE:
		case ENFILE:
		case ENOSPC:
			/*
			 * If it's a "temporary" error, we retry up to 3 times,
			 * waiting up to 12 seconds.  While it's not a problem
			 * if we can't open a database, an inability to open a
			 * log file is cause for serious dismay.
			 */
			(void)__os_sleep(dbenv, nrepeat * 2, 0);
			break;
		case EBUSY:
		case EINTR:
			/*
			 * If it was an EINTR or EBUSY, retry immediately,
			 * DB_RETRY times.
			 */
			if (++retries < DB_RETRY)
				--nrepeat;
			break;
		}
	}

err:	if (ret != 0) {
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
	BOOL success;
	int ret;

	ret = 0;

	/*
	 * If we have a valid handle, close it and unlink any temporary
	 * file.
	 */
	if (F_ISSET(fhp, DB_FH_OPENED)) {
		do {
			if (DB_GLOBAL(j_close) != NULL)
				success = (DB_GLOBAL(j_close)(fhp->fd) == 0);
			else if (fhp->handle != INVALID_HANDLE_VALUE) {
				success = CloseHandle(fhp->handle);
				if (!success)
					__os_set_errno(__os_win32_errno());
			}
			else
				success = (close(fhp->fd) == 0);
		} while (!success && (ret = __os_get_errno()) == EINTR);

		if (ret != 0)
			__db_err(dbenv, "CloseHandle: %s", strerror(ret));
	}

	__os_free(dbenv, fhp);

	return (ret);
}
