/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: mp_resize.c,v 12.5 2007/06/05 11:55:28 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __memp_add_bucket __P((DB_MPOOL *));
static int __memp_add_region __P((DB_MPOOL *));
static int __memp_map_regions __P((DB_MPOOL *));
static int __memp_merge_buckets
    __P((DB_MPOOL *, u_int32_t, u_int32_t, u_int32_t));
static int __memp_remove_bucket __P((DB_MPOOL *));
static int __memp_remove_region __P((DB_MPOOL *));

/*
 * PUBLIC: int __memp_get_bucket
 * PUBLIC:     __P((DB_MPOOLFILE *, db_pgno_t, REGINFO **, DB_MPOOL_HASH **));
 */
int
__memp_get_bucket(dbmfp, pgno, infopp, hpp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t pgno;
	REGINFO **infopp;
	DB_MPOOL_HASH **hpp;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp, *mp;
	REGINFO *infop;
	roff_t mf_offset;
	u_int32_t bucket, nbuckets, new_bucket, new_nbuckets, region;
	u_int32_t *regids;
	int ret;

	dbenv = dbmfp->dbenv;
	dbmp = dbenv->mp_handle;
	mf_offset = R_OFFSET(dbmp->reginfo, dbmfp->mfp);
	mp = dbmp->reginfo[0].primary;
	ret = 0;

	for (;;) {
		nbuckets = mp->nbuckets;
		MP_BUCKET(mf_offset, pgno, nbuckets, bucket);

		/*
		 * Once we work out which region we are looking in, we have to
		 * check that we have that region mapped, and that the version
		 * we have matches the ID in the main mpool region.  Otherwise
		 * we have to go and map in any regions that don't match and
		 * retry.
		 */
		region = NREGION(mp, bucket);
		regids = R_ADDR(dbmp->reginfo, mp->regids);

		for (;;) {
			infop = *infopp = &dbmp->reginfo[region];
			c_mp = infop->primary;

			/* If we have the correct region mapped, we're done. */
			if (c_mp != NULL && regids[region] == infop->id)
				break;
			if ((ret = __memp_map_regions(dbmp)) != 0)
				return (ret);
		}

		/* If our caller wants the hash bucket, lock it here. */
		if (hpp != NULL) {
			hp = R_ADDR(infop, c_mp->htab);
			hp = &hp[bucket - region * mp->htab_buckets];

			MUTEX_LOCK(dbenv, hp->mtx_hash);

			/*
			 * Check that we still have the correct region mapped.
			 */
			if (regids[region] != infop->id) {
				MUTEX_UNLOCK(dbenv, hp->mtx_hash);
				continue;
			}

			/*
			 * Now that the bucket is locked, we need to check that
			 * the cache has not been resized while we waited.
			 */
			new_nbuckets = mp->nbuckets;
			if (nbuckets != new_nbuckets) {
				MP_BUCKET(mf_offset, pgno, new_nbuckets,
				    new_bucket);

				if (new_bucket != bucket) {
					MUTEX_UNLOCK(dbenv, hp->mtx_hash);
					continue;
				}
			}

			*hpp = hp;
		}

		break;
	}

	return (ret);
}

static int
__memp_merge_buckets(dbmp, new_nbuckets, old_bucket, new_bucket)
	DB_MPOOL *dbmp;
	u_int32_t new_nbuckets, old_bucket, new_bucket;
{
	BH *alloc_bhp, *bhp, *current_bhp, *new_bhp, *next_bhp;
	DB_ENV *dbenv;
	DB_MPOOL_HASH *new_hp, *old_hp;
	MPOOL *mp, *new_mp, *old_mp;
	MPOOLFILE *mfp;
	REGINFO *new_infop, *old_infop;
	u_int32_t bucket, high_mask, new_region, old_region;
	int ret;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	new_bhp = NULL;
	ret = 0;

	MP_MASK(new_nbuckets, high_mask);

	old_region = NREGION(mp, old_bucket);
	old_infop = &dbmp->reginfo[old_region];
	old_mp = old_infop->primary;
	old_hp = R_ADDR(old_infop, old_mp->htab);
	old_hp = &old_hp[old_bucket - old_region * mp->htab_buckets];

	new_region = NREGION(mp, new_bucket);
	new_infop = &dbmp->reginfo[new_region];
	new_mp = new_infop->primary;
	new_hp = R_ADDR(new_infop, new_mp->htab);
	new_hp = &new_hp[new_bucket - new_region * mp->htab_buckets];

	/*
	 * Before merging, we need to check that there are no old buffers left
	 * in the target hash bucket after a previous split.
	 */
free_old:
	MUTEX_LOCK(dbenv, new_hp->mtx_hash);
	SH_TAILQ_FOREACH(bhp, &new_hp->hash_bucket, hq, __bh) {
		MP_BUCKET(bhp->mf_offset, bhp->pgno, mp->nbuckets, bucket);

		if (bucket != new_bucket) {
			/*
			 * There is no way that an old buffer can be locked
			 * after a split, since everyone will look for it in
			 * the new hash bucket.
			 */
			DB_ASSERT(dbenv, !F_ISSET(bhp, BH_LOCKED | BH_DIRTY) &&
			    bhp->ref == 0);
			if ((ret = __memp_bhfree(dbmp,
			    new_infop, new_hp, bhp, BH_FREE_FREEMEM)) != 0) {
				MUTEX_UNLOCK(dbenv, new_hp->mtx_hash);
				return (ret);
			}

			/*
			 * The free has modified the list of buffers and
			 * dropped the mutex.  We need to start again.
			 */
			goto free_old;
		}
	}
	MUTEX_UNLOCK(dbenv, new_hp->mtx_hash);

	/*
	 * Before we begin, make sure that all of the buffers we care about are
	 * not in use and not frozen.  We do this because we can't drop the old
	 * hash bucket mutex once we start moving buffers around.
	 */
retry:	MUTEX_LOCK(dbenv, old_hp->mtx_hash);
	SH_TAILQ_FOREACH(bhp, &old_hp->hash_bucket, hq, __bh) {
		MP_HASH_BUCKET(MP_HASH(bhp->mf_offset, bhp->pgno),
		    new_nbuckets, high_mask, bucket);

		if (bucket == new_bucket &&
		    (F_ISSET(bhp, BH_LOCKED) || bhp->ref != 0)) {
			MUTEX_UNLOCK(dbenv, old_hp->mtx_hash);
			__os_yield(dbenv);
			goto retry;
		} else if (bucket == new_bucket && F_ISSET(bhp, BH_FROZEN)) {
			if (BH_OBSOLETE(bhp, old_hp->old_reader))
				alloc_bhp = NULL;
			else {
				++bhp->ref;
				mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);
				MUTEX_UNLOCK(dbenv, old_hp->mtx_hash);
				if ((ret = __memp_alloc(dbmp,
				    old_infop, mfp, 0, NULL, &alloc_bhp)) != 0)
					return (ret);
				MUTEX_LOCK(dbenv, old_hp->mtx_hash);
			}
			if ((ret = __memp_bh_thaw(dbmp,
			    old_infop, old_hp, bhp, alloc_bhp)) != 0) {
				MUTEX_UNLOCK(dbenv, old_hp->mtx_hash);
				return (ret);
			}

			/*
			 * We've dropped the mutex in order to thaw, so we need
			 * to go back to the beginning and check that all of
			 * the buffers we care about are still unlocked and
			 * unreferenced.
			 */
			MUTEX_UNLOCK(dbenv, old_hp->mtx_hash);
			goto retry;
		}
	}

	/*
	 * We now know that all of the buffers we care about are unlocked and
	 * unreferenced.  Go ahead and copy them.
	 */
	SH_TAILQ_FOREACH(bhp, &old_hp->hash_bucket, hq, __bh) {
		MP_HASH_BUCKET(MP_HASH(bhp->mf_offset, bhp->pgno),
		    new_nbuckets, high_mask, bucket);
		mfp = R_ADDR(dbmp->reginfo, bhp->mf_offset);

		/*
		 * We ignore buffers that don't hash to the new bucket.  We
		 * could also ignore clean buffers which are not part of a
		 * multiversion chain as long as they have a backing file.
		 */
		if (bucket != new_bucket || (!F_ISSET(bhp, BH_DIRTY) &&
		    SH_CHAIN_SINGLETON(bhp, vc) && !mfp->no_backing_file))
			continue;

		for (current_bhp = bhp, next_bhp = NULL;
		    current_bhp != NULL;
		    current_bhp = SH_CHAIN_PREV(current_bhp, vc, __bh),
		    next_bhp = alloc_bhp) {
			if ((ret = __memp_alloc(dbmp,
			    new_infop, mfp, 0, NULL, &alloc_bhp)) != 0)
				break;

			alloc_bhp->ref = current_bhp->ref;
			alloc_bhp->ref_sync = current_bhp->ref_sync;
			alloc_bhp->priority = current_bhp->priority;
			alloc_bhp->pgno = current_bhp->pgno;
			alloc_bhp->mf_offset = current_bhp->mf_offset;
			alloc_bhp->flags = current_bhp->flags;
			alloc_bhp->td_off = current_bhp->td_off;

			/*
			 * We've duplicated the buffer, so now we need to
			 * update reference counts, including the counts in the
			 * per-MPOOLFILE and the transaction detail (for MVCC
			 * buffers).
			 */
			MUTEX_LOCK(dbenv, mfp->mutex);
			++mfp->block_cnt;
			MUTEX_UNLOCK(dbenv, mfp->mutex);

			if (alloc_bhp->td_off != INVALID_ROFF &&
			    (ret = __txn_add_buffer(dbenv,
			    R_ADDR(&dbenv->tx_handle->reginfo,
			    alloc_bhp->td_off))) != 0)
				break;

			memcpy(alloc_bhp->buf, bhp->buf, mfp->stat.st_pagesize);

			/*
			 * We build up the MVCC chain first, then insert the
			 * head (stored in new_bhp) once.
			 */
			if (next_bhp == NULL) {
				SH_CHAIN_INIT(alloc_bhp, vc);
				new_bhp = alloc_bhp;
			} else
				SH_CHAIN_INSERT_BEFORE(
				    next_bhp, alloc_bhp, vc, __bh);
		}

		MUTEX_LOCK(dbenv, new_hp->mtx_hash);
		SH_TAILQ_INSERT_TAIL(&new_hp->hash_bucket, new_bhp, hq);
		if (F_ISSET(new_bhp, BH_DIRTY))
			++new_hp->hash_page_dirty;

		/*
		 * We're doing an insertion sort, so it is O(N**2), but since
		 * buckets should be small, that should not matter.  When
		 * splitting a bucket, we traverse in priority order and append
		 * to the new bucket, and __memp_bucket_reorder is O(1) in that
		 * case.
		 */
		__memp_bucket_reorder(dbenv, new_hp, new_bhp);
		MUTEX_UNLOCK(dbenv, new_hp->mtx_hash);

		if (F_ISSET(bhp, BH_DIRTY)) {
			F_CLR(bhp, BH_DIRTY);
			--old_hp->hash_page_dirty;
		}
	}

	if (ret == 0)
		mp->nbuckets = new_nbuckets;
	MUTEX_UNLOCK(dbenv, old_hp->mtx_hash);

	return (ret);
}

static int
__memp_add_bucket(dbmp)
	DB_MPOOL *dbmp;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	u_int32_t high_mask, new_bucket, old_bucket;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;

	new_bucket = mp->nbuckets;
	/* We should always be adding buckets to the last region. */
	DB_ASSERT(dbenv, NREGION(mp, new_bucket) == mp->nreg - 1);
	MP_MASK(mp->nbuckets, high_mask);
	old_bucket = new_bucket & (high_mask >> 1);

	/*
	 * With fixed-sized regions, the new region is always smaller than the
	 * existing total cache size, so buffers always need to be copied.  If
	 * we implement variable region sizes, it's possible that we will be
	 * splitting a hash bucket in the new region.  Catch that here.
	 */
	DB_ASSERT(dbenv, NREGION(mp, old_bucket) != NREGION(mp, new_bucket));

	return (__memp_merge_buckets(dbmp, mp->nbuckets + 1,
	    old_bucket, new_bucket));
}

static int
__memp_add_region(dbmp)
	DB_MPOOL *dbmp;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	REGINFO *infop;
	int ret;
	roff_t reg_size;
	u_int i;
	u_int32_t *regids;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	/* All cache regions are the same size. */
	reg_size = dbmp->reginfo[0].rp->size;
	ret = 0;

	infop = &dbmp->reginfo[mp->nreg];
	infop->dbenv = dbenv;
	infop->type = REGION_TYPE_MPOOL;
	infop->id = INVALID_REGION_ID;
	infop->flags = REGION_CREATE_OK;
	if ((ret = __env_region_attach(dbenv, infop, reg_size)) != 0)
		return (ret);
	if ((ret = __memp_init(dbenv,
	    dbmp, mp->nreg, mp->htab_buckets, mp->max_nreg)) != 0)
		return (ret);
	regids = R_ADDR(dbmp->reginfo, mp->regids);
	regids[mp->nreg++] = infop->id;

	for (i = 0; i < mp->htab_buckets; i++)
		if ((ret = __memp_add_bucket(dbmp)) != 0)
			break;

	return (ret);
}

static int
__memp_remove_bucket(dbmp)
	DB_MPOOL *dbmp;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	u_int32_t high_mask, new_bucket, old_bucket;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;

	old_bucket = mp->nbuckets - 1;

	/* We should always be removing buckets from the last region. */
	DB_ASSERT(dbenv, NREGION(mp, old_bucket) == mp->nreg - 1);
	MP_MASK(mp->nbuckets - 1, high_mask);
	new_bucket = old_bucket & (high_mask >> 1);

	return (__memp_merge_buckets(dbmp, mp->nbuckets - 1,
	    old_bucket, new_bucket));
}

static int
__memp_remove_region(dbmp)
	DB_MPOOL *dbmp;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	REGINFO *infop;
	int ret;
	u_int i;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	ret = 0;

	if (mp->nreg == 1) {
		__db_errx(dbenv, "cannot remove the last cache");
		return (EINVAL);
	}

	for (i = 0; i < mp->htab_buckets; i++)
		if ((ret = __memp_remove_bucket(dbmp)) != 0)
			return (ret);

	/* Detach from the region then destroy it. */
	infop = &dbmp->reginfo[--mp->nreg];
	return (__env_region_detach(dbenv, infop, 1));
}

static int
__memp_map_regions(dbmp)
	DB_MPOOL *dbmp;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	int ret;
	u_int i;
	u_int32_t *regids;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	regids = R_ADDR(dbmp->reginfo, mp->regids);
	ret = 0;

	for (i = 1; i < mp->nreg; ++i) {
		if (dbmp->reginfo[i].primary != NULL &&
		    dbmp->reginfo[i].id == regids[i])
			continue;

		if (dbmp->reginfo[i].primary != NULL)
			ret = __env_region_detach(dbenv, &dbmp->reginfo[i], 0);

		dbmp->reginfo[i].dbenv = dbenv;
		dbmp->reginfo[i].type = REGION_TYPE_MPOOL;
		dbmp->reginfo[i].id = regids[i];
		dbmp->reginfo[i].flags = REGION_JOIN_OK;
		if ((ret =
		    __env_region_attach(dbenv, &dbmp->reginfo[i], 0)) != 0)
			return (ret);
		dbmp->reginfo[i].primary = R_ADDR(&dbmp->reginfo[i],
		    dbmp->reginfo[i].rp->primary);
	}

	for (; i < mp->max_nreg; i++)
		if (dbmp->reginfo[i].primary != NULL &&
		    (ret = __env_region_detach(dbenv,
		    &dbmp->reginfo[i], 0)) != 0)
			break;

	return (ret);
}

/*
 * PUBLIC: int __memp_resize __P((DB_MPOOL *, u_int32_t, u_int32_t));
 */
int
__memp_resize(dbmp, gbytes, bytes)
	DB_MPOOL *dbmp;
	u_int32_t gbytes, bytes;
{
	DB_ENV *dbenv;
	MPOOL *mp;
	int ret;
	u_int32_t ncache;
	roff_t reg_size, total_size;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	reg_size = dbmp->reginfo[0].rp->size;
	total_size = (roff_t)gbytes * GIGABYTE + bytes;
	ncache = (u_int32_t)((total_size + reg_size / 2) / reg_size);

	if (ncache < 1)
		ncache = 1;
	else if (ncache > mp->max_nreg) {
		__db_errx(dbenv,
		    "cannot resize to %lu cache regions: maximum is %lu",
		    (u_long)ncache, (u_long)mp->max_nreg);
		return (EINVAL);
	}

	ret = 0;
	MUTEX_LOCK(dbenv, mp->mtx_resize);
	while (mp->nreg != ncache)
		if ((ret = (mp->nreg < ncache ?
		    __memp_add_region(dbmp) :
		    __memp_remove_region(dbmp))) != 0)
			break;
	MUTEX_UNLOCK(dbenv, mp->mtx_resize);

	return (ret);
}

/*
 * PUBLIC: int __memp_get_cache_max __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
int
__memp_get_cache_max(dbenv, max_gbytesp, max_bytesp)
	DB_ENV *dbenv;
	u_int32_t *max_gbytesp, *max_bytesp;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	roff_t reg_size, max_size;

	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->mp_handle, "DB_ENV->get_mp_max_ncache", DB_INIT_MPOOL);

	if (MPOOL_ON(dbenv)) {
		dbmp = dbenv->mp_handle;
		mp = dbmp->reginfo[0].primary;
		reg_size = dbmp->reginfo[0].rp->size;
		max_size = mp->max_nreg * reg_size;
		*max_gbytesp = (u_int32_t)(max_size / GIGABYTE);
		*max_bytesp = (u_int32_t)(max_size % GIGABYTE);
	} else {
		*max_gbytesp = dbenv->mp_max_gbytes;
		*max_bytesp = dbenv->mp_max_bytes;
	}

	return (0);
}

/*
 * PUBLIC: int __memp_set_cache_max __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__memp_set_cache_max(dbenv, max_gbytes, max_bytes)
	DB_ENV *dbenv;
	u_int32_t max_gbytes, max_bytes;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_cache_max");
	dbenv->mp_max_gbytes = max_gbytes;
	dbenv->mp_max_bytes = max_bytes;

	return (0);
}
