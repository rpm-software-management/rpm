/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_stat.c,v 11.26 2003/02/20 14:36:07 mjc Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_exists --
 *	Return if the file exists.
 */
int
__os_exists(path, isdirp)
	const char *path;
	int *isdirp;
{
	int ret, retries;
	DWORD attrs;

	if (DB_GLOBAL(j_exists) != NULL)
		return (DB_GLOBAL(j_exists)(path, isdirp));

	ret = retries = 0;
	do {
		attrs = GetFileAttributes(path);
		if (attrs == (DWORD)-1)
			ret = __os_win32_errno();
	} while ((ret == EINTR || ret == EBUSY) &&
	    ++retries < DB_RETRY);

	if (ret != 0)
		return (ret);

	if (isdirp != NULL)
		*isdirp = (attrs & FILE_ATTRIBUTE_DIRECTORY);

	return (0);
}

/*
 * __os_ioinfo --
 *	Return file size and I/O size; abstracted to make it easier
 *	to replace.
 */
int
__os_ioinfo(dbenv, path, fhp, mbytesp, bytesp, iosizep)
	DB_ENV *dbenv;
	const char *path;
	DB_FH *fhp;
	u_int32_t *mbytesp, *bytesp, *iosizep;
{
	int ret, retries;
	BY_HANDLE_FILE_INFORMATION bhfi;
	unsigned __int64 filesize;

	retries = 0;
	if (DB_GLOBAL(j_ioinfo) != NULL)
		return (DB_GLOBAL(j_ioinfo)(path,
		    fhp->fd, mbytesp, bytesp, iosizep));

retry:	if (!GetFileInformationByHandle(fhp->handle, &bhfi)) {
		if (((ret = __os_win32_errno()) == EINTR || ret == EBUSY) &&
		    ++retries < DB_RETRY)
			goto retry;
		__db_err(dbenv,
		    "GetFileInformationByHandle: %s", strerror(ret));
		return (ret);
	}

	filesize = ((unsigned __int64)bhfi.nFileSizeHigh << 32) +
	    bhfi.nFileSizeLow;

	/* Return the size of the file. */
	if (mbytesp != NULL)
		*mbytesp = (u_int32_t)(filesize / MEGABYTE);
	if (bytesp != NULL)
		*bytesp = (u_int32_t)(filesize % MEGABYTE);

	/*
	 * The filesystem blocksize is not easily available.  In particular,
	 * the values returned by GetDiskFreeSpace() are not very helpful
	 * (NTFS volumes often report 512B clusters, which are too small to
	 * be a useful default).
	 */
	if (iosizep != NULL)
		*iosizep = DB_DEF_IOSIZE;
	return (0);
}
