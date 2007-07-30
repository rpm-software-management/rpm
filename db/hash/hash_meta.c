/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999,2007 Oracle.  All rights reserved.
 *
 * $Id: hash_meta.c,v 12.11 2007/05/17 15:15:38 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"

/*
 * Acquire the meta-data page.
 *
 * PUBLIC: int __ham_get_meta __P((DBC *));
 */
int
__ham_get_meta(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_MPOOLFILE *mpf;
	HASH *hashp;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	hashp = dbp->h_internal;
	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __db_lget(dbc, 0,
	     hashp->meta_pgno, DB_LOCK_READ, 0, &hcp->hlock)) != 0)
		return (ret);

	if ((ret = __memp_fget(mpf, &hashp->meta_pgno, dbc->txn,
	    DB_MPOOL_CREATE, &hcp->hdr)) != 0)
		(void)__LPUT(dbc, hcp->hlock);

	return (ret);
}

/*
 * Release the meta-data page.
 *
 * PUBLIC: int __ham_release_meta __P((DBC *));
 */
int
__ham_release_meta(dbc)
	DBC *dbc;
{
	DB_MPOOLFILE *mpf;
	HASH_CURSOR *hcp;
	int ret;

	mpf = dbc->dbp->mpf;
	hcp = (HASH_CURSOR *)dbc->internal;

	if (hcp->hdr != NULL) {
		if ((ret = __memp_fput(mpf, hcp->hdr, dbc->priority)) != 0)
			return (ret);
		hcp->hdr = NULL;
	}

	return (__TLPUT(dbc, hcp->hlock));
}

/*
 * Mark the meta-data page dirty.
 *
 * PUBLIC: int __ham_dirty_meta __P((DBC *, u_int32_t));
 */
int
__ham_dirty_meta(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DB *dbp;
	HASH *hashp;
	HASH_CURSOR *hcp;
	int ret;

	dbp = dbc->dbp;
	hashp = dbp->h_internal;
	hcp = (HASH_CURSOR *)dbc->internal;

	if ((ret = __db_lget(dbc, LCK_COUPLE,
	     hashp->meta_pgno, DB_LOCK_WRITE, 0, &hcp->hlock)) != 0)
		return (ret);

	return (__memp_dirty(dbp->mpf,
	    &hcp->hdr, dbc->txn, dbc->priority, flags));
}
