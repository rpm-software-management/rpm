/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rename.c,v 12.10 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_rename --
 *	Rename a file.
 */
int
__os_rename(dbenv, oldname, newname, silent)
	DB_ENV *dbenv;
	const char *oldname, *newname;
	u_int32_t silent;
{
	_TCHAR *toldname, *tnewname;
	int ret;

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: rename %s to %s", oldname, newname);

	TO_TSTRING(dbenv, oldname, toldname, ret);
	if (ret != 0)
		return (ret);
	TO_TSTRING(dbenv, newname, tnewname, ret);
	if (ret != 0) {
		FREE_STRING(dbenv, toldname);
		return (ret);
	}

	if (!MoveFile(toldname, tnewname))
		ret = __os_get_syserr();

	if (__os_posix_err(ret) == EEXIST) {
		ret = 0;
#ifndef DB_WINCE
		if (__os_is_winnt()) {
			if (!MoveFileEx(
			    toldname, tnewname, MOVEFILE_REPLACE_EXISTING))
				ret = __os_get_syserr();
		} else
#endif
		{
			/*
			 * There is no MoveFileEx for Win9x/Me/CE, so we have to
			 * do the best we can.  Note that the MoveFile call
			 * above would have succeeded if oldname and newname
			 * refer to the same file, so we don't need to check
			 * that here.
			 */
			(void)DeleteFile(tnewname);
			if (!MoveFile(toldname, tnewname))
				ret = __os_get_syserr();
		}
	}

	FREE_STRING(dbenv, tnewname);
	FREE_STRING(dbenv, toldname);

	if (ret != 0) {
		if (silent == 0)
			__db_syserr(
			    dbenv, ret, "MoveFileEx %s %s", oldname, newname);
		ret = __os_posix_err(ret);
	}

	return (ret);
}
