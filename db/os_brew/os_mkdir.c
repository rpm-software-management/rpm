/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_mkdir.c,v 1.4 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_mkdir --
 *	Create a directory.
 */
int
__os_mkdir(dbenv, name, mode)
	DB_ENV *dbenv;
	const char *name;
	int mode;
{
	IFileMgr *ifmp;
	int ret;

	COMPQUIET(mode, 0);

	FILE_MANAGER_CREATE(dbenv, ifmp, ret);
	if (ret != 0)
		return (ret);

	if (IFILEMGR_MkDir(ifmp, name) == SUCCESS)
		ret = 0;
	else
		FILE_MANAGER_ERR(dbenv, ifmp, name, "IFILEMGR_MkDir", ret);

	IFILEMGR_Release(ifmp);

	return (ret);
}
