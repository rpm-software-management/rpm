/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: ce_time.c,v 12.4 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * time --
 *
 * PUBLIC: #ifndef HAVE_TIME
 * PUBLIC: time_t time __P((time_t *));
 * PUBLIC: #endif
 */
time_t
time(timer)
	time_t *timer;
{
	/*
	 * WinCE does not have a POSIX time implementation
	 * It only has a GetSystemTime, which returns a struct
	 * with time day/month/year.
	 * The API has a GetSystemTimeAsFileTime documented, but
	 * it does not seem to exist in WinCE.
	 */

static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
static const __int64 SECS_TO_100NS = 10000000; /* 10^7 */

	struct _SYSTEMTIME stime;
	struct _FILETIME ftime;
	__int64 res;
	GetSystemTime(&stime);
	SystemTimeToFileTime(&stime, &ftime);

	memcpy(&res, &ftime, sizeof(__int64));

	res = (res/SECS_TO_100NS) - SECS_BETWEEN_EPOCHS;

	/*
	 * TODO: validate result.
	 * assert((time_t)res == res);
	 */

	if (timer != NULL)
		*timer = (time_t)res;
	return ((time_t)res);
}
