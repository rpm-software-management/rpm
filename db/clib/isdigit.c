/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: isdigit.c,v 1.2 2006/08/24 14:45:10 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * isdigit --
 *
 * PUBLIC: #ifndef HAVE_ISDIGIT
 * PUBLIC: int isdigit __P((int));
 * PUBLIC: #endif
 */
int
isdigit(c)
	int c;
{
	/*
	 * Depends on ASCII-like character ordering.
	 */
	return (c >= '0' && c <= '9' ? 1 : 0);
}
