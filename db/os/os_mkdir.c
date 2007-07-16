/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_mkdir.c,v 12.16 2006/08/24 14:46:18 bostic Exp $
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

	COMPQUIET(dbenv, NULL);

	/* Make the directory, with paranoid permissions. */
#ifdef HAVE_VXWORKS
	RETRY_CHK((mkdir((char *)name)), ret);
#else
#ifdef DB_WIN32
	RETRY_CHK((_mkdir(name)), ret);
#else
	RETRY_CHK((mkdir(name, __db_omode("rwx------"))), ret);
#endif
	if (ret != 0)
		return (__os_posix_err(ret));

	/* Set the absolute permissions, if specified. */
#ifndef DB_WIN32
	if (mode != 0) {
		RETRY_CHK((chmod(name, mode)), ret);
		if (ret != 0)
			ret = __os_posix_err(ret);
	}
#endif
#endif
	return (ret);
}
