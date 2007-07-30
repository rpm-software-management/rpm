/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: isprint.c,v 1.4 2007/05/17 15:14:54 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * isprint --
 *
 * PUBLIC: #ifndef HAVE_ISPRINT
 * PUBLIC: int isprint __P((int));
 * PUBLIC: #endif
 */
int
isprint(c)
	int c;
{
	/*
	 * Depends on ASCII character values.
	 */
	return ((c >= ' ' && c <= '~') ? 1 : 0);
}
