/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2001
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: log.c,v 11.69 2001/10/02 01:33:40 bostic Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "log.h"
#include "db_dispatch.h"
#include "txn.h"
#include "txn_auto.h"

static int __log_init __P((DB_ENV *, DB_LOG *));
static int __log_recover __P((DB_LOG *));
static size_t __log_region_size __P((DB_ENV *));

/*
 * __log_open --
 *	Internal version of log_open: only called from DB_ENV->open.
 *
 * PUBLIC: int __log_open __P((DB_ENV *));
 */
int
__log_open(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	LOG *lp;
	int ret;

	/* Create/initialize the DB_LOG structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOG), &dblp)) != 0)
		return (ret);
	dblp->dbenv = dbenv;

	/* Join/create the log region. */
	dblp->reginfo.type = REGION_TYPE_LOG;
	dblp->reginfo.id = INVALID_REGION_ID;
	dblp->reginfo.mode = dbenv->db_mode;
	dblp->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&dblp->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(
	    dbenv, &dblp->reginfo, __log_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&dblp->reginfo, REGION_CREATE))
		if ((ret = __log_init(dbenv, dblp)) != 0)
			goto err;

	/* Set the local addresses. */
	lp = dblp->reginfo.primary =
	    R_ADDR(&dblp->reginfo, dblp->reginfo.rp->primary);

	/*
	 * If the region is threaded, then we have to lock both the handles
	 * and the region, and we need to allocate a mutex for that purpose.
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, &dblp->reginfo, 1, &dblp->mutexp)) != 0)
			goto err;
		if ((ret = __db_shmutex_init(dbenv, dblp->mutexp, 0,
		    MUTEX_THREAD, &dblp->reginfo,
		    (REGMAINT *)R_ADDR(&dblp->reginfo, lp->maint_off))) != 0)
			goto err;
	}

	/* Initialize the rest of the structure. */
	dblp->bufp = R_ADDR(&dblp->reginfo, lp->buffer_off);

	/*
	 * Set the handle -- we may be about to run recovery, which allocates
	 * log cursors.  Log cursors require logging be already configured,
	 * and the handle being set is what demonstrates that.
	 */
	dbenv->lg_handle = dblp;

	/* If we created the region, run recovery. */
	if (F_ISSET(&dblp->reginfo, REGION_CREATE))
		if ((ret = __log_recover(dblp)) != 0)
			goto err;

	R_UNLOCK(dbenv, &dblp->reginfo);
	return (0);

err:	if (dblp->reginfo.addr != NULL) {
		if (F_ISSET(&dblp->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);
		R_UNLOCK(dbenv, &dblp->reginfo);
		(void)__db_r_detach(dbenv, &dblp->reginfo, 0);
	}

	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);

	__os_free(dbenv, dblp, sizeof(*dblp));

	return (ret);
}

/*
 * __log_init --
 *	Initialize a log region in shared memory.
 */
static int
__log_init(dbenv, dblp)
	DB_ENV *dbenv;
	DB_LOG *dblp;
{
	LOG *region;
	int ret;
	void *p;
#ifdef  MUTEX_SYSTEM_RESOURCES
	u_int8_t *addr;
#endif

	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(*region), 0, &dblp->reginfo.primary)) != 0)
		goto mem_err;
	dblp->reginfo.rp->primary =
	    R_OFFSET(&dblp->reginfo, dblp->reginfo.primary);
	region = dblp->reginfo.primary;
	memset(region, 0, sizeof(*region));

	region->persist.lg_max = dbenv->lg_max;
	region->persist.magic = DB_LOGMAGIC;
	region->persist.version = DB_LOGVERSION;
	region->persist.mode = dbenv->db_mode;
	SH_TAILQ_INIT(&region->fq);

	/* Initialize LOG LSNs. */
	region->lsn.file = 1;
	region->lsn.offset = 0;
	region->waiting_lsn.file = 1;
	region->waiting_lsn.offset = 0;
	region->ready_lsn.file = 1;
	region->ready_lsn.offset = 0;
	region->t_lsn.file = 1;
	region->t_lsn.offset = 0;

#ifdef  MUTEX_SYSTEM_RESOURCES
	/* Allocate room for the txn maintenance info and initialize it. */
	if ((ret = __db_shalloc(dblp->reginfo.addr,
	    sizeof(REGMAINT) + LG_MAINT_SIZE, 0, &addr)) != 0)
		goto mem_err;
	__db_maintinit(&dblp->reginfo, addr, LG_MAINT_SIZE);
	region->maint_off = R_OFFSET(&dblp->reginfo, addr);
#endif

	if ((ret = __db_shmutex_init(dbenv, &region->flush, 0,
	    0, &dblp->reginfo,
	    (REGMAINT *)R_ADDR(&dblp->reginfo, region->maint_off))) != 0)
		return (ret);

	/* Initialize the buffer. */
	if ((ret =
	    __db_shalloc(dblp->reginfo.addr, dbenv->lg_bsize, 0, &p)) != 0) {
mem_err:	__db_err(dbenv, "Unable to allocate memory for the log buffer");
		return (ret);
	}
	region->buffer_size = dbenv->lg_bsize;
	region->buffer_off = R_OFFSET(&dblp->reginfo, p);

	/* Initialize the commit Queue. */
	SH_TAILQ_INIT(&region->free_commits);
	SH_TAILQ_INIT(&region->commits);
	region->ncommit = 0;

	return (0);
}

/*
 * __log_recover --
 *	Recover a log.
 */
static int
__log_recover(dblp)
	DB_LOG *dblp;
{
	DBT dbt;
	DB_ENV *dbenv;
	DB_LOGC *logc;
	DB_LSN lsn;
	LOG *lp;
	int cnt, found_checkpoint, ret;
	u_int32_t chk;
	logfile_validity status;

	logc = NULL;
	lp = dblp->reginfo.primary;

	/*
	 * Find a log file.  If none exist, we simply return, leaving
	 * everything initialized to a new log.
	 */
	if ((ret = __log_find(dblp, 0, &cnt, &status)) != 0)
		return (ret);
	if (cnt == 0)
		return (0);

	/*
	 * If the last file is an old version, readable or no, start a new
	 * file.  Don't bother finding checkpoints;  if we didn't take a
	 * checkpoint right before upgrading, the user screwed up anyway.
	 */
	if (status == DB_LV_OLD_READABLE || status == DB_LV_OLD_UNREADABLE) {
		lp->lsn.file = lp->s_lsn.file = cnt + 1;
		lp->lsn.offset = lp->s_lsn.offset = 0;
		goto skipsearch;
	}
	DB_ASSERT(status == DB_LV_NORMAL);

	/*
	 * We have the last useful log file and we've loaded any persistent
	 * information.  Set the end point of the log past the end of the last
	 * file. Read the last file, looking for the last checkpoint and
	 * the log's end.
	 */
	lp->lsn.file = cnt + 1;
	lp->lsn.offset = 0;
	lsn.file = cnt;
	lsn.offset = 0;

	/*
	 * Allocate a cursor and set it to the first record.  This shouldn't
	 * fail, leave error messages on.
	 */
	dbenv = dblp->dbenv;
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);
	F_SET(logc, DB_LOG_LOCKED);
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = logc->get(logc, &lsn, &dbt, DB_SET)) != 0)
		goto err;

	/*
	 * Read to the end of the file, looking for checkpoints.  This will
	 * fail at some point, so turn off error messages.
	 */
	F_SET(logc, DB_LOG_SILENT_ERR);
	found_checkpoint = 0;
	while (logc->get(logc, &lsn, &dbt, DB_NEXT) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&chk, dbt.data, sizeof(u_int32_t));
		if (chk == DB_txn_ckp) {
			lp->chkpt_lsn = lsn;
			found_checkpoint = 1;
		}
	}
	F_CLR(logc, DB_LOG_SILENT_ERR);

	/*
	 * We now know where the end of the log is.  Set the first LSN that
	 * we want to return to an application and the LSN of the last known
	 * record on disk.
	 */
	lp->lsn = lsn;
	lp->s_lsn = lsn;
	lp->lsn.offset += logc->c_len;
	lp->s_lsn.offset += logc->c_len;

	/* Set up the current buffer information, too. */
	lp->len = logc->c_len;
	lp->b_off = 0;
	lp->w_off = lp->lsn.offset;

	/*
	 * It's possible that we didn't find a checkpoint because there wasn't
	 * one in the last log file.  Start searching.
	 */
	if (!found_checkpoint && cnt > 1) {
		lsn.file = cnt;
		lsn.offset = 0;

		/* Set the cursor; shouldn't fail, leave error messages on. */
		if ((ret = logc->get(logc, &lsn, &dbt, DB_SET)) != 0)
			goto err;

		/*
		 * Read to the end of the file, looking for checkpoints.  This
		 * can fail if there are no checkpoints in any log file, so we
		 * turn off error messages.
		 */
		F_SET(logc, DB_LOG_SILENT_ERR);
		while (logc->get(logc, &lsn, &dbt, DB_PREV) == 0) {
			if (dbt.size < sizeof(u_int32_t))
				continue;
			memcpy(&chk, dbt.data, sizeof(u_int32_t));
			if (chk == DB_txn_ckp) {
				lp->chkpt_lsn = lsn;
				found_checkpoint = 1;
				break;
			}
		}
		F_CLR(logc, DB_LOG_SILENT_ERR);
	}

	/* If we never find a checkpoint, that's okay, just 0 it out. */
	if (!found_checkpoint)
skipsearch:	ZERO_LSN(lp->chkpt_lsn);

	if (FLD_ISSET(dblp->dbenv->verbose, DB_VERB_RECOVERY))
		__db_err(dblp->dbenv,
		    "Finding last valid log LSN: file: %lu offset %lu",
		    (u_long)lp->lsn.file, (u_long)lp->lsn.offset);

err:	if (logc != NULL)
		(void)logc->close(logc, 0);

	return (ret);
}

/*
 * __log_find --
 *	Try to find a log file.  If find_first is set, valp will contain
 * the number of the first readable log file, else it will contain the number
 * of the last log file (which may be too old to read).
 *
 * PUBLIC: int __log_find __P((DB_LOG *, int, int *, logfile_validity *));
 */
int
__log_find(dblp, find_first, valp, statusp)
	DB_LOG *dblp;
	int find_first, *valp;
	logfile_validity *statusp;
{
	char *c, **names, *p, *q, savech;
	const char *dir;
	int cnt, fcnt, ret;
	logfile_validity logval_status, status;
	u_int32_t clv, logval;

	logval_status = status = DB_LV_NONEXISTENT;

	/* Return a value of 0 as the log file number on failure. */
	*valp = 0;

	/* Find the directory name. */
	if ((ret = __log_name(dblp, 1, &p, NULL, 0)) != 0)
		return (ret);
	if ((q = __db_rpath(p)) == NULL) {
		COMPQUIET(savech, 0);
		dir = PATH_DOT;
	} else {
		savech = *q;
		*q = '\0';
		dir = p;
	}

	/* Get the list of file names. */
	ret = __os_dirlist(dblp->dbenv, dir, &names, &fcnt);

	/*
	 * !!!
	 * We overwrote a byte in the string with a nul.  Restore the string
	 * so that the diagnostic checks in the memory allocation code work
	 * and any error messages display the right file name.
	 */
	if (q != NULL)
		*q = savech;

	if (ret != 0) {
		__db_err(dblp->dbenv, "%s: %s", dir, db_strerror(ret));
		__os_freestr(dblp->dbenv, p);
		return (ret);
	}

	/* Search for a valid log file name. */
	for (cnt = fcnt, clv = logval = 0; --cnt >= 0;) {
		if (strncmp(names[cnt], LFPREFIX, sizeof(LFPREFIX) - 1) != 0)
			continue;

		/*
		 * Names of the form log\.[0-9]* are reserved for DB.  Other
		 * names sharing LFPREFIX, such as "log.db", are legal.
		 */
		for (c = names[cnt] + sizeof(LFPREFIX) - 1; *c != '\0'; c++)
			if (!isdigit((int)*c))
				break;
		if (*c != '\0')
			continue;

		/*
		 * Use atol, not atoi; if an "int" is 16-bits, the largest
		 * log file name won't fit.
		 */
		clv = atol(names[cnt] + (sizeof(LFPREFIX) - 1));
		if (find_first) {
			if (logval != 0 && clv > logval)
				continue;
		} else
			if (logval != 0 && clv < logval)
				continue;

		/*
		 * Take note of whether the log file logval is
		 * an old version or incompletely initialized.
		 */
		if ((ret = __log_valid(dblp, clv, 1, &status)) != 0) {
			__db_err(dblp->dbenv, "Invalid log file: %s: %s",
			    names[cnt], db_strerror(ret));
			goto err;
		}
		switch (status) {
		case DB_LV_INCOMPLETE:
			/*
			 * It's acceptable for the last log file to
			 * have been incompletely initialized--it's possible
			 * to create a log file but not write anything to it,
			 * and recovery needs to gracefully handle this.
			 *
			 * Just ignore it;  we don't want to return this
			 * as a valid log file.
			 */
			break;
		case DB_LV_NONEXISTENT:
			/* Should never happen. */
			DB_ASSERT(0);
			break;
		case DB_LV_NORMAL:
		case DB_LV_OLD_READABLE:
			logval = clv;
			logval_status = status;
			break;
		case DB_LV_OLD_UNREADABLE:
			/*
			 * Continue;  we want the oldest valid log,
			 * and clv is too old to be useful.  We don't
			 * want it to supplant logval if we're looking for
			 * the oldest valid log, but we do want to return
			 * it if it's the last log file--we want the very
			 * last file number, so that our caller can
			 * start a new file after it.
			 *
			 * The code here assumes that there will never
			 * be a too-old log that's preceded by a log
			 * of the current version, but in order to
			 * attain that state of affairs the user
			 * would have had to really seriously screw
			 * up;  I think we can safely assume this won't
			 * happen.
			 */
			if (!find_first) {
				logval = clv;
				logval_status = status;
			}
			break;
		}
	}

	*valp = logval;

err:	__os_dirfree(dblp->dbenv, names, fcnt);
	__os_freestr(dblp->dbenv, p);
	*statusp = logval_status;

	return (ret);
}

/*
 * log_valid --
 *	Validate a log file.  Returns an error code in the event of
 *	a fatal flaw in a the specified log file;  returns success with
 *	a code indicating the currentness and completeness of the specified
 *	log file if it is not unexpectedly flawed (that is, if it's perfectly
 *	normal, if it's zero-length, or if it's an old version).
 *
 * PUBLIC: int __log_valid __P((DB_LOG *, u_int32_t, int, logfile_validity *));
 */
int
__log_valid(dblp, number, set_persist, statusp)
	DB_LOG *dblp;
	u_int32_t number;
	int set_persist;
	logfile_validity *statusp;
{
	DB_FH fh;
	LOG *region;
	LOGP persist;
	char *fname;
	int ret;
	logfile_validity status;
	size_t nw;

	status = DB_LV_NORMAL;

	/* Try to open the log file. */
	if ((ret = __log_name(dblp,
	    number, &fname, &fh, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
		__os_freestr(dblp->dbenv, fname);
		return (ret);
	}

	/* Try to read the header. */
	if ((ret =
	    __os_seek(dblp->dbenv,
	    &fh, 0, 0, sizeof(HDR), 0, DB_OS_SEEK_SET)) != 0 ||
	    (ret =
	    __os_read(dblp->dbenv, &fh, &persist, sizeof(LOGP), &nw)) != 0 ||
	    nw != sizeof(LOGP)) {
		if (ret == 0)
			status = DB_LV_INCOMPLETE;
		else
			/*
			 * The error was a fatal read error, not just an
			 * incompletely initialized log file.
			 */
			__db_err(dblp->dbenv, "Ignoring log file: %s: %s",
			    fname, db_strerror(ret));

		(void)__os_closehandle(&fh);
		goto err;
	}
	(void)__os_closehandle(&fh);

	/* Validate the header. */
	if (persist.magic != DB_LOGMAGIC) {
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: magic number %lx, not %lx",
		    fname, (u_long)persist.magic, (u_long)DB_LOGMAGIC);
		ret = EINVAL;
		goto err;
	}

	/*
	 * Set our status code to indicate whether the log file
	 * belongs to an unreadable or readable old version;  leave it
	 * alone if and only if the log file version is the current one.
	 */
	if (persist.version > DB_LOGVERSION) {
		/* This is a fatal error--the log file is newer than DB. */
		__db_err(dblp->dbenv,
		    "Ignoring log file: %s: unsupported log version %lu",
		    fname, (u_long)persist.version);
		ret = EINVAL;
		goto err;
	} else if (persist.version < DB_LOGOLDVER) {
		status = DB_LV_OLD_UNREADABLE;
		/*
		 * We don't want to set persistent info based on an
		 * unreadable region, so jump to "err".
		 */
		goto err;
	} else if (persist.version < DB_LOGVERSION)
		status = DB_LV_OLD_READABLE;

	/*
	 * If the log is thus far readable and we're doing system
	 * initialization, set the region's persistent information
	 * based on the headers.
	 */
	if (set_persist) {
		region = dblp->reginfo.primary;
		region->persist.lg_max = persist.lg_max;
		region->persist.mode = persist.mode;
	}

err:	__os_freestr(dblp->dbenv, fname);
	*statusp = status;
	return (ret);
}

/*
 * __log_dbenv_refresh --
 *	Clean up after the log system on a close or failed open.  Called only
 * from __dbenv_refresh.  (Formerly called __log_close.)
 *
 * PUBLIC: int __log_dbenv_refresh __P((DB_ENV *));
 */
int
__log_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	u_int32_t buffer_size;
	int ret, t_ret;

	dblp = dbenv->lg_handle;
	buffer_size = ((LOG *)dblp->reginfo.primary)->buffer_size;

	/* We may have opened files as part of XA; if so, close them. */
	F_SET(dblp, DBLOG_RECOVER);
	__log_close_files(dbenv);

	/* Discard the per-thread lock. */
	if (dblp->mutexp != NULL)
		__db_mutex_free(dbenv, &dblp->reginfo, dblp->mutexp);

	/* Detach from the region. */
	ret = __db_r_detach(dbenv, &dblp->reginfo, 0);

	/* Close open files, release allocated memory. */
	if (F_ISSET(&dblp->lfh, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(&dblp->lfh)) != 0 && ret == 0)
		ret = t_ret;
	if (dblp->dbentry != NULL)
		__os_free(dbenv, dblp->dbentry,
		    (dblp->dbentry_cnt * sizeof(DB_ENTRY)));

	__os_free(dbenv, dblp, sizeof(*dblp));

	dbenv->lg_handle = NULL;
	return (ret);
}

/*
 * __log_stat --
 *	Return log statistics.
 *
 * PUBLIC: int __log_stat __P((DB_ENV *, DB_LOG_STAT **, u_int32_t));
 */
int
__log_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOG_STAT **statp;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LOG_STAT *stats;
	LOG *region;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_stat", DB_INIT_LOG);

	*statp = NULL;
	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->log_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	dblp = dbenv->lg_handle;
	region = dblp->reginfo.primary;

	if ((ret = __os_umalloc(dbenv, sizeof(DB_LOG_STAT), &stats)) != 0)
		return (ret);

	/* Copy out the global statistics. */
	R_LOCK(dbenv, &dblp->reginfo);
	*stats = region->stat;
	if (LF_ISSET(DB_STAT_CLEAR))
		memset(&region->stat, 0, sizeof(region->stat));

	stats->st_magic = region->persist.magic;
	stats->st_version = region->persist.version;
	stats->st_mode = region->persist.mode;
	stats->st_lg_bsize = region->buffer_size;
	stats->st_lg_max = region->persist.lg_max;

	stats->st_region_wait = dblp->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = dblp->reginfo.rp->mutex.mutex_set_nowait;
	if (LF_ISSET(DB_STAT_CLEAR)) {
		dblp->reginfo.rp->mutex.mutex_set_wait = 0;
		dblp->reginfo.rp->mutex.mutex_set_nowait = 0;
	}
	stats->st_regsize = dblp->reginfo.rp->size;

	stats->st_cur_file = region->lsn.file;
	stats->st_cur_offset = region->lsn.offset;
	stats->st_disk_file = region->s_lsn.file;
	stats->st_disk_offset = region->s_lsn.offset;

	R_UNLOCK(dbenv, &dblp->reginfo);

	*statp = stats;
	return (0);
}

/*
 * __log_lastckp --
 *	Return the current chkpt_lsn, so that we can store it in
 *	the transaction region and keep the chain of checkpoints
 *	unbroken across environment recreates.
 *
 * PUBLIC: int __log_lastckp __P((DB_ENV *, DB_LSN *));
 */
int
__log_lastckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	LOG *lp;

	lp = (LOG *)(((DB_LOG *)dbenv->lg_handle)->reginfo.primary);

	*lsnp = lp->chkpt_lsn;
	return (0);
}

/*
 * __log_region_size --
 *	Return the amount of space needed for the log region.
 *	Make the region large enough to hold txn_max transaction
 *	detail structures  plus some space to hold thread handles
 *	and the beginning of the shalloc region and anything we
 *	need for mutex system resource recording.
 */
static size_t
__log_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	s = dbenv->lg_regionmax + dbenv->lg_bsize;
#ifdef MUTEX_SYSTEM_RESOURCES
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		s += sizeof(REGMAINT) + LG_MAINT_SIZE;
#endif
	return (s);
}

/*
 * __log_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __log_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__log_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	__db_shlocks_destroy(infop, (REGMAINT *)R_ADDR(infop,
	    ((LOG *)R_ADDR(infop, infop->rp->primary))->maint_off));

	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
}

/*
 * __log_vtruncate
 *	This is a virtual truncate.  We set up the log indicators to
 * make everyone believe that the given record is the last one in the
 * log.  Returns with the next valid LSN (i.e., the LSN of the next
 * record to be written). This is used in replication to discard records
 * in the log file that do not agree with the master.
 *
 * PUBLIC: int __log_vtruncate __P((DB_ENV *, DB_LSN *, DB_LSN *));
 */
int
__log_vtruncate(dbenv, lsn, ckplsn)
	DB_ENV *dbenv;
	DB_LSN *lsn, *ckplsn;
{
	DBT log_dbt;
	DB_FH fh;
	DB_LOG *dblp;
	DB_LOGC *logc;
	LOG *lp;
	u_int32_t bytes, c_len;
	int fn, ret, t_ret;
	char *fname;

	/* Need to find out the length of this soon-to-be-last record. */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);
	memset(&log_dbt, 0, sizeof(log_dbt));
	ret = logc->get(logc, lsn, &log_dbt, DB_SET);
	c_len = logc->c_len;
	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		return (ret);

	/* Now do the truncate. */
	dblp = (DB_LOG *)dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);
	lp->lsn = *lsn;
	lp->len = c_len;
	lp->lsn.offset += lp->len;
	lp->chkpt_lsn = *ckplsn;

	/*
	 * I am going to assume that the number of bytes written since
	 * the last checkpoint doesn't exceed a 32-bit number.
	 */
	DB_ASSERT(lp->lsn.file >= ckplsn->file);
	bytes = 0;
	if (ckplsn->file != lp->lsn.file) {
		bytes = lp->persist.lg_max - ckplsn->offset;
		if (lp->lsn.file > ckplsn->file + 1)
			bytes += lp->persist.lg_max *
			    (lp->lsn.file - ckplsn->file - 1);
		bytes += lp->lsn.offset;
	} else
		bytes = lp->lsn.offset - ckplsn->offset;

	lp->stat.st_wc_mbytes += bytes / MEGABYTE;
	lp->stat.st_wc_bytes += bytes % MEGABYTE;

	/*
	 * If the saved lsn is greater than our new end of log, reset it
	 * to our current end of log.
	 */
	if (log_compare(&lp->s_lsn, lsn) > 0)
		lp->s_lsn = lp->lsn;

	/*
	 * If the new end of log is in the middle of the
	 * buffer, don't change the w_off.  If the new end
	 * is before the w_off then reset w_off to the new
	 * end of log.
	 */
	if (lp->w_off >= lp->lsn.offset) {
		lp->w_off = lp->lsn.offset;
		lp->b_off = 0;
	} else
		lp->b_off = lp->lsn.offset - lp->w_off;

	ZERO_LSN(lp->waiting_lsn);
	lp->ready_lsn = lp->lsn;
	lp->f_lsn = lp->lsn;

	/* Now throw away any extra log files that we have around. */
	for (fn = lp->lsn.file + 1;; fn++) {
		if (__log_name(dblp, fn, &fname, &fh, DB_OSO_RDONLY) != 0)
			break;
		(void)__os_closehandle(&fh);
		if ((ret = __os_unlink(dbenv, fname)) != 0)
			break;
		__os_freestr(dbenv, fname);
	}
	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}

/*
 * __log_is_outdated --
 *	Used by the replication system to identify if a client's logs
 * are too old.  The log represented by dbenv is compared to the file
 * number passed in fnum.  If the log file fnum does not exist and is
 * lower-numbered than the current logs, the we return *outdatedp non
 * zero, else we return it 0.
 *
 * PUBLIC:  int __log_is_outdated __P((DB_ENV *dbenv,
 * PUBLIC:     u_int32_t fnum, int *outdatedp));
 */
int
__log_is_outdated(dbenv, fnum, outdatedp)
	DB_ENV *dbenv;
	u_int32_t fnum;
	int *outdatedp;
{
	DB_LOG *dblp;
	LOG *lp;
	char *name;
	int ret;
	u_int32_t cfile;

	dblp = dbenv->lg_handle;
	*outdatedp = 0;

	if ((ret = __log_name(dblp, fnum, &name, NULL, 0)) != 0)
		return (ret);

	/* If the file exists, we're just fine. */
	if (__os_exists(name, NULL) == 0)
		goto out;

	/*
	 * It didn't exist, decide if the file number is too big or
	 * too little.  If it's too little, then we need to indicate
	 * that the LSN is outdated.
	 */
	R_LOCK(dbenv, &dblp->reginfo);
	lp = (LOG *)dblp->reginfo.primary;
	cfile = lp->lsn.file;
	R_UNLOCK(dbenv, &dblp->reginfo);

	if (cfile > fnum)
		*outdatedp = 1;
out:	__os_freestr(dbenv, name);
	return (ret);
}
