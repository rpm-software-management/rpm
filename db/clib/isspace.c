/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: isspace.c,v 1.4 2007/05/17 15:14:54 bostic Exp $
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
