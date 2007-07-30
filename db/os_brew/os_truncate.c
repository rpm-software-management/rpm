/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004,2007 Oracle.  All rights reserved.
 *
 * $Id: os_truncate.c,v 1.4 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_truncate --
 *	Truncate the file.
 */
int
__os_truncate(dbenv, fhp, pgno, pgsize)
	DB_ENV *dbenv;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pgsize;
{
	IFileMgr *pIFileMgr;
	off_t offset;
	int ret;

	FILE_MANAGER_CREATE(dbenv, pIFileMgr, ret);
	if (ret != 0)
		return (ret);

	/*
	 * Truncate a file so that "pgno" is discarded from the end of the
	 * file.
	 */
	offset = (off_t)pgsize * pgno;

	if (IFILE_Truncate(fhp->ifp, offset) == SUCCESS)
		ret = 0;
	else
		FILE_MANAGER_ERR(dbenv, pIFileMgr, NULL, "IFILE_Truncate", ret);

	IFILEMGR_Release(pIFileMgr);

	return (ret);
}
