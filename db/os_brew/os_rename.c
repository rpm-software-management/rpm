/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rename.c,v 1.4 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_rename --
 *	Rename a file.
 */
int
__os_rename(dbenv, old, new, silent)
	DB_ENV *dbenv;
	const char *old, *new;
	u_int32_t silent;
{
	IFileMgr *pIFileMgr;
	int ret;

	FILE_MANAGER_CREATE(dbenv, pIFileMgr, ret);
	if (ret != 0)
		return (ret);

	if (IFILEMGR_Rename(pIFileMgr, old, new) == SUCCESS)
		ret = 0;
	else
		if (!silent)
			FILE_MANAGER_ERR(dbenv,
			    pIFileMgr, old, "IFILEMGR_Rename", ret);
		else
			ret = __os_posix_err(IFILEMGR_GetLastError(pIFileMgr));

	IFILEMGR_Release(pIFileMgr);
	return (ret);
}
