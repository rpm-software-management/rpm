/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_fsync.c,v 12.10 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_fsync --
 *	Flush a file descriptor.
 */
int
__os_fsync(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	int ret;

	/*
	 * Do nothing if the file descriptor has been marked as not requiring
	 * any sync to disk.
	 */
	if (F_ISSET(fhp, DB_FH_NOSYNC))
		return (0);

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: flush %s", fhp->name);

	RETRY_CHK((!FlushFileBuffers(fhp->handle)), ret);
	if (ret != 0) {
		__db_syserr(dbenv, ret, "FlushFileBuffers");
		ret = __os_posix_err(ret);
	}
	return (ret);
}
