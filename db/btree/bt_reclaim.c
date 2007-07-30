/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998,2007 Oracle.  All rights reserved.
 *
 * $Id: bt_reclaim.c,v 12.9 2007/05/17 15:14:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/lock.h"

/*
 * __bam_reclaim --
 *	Free a database.
 *
 * PUBLIC: int __bam_reclaim __P((DB *, DB_TXN *));
 */
int
__bam_reclaim(dbp, txn)
	DB *dbp;
	DB_TXN *txn;
{
	DBC *dbc;
	DB_LOCK meta_lock;
	int ret, t_ret;

	/* Acquire a cursor. */
	if ((ret = __db_cursor(dbp, txn, &dbc, 0)) != 0)
		return (ret);

	/* Write lock the metapage for deallocations. */
	if ((ret = __db_lget(dbc,
	    0, PGNO_BASE_MD, DB_LOCK_WRITE, 0, &meta_lock)) != 0)
		goto err;

	/* Avoid locking every page, we have the handle locked exclusive. */
	F_SET(dbc, DBC_DONTLOCK);

	/* Walk the tree, freeing pages. */
	ret = __bam_traverse(dbc,
	    DB_LOCK_WRITE, dbc->internal->root, __db_reclaim_callback, dbc);

	__TLPUT(dbc, meta_lock);
	/* Discard the cursor. */
err:	if ((t_ret = __dbc_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __bam_truncate --
 *	Truncate a database.
 *
 * PUBLIC: int __bam_truncate __P((DBC *, u_int32_t *));
 */
int
__bam_truncate(dbc, countp)
	DBC *dbc;
	u_int32_t *countp;
{
	db_trunc_param trunc;
	int ret;

	trunc.count = 0;
	trunc.dbc = dbc;

	/* Walk the tree, freeing pages. */
	ret = __bam_traverse(dbc,
	    DB_LOCK_WRITE, dbc->internal->root, __db_truncate_callback, &trunc);

	if (countp != NULL)
		*countp = trunc.count;

	return (ret);
}
