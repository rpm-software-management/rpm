/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2003
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_method.c,v 11.40 2003/06/30 17:20:19 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

#ifdef HAVE_RPC
#include "dbinc_auto/db_server.h"
#include "dbinc_auto/rpc_client_ext.h"
#endif

static int __memp_get_mp_maxwrite __P((DB_ENV *, int *, int *));
static int __memp_set_mp_maxwrite __P((DB_ENV *, int, int));
static int __memp_get_mp_mmapsize __P((DB_ENV *, size_t *));

/*
 * __memp_dbenv_create --
 *	Mpool specific creation of the DB_ENV structure.
 *
 * PUBLIC: void __memp_dbenv_create __P((DB_ENV *));
 */
void
__memp_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 *
	 * We default to 32 8K pages.  We don't default to a flat 256K, because
	 * some systems require significantly more memory to hold 32 pages than
	 * others.  For example, HP-UX with POSIX pthreads needs 88 bytes for
	 * a POSIX pthread mutex and almost 200 bytes per buffer header, while
	 * Solaris needs 24 and 52 bytes for the same structures.  The minimum
	 * number of hash buckets is 37.  These contain a mutex also.
	 */
	dbenv->mp_bytes =
	    32 * ((8 * 1024) + sizeof(BH)) + 37 * sizeof(DB_MPOOL_HASH);
	dbenv->mp_ncache = 1;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->get_cachesize = __dbcl_env_get_cachesize;
		dbenv->set_cachesize = __dbcl_env_cachesize;
		dbenv->get_mp_maxwrite = __dbcl_get_mp_maxwrite;
		dbenv->set_mp_maxwrite = __dbcl_set_mp_maxwrite;
		dbenv->get_mp_mmapsize = __dbcl_get_mp_mmapsize;
		dbenv->set_mp_mmapsize = __dbcl_set_mp_mmapsize;
		dbenv->memp_dump_region = NULL;
		dbenv->memp_register = __dbcl_memp_register;
		dbenv->memp_stat = __dbcl_memp_stat;
		dbenv->memp_sync = __dbcl_memp_sync;
		dbenv->memp_trickle = __dbcl_memp_trickle;
	} else
#endif
	{
		dbenv->get_cachesize = __memp_get_cachesize;
		dbenv->set_cachesize = __memp_set_cachesize;
		dbenv->get_mp_maxwrite = __memp_get_mp_maxwrite;
		dbenv->set_mp_maxwrite = __memp_set_mp_maxwrite;
		dbenv->get_mp_mmapsize = __memp_get_mp_mmapsize;
		dbenv->set_mp_mmapsize = __memp_set_mp_mmapsize;
		dbenv->memp_dump_region = __memp_dump_region;
		dbenv->memp_register = __memp_register_pp;
		dbenv->memp_stat = __memp_stat_pp;
		dbenv->memp_sync = __memp_sync_pp;
		dbenv->memp_trickle = __memp_trickle_pp;
	}
	dbenv->memp_fcreate = __memp_fcreate_pp;
}

/*
 * __memp_get_cachesize --
 *	{DB_ENV,DB}->get_cachesize.
 *
 * PUBLIC: int __memp_get_cachesize
 * PUBLIC:         __P((DB_ENV *, u_int32_t *, u_int32_t *, int *));
 */
int
__memp_get_cachesize(dbenv, gbytesp, bytesp, ncachep)
	DB_ENV *dbenv;
	u_int32_t *gbytesp, *bytesp;
	int *ncachep;
{
	if (gbytesp != NULL)
		*gbytesp = dbenv->mp_gbytes;
	if (bytesp != NULL)
		*bytesp = dbenv->mp_bytes;
	if (ncachep != NULL)
		*ncachep = dbenv->mp_ncache;
	return (0);
}

/*
 * __memp_set_cachesize --
 *	{DB_ENV,DB}->set_cachesize.
 *
 * PUBLIC: int __memp_set_cachesize __P((DB_ENV *, u_int32_t, u_int32_t, int));
 */
int
__memp_set_cachesize(dbenv, gbytes, bytes, ncache)
	DB_ENV *dbenv;
	u_int32_t gbytes, bytes;
	int ncache;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_cachesize");

	/* Normalize the values. */
	if (ncache == 0)
		ncache = 1;

	/*
	 * You can only store 4GB-1 in an unsigned 32-bit value, so correct for
	 * applications that specify 4GB cache sizes -- we know what they meant.
	 */
	if (gbytes / ncache == 4 && bytes == 0) {
		--gbytes;
		bytes = GIGABYTE - 1;
	} else {
		gbytes += bytes / GIGABYTE;
		bytes %= GIGABYTE;
	}

	/* Avoid too-large cache sizes, they result in a region size of zero. */
	if (gbytes / ncache > 4 || (gbytes / ncache == 4 && bytes != 0)) {
		__db_err(dbenv, "individual cache size too large");
		return (EINVAL);
	}

	/*
	 * If the application requested less than 500Mb, increase the cachesize
	 * by 25% and factor in the size of the hash buckets to account for our
	 * overhead.  (I'm guessing caches over 500Mb are specifically sized,
	 * that is, it's a large server and the application actually knows how
	 * much memory is available.  We only document the 25% overhead number,
	 * not the hash buckets, but I don't see a reason to confuse the issue,
	 * it shouldn't matter to an application.)
	 *
	 * There is a minimum cache size, regardless.
	 */
	if (gbytes == 0) {
		if (bytes < 500 * MEGABYTE)
			bytes += (bytes / 4) + 37 * sizeof(DB_MPOOL_HASH);
		if (bytes / ncache < DB_CACHESIZE_MIN)
			bytes = ncache * DB_CACHESIZE_MIN;
	}

	dbenv->mp_gbytes = gbytes;
	dbenv->mp_bytes = bytes;
	dbenv->mp_ncache = ncache;

	return (0);
}

static int
__memp_get_mp_maxwrite(dbenv, maxwritep, maxwrite_sleepp)
	DB_ENV *dbenv;
	int *maxwritep, *maxwrite_sleepp;
{
	*maxwritep = dbenv->mp_maxwrite;
	*maxwrite_sleepp = dbenv->mp_maxwrite_sleep;
	return (0);
}

/*
 * __memp_set_mp_maxwrite --
 *	Set the maximum continuous I/O count.
 */
static int
__memp_set_mp_maxwrite(dbenv, maxwrite, maxwrite_sleep)
	DB_ENV *dbenv;
	int maxwrite, maxwrite_sleep;
{
	dbenv->mp_maxwrite = maxwrite;
	dbenv->mp_maxwrite_sleep = maxwrite_sleep;
	return (0);
}

static int
__memp_get_mp_mmapsize(dbenv, mp_mmapsizep)
	DB_ENV *dbenv;
	size_t *mp_mmapsizep;
{
	*mp_mmapsizep = dbenv->mp_mmapsize;
	return (0);
}

/*
 * __memp_set_mp_mmapsize --
 *	DB_ENV->set_mp_mmapsize.
 *
 * PUBLIC: int __memp_set_mp_mmapsize __P((DB_ENV *, size_t));
 */
int
__memp_set_mp_mmapsize(dbenv, mp_mmapsize)
	DB_ENV *dbenv;
	size_t mp_mmapsize;
{
	dbenv->mp_mmapsize = mp_mmapsize;
	return (0);
}

/*
 * __memp_nameop
 *	Remove or rename a file in the pool.
 *
 * PUBLIC: int __memp_nameop __P((DB_ENV *,
 * PUBLIC:     u_int8_t *, const char *, const char *, const char *));
 *
 * XXX
 * Undocumented interface: DB private.
 */
int
__memp_nameop(dbenv, fileid, newname, fullold, fullnew)
	DB_ENV *dbenv;
	u_int8_t *fileid;
	const char *newname, *fullold, *fullnew;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	roff_t newname_off;
	int locked, ret;
	void *p;

	locked = 0;
	dbmp = NULL;

	if (!MPOOL_ON(dbenv))
		goto fsop;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/*
	 * Remove or rename a file that the mpool might know about.  We assume
	 * that the fop layer has the file locked for exclusive access, so we
	 * don't worry about locking except for the mpool mutexes.  Checkpoint
	 * can happen at any time, independent of file locking, so we have to
	 * do the actual unlink or rename system call to avoid any race.
	 *
	 * If this is a rename, allocate first, because we can't recursively
	 * grab the region lock.
	 */
	if (newname == NULL)
		p = NULL;
	else {
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, strlen(newname) + 1, &newname_off, &p)) != 0)
			return (ret);
		memcpy(p, newname, strlen(newname) + 1);
	}

	locked = 1;
	R_LOCK(dbenv, dbmp->reginfo);

	/*
	 * Find the file -- if mpool doesn't know about this file, that's not
	 * an error-- we may not have it open.
	 */
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
		/* Ignore non-active files. */
		if (mfp->deadfile || F_ISSET(mfp, MP_TEMP))
			continue;

		/* Ignore non-matching files. */
		if (memcmp(fileid, R_ADDR(
		    dbmp->reginfo, mfp->fileid_off), DB_FILE_ID_LEN) != 0)
			continue;

		/* If newname is NULL, we're removing the file. */
		if (newname == NULL) {
			MUTEX_LOCK(dbenv, &mfp->mutex);
			mfp->deadfile = 1;
			MUTEX_UNLOCK(dbenv, &mfp->mutex);
		} else {
			/*
			 * Else, it's a rename.  We've allocated memory
			 * for the new name.  Swap it with the old one.
			 */
			p = R_ADDR(dbmp->reginfo, mfp->path_off);
			mfp->path_off = newname_off;
		}
		break;
	}

	/* Delete the memory we no longer need. */
	if (p != NULL)
		__db_shalloc_free(dbmp->reginfo[0].addr, p);

fsop:	if (newname == NULL)
		ret = __os_unlink(dbenv, fullold);
	else
		ret = __os_rename(dbenv, fullold, fullnew, 1);

	if (locked)
		R_UNLOCK(dbenv, dbmp->reginfo);

	return (ret);
}

/*
 * __memp_get_refcnt
 *	Return a reference count, given a fileid.
 *
 * PUBLIC: int __memp_get_refcnt __P((DB_ENV *, u_int8_t *, int *));
 */
int
__memp_get_refcnt(dbenv, fileid, refp)
	DB_ENV *dbenv;
	u_int8_t *fileid;
	int *refp;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	MPOOLFILE *mfp;

	*refp = 0;

	if (!MPOOL_ON(dbenv))
		return (0);

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	R_LOCK(dbenv, dbmp->reginfo);
	/*
	 * Find the file -- if mpool doesn't know about this file, the
	 * reference count is 0.
	 */
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {

		/* Ignore non-active files. */
		if (mfp->deadfile || F_ISSET(mfp, MP_TEMP))
			continue;

		/* Ignore non-matching files. */
		if (memcmp(fileid, R_ADDR(
		    dbmp->reginfo, mfp->fileid_off), DB_FILE_ID_LEN) != 0)
			continue;

		*refp = mfp->mpf_cnt;
		break;
	}
	R_UNLOCK(dbenv, dbmp->reginfo);

	return (0);
}
