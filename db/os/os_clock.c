/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_clock.c,v 12.11 2006/08/24 14:46:17 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_clock --
 *	Return the current time-of-day clock in seconds and microseconds.
 *
 * PUBLIC: void __os_clock __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
void
__os_clock(dbenv, secsp, usecsp)
	DB_ENV *dbenv;
	u_int32_t *secsp, *usecsp;	/* Seconds and microseconds. */
{
	const char *sc;
	int ret;

#if defined(HAVE_GETTIMEOFDAY)
	struct timeval tp;

	RETRY_CHK((gettimeofday(&tp, NULL)), ret);
	if (ret != 0) {
		sc = "gettimeofday";
		goto err;
	}

	if (secsp != NULL)
		*secsp = (u_int32_t)tp.tv_sec;
	if (usecsp != NULL)
		*usecsp = (u_int32_t)tp.tv_usec;
#endif
#if !defined(HAVE_GETTIMEOFDAY) && defined(HAVE_CLOCK_GETTIME)
	struct timespec tp;

	RETRY_CHK((clock_gettime(CLOCK_REALTIME, &tp)), ret);
	if (ret != 0) {
		sc = "clock_gettime";
		goto err;
	}

	if (secsp != NULL)
		*secsp = tp.tv_sec;
	if (usecsp != NULL)
		*usecsp = tp.tv_nsec / 1000;
#endif
#if !defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_CLOCK_GETTIME)
	time_t now;

	RETRY_CHK((time(&now) == (time_t)-1 ? 1 : 0), ret);
	if (ret != 0) {
		sc = "time";
		goto err;
	}

	if (secsp != NULL)
		*secsp = now;
	if (usecsp != NULL)
		*usecsp = 0;
#endif
	return;

err:	__db_syserr(dbenv, ret, "%s", sc);
	(void)__db_panic(dbenv, __os_posix_err(ret));
}
