/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: xa_stub.c,v 1.3 2007/05/17 15:16:00 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/txn.h"

/*
 * If the library wasn't compiled with XA support, various routines
 * aren't available.  Stub them here, returning an appropriate error.
 */
static int __db_noxa __P((DB_ENV *));

/*
 * __db_noxa --
 *	Error when a Berkeley DB build doesn't include XA support.
 */
static int
__db_noxa(dbenv)
	DB_ENV *dbenv;
{
	__db_errx(dbenv,
	    "library build did not include support for XA");
	return (DB_OPNOTSUP);
}

int
__db_xa_create(dbp)
	DB *dbp;
{
	return (__db_noxa(dbp->dbenv));
}
