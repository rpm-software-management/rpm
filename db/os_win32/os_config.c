/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_config.c,v 11.15 2003/01/08 05:34:00 bostic Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_is_winnt --
 *	Return 1 if Windows/NT, otherwise 0.
 *
 * PUBLIC: int __os_is_winnt __P((void));
 */
int
__os_is_winnt()
{
	static int __os_type = -1;

	/*
	 * The value of __os_type is computed only once, and cached to
	 * avoid the overhead of repeated calls to GetVersion().
	 */
	if (__os_type == -1) {
		if ((GetVersion() & 0x80000000) == 0)
			__os_type = 1;
		else
			__os_type = 0;
	}
	return (__os_type);
}

/*
 * __os_fs_notzero --
 *	Return 1 if allocated filesystem blocks are not zeroed.
 */
int
__os_fs_notzero()
{
	/*
	 * Windows/NT zero-fills pages that were never explicitly written to
	 * the file.  Windows 95/98 gives you random garbage, and that breaks
	 * Berkeley DB.
	 */
	return (__os_is_winnt() ? 0 : 1);
}
