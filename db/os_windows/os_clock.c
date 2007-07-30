/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: os_clock.c,v 12.11 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_gettime --
 *	Return the current time-of-day clock in seconds and nanoseconds.
 */
void
__os_gettime(dbenv, tp)
	DB_ENV *dbenv;
	db_timespec *tp;
{
#ifdef DB_WINCE
	DWORD ticks;

	ticks = GetTickCount();

	tp->tv_sec = (u_int32_t)(ticks / 1000);
	tp->tv_nsec = (u_int32_t)((ticks % 1000) * MS_PER_NS);
#else
	struct _timeb now;

	_ftime(&now);
	tp->tv_sec = now.time;
	tp->tv_nsec = now.millitm * MS_PER_NS;
#endif
}
