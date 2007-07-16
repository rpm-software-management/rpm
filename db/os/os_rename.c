/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_rename.c,v 12.7 2006/08/24 14:46:18 bostic Exp $
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
__os_rename(dbenv, old, new, silent)
	DB_ENV *dbenv;
	const char *old, *new;
	u_int32_t silent;
{
	int ret;

	if (DB_GLOBAL(j_rename) != NULL)
		ret = DB_GLOBAL(j_rename)(old, new);
	else
		RETRY_CHK((rename(old, new)), ret);

	/*
	 * If "silent" is not set, then errors are OK and we should not output
	 * an error message.
	 */
	if (ret != 0) {
		if (!silent)
			__db_syserr(dbenv, ret, "rename %s %s", old, new);
		ret = __os_posix_err(ret);
	}
	return (ret);
}
