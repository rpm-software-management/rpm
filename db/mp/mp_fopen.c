/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2001
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: mp_fopen.c,v 11.60 2001/10/04 21:26:56 bostic Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

static int  __memp_fclose __P((DB_MPOOLFILE *, u_int32_t));
static int  __memp_fopen __P((DB_MPOOLFILE *,
		const char *, u_int32_t, int, size_t));
static int  __memp_mf_open __P((DB_MPOOLFILE *,
		const char *, size_t, db_pgno_t, u_int32_t, MPOOLFILE **));
static void __memp_last_pgno __P((DB_MPOOLFILE *, db_pgno_t *));
static void __memp_refcnt __P((DB_MPOOLFILE *, db_pgno_t *));
static int  __memp_set_clear_len __P((DB_MPOOLFILE *, u_int32_t));
static int  __memp_set_fileid __P((DB_MPOOLFILE *, u_int8_t *));
static int  __memp_set_ftype __P((DB_MPOOLFILE *, int));
static int  __memp_set_lsn_offset __P((DB_MPOOLFILE *, int32_t));
static int  __memp_set_pgcookie __P((DB_MPOOLFILE *, DBT *));
static void __memp_set_unlink __P((DB_MPOOLFILE *, int));

/*
 * MEMP_FREMOVE --
 *	Discard an MPOOLFILE and any buffers it references: update the flags
 *	so we never try to write buffers associated with the file, nor can we
 *	find it when looking for files to join.  In addition, clear the ftype
 *	field, there's no reason to post-process pages, they can be discarded
 *	by any thread.
 */
#define	MEMP_FREMOVE(mfp) {						\
	mfp->ftype = 0;							\
	F_SET(mfp, MP_DEADFILE);					\
}

/* Initialization methods cannot be called after open is called. */
#define	MPF_ILLEGAL_AFTER_OPEN(dbmfp, name)				\
	if (F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbmp->dbenv, name, 1));

/*
 * __memp_fcreate --
 *	Create a DB_MPOOLFILE handle.
 *
 * PUBLIC: int __memp_fcreate __P((DB_ENV *, DB_MPOOLFILE **, u_int32_t));
 */
int
__memp_fcreate(dbenv, retp, flags)
	DB_ENV *dbenv;
	DB_MPOOLFILE **retp;
	u_int32_t flags;
{
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_fcreate", DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "memp_fcreate", flags, 0)) != 0)
		return (ret);

	/* Allocate and initialize the per-process structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_MPOOLFILE), &dbmfp)) != 0)
		return (ret);
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &dbmfp->fhp)) != 0) {
		__os_free(dbenv, dbmfp, sizeof(DB_MPOOLFILE));
		return (ret);
	}

	/* Allocate and initialize a mutex if necessary. */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, dbmp->reginfo, 0, &dbmfp->mutexp)) != 0)
			return (ret);

		if ((ret = __db_shmutex_init(dbenv, dbmfp->mutexp, 0,
		    MUTEX_THREAD, dbmp->reginfo,
		    (REGMAINT *)R_ADDR(dbmp->reginfo,
		    ((MPOOL *)dbmp->reginfo->primary)->maint_off))) != 0) {
			__db_mutex_free(dbenv, dbmp->reginfo, dbmfp->mutexp);
			return (ret);
		}
	}

	dbmfp->ref = 1;
	dbmfp->lsn_offset = -1;
	dbmfp->dbmp = dbmp;

	dbmfp->close = __memp_fclose;
	dbmfp->get = __memp_fget;
	dbmfp->last_pgno = __memp_last_pgno;
	dbmfp->open = __memp_fopen;
	dbmfp->put = __memp_fput;
	dbmfp->refcnt = __memp_refcnt;
	dbmfp->set = __memp_fset;
	dbmfp->set_clear_len = __memp_set_clear_len;
	dbmfp->set_fileid = __memp_set_fileid;
	dbmfp->set_ftype = __memp_set_ftype;
	dbmfp->set_lsn_offset = __memp_set_lsn_offset;
	dbmfp->set_pgcookie = __memp_set_pgcookie;
	dbmfp->set_unlink = __memp_set_unlink;
	dbmfp->sync = __memp_fsync;

	/* Add the file to the environment's list of files. */
	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	TAILQ_INSERT_TAIL(&dbmp->dbmfq, dbmfp, q);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	*retp = dbmfp;
	return (0);
}

/*
 * __memp_set_clear_len --
 *	Set the clear length.
 */
static int
__memp_set_clear_len(dbmfp, clear_len)
	DB_MPOOLFILE *dbmfp;
	u_int32_t clear_len;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_clear_len");

	dbmfp->clear_len = clear_len;
	return (0);
}

/*
 * __memp_set_fileid --
 *	Set the file ID.
 */
static int
__memp_set_fileid(dbmfp, fileid)
	DB_MPOOLFILE *dbmfp;
	u_int8_t *fileid;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_fileid");

	dbmfp->fileid = fileid;
	return (0);
}

/*
 * __memp_set_ftype --
 *	Set the file type (as registered).
 */
static int
__memp_set_ftype(dbmfp, ftype)
	DB_MPOOLFILE *dbmfp;
	int ftype;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_ftype");

	dbmfp->ftype = ftype;
	return (0);
}

/*
 * __memp_set_lsn_offset --
 *	Set the page's LSN offset.
 */
static int
__memp_set_lsn_offset(dbmfp, lsn_offset)
	DB_MPOOLFILE *dbmfp;
	int32_t lsn_offset;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_lsn_offset");

	dbmfp->lsn_offset = lsn_offset;
	return (0);
}

/*
 * __memp_set_pgcookie --
 *	Set the pgin/pgout cookie.
 */
static int
__memp_set_pgcookie(dbmfp, pgcookie)
	DB_MPOOLFILE *dbmfp;
	DBT *pgcookie;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_pgcookie");

	dbmfp->pgcookie = pgcookie;
	return (0);
}

/*
 * HACK ALERT:
 * rpm needs to carry an open dbenv for a hash database into a chroot.
 * db-3.3.11 tries to do an off page hash access through a mempool, which
 * tries to reopen the original database path.
 */
const char * chroot_prefix = NULL;

/*
 * __memp_fopen --
 *	Open a backing file for the memory pool.
 */
static int
__memp_fopen(dbmfp, path, flags, mode, pagesize)
	DB_MPOOLFILE *dbmfp;
	const char *path;
	u_int32_t flags;
	int mode;
	size_t pagesize;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "memp_fopen", flags,
	    DB_CREATE | DB_EXTENT |
	    DB_NOMMAP | DB_ODDFILESIZE | DB_RDONLY | DB_TRUNCATE)) != 0)
		return (ret);

	/*
	 * Require a non-zero, power-of-two pagesize, smaller than the
	 * clear length.
	 */
	if (pagesize == 0 || !POWER_OF_TWO(pagesize)) {
		__db_err(dbenv,
		    "memp_fopen: page sizes must be a power-of-2");
		return (EINVAL);
	}
	if (dbmfp->clear_len > pagesize) {
		__db_err(dbenv,
		    "memp_fopen: clear length larger than page size.");
		return (EINVAL);
	}

	/* Read-only checks, and local flag. */
	if (LF_ISSET(DB_RDONLY)) {
		if (path == NULL) {
			__db_err(dbenv,
			    "memp_fopen: temporary files can't be readonly");
			return (EINVAL);
		}
		F_SET(dbmfp, MP_READONLY);
	}

	if ((ret = __memp_fopen_int(
	    dbmfp, NULL, path, flags, mode, pagesize, 1)) != 0)
		return (ret);

	F_SET(dbmfp, MP_OPEN_CALLED);
	return (0);
}

/*
 * __memp_fopen_int --
 *	Open a backing file for the memory pool; internal version.
 *
 * PUBLIC: int __memp_fopen_int __P((DB_MPOOLFILE *,
 * PUBLIC:     MPOOLFILE *, const char *, u_int32_t, int, size_t, int));
 */
int
__memp_fopen_int(dbmfp, mfp, path, flags, mode, pagesize, needlock)
	DB_MPOOLFILE *dbmfp;
	MPOOLFILE *mfp;
	const char *path;
	u_int32_t flags;
	int mode, needlock;
	size_t pagesize;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	db_pgno_t last_pgno;
	size_t maxmap;
	u_int32_t mbytes, bytes, oflags;
	int ret;
	u_int8_t idbuf[DB_FILE_ID_LEN];
	char *rpath, *rpath_orig;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	ret = 0;
	rpath = rpath_orig = NULL;

	if (path == NULL)
		last_pgno = 0;
	else {
		/* Get the real name for this file and open it. */
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, path, 0, NULL, &rpath)) != 0)
			goto err;

		rpath_orig = rpath;
if (chroot_prefix) {
    int chrlen = strlen(chroot_prefix);
    if (!strncmp(rpath, chroot_prefix, chrlen))
	rpath += chrlen;
}
		oflags = 0;
		if (LF_ISSET(DB_CREATE))
			oflags |= DB_OSO_CREATE;
		if (LF_ISSET(DB_RDONLY))
			oflags |= DB_OSO_RDONLY;
		if ((ret =
		   __os_open(dbenv, rpath, oflags, mode, dbmfp->fhp)) != 0) {
			if (!LF_ISSET(DB_EXTENT))
				__db_err(dbenv,
				    "%s: %s", rpath, db_strerror(ret));
			goto err;
		}

		/*
		 * Don't permit files that aren't a multiple of the pagesize,
		 * and find the number of the last page in the file, all the
		 * time being careful not to overflow 32 bits.
		 *
		 * !!!
		 * We can't use off_t's here, or in any code in the mainline
		 * library for that matter.  (We have to use them in the os
		 * stubs, of course, as there are system calls that take them
		 * as arguments.)  The reason is that some customers build in
		 * environments where an off_t is 32-bits, but still run where
		 * offsets are 64-bits, and they pay us a lot of money.
		 */
		if ((ret = __os_ioinfo(dbenv, rpath,
		    dbmfp->fhp, &mbytes, &bytes, NULL)) != 0) {
			__db_err(dbenv, "%s: %s", rpath, db_strerror(ret));
			goto err;
		}

		/*
		 * If we're doing a verify, we might have to cope with
		 * a truncated file;  if the file size is not a multiple
		 * of the page size, round down to a page -- we'll
		 * take care of the partial page outside the memp system.
		 */
		if (bytes % pagesize != 0) {
			if (LF_ISSET(DB_ODDFILESIZE))
				/*
				 * During verify or recovery, we might have
				 * to cope with a truncated file; round down,
				 * we'll worry about the partial page outside
				 * the memp system.
				 */
				bytes -= (bytes % pagesize);
			else {
				__db_err(dbenv,
			"%s: file size not a multiple of the pagesize",
				    rpath);
				ret = EINVAL;
				goto err;
			}
		}

		last_pgno = mbytes * (MEGABYTE / pagesize);
		last_pgno += bytes / pagesize;

		/* Correction: page numbers are zero-based, not 1-based. */
		if (last_pgno != 0)
			--last_pgno;

		/*
		 * Get the file id if we weren't given one.  Generated file id's
		 * don't use timestamps, otherwise there'd be no chance of any
		 * other process joining the party.
		 */
		if (dbmfp->fileid == NULL) {
			if ((ret = __os_fileid(dbenv, rpath, 0, idbuf)) != 0)
				goto err;
			dbmfp->fileid = idbuf;
		}
	}

	/*
	 * If we weren't provided an underlying shared object to join with,
	 * find/allocate the shared file objects.  Also allocate space for
	 * for the per-process thread lock.
	 */
	if (needlock)
		R_LOCK(dbenv, dbmp->reginfo);
	if (mfp == NULL)
		ret = __memp_mf_open(
		    dbmfp, path, pagesize, last_pgno, flags, &mfp);
	else {
		++mfp->mpf_cnt;
		ret = 0;
	}
	dbmfp->mfp = mfp;
	if (needlock)
		R_UNLOCK(dbenv, dbmp->reginfo);
	if (ret != 0)
		goto err;

	/*
	 * If a file:
	 *	+ is read-only
	 *	+ isn't temporary
	 *	+ doesn't require any pgin/pgout support
	 *	+ the DB_NOMMAP flag wasn't set (in either the file open or
	 *	  the environment in which it was opened)
	 *	+ and is less than mp_mmapsize bytes in size
	 *
	 * we can mmap it instead of reading/writing buffers.  Don't do error
	 * checking based on the mmap call failure.  We want to do normal I/O
	 * on the file if the reason we failed was because the file was on an
	 * NFS mounted partition, and we can fail in buffer I/O just as easily
	 * as here.
	 *
	 * XXX
	 * We'd like to test to see if the file is too big to mmap.  Since we
	 * don't know what size or type off_t's or size_t's are, or the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way is
	 * flatly impossible.  Hope that mmap fails if the file is too large.
	 */
#define	DB_MAXMMAPSIZE	(10 * 1024 * 1024)	/* 10 Mb. */
	if (F_ISSET(mfp, MP_CAN_MMAP)) {
		if (!F_ISSET(dbmfp, MP_READONLY))
			F_CLR(mfp, MP_CAN_MMAP);
		if (path == NULL)
			F_CLR(mfp, MP_CAN_MMAP);
		if (dbmfp->ftype != 0)
			F_CLR(mfp, MP_CAN_MMAP);
		if (LF_ISSET(DB_NOMMAP) || F_ISSET(dbenv, DB_ENV_NOMMAP))
			F_CLR(mfp, MP_CAN_MMAP);
		maxmap = dbenv->mp_mmapsize == 0 ?
		    DB_MAXMMAPSIZE : dbenv->mp_mmapsize;
		if (mbytes > maxmap / MEGABYTE ||
		    (mbytes == maxmap / MEGABYTE && bytes >= maxmap % MEGABYTE))
			F_CLR(mfp, MP_CAN_MMAP);
	}
	dbmfp->addr = NULL;
	if (F_ISSET(mfp, MP_CAN_MMAP)) {
		dbmfp->len = (size_t)mbytes * MEGABYTE + bytes;
		if (__os_mapfile(dbenv, rpath,
		    dbmfp->fhp, dbmfp->len, 1, &dbmfp->addr) != 0) {
			dbmfp->addr = NULL;
			F_CLR(mfp, MP_CAN_MMAP);
		}
	}
	if (rpath != NULL)
		__os_freestr(dbenv, rpath_orig);

	return (0);

err:	if (rpath != NULL)
		__os_freestr(dbenv, rpath_orig);
	if (F_ISSET(dbmfp->fhp, DB_FH_VALID))
		(void)__os_closehandle(dbmfp->fhp);
	return (ret);
}

/*
 * __memp_mf_open --
 *	Open an MPOOLFILE.
 */
static int
__memp_mf_open(dbmfp, path, pagesize, last_pgno, flags, retp)
	DB_MPOOLFILE *dbmfp;
	const char *path;
	size_t pagesize;
	db_pgno_t last_pgno;
	u_int32_t flags;
	MPOOLFILE **retp;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	int ret;
	void *p;

#define	ISTEMPORARY	(path == NULL)

	dbmp = dbmfp->dbmp;

	/*
	 * If not creating a temporary file, walk the list of MPOOLFILE's,
	 * looking for a matching file.  Files backed by temporary files
	 * or previously removed files can't match.
	 *
	 * DB_TRUNCATE support.
	 *
	 * The fileID is a filesystem unique number (e.g., a UNIX dev/inode
	 * pair) plus a timestamp.  If files are removed and created in less
	 * than a second, the fileID can be repeated.  The problem with
	 * repetition happens when the file that previously had the fileID
	 * value still has pages in the pool, since we don't want to use them
	 * to satisfy requests for the new file.
	 *
	 * Because the DB_TRUNCATE flag reuses the dev/inode pair, repeated
	 * opens with that flag set guarantees matching fileIDs when the
	 * machine can open a file and then re-open with truncate within a
	 * second.  For this reason, we pass that flag down, and, if we find
	 * a matching entry, we ensure that it's never found again, and we
	 * create a new entry for the current request.
	 */
	if (!ISTEMPORARY) {
		mp = dbmp->reginfo[0].primary;
		for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
		    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
			if (F_ISSET(mfp, MP_DEADFILE | MP_TEMP))
				continue;
			if (memcmp(dbmfp->fileid, R_ADDR(dbmp->reginfo,
			    mfp->fileid_off), DB_FILE_ID_LEN) == 0) {
				if (LF_ISSET(DB_TRUNCATE)) {
					MEMP_FREMOVE(mfp);
					continue;
				}
				if (dbmfp->clear_len != mfp->clear_len ||
				    pagesize != mfp->stat.st_pagesize) {
					__db_err(dbmp->dbenv,
				    "%s: page size or clear length changed",
					    path);
					return (EINVAL);
				}

				/*
				 * It's possible that our needs for pre- and
				 * post-processing are changing.  For example,
				 * an application created a hash subdatabase
				 * in a database that was previously all btree.
				 */
				if (dbmfp->ftype != 0)
					mfp->ftype = dbmfp->ftype;

				++mfp->mpf_cnt;

				*retp = mfp;
				return (0);
			}
		}
	}

	/* Allocate a new MPOOLFILE. */
	if ((ret = __memp_alloc(
	    dbmp, dbmp->reginfo, NULL, sizeof(MPOOLFILE), NULL, &mfp)) != 0)
		goto mem_err;
	*retp = mfp;

	/* Initialize the structure. */
	memset(mfp, 0, sizeof(MPOOLFILE));
	mfp->mpf_cnt = 1;
	mfp->ftype = dbmfp->ftype;
	mfp->lsn_off = dbmfp->lsn_offset;
	mfp->clear_len = dbmfp->clear_len;

	/*
	 * If the user specifies DB_MPOOL_LAST or DB_MPOOL_NEW on a memp_fget,
	 * we have to know the last page in the file.  Figure it out and save
	 * it away.
	 */
	mfp->stat.st_pagesize = pagesize;
	mfp->orig_last_pgno = mfp->last_pgno = last_pgno;

	if (ISTEMPORARY)
		F_SET(mfp, MP_TEMP);
	else {
		/* Copy the file path into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, strlen(path) + 1, &mfp->path_off, &p)) != 0)
			goto err;
		memcpy(p, path, strlen(path) + 1);

		/* Copy the file identification string into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, DB_FILE_ID_LEN, &mfp->fileid_off, &p)) != 0)
			goto err;
		memcpy(p, dbmfp->fileid, DB_FILE_ID_LEN);

		F_SET(mfp, MP_CAN_MMAP);
		if (LF_ISSET(DB_EXTENT))
			F_SET(mfp, MP_EXTENT);
	}

	/* Copy the page cookie into shared memory. */
	if (dbmfp->pgcookie == NULL || dbmfp->pgcookie->size == 0) {
		mfp->pgcookie_len = 0;
		mfp->pgcookie_off = 0;
	} else {
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, dbmfp->pgcookie->size, &mfp->pgcookie_off, &p)) != 0)
			goto err;
		memcpy(p, dbmfp->pgcookie->data, dbmfp->pgcookie->size);
		mfp->pgcookie_len = dbmfp->pgcookie->size;
	}

	/* Prepend the MPOOLFILE to the list of MPOOLFILE's. */
	mp = dbmp->reginfo[0].primary;
	SH_TAILQ_INSERT_HEAD(&mp->mpfq, mfp, q, __mpoolfile);

	if (0) {
err:		if (mfp->path_off != 0)
			__db_shalloc_free(dbmp->reginfo[0].addr,
			    R_ADDR(dbmp->reginfo, mfp->path_off));
		if (mfp->fileid_off != 0)
			__db_shalloc_free(dbmp->reginfo[0].addr,
			    R_ADDR(dbmp->reginfo, mfp->fileid_off));
		if (mfp != NULL)
			__db_shalloc_free(dbmp->reginfo[0].addr, mfp);
mem_err:	__db_err(dbmp->dbenv,
		    "Unable to allocate memory for mpool file");
	}
	return (ret);
}

/*
 * __memp_last_pgno --
 *	Return the page number of the last page in the file.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_last_pgno(dbmfp, pgnoaddr)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

	R_LOCK(dbenv, dbmp->reginfo);
	*pgnoaddr = dbmfp->mfp->last_pgno;
	R_UNLOCK(dbenv, dbmp->reginfo);
}

/*
 * __memp_refcnt --
 *	Return the current reference count.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_refcnt(dbmfp, cntp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *cntp;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

	R_LOCK(dbenv, dbmp->reginfo);
	*cntp = dbmfp->mfp->mpf_cnt;
	R_UNLOCK(dbenv, dbmp->reginfo);
}

/*
 * __memp_set_unlink --
 *	Set unlink on last close flag.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_set_unlink(dbmpf, set)
	DB_MPOOLFILE *dbmpf;
	int set;
{
	DB_MPOOL *dbmp;

	dbmp = dbmpf->dbmp;

	if (set) {
		R_LOCK(dbmp->dbenv, dbmp->reginfo);
		F_SET(dbmpf->mfp, MP_UNLINK);
		R_UNLOCK(dbmp->dbenv, dbmp->reginfo);
	} else {
		/*
		 * This bit is protected in the queue code because the metapage
		 * is locked, so we can avoid getting the region lock.  If this
		 * gets used from other than the queue code, we cannot.
		 */
		if (F_ISSET(dbmpf->mfp, MP_UNLINK)) {
			R_LOCK(dbmp->dbenv, dbmp->reginfo);
			F_CLR(dbmpf->mfp, MP_UNLINK);
			R_UNLOCK(dbmp->dbenv, dbmp->reginfo);
		}
	}
}

/*
 * memp_fclose --
 *	Close a backing file for the memory pool.
 */
static int
__memp_fclose(dbmfp, flags)
	DB_MPOOLFILE *dbmfp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbmfp->dbmp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * XXX
	 * DB_MPOOL_DISCARD: Undocumented flag: DB private.
	 */
	if (flags != 0 && (ret = __db_fchk(dbenv,
	    "DB_MPOOLFILE->close", flags, DB_MPOOL_DISCARD)) != 0)
		return (ret);

	return (__memp_fclose_int(dbmfp, flags, 1));
}

/*
 * __memp_fclose_int --
 *	Internal version of __memp_fclose.
 *
 * PUBLIC: int __memp_fclose_int __P((DB_MPOOLFILE *, u_int32_t, int));
 */
int
__memp_fclose_int(dbmfp, flags, needlock)
	DB_MPOOLFILE *dbmfp;
	u_int32_t flags;
	int needlock;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	char *rpath;
	int ret, t_ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	ret = 0;

	/*
	 * Remove the DB_MPOOLFILE from the queue.  This has to happen before
	 * we perform any action that can fail, otherwise __memp_close may
	 * loop infinitely when calling us to discard all of the DB_MPOOLFILEs.
	 */
	for (;;) {
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);

		/*
		 * We have to reference count DB_MPOOLFILE structures as other
		 * threads may be using them.  The problem only happens if the
		 * application makes a bad design choice.  Here's the path:
		 *
		 * Thread A opens a database.
		 * Thread B uses thread A's DB_MPOOLFILE to write a buffer
		 *    in order to free up memory in the mpool cache.
		 * Thread A closes the database while thread B is using the
		 *    DB_MPOOLFILE structure.
		 *
		 * By opening all databases before creating the threads, and
		 * closing them after the threads have exited, applications
		 * get better performance and avoid the problem path entirely.
		 *
		 * Regardless, holding the DB_MPOOLFILE to flush a dirty buffer
		 * is a short-term lock, even in worst case, since we better be
		 * the only thread of control using the DB_MPOOLFILE structure
		 * to read pages *into* the cache.  Wait until we're the only
		 * reference holder and remove the DB_MPOOLFILE structure from
		 * the list, so nobody else can even find it.
		 */
		if (dbmfp->ref == 1) {
			TAILQ_REMOVE(&dbmp->dbmfq, dbmfp, q);
			break;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

		(void)__os_sleep(dbenv, 1, 0);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	/* Complain if pinned blocks never returned. */
	if (dbmfp->pinref != 0)
		__db_err(dbenv, "%s: close: %lu blocks left pinned",
		    __memp_fn(dbmfp), (u_long)dbmfp->pinref);

	/* Discard any mmap information. */
	if (dbmfp->addr != NULL &&
	    (ret = __os_unmapfile(dbenv, dbmfp->addr, dbmfp->len)) != 0)
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(ret));

	/* Close the file; temporary files may not yet have been created. */
	if (F_ISSET(dbmfp->fhp, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(dbmfp->fhp)) != 0) {
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(t_ret));
		if (ret == 0)
			ret = t_ret;
	}

	/* Discard the thread mutex. */
	if (dbmfp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmfp->mutexp);

	/*
	 * Discard our reference on the the underlying MPOOLFILE, and close
	 * it if it's no longer useful to anyone.
	 *
	 * If it's a temp file, all outstanding references belong to unflushed
	 * buffers.  (A temp file can only be referenced by one DB_MPOOLFILE).
	 * We don't care about preserving any of those buffers, so mark the
	 * MPOOLFILE as dead so that even the dirty ones just get discarded
	 * when we try to flush them.
	 */
	if ((mfp = dbmfp->mfp) == NULL)
		goto done;
	if (needlock)
		R_LOCK(dbenv, dbmp->reginfo);
	if (--mfp->mpf_cnt == 0 || LF_ISSET(DB_MPOOL_DISCARD)) {
		if (LF_ISSET(DB_MPOOL_DISCARD) ||
		    F_ISSET(mfp, MP_TEMP | MP_UNLINK))
			MEMP_FREMOVE(mfp);
		if (F_ISSET(mfp, MP_UNLINK)) {
			if ((t_ret = __db_appname(dbmp->dbenv,
			    DB_APP_DATA, NULL, R_ADDR(dbmp->reginfo,
			    mfp->path_off), 0, NULL, &rpath)) != 0 && ret == 0)
				ret = t_ret;
			if (t_ret == 0 && (t_ret =
			    __os_unlink(dbmp->dbenv, rpath) != 0) && ret == 0)
				ret = t_ret;
			__os_free(dbenv, rpath, 0);
		}
		if (mfp->block_cnt == 0)
			__memp_mf_discard(dbmp, mfp);
	}
	if (needlock)
		R_UNLOCK(dbenv, dbmp->reginfo);

done:	/* Discard the DB_MPOOLFILE structure. */
	__os_free(dbenv, dbmfp->fhp, sizeof(DB_FH));
	__os_free(dbenv, dbmfp, sizeof(DB_MPOOLFILE));

	return (ret);
}

/*
 * __memp_mf_discard --
 *	Discard an MPOOLFILE.
 *
 * PUBLIC: void __memp_mf_discard __P((DB_MPOOL *, MPOOLFILE *));
 */
void
__memp_mf_discard(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	MPOOL *mp;

	mp = dbmp->reginfo[0].primary;

	/* Delete from the list of MPOOLFILEs. */
	SH_TAILQ_REMOVE(&mp->mpfq, mfp, q, __mpoolfile);

	/* Free the space. */
	if (mfp->path_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->path_off));
	if (mfp->fileid_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->fileid_off));
	if (mfp->pgcookie_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->pgcookie_off));
	__db_shalloc_free(dbmp->reginfo[0].addr, mfp);
}

/*
 * __memp_fn --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fn __P((DB_MPOOLFILE *));
 */
char *
__memp_fn(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	return (__memp_fns(dbmfp->dbmp, dbmfp->mfp));
}

/*
 * __memp_fns --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fns __P((DB_MPOOL *, MPOOLFILE *));
 *
 */
char *
__memp_fns(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	if (mfp->path_off == 0)
		return ((char *)"temporary");

	return ((char *)R_ADDR(dbmp->reginfo, mfp->path_off));
}
