/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rpath.c,v 12.7 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __db_rpath --
 *	Return the last path separator in the path or NULL if none found.
 *
 * PUBLIC: char *__db_rpath __P((const char *));
 */
char *
__db_rpath(path)
	const char *path;
{
	const char *s, *last;

	s = path;
	last = NULL;
	if (PATH_SEPARATOR[1] != '\0') {
		for (; s[0] != '\0'; ++s)
			if (strchr(PATH_SEPARATOR, s[0]) != NULL)
				last = s;
	} else
		for (; s[0] != '\0'; ++s)
			if (s[0] == PATH_SEPARATOR[0])
				last = s;
	return ((char *)last);
}
