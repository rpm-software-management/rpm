/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: os_vx_finit.c,v 1.3 2001/06/01 18:35:55 bostic Exp ";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_fs_notzero --
 *	Return 1 if allocated filesystem blocks are not zeroed.
 *
 * PUBLIC: int __os_fs_notzero __P((void));
 */
int
__os_fs_notzero()
{
	/*
	 * Some VxWorks FS drivers do not zero-fill pages that were never
	 * explicitly written to the file, they give you random garbage,
	 * and that breaks Berkeley DB.
	 */
	return (1);
}
