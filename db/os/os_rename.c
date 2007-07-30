/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rename.c,v 12.10 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_rename --
 *	Rename a file.
 *
 * PUBLIC: int __os_rename __P((DB_ENV *,
 * PUBLIC:    const char *, const char *, u_int32_t));
 */
int
__os_rename(dbenv, oldname, newname, silent)
	DB_ENV *dbenv;
	const char *oldname, *newname;
	u_int32_t silent;
{
	int ret;

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: rename %s to %s", oldname, newname);

	if (DB_GLOBAL(j_rename) != NULL)
		ret = DB_GLOBAL(j_rename)(oldname, newname);
	else
		RETRY_CHK((rename(oldname, newname)), ret);

	/*
	 * If "silent" is not set, then errors are OK and we should not output
	 * an error message.
	 */
	if (ret != 0) {
		if (!silent)
			__db_syserr(
			    dbenv, ret, "rename %s %s", oldname, newname);
		ret = __os_posix_err(ret);
	}
	return (ret);
}
