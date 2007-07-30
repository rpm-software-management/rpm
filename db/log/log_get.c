/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: log_get.c,v 12.40 2007/05/17 15:15:44 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"
#include "dbinc/hmac.h"
#include "dbinc/log.h"
#include "dbinc/hash.h"

typedef enum { L_ALREADY, L_ACQUIRED, L_NONE } RLOCK;

static int __logc_close_pp __P((DB_LOGC *, u_int32_t));
static int __logc_get_pp __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
static int __logc_get_int __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
static int __logc_hdrchk __P((DB_LOGC *, DB_LSN *, HDR *, int *));
static int __logc_incursor __P((DB_LOGC *, DB_LSN *, HDR *, u_int8_t **));
static int __logc_inregion __P((DB_LOGC *,
	       DB_LSN *, RLOCK *, DB_LSN *, HDR *, u_int8_t **, int *));
static int __logc_io __P((DB_LOGC *,
	       u_int32_t, u_int32_t, void *, size_t *, int *));
static int __logc_ondisk __P((DB_LOGC *,
	       DB_LSN *, DB_LSN *, u_int32_t, HDR *, u_int8_t **, int *));
static int __logc_set_maxrec __P((DB_LOGC *, char *));
static int __logc_shortread __P((DB_LOGC *, DB_LSN *, int));
static int __logc_version_pp __P((DB_LOGC *, u_int32_t *, u_int32_t));

/*
 * __log_cursor_pp --
 *	DB_ENV->log_cursor
 *
 * PUBLIC: int __log_cursor_pp __P((DB_ENV *, DB_LOGC **, u_int32_t));
 */
int
__log_cursor_pp(dbenv, logcp, flags)
	DB_ENV *dbenv;
	DB_LOGC **logcp;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_cursor", DB_INIT_LOG);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB_ENV->log_cursor", flags, 0)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__log_cursor(dbenv, logcp)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __log_cursor --
 *	Create a log cursor.
 *
 * PUBLIC: int __log_cursor __P((DB_ENV *, DB_LOGC **));
 */
int
__log_cursor(dbenv, logcp)
	DB_ENV *dbenv;
	DB_LOGC **logcp;
{
	DB_LOGC *logc;
	int ret;

	*logcp = NULL;

	/* Allocate memory for the cursor. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_LOGC), &logc)) != 0)
		return (ret);

	logc->bp_size = LG_CURSOR_BUF_SIZE;
	/*
	 * Set this to something positive.
	 */
	logc->bp_maxrec = MEGABYTE;
	if ((ret = __os_malloc(dbenv, logc->bp_size, &logc->bp)) != 0) {
		__os_free(dbenv, logc);
		return (ret);
	}

	logc->dbenv = dbenv;
	logc->close = __logc_close_pp;
	logc->get = __logc_get_pp;
	logc->version = __logc_version_pp;

	*logcp = logc;
	return (0);
}

/*
 * __logc_close_pp --
 *	DB_LOGC->close pre/post processing.
 */
static int
__logc_close_pp(logc, flags)
	DB_LOGC *logc;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	DB_ENV *dbenv;
	int ret;

	dbenv = logc->dbenv;

	PANIC_CHECK(dbenv);
	if ((ret = __db_fchk(dbenv, "DB_LOGC->close", flags, 0)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__logc_close(logc)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __logc_close --
 *	DB_LOGC->close.
 *
 * PUBLIC: int __logc_close __P((DB_LOGC *));
 */
int
__logc_close(logc)
	DB_LOGC *logc;
{
	DB_ENV *dbenv;

	dbenv = logc->dbenv;

	if (logc->fhp != NULL) {
		(void)__os_closehandle(dbenv, logc->fhp);
		logc->fhp = NULL;
	}

	if (logc->dbt.data != NULL)
		__os_free(dbenv, logc->dbt.data);

	__os_free(dbenv, logc->bp);
	__os_free(dbenv, logc);

	return (0);
}

/*
 * __logc_version_pp --
 *	DB_LOGC->version.
 */
static int
__logc_version_pp(logc, versionp, flags)
	DB_LOGC *logc;
	u_int32_t *versionp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_THREAD_INFO *ip;
	int ret;

	dbenv = logc->dbenv;

	PANIC_CHECK(dbenv);
	if ((ret = __db_fchk(dbenv, "DB_LOGC->version", flags, 0)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__logc_version(logc, versionp)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __logc_version --
 *	DB_LOGC->version.
 *
 * PUBLIC: int __logc_version __P((DB_LOGC *, u_int32_t *));
 */
int
__logc_version(logc, versionp)
	DB_LOGC *logc;
	u_int32_t *versionp;
{
	DB_ENV *dbenv;
	DB_LSN plsn;
	DB_LOGC *plogc;
	DBT hdrdbt;
	LOGP *persist;
	int ret, t_ret;

	dbenv = logc->dbenv;
	if (IS_ZERO_LSN(logc->lsn)) {
		__db_errx(dbenv, "DB_LOGC->get: unset cursor");
		return (EINVAL);
	}
	ret = 0;
	/*
	 * Check if the persist info we have is for the same file
	 * as the current cursor position.  If we already have the
	 * information, then we're done.  If not, we open a new
	 * log cursor and get the header.
	 *
	 * Since most users walk forward through the log when
	 * using this feature (i.e. printlog) we're likely to
	 * have the information we need.
	 */
	if (logc->lsn.file != logc->p_lsn.file) {
		if ((ret = __log_cursor(dbenv, &plogc)) != 0)
			return (ret);
		plsn.file = logc->lsn.file;
		plsn.offset = 0;
		plogc->lsn = plsn;
		memset(&hdrdbt, 0, sizeof(DBT));
		if ((ret = __logc_get_int(plogc,
		    &plsn, &hdrdbt, DB_SET)) == 0) {
			persist = (LOGP *)hdrdbt.data;
			logc->p_lsn = logc->lsn;
			logc->p_version = persist->version;
		}
		if ((t_ret = __logc_close(plogc)) != 0 && ret == 0)
			ret = t_ret;
	}
	/* Return the version. */
	if (ret == 0)
		*versionp = logc->p_version;
	return (ret);
}

/*
 * __logc_get_pp --
 *	DB_LOGC->get pre/post processing.
 */
static int
__logc_get_pp(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_THREAD_INFO *ip;
	int ret;

	dbenv = logc->dbenv;

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	switch (flags) {
	case DB_CURRENT:
	case DB_FIRST:
	case DB_LAST:
	case DB_NEXT:
	case DB_PREV:
		break;
	case DB_SET:
		if (IS_ZERO_LSN(*alsn)) {
			__db_errx(dbenv, "DB_LOGC->get: invalid LSN: %lu/%lu",
			    (u_long)alsn->file, (u_long)alsn->offset);
			return (EINVAL);
		}
		break;
	default:
		return (__db_ferr(dbenv, "DB_LOGC->get", 1));
	}

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv, (__logc_get(logc, alsn, dbt, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __logc_get --
 *	DB_LOGC->get.
 *
 * PUBLIC: int __logc_get __P((DB_LOGC *, DB_LSN *, DBT *, u_int32_t));
 */
int
__logc_get(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_LSN saved_lsn;
	LOGP *persist;
	int ret;

	dbenv = logc->dbenv;

	/*
	 * On error, we take care not to overwrite the caller's LSN.  This
	 * is because callers looking for the end of the log loop using the
	 * DB_NEXT flag, and expect to take the last successful lsn out of
	 * the passed-in structure after DB_LOGC->get fails with DB_NOTFOUND.
	 *
	 * !!!
	 * This line is often flagged an uninitialized memory read during a
	 * Purify or similar tool run, as the application didn't initialize
	 * *alsn.  If the application isn't setting the DB_SET flag, there is
	 * no reason it should have initialized *alsn, but we can't know that
	 * and we want to make sure we never overwrite whatever the application
	 * put in there.
	 */
	saved_lsn = *alsn;
	/*
	 * If we get one of the log's header records as a result of doing a
	 * DB_FIRST, DB_NEXT, DB_LAST or DB_PREV, repeat the operation, log
	 * file header records aren't useful to applications.
	 */
	if ((ret = __logc_get_int(logc, alsn, dbt, flags)) != 0) {
		*alsn = saved_lsn;
		return (ret);
	}
	if (alsn->offset == 0 && (flags == DB_FIRST ||
	    flags == DB_NEXT || flags == DB_LAST || flags == DB_PREV)) {
		switch (flags) {
		case DB_FIRST:
			flags = DB_NEXT;
			break;
		case DB_LAST:
			flags = DB_PREV;
			break;
		case DB_NEXT:
		case DB_PREV:
		default:
			break;
		}
		/*
		 * If we're walking the log and we find a persist header
		 * then store so that we may use it later if needed.
		 */
		persist = (LOGP *)dbt->data;
		logc->p_lsn = *alsn;
		logc->p_version = persist->version;
		if (F_ISSET(dbt, DB_DBT_MALLOC)) {
			__os_free(dbenv, dbt->data);
			dbt->data = NULL;
		}
		if ((ret = __logc_get_int(logc, alsn, dbt, flags)) != 0) {
			*alsn = saved_lsn;
			return (ret);
		}
	}

	return (0);
}

/*
 * __logc_get_int --
 *	Get a log record; internal version.
 */
static int
__logc_get_int(logc, alsn, dbt, flags)
	DB_LOGC *logc;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	DB_CIPHER *db_cipher;
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB_LSN last_lsn, nlsn;
	HDR hdr;
	LOG *lp;
	RLOCK rlock;
	logfile_validity status;
	u_int32_t cnt;
	u_int8_t *rp;
	int eof, is_hmac, need_cksum, ret;

	dbenv = logc->dbenv;
	db_cipher = dbenv->crypto_handle;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	is_hmac = 0;

	/*
	 * We don't acquire the log region lock until we need it, and we
	 * release it as soon as we're done.
	 */
	rlock = F_ISSET(logc, DB_LOG_LOCKED) ? L_ALREADY : L_NONE;

	nlsn = logc->lsn;
	switch (flags) {
	case DB_NEXT:				/* Next log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* Increment the cursor by the cursor record size. */
			nlsn.offset += logc->len;
			break;
		}
		flags = DB_FIRST;
		/* FALLTHROUGH */
	case DB_FIRST:				/* First log record. */
		/* Find the first log file. */
		if ((ret = __log_find(dblp, 1, &cnt, &status)) != 0)
			goto err;

		/*
		 * DB_LV_INCOMPLETE:
		 *	Theoretically, the log file we want could be created
		 *	but not yet written, the "first" log record must be
		 *	in the log buffer.
		 * DB_LV_NORMAL:
		 * DB_LV_OLD_READABLE:
		 *	We found a log file we can read.
		 * DB_LV_NONEXISTENT:
		 *	No log files exist, the "first" log record must be in
		 *	the log buffer.
		 * DB_LV_OLD_UNREADABLE:
		 *	No readable log files exist, we're at the cross-over
		 *	point between two versions.  The "first" log record
		 *	must be in the log buffer.
		 */
		switch (status) {
		case DB_LV_INCOMPLETE:
			DB_ASSERT(dbenv, lp->lsn.file == cnt);
			/* FALLTHROUGH */
		case DB_LV_NORMAL:
		case DB_LV_OLD_READABLE:
			nlsn.file = cnt;
			break;
		case DB_LV_NONEXISTENT:
			nlsn.file = 1;
			DB_ASSERT(dbenv, lp->lsn.file == nlsn.file);
			break;
		case DB_LV_OLD_UNREADABLE:
			nlsn.file = cnt + 1;
			DB_ASSERT(dbenv, lp->lsn.file == nlsn.file);
			break;
		}
		nlsn.offset = 0;
		break;
	case DB_CURRENT:			/* Current log record. */
		break;
	case DB_PREV:				/* Previous log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* If at start-of-file, move to the previous file. */
			if (nlsn.offset == 0) {
				if (nlsn.file == 1) {
					ret = DB_NOTFOUND;
					goto err;
				}
				if ((!lp->db_log_inmemory &&
				    (__log_valid(dblp, nlsn.file - 1, 0, NULL,
				    0, &status, NULL) != 0 ||
				    (status != DB_LV_NORMAL &&
				    status != DB_LV_OLD_READABLE)))) {
					ret = DB_NOTFOUND;
					goto err;
				}

				--nlsn.file;
			}
			nlsn.offset = logc->prev;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:				/* Last log record. */
		if (rlock == L_NONE) {
			rlock = L_ACQUIRED;
			LOG_SYSTEM_LOCK(dbenv);
		}
		nlsn.file = lp->lsn.file;
		nlsn.offset = lp->lsn.offset - lp->len;
		break;
	case DB_SET:				/* Set log record. */
		nlsn = *alsn;
		break;
	default:
		ret = __db_unknown_path(dbenv, "__logc_get_int");
		goto err;
	}

	if (0) {				/* Move to the next file. */
next_file:	++nlsn.file;
		nlsn.offset = 0;
	}

	/*
	 * The above switch statement should have set nlsn to the lsn of
	 * the requested record.
	 */

	if (CRYPTO_ON(dbenv)) {
		hdr.size = HDR_CRYPTO_SZ;
		is_hmac = 1;
	} else {
		hdr.size = HDR_NORMAL_SZ;
		is_hmac = 0;
	}

	/*
	 * Check to see if the record is in the cursor's buffer -- if so,
	 * we'll need to checksum it.
	 */
	if ((ret = __logc_incursor(logc, &nlsn, &hdr, &rp)) != 0)
		goto err;
	if (rp != NULL)
		goto cksum;

	/*
	 * Look to see if we're moving backward in the log with the last record
	 * coming from the disk -- it means the record can't be in the region's
	 * buffer.  Else, check the region's buffer.
	 *
	 * If the record isn't in the region's buffer, then either logs are
	 * in-memory, and we're done, or we're going to have to read the
	 * record from disk.  We want to make a point of not reading past the
	 * end of the logical log (after recovery, there may be data after the
	 * end of the logical log, not to mention the log file may have been
	 * pre-allocated).  So, zero out last_lsn, and initialize it inside
	 * __logc_inregion -- if it's still zero when we check it in
	 * __logc_ondisk, that's OK, it just means the logical end of the log
	 * isn't an issue for this request.
	 */
	ZERO_LSN(last_lsn);
	if (!F_ISSET(logc, DB_LOG_DISK) ||
	    LOG_COMPARE(&nlsn, &logc->lsn) > 0) {
		F_CLR(logc, DB_LOG_DISK);

		if ((ret = __logc_inregion(logc,
		    &nlsn, &rlock, &last_lsn, &hdr, &rp, &need_cksum)) != 0)
			goto err;
		if (rp != NULL) {
			/*
			 * If we read the entire record from the in-memory log
			 * buffer, we don't need to checksum it, nor do we need
			 * to worry about vtruncate issues.
			 */
			if (need_cksum)
				goto cksum;
			goto from_memory;
		}
		if (lp->db_log_inmemory)
			goto nohdr;
	}

	/*
	 * We have to read from an on-disk file to retrieve the record.
	 * If we ever can't retrieve the record at offset 0, we're done,
	 * return EOF/DB_NOTFOUND.
	 *
	 * Discard the region lock if we're still holding it, the on-disk
	 * reading routines don't need it.
	 */
	if (rlock == L_ACQUIRED) {
		rlock = L_NONE;
		LOG_SYSTEM_UNLOCK(dbenv);
	}
	if ((ret = __logc_ondisk(
	    logc, &nlsn, &last_lsn, flags, &hdr, &rp, &eof)) != 0)
		goto err;
	if (eof) {
		/*
		 * Only DB_NEXT automatically moves to the next file, and
		 * it only happens once.
		 */
		if (flags != DB_NEXT || nlsn.offset == 0)
			return (DB_NOTFOUND);
		goto next_file;
	}
	F_SET(logc, DB_LOG_DISK);

cksum:	/*
	 * Discard the region lock if we're still holding it.  (The path to
	 * get here is we acquired the region lock because of the caller's
	 * flag argument, but we found the record in the in-memory or cursor
	 * buffers.  Improbable, but it's easy to avoid.)
	 */
	if (rlock == L_ACQUIRED) {
		rlock = L_NONE;
		LOG_SYSTEM_UNLOCK(dbenv);
	}

	/*
	 * Checksum: there are two types of errors -- a configuration error
	 * or a checksum mismatch.  The former is always bad.  The latter is
	 * OK if we're searching for the end of the log, and very, very bad
	 * if we're reading random log records.
	 */
	if ((ret = __db_check_chksum(dbenv, &hdr, db_cipher,
	    hdr.chksum, rp + hdr.size, hdr.len - hdr.size, is_hmac)) != 0) {
		if (F_ISSET(logc, DB_LOG_SILENT_ERR)) {
			if (ret == 0 || ret == -1)
				ret = EIO;
		} else if (ret == -1) {
			__db_errx(dbenv,
		    "DB_LOGC->get: log record LSN %lu/%lu: checksum mismatch",
			    (u_long)nlsn.file, (u_long)nlsn.offset);
			__db_errx(dbenv,
		    "DB_LOGC->get: catastrophic recovery may be required");
			ret = __db_panic(dbenv, DB_RUNRECOVERY);
		}
		goto err;
	}

	/*
	 * If we got a 0-length record, that means we're in the midst of
	 * some bytes that got 0'd as the result of a vtruncate.  We're
	 * going to have to retry.
	 */
	if (hdr.len == 0) {
nohdr:		switch (flags) {
		case DB_FIRST:
		case DB_NEXT:
			/* Zero'd records always indicate the end of a file. */
			goto next_file;
		case DB_LAST:
		case DB_PREV:
			/*
			 * We should never get here.  If we recover a log
			 * file with 0's at the end, we'll treat the 0'd
			 * headers as the end of log and ignore them.  If
			 * we're reading backwards from another file, then
			 * the first record in that new file should have its
			 * prev field set correctly.
			 */
			__db_errx(dbenv,
		"Encountered zero length records while traversing backwards");
			ret = __db_panic(dbenv, DB_RUNRECOVERY);
			goto err;
		case DB_SET:
		default:
			/* Return the 0-length record. */
			break;
		}
	}

from_memory:
	/*
	 * Discard the region lock if we're still holding it.  (The path to
	 * get here is we acquired the region lock because of the caller's
	 * flag argument, but we found the record in the in-memory or cursor
	 * buffers.  Improbable, but it's easy to avoid.)
	 */
	if (rlock == L_ACQUIRED) {
		rlock = L_NONE;
		LOG_SYSTEM_UNLOCK(dbenv);
	}

	/* Copy the record into the user's DBT. */
	if ((ret = __db_retcopy(dbenv, dbt, rp + hdr.size,
	    (u_int32_t)(hdr.len - hdr.size),
	    &logc->dbt.data, &logc->dbt.ulen)) != 0)
		goto err;

	if (CRYPTO_ON(dbenv)) {
		if ((ret = db_cipher->decrypt(dbenv, db_cipher->data,
		    hdr.iv, dbt->data, hdr.len - hdr.size)) != 0) {
			ret = EAGAIN;
			goto err;
		}
		/*
		 * Return the original log record size to the user,
		 * even though we've allocated more than that, possibly.
		 * The log record is decrypted in the user dbt, not in
		 * the buffer, so we must do this here after decryption,
		 * not adjust the len passed to the __db_retcopy call.
		 */
		dbt->size = hdr.orig_size;
	}

	/* Update the cursor and the returned LSN. */
	*alsn = nlsn;
	logc->lsn = nlsn;
	logc->len = hdr.len;
	logc->prev = hdr.prev;

err:	if (rlock == L_ACQUIRED)
		LOG_SYSTEM_UNLOCK(dbenv);

	return (ret);
}

/*
 * __logc_incursor --
 *	Check to see if the requested record is in the cursor's buffer.
 */
static int
__logc_incursor(logc, lsn, hdr, pp)
	DB_LOGC *logc;
	DB_LSN *lsn;
	HDR *hdr;
	u_int8_t **pp;
{
	u_int8_t *p;
	int eof;

	*pp = NULL;

	/*
	 * Test to see if the requested LSN could be part of the cursor's
	 * buffer.
	 *
	 * The record must be part of the same file as the cursor's buffer.
	 * The record must start at a byte offset equal to or greater than
	 * the cursor buffer.
	 * The record must not start at a byte offset after the cursor
	 * buffer's end.
	 */
	if (logc->bp_lsn.file != lsn->file)
		return (0);
	if (logc->bp_lsn.offset > lsn->offset)
		return (0);
	if (logc->bp_lsn.offset + logc->bp_rlen <= lsn->offset + hdr->size)
		return (0);

	/*
	 * Read the record's header and check if the record is entirely held
	 * in the buffer.  If the record is not entirely held, get it again.
	 * (The only advantage in having part of the record locally is that
	 * we might avoid a system call because we already have the HDR in
	 * memory.)
	 *
	 * If the header check fails for any reason, it must be because the
	 * LSN is bogus.  Fail hard.
	 */
	p = logc->bp + (lsn->offset - logc->bp_lsn.offset);
	memcpy(hdr, p, hdr->size);
	if (__logc_hdrchk(logc, lsn, hdr, &eof))
		return (DB_NOTFOUND);
	if (eof || logc->bp_lsn.offset + logc->bp_rlen < lsn->offset + hdr->len)
		return (0);

	*pp = p;				/* Success. */

	return (0);
}

/*
 * __logc_inregion --
 *	Check to see if the requested record is in the region's buffer.
 */
static int
__logc_inregion(logc, lsn, rlockp, last_lsn, hdr, pp, need_cksump)
	DB_LOGC *logc;
	DB_LSN *lsn, *last_lsn;
	RLOCK *rlockp;
	HDR *hdr;
	u_int8_t **pp;
	int *need_cksump;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	LOG *lp;
	size_t b_region, len, nr;
	u_int32_t b_disk;
	int eof, ret;
	u_int8_t *p;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;
	lp = dbenv->lg_handle->reginfo.primary;

	ret = 0;
	b_region = 0;
	*pp = NULL;
	*need_cksump = 0;

	/* If we haven't yet acquired the log region lock, do so. */
	if (*rlockp == L_NONE) {
		*rlockp = L_ACQUIRED;
		LOG_SYSTEM_LOCK(dbenv);
	}

	/*
	 * The routines to read from disk must avoid reading past the logical
	 * end of the log, so pass that information back to it.
	 *
	 * Since they're reading directly from the disk, they must also avoid
	 * reading past the offset we've written out.  If the log was
	 * truncated, it's possible that there are zeroes or garbage on
	 * disk after this offset, and the logical end of the log can
	 * come later than this point if the log buffer isn't empty.
	 */
	*last_lsn = lp->lsn;
	if (!lp->db_log_inmemory && last_lsn->offset > lp->w_off)
		last_lsn->offset = lp->w_off;

	/*
	 * Test to see if the requested LSN could be part of the region's
	 * buffer.
	 *
	 * During recovery, we read the log files getting the information to
	 * initialize the region.  In that case, the region's lsn field will
	 * not yet have been filled in, use only the disk.
	 *
	 * The record must not start at a byte offset after the region buffer's
	 * end, since that means the request is for a record after the end of
	 * the log.  Do this test even if the region's buffer is empty -- after
	 * recovery, the log files may continue past the declared end-of-log,
	 * and the disk reading routine will incorrectly attempt to read the
	 * remainder of the log.
	 *
	 * Otherwise, test to see if the region's buffer actually has what we
	 * want:
	 *
	 * The buffer must have some useful content.
	 * The record must be in the same file as the region's buffer and must
	 * start at a byte offset equal to or greater than the region's buffer.
	 */
	if (IS_ZERO_LSN(lp->lsn))
		return (0);
	if (LOG_COMPARE(lsn, &lp->lsn) >= 0)
		return (DB_NOTFOUND);
	else if (lp->db_log_inmemory) {
		if ((ret = __log_inmem_lsnoff(dblp, lsn, &b_region)) != 0)
			return (ret);
	} else if (lp->b_off == 0 || LOG_COMPARE(lsn, &lp->f_lsn) < 0)
		return (0);

	/*
	 * The current contents of the cursor's buffer will be useless for a
	 * future call, we're about to overwrite it -- trash it rather than
	 * try and make it look correct.
	 */
	logc->bp_rlen = 0;

	/*
	 * If the requested LSN is greater than the region buffer's first
	 * byte, we know the entire record is in the buffer on a good LSN.
	 *
	 * If we're given a bad LSN, the "entire" record might not be in
	 * our buffer in order to fail at the chksum.  __logc_hdrchk made
	 * sure our dest buffer fits, via bp_maxrec, but we also need to
	 * make sure we don't run off the end of this buffer, the src.
	 *
	 * There is one case where the header check can fail: on a scan through
	 * in-memory logs, when we reach the end of a file we can read an empty
	 * header.  In that case, it's safe to return zero, here: it will be
	 * caught in our caller.  Otherwise, the LSN is bogus.  Fail hard.
	 */
	if (lp->db_log_inmemory || LOG_COMPARE(lsn, &lp->f_lsn) > 0) {
		if (!lp->db_log_inmemory)
			b_region = lsn->offset - lp->w_off;
		__log_inmem_copyout(dblp, b_region, hdr, hdr->size);
		if (__logc_hdrchk(logc, lsn, hdr, &eof) != 0)
			return (DB_NOTFOUND);
		if (eof)
			return (0);
		if (lp->db_log_inmemory) {
			if (RINGBUF_LEN(lp, b_region, lp->b_off) < hdr->len)
				return (DB_NOTFOUND);
		} else if (lsn->offset + hdr->len > lp->w_off + lp->buffer_size)
			return (DB_NOTFOUND);
		if (logc->bp_size <= hdr->len) {
			len = (size_t)DB_ALIGN((uintmax_t)hdr->len * 2, 128);
			if ((ret =
			    __os_realloc(logc->dbenv, len, &logc->bp)) != 0)
				 return (ret);
			logc->bp_size = (u_int32_t)len;
		}
		__log_inmem_copyout(dblp, b_region, logc->bp, hdr->len);
		*pp = logc->bp;
		return (0);
	}

	DB_ASSERT(dbenv, !lp->db_log_inmemory);

	/*
	 * There's a partial record, that is, the requested record starts
	 * in a log file and finishes in the region buffer.  We have to
	 * find out how many bytes of the record are in the region buffer
	 * so we can copy them out into the cursor buffer.  First, check
	 * to see if the requested record is the only record in the region
	 * buffer, in which case we should copy the entire region buffer.
	 *
	 * Else, walk back through the region's buffer to find the first LSN
	 * after the record that crosses the buffer boundary -- we can detect
	 * that LSN, because its "prev" field will reference the record we
	 * want.  The bytes we need to copy from the region buffer are the
	 * bytes up to the record we find.  The bytes we'll need to allocate
	 * to hold the log record are the bytes between the two offsets.
	 */
	b_disk = lp->w_off - lsn->offset;
	if (lp->b_off <= lp->len)
		b_region = (u_int32_t)lp->b_off;
	else
		for (p = dblp->bufp + (lp->b_off - lp->len);;) {
			memcpy(hdr, p, hdr->size);
			if (hdr->prev == lsn->offset) {
				b_region = (u_int32_t)(p - dblp->bufp);
				break;
			}
			p = dblp->bufp + (hdr->prev - lp->w_off);
		}

	/*
	 * If we don't have enough room for the record, we have to allocate
	 * space.  We have to do it while holding the region lock, which is
	 * truly annoying, but there's no way around it.  This call is why
	 * we allocate cursor buffer space when allocating the cursor instead
	 * of waiting.
	 */
	if (logc->bp_size <= b_region + b_disk) {
		len = (size_t)DB_ALIGN((uintmax_t)(b_region + b_disk) * 2, 128);
		if ((ret = __os_realloc(logc->dbenv, len, &logc->bp)) != 0)
			return (ret);
		logc->bp_size = (u_int32_t)len;
	}

	/* Copy the region's bytes to the end of the cursor's buffer. */
	p = (logc->bp + logc->bp_size) - b_region;
	memcpy(p, dblp->bufp, b_region);

	/* Release the region lock. */
	if (*rlockp == L_ACQUIRED) {
		*rlockp = L_NONE;
		LOG_SYSTEM_UNLOCK(dbenv);
	}

	/*
	 * Read the rest of the information from disk.  Neither short reads
	 * or EOF are acceptable, the bytes we want had better be there.
	 */
	if (b_disk != 0) {
		p -= b_disk;
		nr = b_disk;
		if ((ret = __logc_io(
		    logc, lsn->file, lsn->offset, p, &nr, NULL)) != 0)
			return (ret);
		if (nr < b_disk)
			return (__logc_shortread(logc, lsn, 0));

		/* We read bytes from the disk, we'll need to checksum them. */
		*need_cksump = 1;
	}

	/* Copy the header information into the caller's structure. */
	memcpy(hdr, p, hdr->size);

	*pp = p;
	return (0);
}

/*
 * __logc_ondisk --
 *	Read a record off disk.
 */
static int
__logc_ondisk(logc, lsn, last_lsn, flags, hdr, pp, eofp)
	DB_LOGC *logc;
	DB_LSN *lsn, *last_lsn;
	u_int32_t flags;
	int *eofp;
	HDR *hdr;
	u_int8_t **pp;
{
	DB_ENV *dbenv;
	size_t len, nr;
	u_int32_t offset;
	int ret;

	dbenv = logc->dbenv;
	*eofp = 0;

	nr = hdr->size;
	if ((ret =
	    __logc_io(logc, lsn->file, lsn->offset, hdr, &nr, eofp)) != 0)
		return (ret);
	if (*eofp)
		return (0);

	/*
	 * If the read was successful, but we can't read a full header, assume
	 * we've hit EOF.  We can't check that the header has been partially
	 * zeroed out, but it's unlikely that this is caused by a write failure
	 * since the header is written as a single write call and it's less
	 * than sector.
	 */
	if (nr < hdr->size) {
		*eofp = 1;
		return (0);
	}

	/* Check the HDR. */
	if ((ret = __logc_hdrchk(logc, lsn, hdr, eofp)) != 0)
		return (ret);
	if (*eofp)
		return (0);

	/*
	 * Regardless of how we return, the previous contents of the cursor's
	 * buffer are useless -- trash it.
	 */
	logc->bp_rlen = 0;

	/*
	 * Otherwise, we now (finally!) know how big the record is.  (Maybe
	 * we should have just stuck the length of the record into the LSN!?)
	 * Make sure we have enough space.
	 */
	if (logc->bp_size <= hdr->len) {
		len = (size_t)DB_ALIGN((uintmax_t)hdr->len * 2, 128);
		if ((ret = __os_realloc(dbenv, len, &logc->bp)) != 0)
			return (ret);
		logc->bp_size = (u_int32_t)len;
	}

	/*
	 * If we're moving forward in the log file, read this record in at the
	 * beginning of the buffer.  Otherwise, read this record in at the end
	 * of the buffer, making sure we don't try and read before the start
	 * of the file.  (We prefer positioning at the end because transaction
	 * aborts use DB_SET to move backward through the log and we might get
	 * lucky.)
	 *
	 * Read a buffer's worth, without reading past the logical EOF.  The
	 * last_lsn may be a zero LSN, but that's OK, the test works anyway.
	 */
	if (flags == DB_FIRST || flags == DB_NEXT)
		offset = lsn->offset;
	else if (lsn->offset + hdr->len < logc->bp_size)
		offset = 0;
	else
		offset = (lsn->offset + hdr->len) - logc->bp_size;

	nr = logc->bp_size;
	if (lsn->file == last_lsn->file && offset + nr >= last_lsn->offset)
		nr = last_lsn->offset - offset;

	if ((ret =
	    __logc_io(logc, lsn->file, offset, logc->bp, &nr, eofp)) != 0)
		return (ret);

	/*
	 * We should have at least gotten the bytes up-to-and-including the
	 * record we're reading.
	 */
	if (nr < (lsn->offset + hdr->len) - offset)
		return (__logc_shortread(logc, lsn, 1));

	/*
	 * Set up the return information.
	 *
	 * !!!
	 * No need to set the bp_lsn.file field, __logc_io set it for us.
	 */
	logc->bp_rlen = (u_int32_t)nr;
	logc->bp_lsn.offset = offset;

	*pp = logc->bp + (lsn->offset - offset);

	return (0);
}

/*
 * __logc_hdrchk --
 *
 * Check for corrupted HDRs before we use them to allocate memory or find
 * records.
 *
 * If the log files were pre-allocated, a zero-filled HDR structure is the
 * logical file end.  However, we can see buffers filled with 0's during
 * recovery, too (because multiple log buffers were written asynchronously,
 * and one made it to disk before a different one that logically precedes
 * it in the log file.
 *
 * Check for impossibly large records.  The malloc should fail later, but we
 * have customers that run mallocs that treat all allocation failures as fatal
 * errors.
 *
 * Note that none of this is necessarily something awful happening.  We let
 * the application hand us any LSN they want, and it could be a pointer into
 * the middle of a log record, there's no way to tell.
 */
static int
__logc_hdrchk(logc, lsn, hdr, eofp)
	DB_LOGC *logc;
	DB_LSN *lsn;
	HDR *hdr;
	int *eofp;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = logc->dbenv;

	/*
	 * Check EOF before we do any other processing.
	 */
	if (eofp != NULL) {
		if (hdr->prev == 0 && hdr->chksum[0] == 0 && hdr->len == 0) {
			*eofp = 1;
			return (0);
		}
		*eofp = 0;
	}

	/*
	 * Sanity check the log record's size.
	 * We must check it after "virtual" EOF above.
	 */
	if (hdr->len <= hdr->size)
		goto err;

	/*
	 * If the cursor's max-record value isn't yet set, it means we aren't
	 * reading these records from a log file and no check is necessary.
	 */
	if (logc->bp_maxrec != 0 && hdr->len > logc->bp_maxrec) {
		/*
		 * If we fail the check, there's the pathological case that
		 * we're reading the last file, it's growing, and our initial
		 * check information was wrong.  Get it again, to be sure.
		 */
		if ((ret = __logc_set_maxrec(logc, NULL)) != 0) {
			__db_err(dbenv, ret, "DB_LOGC->get");
			return (ret);
		}
		if (logc->bp_maxrec != 0 && hdr->len > logc->bp_maxrec)
			goto err;
	}
	return (0);

err:	if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
		__db_errx(dbenv,
		    "DB_LOGC->get: LSN %lu/%lu: invalid log record header",
		    (u_long)lsn->file, (u_long)lsn->offset);
	return (EIO);
}

/*
 * __logc_io --
 *	Read records from a log file.
 */
static int
__logc_io(logc, fnum, offset, p, nrp, eofp)
	DB_LOGC *logc;
	u_int32_t fnum, offset;
	void *p;
	size_t *nrp;
	int *eofp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	LOG *lp;
	int ret;
	char *np;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	/*
	 * If we've switched files, discard the current file handle and acquire
	 * a new one.
	 */
	if (logc->fhp != NULL && logc->bp_lsn.file != fnum) {
		ret = __os_closehandle(dbenv, logc->fhp);
		logc->fhp = NULL;
		logc->bp_lsn.file = 0;

		if (ret != 0)
			return (ret);
	}
	if (logc->fhp == NULL) {
		if ((ret = __log_name(dblp, fnum,
		    &np, &logc->fhp, DB_OSO_RDONLY | DB_OSO_SEQ)) != 0) {
			/*
			 * If we're allowed to return EOF, assume that's the
			 * problem, set the EOF status flag and return 0.
			 */
			if (eofp != NULL) {
				*eofp = 1;
				ret = 0;
			} else if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
				__db_err(dbenv, ret, "DB_LOGC->get: %s", np);
			__os_free(dbenv, np);
			return (ret);
		}

		if ((ret = __logc_set_maxrec(logc, np)) != 0) {
			__db_err(dbenv, ret, "DB_LOGC->get: %s", np);
			__os_free(dbenv, np);
			return (ret);
		}
		__os_free(dbenv, np);

		logc->bp_lsn.file = fnum;
	}

	STAT(++lp->stat.st_rcount);
	/* Seek to the record's offset and read the data. */
	if ((ret = __os_io(dbenv, DB_IO_READ,
	    logc->fhp, 0, 0, offset, (u_int32_t)*nrp, p, nrp)) != 0) {
		if (!F_ISSET(logc, DB_LOG_SILENT_ERR))
			__db_err(dbenv, ret,
			    "DB_LOGC->get: LSN: %lu/%lu: read",
			    (u_long)fnum, (u_long)offset);
		return (ret);
	}

	return (0);
}

/*
 * __logc_shortread --
 *	Read was short -- return a consistent error message and error.
 */
static int
__logc_shortread(logc, lsn, check_silent)
	DB_LOGC *logc;
	DB_LSN *lsn;
	int check_silent;
{
	if (!check_silent || !F_ISSET(logc, DB_LOG_SILENT_ERR))
		__db_errx(logc->dbenv, "DB_LOGC->get: LSN: %lu/%lu: short read",
		    (u_long)lsn->file, (u_long)lsn->offset);
	return (EIO);
}

/*
 * __logc_set_maxrec --
 *	Bound the maximum log record size in a log file.
 */
static int
__logc_set_maxrec(logc, np)
	DB_LOGC *logc;
	char *np;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	LOG *lp;
	u_int32_t mbytes, bytes;
	int ret;

	dbenv = logc->dbenv;
	dblp = dbenv->lg_handle;

	/*
	 * We don't want to try and allocate huge chunks of memory because
	 * applications with error-checking malloc's often consider that a
	 * hard failure.  If we're about to look at a corrupted record with
	 * a bizarre size, we need to know before trying to allocate space
	 * to hold it.  We could read the persistent data at the beginning
	 * of the file but that's hard -- we may have to decrypt it, checksum
	 * it and so on.  Stat the file instead.
	 */
	if (logc->fhp != NULL) {
		if ((ret = __os_ioinfo(dbenv, np, logc->fhp,
		    &mbytes, &bytes, NULL)) != 0)
			return (ret);
		if (logc->bp_maxrec < (mbytes * MEGABYTE + bytes))
			logc->bp_maxrec = mbytes * MEGABYTE + bytes;
	}

	/*
	 * If reading from the log file currently being written, we could get
	 * an incorrect size, that is, if the cursor was opened on the file
	 * when it had only a few hundred bytes, and then the cursor used to
	 * move forward in the file, after more log records were written, the
	 * original stat value would be wrong.  Use the maximum of the current
	 * log file size and the size of the buffer -- that should represent
	 * the max of any log record currently in the file.
	 *
	 * The log buffer size is set when the environment is opened and never
	 * changed, we don't need a lock on it.
	 */
	lp = dblp->reginfo.primary;
	if (logc->bp_maxrec < lp->buffer_size)
		logc->bp_maxrec = lp->buffer_size;

	return (0);
}

#ifdef HAVE_REPLICATION
/*
 * __log_rep_split --
 *	- Split a log buffer into individual records.
 *
 * This is used by a replication client to process a bulk log message from the
 * master and convert it into individual __rep_apply requests.
 *
 * PUBLIC: int __log_rep_split __P((DB_ENV *, REP_CONTROL *, DBT *, DB_LSN *,
 * PUBLIC:     DB_LSN *));
 */
int
__log_rep_split(dbenv, rp, rec, ret_lsnp, last_lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	DB_LSN *ret_lsnp;
	DB_LSN *last_lsnp;
{
	DB_LSN save_lsn, tmp_lsn;
	DBT logrec;
	REP_CONTROL tmprp;
	u_int32_t len;
	int is_dup, is_perm, ret, save_ret;
	u_int8_t *p, *ep;

	memset(&logrec, 0, sizeof(logrec));
	memset(&save_lsn, 0, sizeof(save_lsn));
	memset(&tmp_lsn, 0, sizeof(tmp_lsn));
	/*
	 * We're going to be modifying the rp LSN contents so make
	 * our own private copy to play with.
	 */
	memcpy(&tmprp, rp, sizeof(tmprp));
	/*
	 * We send the bulk buffer on a PERM record, so often we will have
	 * DB_LOG_PERM set.  However, we only want to mark the last LSN
	 * we have as a PERM record.  So clear it here, and when we're on
	 * the last record below, set it.
	 */
	is_perm = F_ISSET(rp, REPCTL_PERM);
	F_CLR(&tmprp, REPCTL_PERM);
	ret = save_ret = 0;
	for (ep = (u_int8_t *)rec->data + rec->size, p = (u_int8_t *)rec->data;
	    p < ep; ) {
		/*
		 * First thing in the buffer is the length.  Then the LSN
		 * of this record, then the record itself.
		 */
		/*
		 * XXX
		 * If/when we add architecture neutral log files we may want
		 * to send/receive these lengths in network byte order.
		 */
		memcpy(&len, p, sizeof(len));
		p += sizeof(len);
		memcpy(&tmprp.lsn, p, sizeof(DB_LSN));
		p += sizeof(DB_LSN);
		logrec.data = p;
		logrec.size = len;
		RPRINT(dbenv, (dbenv,
		    "log_rep_split: Processing LSN [%lu][%lu]",
		    (u_long)tmprp.lsn.file, (u_long)tmprp.lsn.offset));
		RPRINT(dbenv, (dbenv,
    "log_rep_split: p %#lx ep %#lx logrec data %#lx, size %lu (%#lx)",
		    P_TO_ULONG(p), P_TO_ULONG(ep), P_TO_ULONG(logrec.data),
		    (u_long)logrec.size, (u_long)logrec.size));
		is_dup = 0;
		p += len;
		if (p >= ep && is_perm)
			F_SET(&tmprp, REPCTL_PERM);
		ret = __rep_apply(dbenv,
		    &tmprp, &logrec, &tmp_lsn, &is_dup, last_lsnp);
		RPRINT(dbenv, (dbenv,
		    "log_split: rep_apply ret %d, tmp_lsn [%lu][%lu]",
		    ret, (u_long)tmp_lsn.file, (u_long)tmp_lsn.offset));
#if 0
		/*
		 * This buffer may be old and we've already gotten these
		 * records.  Short-circuit processing this buffer.
		 */
		if (is_dup)
			goto out;
#endif
		switch (ret) {
		/*
		 * If we received the pieces we need for running recovery,
		 * short-circuit because recovery will truncate the log to
		 * the LSN we want anyway.
		 */
		case DB_REP_LOGREADY:
			goto out;
		/*
		 * If we just handled a special record, retain that information.
		 */
		case DB_REP_ISPERM:
		case DB_REP_NOTPERM:
			save_ret = ret;
			save_lsn = tmp_lsn;
			ret = 0;
			break;
		/*
		 * Normal processing, do nothing, just continue.
		 */
		case 0:
			break;
		/*
		 * If we get an error, then stop immediately.
		 */
		default:
			goto out;
		}
	}
out:
	/*
	 * If we finish processing successfully, set our return values
	 * based on what we saw.
	 */
	if (ret == 0) {
		ret = save_ret;
		*ret_lsnp = save_lsn;
	}
	return (ret);
}
#endif
