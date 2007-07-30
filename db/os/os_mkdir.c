/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_mkdir.c,v 12.20 2007/05/17 15:15:46 bostic Exp $
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

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: mkdir %s", name);

	/* Make the directory, with paranoid permissions. */
#if defined(HAVE_VXWORKS)
	RETRY_CHK((mkdir((char *)name)), ret);
#else
	RETRY_CHK((mkdir(name, __db_omode("rwx------"))), ret);
#endif
	if (ret != 0)
		return (__os_posix_err(ret));

	/* Set the absolute permissions, if specified. */
#if !defined(HAVE_VXWORKS)
	if (mode != 0) {
		RETRY_CHK((chmod(name, mode)), ret);
		if (ret != 0)
			ret = __os_posix_err(ret);
	}
#endif
	return (ret);
}
