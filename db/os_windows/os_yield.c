/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_yield.c,v 12.9 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_yield --
 *	Yield the processor.
 */
void
__os_yield(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * The call to Sleep(0) is specified by MSDN to yield the current
	 * thread's time slice to another thread of equal or greater priority.
	 */
	Sleep(0);
}
