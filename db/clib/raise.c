/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: raise.c,v 12.7 2007/05/17 15:14:54 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * raise --
 *	Send a signal to the current process.
 *
 * PUBLIC: #ifndef HAVE_RAISE
 * PUBLIC: int raise __P((int));
 * PUBLIC: #endif
 */
int
raise(s)
	int s;
{
	return (kill(getpid(), s));
}
