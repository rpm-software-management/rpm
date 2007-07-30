/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_stat.c,v 12.12 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_exists --
 *	Return if the file exists.
 */
int
__os_exists(dbenv, path, isdirp)
	DB_ENV *dbenv;
	const char *path;
	int *isdirp;
{
	int ret;
	DWORD attrs;
	_TCHAR *tpath;

	TO_TSTRING(dbenv, path, tpath, ret);
	if (ret != 0)
		return (ret);

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: stat %s", path);

	RETRY_CHK(
	    ((attrs = GetFileAttributes(tpath)) == (DWORD)-1 ? 1 : 0), ret);
	if (ret == 0) {
		if (isdirp != NULL)
			*isdirp = (attrs & FILE_ATTRIBUTE_DIRECTORY);
	} else
		ret = __os_posix_err(ret);

	FREE_STRING(dbenv, tpath);
	return (ret);
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
	int ret;
	BY_HANDLE_FILE_INFORMATION bhfi;
	unsigned __int64 filesize;

	RETRY_CHK((!GetFileInformationByHandle(fhp->handle, &bhfi)), ret);
	if (ret != 0) {
		__db_syserr(dbenv, ret, "GetFileInformationByHandle");
		return (__os_posix_err(ret));
	}

	filesize = ((unsigned __int64)bhfi.nFileSizeHigh << 32) +
	    bhfi.nFileSizeLow;

	/* Return the size of the file. */
	if (mbytesp != NULL)
		*mbytesp = (u_int32_t)(filesize / MEGABYTE);
	if (bytesp != NULL)
		*bytesp = (u_int32_t)(filesize % MEGABYTE);

	/*
	 * The filesystem I/O size is not easily available.  In particular,
	 * the values returned by GetDiskFreeSpace() are not very helpful
	 * (NTFS volumes often report 512B clusters, which are too small to
	 * be a useful default).
	 */
	if (iosizep != NULL)
		*iosizep = DB_DEF_IOSIZE;
	return (0);
}
