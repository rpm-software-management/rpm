/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: mp_fset.c,v 12.23 2007/06/05 11:55:28 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __memp_dirty --
 *	Upgrade a page from a read-only to a writeable pointer.
 *
 * PUBLIC: int __memp_dirty __P((
 * PUBLIC:     DB_MPOOLFILE *, void *, DB_TXN *, DB_CACHE_PRIORITY, u_int32_t));
 */
int
__memp_dirty(dbmfp, addrp, txn, priority, flags)
	DB_MPOOLFILE *dbmfp;
	void *addrp;
	DB_TXN *txn;
	DB_CACHE_PRIORITY priority;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL_HASH *hp;
	DB_TXN *ancestor;
#ifdef DIAG_MVCC
	MPOOLFILE *mfp;
#endif
	REGINFO *infop;
	int ret;
	db_pgno_t pgno;
	void *pgaddr;

	dbenv = dbmfp->dbenv;
	pgaddr = *(void **)addrp;

	/* Convert the page address to a buffer header. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));
	pgno = bhp->pgno;

	if (flags == 0)
		flags = DB_MPOOL_DIRTY;
	DB_ASSERT(dbenv, flags == DB_MPOOL_DIRTY || flags == DB_MPOOL_EDIT);

	if (F_ISSET(dbmfp, MP_READONLY)) {
		__db_errx(dbenv, "%s: dirty flag set for readonly file page",
		    __memp_fn(dbmfp));
		return (EACCES);
	}

	for (ancestor = txn;
	    ancestor != NULL && ancestor->parent != NULL;
	    ancestor = ancestor->parent)
		;

	if (dbmfp->mfp->multiversion &&
	    txn != NULL && !BH_OWNED_BY(dbenv, bhp, ancestor)) {
		if ((ret = __memp_fget(dbmfp,
		    &pgno, txn, flags, addrp)) != 0) {
			if (ret != DB_LOCK_DEADLOCK)
				__db_errx(dbenv,
				    "%s: error getting a page for writing",
				    __memp_fn(dbmfp));
			*(void **)addrp = pgaddr;
			return (ret);
		}

		DB_ASSERT(dbenv,
		    (flags == DB_MPOOL_EDIT && *(void **)addrp == pgaddr) ||
		    (flags != DB_MPOOL_EDIT && *(void **)addrp != pgaddr));

		if ((ret = __memp_fput(dbmfp, pgaddr, priority)) != 0) {
			__db_errx(dbenv,
			    "%s: error releasing a read-only page",
			    __memp_fn(dbmfp));
			(void)__memp_fput(dbmfp, *(void **)addrp, priority);
			*(void **)addrp = NULL;
			return (ret);
		}
		pgaddr = *(void **)addrp;
		bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));
		DB_ASSERT(dbenv, pgno == bhp->pgno);
		return (0);
	}

	MP_GET_BUCKET(dbmfp, pgno, &infop, hp, ret);
	if (ret != 0)
		return (ret);

	/* Set/clear the page bits. */
	if (!F_ISSET(bhp, BH_DIRTY)) {
		++hp->hash_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}
	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

#ifdef DIAG_MVCC
	mfp = R_ADDR(dbenv->mp_handle->reginfo, bhp->mf_offset);
	MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize, PROT_READ | PROT_WRITE);
#endif
	return (0);
}
