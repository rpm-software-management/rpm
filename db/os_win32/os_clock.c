/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: os_clock.c,v 1.2 2001/08/18 17:39:08 dda Exp ";
#endif /* not lint */

#include <sys/types.h>
#include <sys/timeb.h>
#include <string.h>

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 *
 * PUBLIC: int __os_clock __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
int
__os_clock(dbenv, secsp, usecsp)
	DB_ENV *dbenv;
	u_int32_t *secsp, *usecsp;	/* Seconds and microseconds. */
{
	struct _timeb now;

	_ftime(&now);
	if (secsp != NULL)
		*secsp = now.time;
	if (usecsp != NULL)
		*usecsp = now.millitm * 1000;
	return (0);
}
