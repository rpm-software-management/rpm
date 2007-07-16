/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: isspace.c,v 1.2 2006/08/24 14:45:10 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * isspace --
 *
 * PUBLIC: #ifndef HAVE_ISSPACE
 * PUBLIC: int isspace __P((int));
 * PUBLIC: #endif
 */
int
isspace(c)
	int c;
{
	return (c == '\t' || c == '\n' ||
	    c == '\v' || c == '\f' || c == '\r' || c == ' ' ? 1 : 0);
}
