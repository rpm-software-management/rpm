/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: isprint.c,v 1.2 2006/08/24 14:45:10 bostic Exp $
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
