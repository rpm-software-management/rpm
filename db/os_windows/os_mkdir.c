/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_mkdir.c,v 12.3 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_mkdir --
 *	Create a directory.
 *
 * PUBLIC: int __os_mkdir __P((DB_ENV *, const char *, int));
 */
int
__os_mkdir(dbenv, name, mode)
	DB_ENV *dbenv;
	const char *name;
	int mode;
{
	int ret;
	_TCHAR *tname;

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: mkdir %s", name);

	/* Make the directory, with paranoid permissions. */
	TO_TSTRING(dbenv, name, tname, ret);
	if (ret != 0)
		return (ret);
	RETRY_CHK(!CreateDirectory(tname, NULL), ret);
	FREE_STRING(dbenv, tname);
	if (ret != 0)
		return (__os_posix_err(ret));

	return (ret);
}
