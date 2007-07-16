/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: mp_mvcc.c,v 12.24 2006/09/18 13:11:50 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __pgno_cmp __P((const void *, const void *));

/*
 * __memp_bh_priority --
 *	Get the the aggregate priority of a chain of buffer headers.
 *
 * PUBLIC: u_int32_t __memp_bh_priority __P((BH *));
 */
u_int32_t
__memp_bh_priority(bhp)
	BH *bhp;
{
	u_int32_t priority;

	while (SH_CHAIN_HASNEXT(bhp, vc))
		bhp = SH_CHAIN_NEXT(bhp, vc, __bh);

	priority = bhp->priority;

	while ((bhp = SH_CHAIN_PREV(bhp, vc, __bh)) != NULL)
		if (bhp->priority < priority)
			priority = bhp->priority;

	return (priority);
}

/*
 * __memp_bucket_reorder --
 *	Adjust a hash queue so that the given bhp is in priority order.
 *
 * PUBLIC: void __memp_bucket_reorder __P((DB_ENV *, DB_MPOOL_HASH *, BH *));
 */
void
__memp_bucket_reorder(dbenv, hp, bhp)
	DB_ENV *dbenv;
	DB_MPOOL_HASH *hp;
	BH *bhp;
{
	BH *first_bhp, *last_bhp, *next;
	u_int32_t priority;

	DB_ASSERT(dbenv, bhp != NULL);
	COMPQUIET(dbenv, NULL);

	/*
	 * Buffers on hash buckets are sorted by priority -- move the buffer to
	 * the correct position in the list.  We only need to do any work if
	 * there are two or more buffers in the bucket.
	 */
	if ((first_bhp = SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) ==
	     (last_bhp = SH_TAILQ_LAST(&hp->hash_bucket, hq, __bh)))
		goto done;

	while (SH_CHAIN_HASNEXT(bhp, vc))
		bhp = SH_CHAIN_NEXTP(bhp, vc, __bh);

	priority = BH_PRIORITY(bhp);

	/* Optimize common cases. */
	if (bhp != first_bhp && priority <= BH_PRIORITY(first_bhp)) {
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
		SH_TAILQ_INSERT_HEAD(&hp->hash_bucket, bhp, hq, __bh);
	} else if (bhp != last_bhp && priority >= BH_PRIORITY(last_bhp)) {
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
		SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);
	} else {
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
		for (next = SH_TAILQ_FIRST(&hp->hash_bucket, __bh);
		    next != NULL && priority > BH_PRIORITY(next);
		    next = SH_TAILQ_NEXT(next, hq, __bh))
			;
		if (next == NULL)
			SH_TAILQ_INSERT_TAIL(&hp->hash_bucket, bhp, hq);
		else
			SH_TAILQ_INSERT_BEFORE(&hp->hash_bucket,
			    next, bhp, hq, __bh);
	}

done:	/* Reset the hash bucket's priority. */
	hp->hash_priority =
	    BH_PRIORITY(SH_TAILQ_FIRST(&hp->hash_bucket, __bh));
}

/*
 * __memp_bh_settxn --
 *	Set the transaction that owns the given buffer.
 *
 * PUBLIC: int __memp_bh_settxn __P((DB_MPOOL *, MPOOLFILE *mfp, BH *, void *));
 */
int __memp_bh_settxn(dbmp, mfp, bhp, vtd)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	BH *bhp;
	void *vtd;
{
	DB_ENV *dbenv;
	TXN_DETAIL *td;

	dbenv = dbmp->dbenv;
	td = (TXN_DETAIL *)vtd;

	if (td == NULL) {
		__db_errx(dbenv,
		      "%s: non-transactional update to a multiversion file",
		    __memp_fns(dbmp, mfp));
		return (EINVAL);
	}

	if (bhp->td_off != INVALID_ROFF) {
		DB_ASSERT(dbenv, BH_OWNER(dbenv, bhp) == td);
		return (0);
	}

	bhp->td_off = R_OFFSET(&dbenv->tx_handle->reginfo, td);
	return (__txn_add_buffer(dbenv, td));
}

/*
 * __memp_skip_curadj --
 *	Indicate whether a cursor adjustment can be skipped for a snapshot
 *	cursor.
 *
 * PUBLIC: int __memp_skip_curadj __P((DBC *, db_pgno_t));
 */
int
__memp_skip_curadj(dbc, pgno)
	DBC * dbc;
	db_pgno_t pgno;
{
	BH *bhp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	DB_MPOOLFILE *dbmfp;
	DB_TXN *txn;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	REGINFO *infop;
	roff_t mf_offset;
	u_int32_t n_cache;
	int skip;

	dbenv = dbc->dbp->dbenv;
	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;
	dbmfp = dbc->dbp->mpf;
	mfp = dbmfp->mfp;
	mf_offset = R_OFFSET(dbmp->reginfo, mfp);
	skip = 0;

	for (txn = dbc->txn; txn->parent != NULL; txn = txn->parent)
		;

	/*
	 * Determine the cache and hash bucket where this page lives and get
	 * local pointers to them.  Reset on each pass through this code, the
	 * page number can change.
	 */
	n_cache = NCACHE(mp, mf_offset, pgno);
	infop = &dbmp->reginfo[n_cache];
	c_mp = infop->primary;
	hp = R_ADDR(infop, c_mp->htab);
	hp = &hp[NBUCKET(c_mp, mf_offset, pgno)];

	MUTEX_LOCK(dbenv, hp->mtx_hash);
	SH_TAILQ_FOREACH(bhp, &hp->hash_bucket, hq, __bh) {
		if (bhp->pgno != pgno || bhp->mf_offset != mf_offset)
			continue;

		if (!BH_OWNED_BY(dbenv, bhp, txn))
			skip = 1;
		break;
	}
	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

	return (skip);
}

#define	DB_FREEZER_MAGIC 0x06102002

/*
 * __memp_bh_freeze --
 *	Save a buffer header to temporary storage in case it is needed later by
 *	a snapshot transaction.  This function should be called with the hash
 *	bucket locked and will exit with it locked, as it inserts a frozen
 *	buffer after writing the data.
 *
 * PUBLIC: int __memp_bh_freeze __P((DB_MPOOL *, REGINFO *, DB_MPOOL_HASH *,
 * PUBLIC:     BH *, int *));
 */
int
__memp_bh_freeze(dbmp, infop, hp, bhp, need_frozenp)
	DB_MPOOL *dbmp;
	REGINFO *infop;
	DB_MPOOL_HASH *hp;
	BH *bhp;
	int *need_frozenp;
{
	BH *frozen_bhp;
	BH_FROZEN_ALLOC *frozen_alloc;
	DB_ENV *dbenv;
	DB_FH *fhp;
	MPOOL *c_mp;
	MPOOLFILE *bh_mfp;
	db_pgno_t maxpgno, newpgno, nextfree;
	size_t nio;
	int ret, t_ret;
	u_int32_t magic, nbucket, ncache, pagesize;
	char filename[100], *real_name;

	dbenv = dbmp->dbenv;
	c_mp = infop->primary;
	ret = 0;
	/* Find the associated MPOOLFILE. */
	bh_mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
	pagesize = bh_mfp->stat.st_pagesize;
	real_name = NULL;
	fhp = NULL;

	DB_ASSERT(dbenv, bhp->ref == 0);
	DB_ASSERT(dbenv, !F_ISSET(bhp, BH_DIRTY | BH_FROZEN | BH_LOCKED));

	++bhp->ref;
	F_SET(bhp, BH_LOCKED);
	MVCC_MPROTECT(bhp->buf, pagesize, PROT_READ | PROT_WRITE);

	MUTEX_UNLOCK(dbenv, hp->mtx_hash);

	MPOOL_REGION_LOCK(dbenv, infop);
	frozen_bhp = SH_TAILQ_FIRST(&c_mp->free_frozen, __bh);
	if (frozen_bhp != NULL) {
		SH_TAILQ_REMOVE(&c_mp->free_frozen, frozen_bhp, hq, __bh);
		*need_frozenp = SH_TAILQ_EMPTY(&c_mp->free_frozen);
	} else {
		*need_frozenp = 1;

		/* There might be a small amount of unallocated space. */
		if (__db_shalloc(infop,
		    sizeof(BH_FROZEN_ALLOC) + sizeof(BH_FROZEN_PAGE), 0,
		    &frozen_alloc) == 0) {
			frozen_bhp = (BH *)(frozen_alloc + 1);
			SH_TAILQ_INSERT_HEAD(&c_mp->alloc_frozen, frozen_alloc,
			    links, __bh_frozen_a);
		}
	}
	MPOOL_REGION_UNLOCK(dbenv, infop);
	MUTEX_LOCK(dbenv, hp->mtx_hash);

	/*
	 * If we can't get a frozen buffer header, return ENOMEM immediately:
	 * we don't want to call __memp_alloc recursively.  __memp_alloc will
	 * turn the next free page it finds into frozen buffer headers.
	 */
	if (frozen_bhp == NULL) {
		ret = ENOMEM;
		goto err;
	}

	/*
	 * For now, keep things simple and have one file per page size per
	 * cache.  Concurrency will be suboptimal, but debugging should be
	 * simpler.
	 */
	ncache = (u_int32_t)(infop - dbmp->reginfo);
	nbucket = (u_int32_t)(hp - (DB_MPOOL_HASH *)R_ADDR(infop, c_mp->htab));
	snprintf(filename, sizeof(filename), "__db.freezer.%u.%u.%uK",
	    ncache, nbucket, pagesize / 1024);

	if ((ret = __db_appname(dbenv, DB_APP_NONE, filename,
	    0, NULL, &real_name)) != 0)
		goto err;
	if ((ret = __os_open_extend(dbenv, real_name, pagesize,
	    DB_OSO_CREATE | DB_OSO_EXCL, dbenv->db_mode, &fhp)) == 0) {
		/* We're creating the file -- initialize the metadata page. */
		magic = DB_FREEZER_MAGIC;
		maxpgno = newpgno = 0;
		if ((ret = __os_write(dbenv, fhp, &magic, sizeof(u_int32_t),
		    &nio)) < 0 || nio == 0 ||
		    (ret = __os_write(dbenv, fhp, &newpgno, sizeof(db_pgno_t),
		    &nio)) < 0 || nio == 0 ||
		    (ret = __os_write(dbenv, fhp, &maxpgno, sizeof(db_pgno_t),
		    &nio)) < 0 || nio == 0 ||
		    (ret = __os_seek(dbenv, fhp, 0, 0, 0)) != 0)
			goto err;
	} else if (ret == EEXIST)
		ret = __os_open_extend(dbenv, real_name, pagesize, 0,
		    dbenv->db_mode, &fhp);
	if (ret != 0)
		goto err;
	if ((ret = __os_read(dbenv, fhp, &magic, sizeof(u_int32_t),
	    &nio)) < 0 || nio == 0 ||
	    (ret = __os_read(dbenv, fhp, &newpgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0 ||
	    (ret = __os_read(dbenv, fhp, &maxpgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0)
		goto err;
	if (magic != DB_FREEZER_MAGIC) {
		ret = EINVAL;
		goto err;
	}
	if (newpgno == 0) {
		newpgno = ++maxpgno;
		if ((ret = __os_seek(dbenv,
		    fhp, 0, 0, sizeof(u_int32_t) + sizeof(db_pgno_t))) != 0 ||
		    (ret = __os_write(dbenv, fhp, &maxpgno, sizeof(db_pgno_t),
		    &nio)) < 0 || nio == 0)
			goto err;
	} else {
		if ((ret = __os_seek(dbenv, fhp, newpgno, pagesize, 0)) != 0 ||
		    (ret = __os_read(dbenv, fhp, &nextfree, sizeof(db_pgno_t),
		    &nio)) < 0 || nio == 0)
			goto err;
		if ((ret =
		    __os_seek(dbenv, fhp, 0, 0, sizeof(u_int32_t))) != 0 ||
		    (ret = __os_write(dbenv, fhp, &nextfree, sizeof(db_pgno_t),
		    &nio)) < 0 || nio == 0)
			goto err;
	}

	/* Write the buffer to the allocated page. */
	if ((ret = __os_io(dbenv, DB_IO_WRITE, fhp, newpgno, pagesize, 0,
	    pagesize, bhp->buf, &nio)) != 0 || nio == 0)
		goto err;

	/*
	 * Set up the frozen_bhp with the freezer page number.  The original
	 * buffer header is about to be freed, so transfer resources to the
	 * frozen header here.
	 */
#ifdef DIAG_MVCC
	memcpy(frozen_bhp, bhp, SSZ(BH, align_off));
#else
	memcpy(frozen_bhp, bhp, SSZA(BH, buf));
#endif
	frozen_bhp->ref = frozen_bhp->ref_sync = 0;
	F_SET(frozen_bhp, BH_FROZEN);
	F_CLR(frozen_bhp, BH_LOCKED);
	frozen_bhp->priority = UINT32_MAX;
	((BH_FROZEN_PAGE *)frozen_bhp)->spgno = newpgno;

	bhp->td_off = INVALID_ROFF;

	/*
	 * Add the frozen buffer to the version chain and update the hash
	 * bucket if this is the head revision.  __memp_alloc will remove it by
	 * calling __memp_bhfree on the old version of the buffer.
	 */
	SH_CHAIN_INSERT_AFTER(bhp, frozen_bhp, vc, __bh);
	if (!SH_CHAIN_HASNEXT(frozen_bhp, vc)) {
		SH_TAILQ_INSERT_BEFORE(&hp->hash_bucket,
		    bhp, frozen_bhp, hq, __bh);
		SH_TAILQ_REMOVE(&hp->hash_bucket, bhp, hq, __bh);
	}

	/*
	 * Increment the file's block count -- freeing the original buffer will
	 * decrement it.
	 */
	++bh_mfp->block_cnt;
	++hp->hash_frozen;

	if (0) {
err:		if (ret == 0)
			ret = EIO;
		if (frozen_bhp != NULL) {
			MUTEX_UNLOCK(dbenv, hp->mtx_hash);
			MPOOL_REGION_LOCK(dbenv, infop);
			SH_TAILQ_INSERT_TAIL(&c_mp->free_frozen,
			    frozen_bhp, hq);
			MPOOL_REGION_UNLOCK(dbenv, infop);
			MUTEX_LOCK(dbenv, hp->mtx_hash);
		}
	}
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (fhp != NULL &&
	    (t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0 && ret != ENOMEM)
		__db_err(dbenv, ret, "__memp_bh_freeze");
	F_CLR(bhp, BH_LOCKED);
	--bhp->ref;

	/*
	 * If a thread of control is waiting on this buffer, wake it up.
	 */
	if (F_ISSET(hp, IO_WAITER)) {
		F_CLR(hp, IO_WAITER);
		MUTEX_UNLOCK(dbenv, hp->mtx_io);
	}
	return (ret);
}

static int
__pgno_cmp(a, b)
	const void *a, *b;
{
	db_pgno_t *ap, *bp;

	ap = (db_pgno_t *)a;
	bp = (db_pgno_t *)b;

	return (int)(*ap - *bp);
}

/*
 * __memp_bh_thaw --
 *	Free a buffer header in temporary storage.  optionally restore the
 *	buffer (if alloc_bhp != NULL).  This function should be
 *	called with the hash bucket locked and will return with it locked.
 *
 * PUBLIC: int __memp_bh_thaw __P((DB_MPOOL *, REGINFO *,
 * PUBLIC:	DB_MPOOL_HASH *, BH *, BH *));
 */
int
__memp_bh_thaw(dbmp, infop, hp, frozen_bhp, alloc_bhp)
	DB_MPOOL *dbmp;
	REGINFO *infop;
	DB_MPOOL_HASH *hp;
	BH *frozen_bhp, *alloc_bhp;
{
	BH *next_bhp;
	DB_ENV *dbenv;
	DB_FH *fhp;
	MPOOL *c_mp;
	MPOOLFILE *bh_mfp;
	db_pgno_t *freelist, *ppgno, freepgno, maxpgno, spgno;
	size_t nio;
	u_int32_t listsize, magic, nbucket, ncache, ntrunc, nfree, pagesize;
#ifdef HAVE_FTRUNCATE
	int i;
#endif
	int needfree, reorder, ret, t_ret;
	char filename[100], *real_name;

	dbenv = dbmp->dbenv;
	fhp = NULL;
	c_mp = infop->primary;
	bh_mfp = R_ADDR(dbmp->reginfo, frozen_bhp->mf_offset);
	freelist = NULL;
	pagesize = bh_mfp->stat.st_pagesize;
	ret = 0;
	real_name = NULL;

	DB_ASSERT(dbenv, F_ISSET(frozen_bhp, BH_FROZEN));
	DB_ASSERT(dbenv, !F_ISSET(frozen_bhp, BH_LOCKED));
	DB_ASSERT(dbenv, alloc_bhp != NULL ||
	    BH_OBSOLETE(frozen_bhp, hp->old_reader));

	spgno = ((BH_FROZEN_PAGE *)frozen_bhp)->spgno;

	if (alloc_bhp != NULL) {
#ifdef DIAG_MVCC
		memcpy(alloc_bhp, frozen_bhp, SSZ(BH, align_off));
#else
		memcpy(alloc_bhp, frozen_bhp, SSZA(BH, buf));
#endif
		alloc_bhp->ref = 1;
		alloc_bhp->ref_sync = 0;
		F_CLR(alloc_bhp, BH_FROZEN);
	}

	F_SET(frozen_bhp, BH_LOCKED);

	/*
	 * For now, keep things simple and have one file per page size per
	 * cache.  Concurrency will be suboptimal, but debugging should be
	 * simpler.
	 */
	ncache = (u_int32_t)(infop - dbmp->reginfo);
	nbucket = (u_int32_t)(hp - (DB_MPOOL_HASH *)R_ADDR(infop, c_mp->htab));
	snprintf(filename, sizeof(filename), "__db.freezer.%u.%u.%uK",
	    ncache, nbucket, pagesize / 1024);

	if ((ret = __db_appname(dbenv, DB_APP_NONE, filename, 0, NULL,
	    &real_name)) != 0)
		goto err;

	if ((ret = __os_open_extend(dbenv, real_name, pagesize, 0,
	    dbenv->db_mode, &fhp)) != 0)
		goto err;

	/*
	 * Read the first free page number -- we're about to free the page
	 * after we we read it.
	 */
	if ((ret = __os_read(dbenv, fhp, &magic, sizeof(u_int32_t),
	    &nio)) < 0 || nio == 0 ||
	    (ret = __os_read(dbenv, fhp, &freepgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0 ||
	    (ret = __os_read(dbenv, fhp, &maxpgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0)
		goto err;

	if (magic != DB_FREEZER_MAGIC) {
		ret = EINVAL;
		goto err;
	}

	/* Read the buffer from the frozen page. */
	if (alloc_bhp != NULL &&
	    ((ret = __os_io(dbenv, DB_IO_READ, fhp, spgno, pagesize,
	    0, pagesize, alloc_bhp->buf, &nio)) != 0 || nio == 0))
		goto err;

	/*
	 * Free the page from the file.  If it's the last page, truncate.
	 * Otherwise, update free page linked list.
	 */
	needfree = 1;
	if (spgno == maxpgno) {
		listsize = 100;
		if ((ret = __os_malloc(dbenv,
		    listsize * sizeof(db_pgno_t), &freelist)) != 0)
			goto err;
		nfree = 0;
		while (freepgno != 0) {
			if (nfree == listsize) {
				listsize *= 2;
				if ((ret = __os_realloc(dbenv,
				    listsize * sizeof(db_pgno_t),
				    &freelist)) != 0)
					goto err;
			}
			freelist[nfree++] = freepgno;
			if ((ret = __os_seek(
			    dbenv, fhp, freepgno, pagesize, 0)) != 0 ||
			    (ret = __os_read(dbenv, fhp, &freepgno,
			    sizeof(db_pgno_t), &nio)) < 0 || nio == 0)
				goto err;
		}
		freelist[nfree++] = spgno;
		qsort(freelist, nfree, sizeof(db_pgno_t), __pgno_cmp);
		for (ppgno = &freelist[nfree - 1]; ppgno > freelist; ppgno--)
			if (*(ppgno - 1) != *ppgno - 1)
				break;
		ntrunc = (u_int32_t)(&freelist[nfree] - ppgno);
		if (ntrunc == (u_int32_t)maxpgno) {
			needfree = 0;
			ret = __os_closehandle(dbenv, fhp);
			fhp = NULL;
			if (ret != 0 ||
			    (ret = __os_unlink(dbenv, real_name)) != 0)
				goto err;
		}
#ifdef HAVE_FTRUNCATE
		else {
			maxpgno -= (db_pgno_t)ntrunc;
			if ((ret = __os_truncate(dbenv, fhp,
			    maxpgno + 1, pagesize)) != 0)
				goto err;

			/* Fix up the linked list */
			freelist[nfree - ntrunc] = 0;
			if ((ret = __os_seek(
			    dbenv, fhp, 0, 0, sizeof(u_int32_t))) != 0 ||
			    (ret = __os_write(dbenv, fhp, &freelist[0],
			    sizeof(db_pgno_t), &nio)) < 0 || nio == 0 ||
			    (ret = __os_write(dbenv, fhp, &maxpgno,
			    sizeof(db_pgno_t), &nio)) < 0 || nio == 0)
				goto err;

			for (i = 0; i < (int)(nfree - ntrunc); i++)
				if ((ret = __os_seek(dbenv,
				    fhp, freelist[i], pagesize, 0)) != 0 ||
				    (ret = __os_write(dbenv, fhp,
				    &freelist[i + 1], sizeof(db_pgno_t),
				    &nio)) < 0 || nio == 0)
					goto err;
			needfree = 0;
		}
#endif
	}
	if (needfree &&
	    ((ret = __os_seek(dbenv, fhp, spgno, pagesize, 0)) != 0 ||
	    (ret = __os_write(dbenv, fhp, &freepgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0 ||
	    (ret = __os_seek(dbenv, fhp, 0, 0, sizeof(u_int32_t))) != 0 ||
	    (ret = __os_write(dbenv, fhp, &spgno, sizeof(db_pgno_t),
	    &nio)) < 0 || nio == 0))
		goto err;

	/*
	 * Add the thawed buffer (if any) to the version chain.  We can't
	 * do this any earlier, because we can't guarantee that another thread
	 * won't be waiting for it, which means we can't clean up if there are
	 * errors reading from the freezer.  We can't do it any later, because
	 * we're about to free frozen_bhp, and without it we would need to do
	 * another cache lookup to find out where the new page should live.
	 */
	if (alloc_bhp != NULL) {
		SH_CHAIN_INSERT_AFTER(frozen_bhp, alloc_bhp, vc, __bh);
		if (!SH_CHAIN_HASNEXT(alloc_bhp, vc)) {
			SH_TAILQ_INSERT_BEFORE(&hp->hash_bucket,
			    frozen_bhp, alloc_bhp, hq, __bh);
			SH_TAILQ_REMOVE(&hp->hash_bucket, frozen_bhp, hq, __bh);
		}
	}

	reorder = (alloc_bhp == NULL) &&
	    BH_PRIORITY(frozen_bhp) == frozen_bhp->priority;
	if ((next_bhp = SH_CHAIN_NEXT(frozen_bhp, vc, __bh)) == NULL) {
		if ((next_bhp = SH_CHAIN_PREV(frozen_bhp, vc, __bh)) != NULL)
			SH_TAILQ_INSERT_BEFORE(&hp->hash_bucket, frozen_bhp,
			    next_bhp, hq, __bh);
		SH_TAILQ_REMOVE(&hp->hash_bucket, frozen_bhp, hq, __bh);
	}
	SH_CHAIN_REMOVE(frozen_bhp, vc, __bh);
	if (reorder) {
		if (next_bhp != NULL)
			__memp_bucket_reorder(dbenv, hp, next_bhp);
		else
			hp->hash_priority = BH_PRIORITY(SH_TAILQ_FIRST(
			    &hp->hash_bucket, __bh));
	}

	/*
	 * If other threads are waiting for this buffer as well, they will have
	 * incremented the reference count and will be waiting on the I/O mutex.
	 * For that reason, we can't unconditionally free the memory here.
	 */
	if (--frozen_bhp->ref == 0) {
		MUTEX_UNLOCK(dbenv, hp->mtx_hash);

		if (alloc_bhp == NULL && frozen_bhp->td_off != INVALID_ROFF)
			ret = __txn_remove_buffer(dbenv,
			    BH_OWNER(dbenv, frozen_bhp), MUTEX_INVALID);

		MPOOL_REGION_LOCK(dbenv, infop);
		SH_TAILQ_INSERT_TAIL(&c_mp->free_frozen, frozen_bhp, hq);
		MPOOL_REGION_UNLOCK(dbenv, infop);
		MUTEX_LOCK(dbenv, hp->mtx_hash);
	} else {
		DB_ASSERT(dbenv, alloc_bhp != NULL);
		F_CLR(frozen_bhp, BH_FROZEN | BH_LOCKED);
	}

	if (alloc_bhp != NULL)
		++hp->hash_thawed;
	else
		++hp->hash_frozen_freed;

	if (0) {
err:		if (ret == 0)
			ret = EIO;
	}
	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (freelist != NULL)
		__os_free(dbenv, freelist);
	if (fhp != NULL &&
	    (t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		__db_err(dbenv, ret, "__memp_bh_thaw");

	/*
	 * If a thread of control is waiting on this buffer, wake it up.
	 */
	if (F_ISSET(hp, IO_WAITER)) {
		F_CLR(hp, IO_WAITER);
		MUTEX_UNLOCK(dbenv, hp->mtx_io);
	}

	return (ret);
}
