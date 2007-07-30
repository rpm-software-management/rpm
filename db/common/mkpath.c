/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: mkpath.c,v 12.18 2007/05/17 15:14:55 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __db_mkpath -- --
 *	Create intermediate directories.
 *
 * PUBLIC: int __db_mkpath __P((DB_ENV *, const char *));
 */
int
__db_mkpath(dbenv, name)
	DB_ENV *dbenv;
	const char *name;
{
	size_t len;
	int ret;
	char *p, *t, savech;

	/*
	 * Get a copy so we can modify the string.  It's a path and potentially
	 * quite long, so don't allocate the space on the stack.
	 */
	len = strlen(name) + 1;
	if ((ret = __os_malloc(dbenv, len, &t)) != 0)
		return (ret);
	memcpy(t, name, len);

	/*
	 * Cycle through the path, creating intermediate directories.
	 *
	 * Skip the first byte if it's a path separator, it's the start of an
	 * absolute pathname.
	 */
	if (PATH_SEPARATOR[1] == '\0') {
		for (p = t + 1; p[0] != '\0'; ++p)
			if (p[0] == PATH_SEPARATOR[0]) {
				savech = *p;
				*p = '\0';
				if (__os_exists(dbenv, t, NULL) &&
				    (ret = __os_mkdir(
					dbenv, t, dbenv->dir_mode)) != 0)
					break;
				*p = savech;
			}
	} else
		for (p = t + 1; p[0] != '\0'; ++p)
			if (strchr(PATH_SEPARATOR, p[0]) != NULL) {
				savech = *p;
				*p = '\0';
				if (__os_exists(dbenv, t, NULL) &&
				    (ret = __os_mkdir(
					dbenv, t, dbenv->dir_mode)) != 0)
					break;
				*p = savech;
			}

	__os_free(dbenv, t);
	return (ret);
}
