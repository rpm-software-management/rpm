/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: os_clock.c,v 12.14 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_gettime --
 *	Return the current time-of-day clock in seconds and nanoseconds.
 *
 * PUBLIC: void __os_gettime __P((DB_ENV *, db_timespec *));
 */
void
__os_gettime(dbenv, tp)
	DB_ENV *dbenv;
	db_timespec *tp;
{
	const char *sc;
	int ret;

#if defined(HAVE_CLOCK_GETTIME)
	RETRY_CHK((clock_gettime(CLOCK_REALTIME, (struct timespec *)tp)), ret);
	if (ret != 0) {
		sc = "clock_gettime";
		goto err;
	}
#endif
#if !defined(HAVE_CLOCK_GETTIME) && defined(HAVE_GETTIMEOFDAY)
	struct timeval v;

	RETRY_CHK((gettimeofday(&v, NULL)), ret);
	if (ret != 0) {
		sc = "gettimeofday";
		goto err;
	}

	tp->tv_sec = v.tv_sec;
	tp->tv_nsec = v.tv_usec * NS_PER_US;
#endif
#if !defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_CLOCK_GETTIME)
	time_t now;

	RETRY_CHK((time(&now) == (time_t)-1 ? 1 : 0), ret);
	if (ret != 0) {
		sc = "time";
		goto err;
	}

	tp->tv_sec = now;
	tp->tv_nsec = 0;
#endif
	return;

err:	__db_syserr(dbenv, ret, "%s", sc);
	(void)__db_panic(dbenv, __os_posix_err(ret));
}
