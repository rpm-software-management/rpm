/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_clock.c,v 12.6 2006/08/24 14:46:21 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 */
void
__os_clock(dbenv, secsp, usecsp)
	DB_ENV *dbenv;
	u_int32_t *secsp, *usecsp;	/* Seconds and microseconds. */
{
	struct _timeb now;

	_ftime(&now);
	if (secsp != NULL)
		*secsp = (u_int32_t)now.time;
	if (usecsp != NULL)
		*usecsp = now.millitm * 1000;
}
