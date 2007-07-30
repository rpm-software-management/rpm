/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_abs.c,v 12.6 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_abspath --
 *	Return if a path is an absolute path.
 *
 * PUBLIC: int __os_abspath __P((const char *));
 */
int
__os_abspath(path)
	const char *path;
{
	return (path[0] == '/');
}
