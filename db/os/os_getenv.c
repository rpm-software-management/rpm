/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_getenv.c,v 12.7 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_getenv --
 *	Retrieve an environment variable.
 *
 * PUBLIC: int __os_getenv __P((DB_ENV *, const char *, char **, size_t));
 */
int
__os_getenv(dbenv, name, bpp, buflen)
	DB_ENV *dbenv;
	const char *name;
	char **bpp;
	size_t buflen;
{
	/*
	 * If we have getenv, there's a value and the buffer is large enough:
	 *	copy value into the pointer, return 0
	 * If we have getenv, there's a value  and the buffer is too short:
	 *	set pointer to NULL, return EINVAL
	 * If we have getenv and there's no value:
	 *	set pointer to NULL, return 0
	 * If we don't have getenv:
	 *	set pointer to NULL, return 0
	 */
#ifdef HAVE_GETENV
	char *p;

	if ((p = getenv(name)) != NULL) {
		if (strlen(p) < buflen) {
			(void)strcpy(*bpp, p);
			return (0);
		}

		*bpp = NULL;
		__db_errx(dbenv,
		    "%s: buffer too small to hold environment variable %s",
		    name, p);
		return (EINVAL);
	}
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(name, NULL);
	COMPQUIET(buflen, 0);
#endif
	*bpp = NULL;
	return (0);
}
