/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999,2007 Oracle.  All rights reserved.
 *
 * $Id: os_vx_config.c,v 12.8 2007/05/17 15:15:48 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_fs_notzero --
 *	Return 1 if allocated filesystem blocks are not zeroed.
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

/*
 * __os_support_direct_io --
 *      Return 1 if we support direct I/O.
 */
int
__os_support_direct_io()
{
	return (0);
}

/*
 * __os_support_db_register --
 *	Return 1 if the system supports DB_REGISTER.
 */
int
__os_support_db_register()
{
	return (0);
}

/*
 * __os_support_replication --
 *	Return 1 if the system supports replication.
 */
int
__os_support_replication()
{
	return (1);
}
