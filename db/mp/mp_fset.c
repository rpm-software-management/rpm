/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp_fset.c,v 12.16 2006/09/13 14:53:42 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __memp_fset_pp --
 *	DB_MPOOLFILE->set pre/post processing.
 *
 * PUBLIC: int __memp_fset_pp __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__memp_fset_pp(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_THREAD_INFO *ip;
	int ret;

	dbenv = dbmfp->dbenv;

	PANIC_CHECK(dbenv);
	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, "DB_MPOOLFILE->set");

	/* Validate arguments. */
	if (flags == 0)
		return (__db_ferr(dbenv, "memp_fset", 1));

	if ((ret = __db_fchk(dbenv, "memp_fset", flags, DB_MPOOL_DISCARD)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__memp_fset(dbmfp, pgaddr, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __memp_fset --
 *	DB_MPOOLFILE->set.
 *
 * PUBLIC: int __memp_fset __P((DB_MPOOLFILE *, void *, u_int32_t));
 */
int
__memp_fset(dbmfp, pgaddr, flags)
	DB_MPOOLFILE *dbmfp;
	void *pgaddr;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp;
	u_int32_t n_cache;

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;

	DB_ASSERT(dbenv, !LF_ISSET(DB_MPOOL_DIRTY));

	/* Convert the page address to a buffer header and hash bucket. */
	bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));
	n_cache = NCACHE(dbmp->reginfo[0].primary, bhp->mf_offset, bhp->pgno);
	c_mp = dbmp->reginfo[n_cache].primary;
	hp = R_ADDR(&dbmp->reginfo[n_cache], c_mp->htab);
	hp = &hp[NBUCKET(c_mp, bhp->mf_offset, bhp->pgno)];

	MUTEX_LOCK(dbenv, hp->mtx_hash);

	if (LF_ISSET(DB_MPOOL_DISCARD))
		F_SET(bhp, BH_DISCARD);

	MUTEX_UNLOCK(dbenv, hp->mtx_hash);
	return (0);
}

/*
 * __memp_dirty --
 *	Upgrade a page from a read-only to a writeable pointer.
 *
 * PUBLIC: int __memp_dirty __P((DB_MPOOLFILE *, void *, DB_TXN *, u_int32_t));
 */
int
__memp_dirty(dbmfp, addrp, txn, flags)
	DB_MPOOLFILE *dbmfp;
	void *addrp;
	DB_TXN *txn;
	u_int32_t flags;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	DB_TXN *ancestor;
#ifdef DIAG_MVCC
	MPOOLFILE *mfp;
#endif
	MPOOL *c_mp;
	u_int32_t n_cache;
	int ret;
	db_pgno_t pgno;
	void *pgaddr;

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;
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

		if ((ret = __memp_fput(dbmfp, pgaddr, 0)) != 0) {
			__db_errx(dbenv,
			    "%s: error releasing a read-only page",
			    __memp_fn(dbmfp));
			(void)__memp_fput(dbmfp, *(void **)addrp, 0);
			*(void **)addrp = NULL;
			return (ret);
		}
		pgaddr = *(void **)addrp;
		bhp = (BH *)((u_int8_t *)pgaddr - SSZA(BH, buf));
		DB_ASSERT(dbenv, pgno == bhp->pgno);
		return (0);
	}

	n_cache = NCACHE(dbmp->reginfo[0].primary,
	    bhp->mf_offset, bhp->pgno);
	c_mp = dbmp->reginfo[n_cache].primary;
	hp = R_ADDR(&dbmp->reginfo[n_cache], c_mp->htab);
	hp = &hp[NBUCKET(c_mp, bhp->mf_offset, bhp->pgno)];

	MUTEX_LOCK(dbenv, hp->mtx_hash);
	/* Set/clear the page bits. */
	if (!F_ISSET(bhp, BH_DIRTY)) {
		++hp->hash_page_dirty;
		F_SET(bhp, BH_DIRTY);
	}
	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

#ifdef DIAG_MVCC
	mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
	MVCC_MPROTECT(bhp->buf, mfp->stat.st_pagesize, PROT_READ | PROT_WRITE);
#endif
	return (0);
}
