/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2007 Oracle.  All rights reserved.
 *
 * $Id: rep_lease.c,v 12.5 2007/06/19 19:43:45 sue Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"

static void __rep_find_entry __P((DB_ENV *, REP *, int, REP_LEASE_ENTRY **));

/*
 * __rep_update_grant -
 *      Update a client's lease grant for this perm record
 *	and send the grant to the master.  Caller must
 *	hold the mtx_clientdb mutex.
 *
 * PUBLIC: int __rep_update_grant __P((DB_ENV *, db_timespec *));
 */
int
__rep_update_grant(dbenv, ts)
	DB_ENV *dbenv;
	db_timespec *ts;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	DBT lease_dbt;
	LOG *lp;
	REP *rep;
	REP_GRANT_INFO gi;
	db_timespec mytime;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	timespecclear(&mytime);

	/*
	 * Get current time, and add in the (skewed) lease duration
	 * time to send the grant to the master.
	 */
	__os_gettime(dbenv, &mytime);
	timespecadd(&mytime, &rep->lease_duration);
	REP_SYSTEM_LOCK(dbenv);
	/*
	 * If we are in an election, we cannot grant the lease.
	 * We need to check under the region mutex.
	 */
	if (IN_ELECTION(rep)) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}
	if (timespeccmp(&mytime, &rep->grant_expire, >))
		rep->grant_expire = mytime;
	REP_SYSTEM_UNLOCK(dbenv);

	/*
	 * Send the LEASE_GRANT message with the current lease grant
	 * no matter if we've actually extended the lease or not.
	 */
	memset(&gi, 0, sizeof(gi));
#ifdef	DIAGNOSTIC
	gi.expire_time = rep->grant_expire;
#endif
	gi.msg_time = *ts;

	DB_INIT_DBT(lease_dbt, &gi, sizeof(gi));
	(void)__rep_send_message(dbenv, rep->master_id, REP_LEASE_GRANT,
	    &lp->max_perm_lsn, &lease_dbt, 0, 0);
	return (0);
}

/*
 * __rep_islease_granted -
 *      Return 0 if this client has no outstanding lease granted.
 *	Return 1 otherwise.
 *	Caller must hold the REP_SYSTEM (region) mutex.
 *
 * PUBLIC: int __rep_islease_granted __P((DB_ENV *));
 */
int
__rep_islease_granted(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;
	db_timespec mytime;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	/*
	 * Get current time and compare against our granted lease.
	 */
	timespecclear(&mytime);
	__os_gettime(dbenv, &mytime);

	return (timespeccmp(&mytime, &rep->grant_expire, <=) ? 1 : 0);
}

/*
 * __rep_lease_table_alloc -
 *	Allocate the lease table on a master.  Called with rep mutex
 * held.  We need to acquire the env region mutex, so we need to
 * make sure we never acquire those mutexes in the opposite order.
 *
 * PUBLIC: int __rep_lease_table_alloc __P((DB_ENV *, int));
 */
int
__rep_lease_table_alloc(dbenv, nsites)
	DB_ENV *dbenv;
	int nsites;
{
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	REP_LEASE_ENTRY *le, *table;
	int i, *lease, ret;

	rep = dbenv->rep_handle->region;

	infop = dbenv->reginfo;
	renv = infop->primary;
	MUTEX_LOCK(dbenv, renv->mtx_regenv);
	if ((ret = __env_alloc(infop, (size_t)nsites * sizeof(REP_LEASE_ENTRY),
	    &lease)) == 0) {
		if (rep->lease_off != INVALID_ROFF)
			__env_alloc_free(infop,
			    R_ADDR(infop, rep->lease_off));
		rep->lease_off = R_OFFSET(infop, lease);
	}
	MUTEX_UNLOCK(dbenv, renv->mtx_regenv);
	table = R_ADDR(infop, rep->lease_off);
	for (i = 0; i < nsites; i++) {
		le = &table[i];
		le->eid = DB_EID_INVALID;
		timespecclear(&le->start_time);
		timespecclear(&le->end_time);
		ZERO_LSN(le->lease_lsn);
	}
	return (ret);
}

/*
 * __rep_lease_grant -
 *	Handle incoming REP_LEASE_GRANT message on a master.
 *
 * PUBLIC: int __rep_lease_grant __P((DB_ENV *, REP_CONTROL *, DBT *, int));
 */
int
__rep_lease_grant(dbenv, rp, rec, eid)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	int eid;
{
	DB_REP *db_rep;
	REP *rep;
	REP_GRANT_INFO *gi;
	REP_LEASE_ENTRY *le;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	gi = (REP_GRANT_INFO *)rec->data;
	le = NULL;

	/*
	 * Get current time, and add in the (skewed) lease duration
	 * time to send the grant to the master.
	 */
	REP_SYSTEM_LOCK(dbenv);
	__rep_find_entry(dbenv, rep, eid, &le);
	/*
	 * We either get back this site's entry, or an empty entry
	 * that we need to initialize.
	 */
	DB_ASSERT(dbenv, le != NULL);
	/*
	 * Update the entry if it is an empty entry or if the new
	 * lease grant is a later start time than the current one.
	 */
	RPRINT(dbenv, (dbenv, "lease_grant: gi msg_time %lu %lu",
	    (u_long)gi->msg_time.tv_sec, (u_long)gi->msg_time.tv_nsec));
	if (le->eid == DB_EID_INVALID ||
	    timespeccmp(&gi->msg_time, &le->start_time, >)) {
		le->eid = eid;
		le->start_time = gi->msg_time;
		le->end_time = le->start_time;
		timespecadd(&le->end_time, &rep->lease_duration);
		RPRINT(dbenv, (dbenv,
    "lease_grant: eid %d, start %lu %lu, end %lu %lu, duration %lu %lu",
    le->eid, (u_long)le->start_time.tv_sec, (u_long)le->start_time.tv_nsec,
    (u_long)le->end_time.tv_sec, (u_long)le->end_time.tv_nsec,
    (u_long)rep->lease_duration.tv_sec, (u_long)rep->lease_duration.tv_nsec));
		/*
		 * XXX Is this really true?  Could we have a lagging
		 * record that has a later start time, but smaller
		 * LSN than we have previously seen??
		 */
		DB_ASSERT(dbenv, LOG_COMPARE(&rp->lsn, &le->lease_lsn) >= 0);
		le->lease_lsn = rp->lsn;
	}
	REP_SYSTEM_UNLOCK(dbenv);
	return (0);
}

/*
 * Find the entry for the given EID.  Or the first empty one.
 */
static void
__rep_find_entry(dbenv, rep, eid, lep)
	DB_ENV *dbenv;
	REP *rep;
	int eid;
	REP_LEASE_ENTRY **lep;
{
	REGINFO *infop;
	REP_LEASE_ENTRY *le, *table;
	int i;

	infop = dbenv->reginfo;
	table = R_ADDR(infop, rep->lease_off);

	for (i = 0; i < rep->nsites; i++) {
		le = &table[i];
		/*
		 * Find either the one that matches the client's
		 * EID or the first empty one.
		 */
		if (le->eid == eid || le->eid == DB_EID_INVALID) {
			*lep = le;
			return;
		}
	}
	return;
}

/*
 * __rep_lease_check -
 *      Return 0 if this master holds valid leases and can confirm
 *	its mastership.  If leases are expired, an attempt is made
 *	to refresh the leases.  If that fails, then return the
 *	DB_REP_LEASE_EXPIRED error to the user.  No mutexes held.
 *
 * PUBLIC: int __rep_lease_check __P((DB_ENV *, int));
 */
int
__rep_lease_check(dbenv, refresh)
	DB_ENV *dbenv;
	int refresh;
{
	DB_LOG *dblp;
	DB_LSN lease_lsn;
	DB_REP *db_rep;
	LOG *lp;
	REGINFO *infop;
	REP *rep;
	REP_LEASE_ENTRY *le, *table;
	db_timespec curtime;
	int i, min_leases, ret, tries, valid_leases;

	infop = dbenv->reginfo;
	tries = 0;
retry:
	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	LOG_SYSTEM_LOCK(dbenv);
	lease_lsn = lp->max_perm_lsn;
	LOG_SYSTEM_UNLOCK(dbenv);
	REP_SYSTEM_LOCK(dbenv);
	min_leases = rep->nsites / 2;

	__os_gettime(dbenv, &curtime);

	RPRINT(dbenv, (dbenv, "lease_check: min_leases %d curtime %lu %lu",
	    min_leases, (u_long)curtime.tv_sec, (u_long)curtime.tv_nsec));
	table = R_ADDR(infop, rep->lease_off);
	for (i = 0, valid_leases = 0;
	    i < rep->nsites && valid_leases < min_leases; i++) {
		le = &table[i];
		/*
		 * Count this lease as valid if:
		 * - It is a valid entry (has an EID).
		 * - The lease has not expired.
		 * - The LSN is up to date.
		 */
		if (le->eid != DB_EID_INVALID) {
			RPRINT(dbenv, (dbenv,
		    "lease_check: valid %d eid %d, lease_lsn [%lu][%lu]",
			    valid_leases, le->eid, (u_long)le->lease_lsn.file,
			    (u_long)le->lease_lsn.offset));
			RPRINT(dbenv, (dbenv, "lease_check: endtime %lu %lu",
			    (u_long)le->end_time.tv_sec,
			    (u_long)le->end_time.tv_nsec));
		}
		if (le->eid != DB_EID_INVALID &&
		    timespeccmp(&le->end_time, &curtime, >=) &&
		    LOG_COMPARE(&le->lease_lsn, &lease_lsn) == 0)
			valid_leases++;
	}
	REP_SYSTEM_UNLOCK(dbenv);

	/*
	 * Now see if we have enough.
	 */
	RPRINT(dbenv, (dbenv, "valid %d, min %d", valid_leases, min_leases));
	if (valid_leases < min_leases) {
		if (!refresh)
			ret = DB_REP_LEASE_EXPIRED;
		else {
			/*
			 * If we are successful, we need to recheck the leases
			 * because the lease grant messages may have raced with
			 * the PERM acknowledgement.  Give the grant messages
			 * a chance to arrive and be processed.
			 */
			if ((ret = __rep_lease_refresh(dbenv)) == 0) {
				if (tries <= LEASE_REFRESH_TRIES) {
					/*
					 * If we were successful sending, but
					 * not in racing the message threads,
					 * then yield the processor so that
					 * the message threads get a chance
					 * to run.
					 */
					if (tries > 0)
						__os_sleep(dbenv, 1, 0);
					tries++;
					goto retry;
				} else
					ret = DB_REP_LEASE_EXPIRED;
			}
		}
	}

	return (ret);
}

/*
 * __rep_lease_refresh -
 *	Find the last permanent record and send that out so that it
 *	forces clients to grant their leases.
 *
 * PUBLIC: int __rep_lease_refresh __P((DB_ENV *));
 */
int
__rep_lease_refresh(dbenv)
	DB_ENV *dbenv;
{
	DBT rec;
	DB_LOGC *logc;
	DB_LSN lsn;
	DB_REP *db_rep;
	REP *rep;
	int ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	memset(&rec, 0, sizeof(rec));
	memset(&lsn, 0, sizeof(lsn));
	/*
	 * Use __rep_log_backup to find the last PERM record.
	 */
	if ((ret = __rep_log_backup(dbenv, rep, logc, &lsn)) != 0)
		goto err;

	if ((ret = __logc_get(logc, &lsn, &rec, DB_CURRENT)) != 0)
		goto err;

	if ((ret = __rep_send_message(dbenv,
	    DB_EID_BROADCAST, REP_LOG, &lsn, &rec, REPCTL_PERM, 0)) != 0) {
		/*
		 * If we do not get an ack, we expire leases.
		 */
		(void)__rep_lease_expire(dbenv, 0);
		ret = DB_REP_LEASE_EXPIRED;
	}

err:	if ((t_ret = __logc_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_lease_expire -
 *	Proactively expire all leases granted to us.
 *
 * PUBLIC: int __rep_lease_expire __P((DB_ENV *, int));
 */
int
__rep_lease_expire(dbenv, locked)
	DB_ENV *dbenv;
	int locked;
{
	DB_REP *db_rep;
	REGINFO *infop;
	REP *rep;
	REP_LEASE_ENTRY *le, *table;
	int i, ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	infop = dbenv->reginfo;

	if (!locked)
		REP_SYSTEM_LOCK(dbenv);
	if (rep->lease_off != INVALID_ROFF) {
		table = R_ADDR(infop, rep->lease_off);
		/*
		 * Expire all leases forcibly.  We are guaranteed that the
		 * start_time for all leases are not in the future.  Therefore,
		 * set the end_time to the start_time.
		 */
		for (i = 0; i < rep->nsites; i++) {
			le = &table[i];
			le->end_time = le->start_time;
		}
	}
	if (!locked)
		REP_SYSTEM_UNLOCK(dbenv);
	return (ret);
}

/*
 * __rep_lease_waittime -
 *	Return the amount of time remaining on a granted lease.
 * Assume the caller holds the REP_SYSTEM (region) mutex.
 *
 * PUBLIC: db_timeout_t __rep_lease_waittime __P((DB_ENV *));
 */
db_timeout_t
__rep_lease_waittime(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;
	db_timespec exptime, mytime;
	db_timeout_t to;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	exptime = rep->grant_expire;
	to = 0;
	/*
	 * If the lease has never been granted, we must wait a full
	 * lease timeout because we could be freshly rebooted after
	 * a crash and a lease could be granted from a previous
	 * incarnation of this client.
	 */
	RPRINT(dbenv, (dbenv,
    "wait_time: grant_expire %lu %lu lease_to %lu",
	    (u_long)exptime.tv_sec, (u_long)exptime.tv_nsec,
	    (u_long)rep->lease_timeout));
	if (!timespecisset(&exptime))
		to = rep->lease_timeout;
	else {
		__os_gettime(dbenv, &mytime);
		RPRINT(dbenv, (dbenv,
    "wait_time: mytime %lu %lu, grant_expire %lu %lu",
		    (u_long)mytime.tv_sec, (u_long)mytime.tv_nsec,
		    (u_long)exptime.tv_sec, (u_long)exptime.tv_nsec));
		if (timespeccmp(&mytime, &exptime, <=)) {
			/*
			 * If the current time is before the grant expiration
			 * compute the difference and return remaining grant
			 * time.
			 */
			timespecsub(&exptime, &mytime);
			DB_TIMESPEC_TO_TIMEOUT(to, &exptime);
		}
	}
	return (to);
}
