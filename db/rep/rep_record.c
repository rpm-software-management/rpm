/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: rep_record.c,v 1.64 2001/11/16 16:29:10 bostic Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#endif

#include "db_int.h"
#include "log.h"
#include "txn.h"
#include "rep.h"
#include "db_page.h"
#include "db_am.h"
#include "db_shash.h"
#include "lock.h"

static int __rep_apply __P((DB_ENV *, REP_CONTROL *, DBT *));
static int __rep_bt_cmp __P((DB *, const DBT *, const DBT *));
static int __rep_process_txn __P((DB_ENV *, DBT *));

#define	IS_SIMPLE(R)	\
	((R) != DB_txn_regop && (R) != DB_txn_ckp && (R) != DB_log_register)

/*
 * This is a bit of a hack.  If we set the offset to be the sizeof the
 * persistent log structure, then we'll match the correct LSN on the
 * next log write.
 *
 * If lp->ready_lsn is [1][0], we need to "change" to the first log
 * file (we currently have none).  However, in this situation, we
 * don't want to wind up at LSN [2][whatever], we want to wind up at
 * LSN [1][whatever], so don't set LOG_NEWFILE.  The guts of the log
 * system will take care of actually writing the persistent header,
 * since we're doing a log_put to an empty log.
 *
 * If lp->ready_lsn is [m-1][n] for some m > 1, n > 0, we really do need to
 * change to the first log file.  Not only do we need to jump to lsn
 * [m][0], we need to write out a persistent header there, so set
 * LOG_NEWFILE so the right stuff happens in the bowels of log_put.
 * Note that we could dispense with LOG_NEWFILE by simply relying upon
 * the log system to decide to switch files at the same time the
 * master did--lg_max should be the same in both places--but this is
 * scary.
 */
#define	CHANGE_FILES do {						\
	if (!(lp->ready_lsn.file == 1 && lp->ready_lsn.offset == 0)) {	\
		lp->ready_lsn.file++;					\
		F_SET(lp, LOG_NEWFILE);					\
	}								\
	lp->ready_lsn.offset = sizeof(struct __log_persist) +		\
	    sizeof(struct __hdr);					\
	/* Make this evaluate to a simple rectype. */			\
	rectype = 0;							\
} while (0)

/*
 * __rep_process_message --
 *
 * This routine takes an incoming message and processes it.
 *
 * control: contains the control fields from the record
 * rec: contains the actual record
 * eidp: contains the machine id of the sender of the message;
 *	in the case of a DB_NEWMASTER message, returns the eid
 *	of the new master.
 *
 * PUBLIC: int __rep_process_message __P((DB_ENV *, DBT *, DBT *, int *));
 */
int
__rep_process_message(dbenv, control, rec, eidp)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int *eidp;
{
	DBT *d, data_dbt, lsndbt, mylog;
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN lsn, newfilelsn, oldfilelsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	REP_CONTROL *rp;
	REP_VOTE_INFO *vi;
	u_int32_t gen, type;
	int done, i, master, old, recovering, ret, t_ret, *tally;

	PANIC_CHECK(dbenv);

	/* Control argument must be non-Null. */
	if (control == NULL || control->size == 0) {
		__db_err(dbenv,
	"DB_ENV->rep_process_message: control argument must be specified");
		return (EINVAL);
	}

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
	gen = rep->gen;
	recovering = F_ISSET(rep, REP_F_RECOVER);
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);

	/*
	 * dbenv->rep_db is the handle for the repository used for applying log
	 * records.
	 */
	rp = (REP_CONTROL *)control->data;

	/* Complain if we see an improper version number. */
	if (rp->rep_version != DB_REPVERSION) {
		__db_err(dbenv,
		    "unexpected replication message version %d, expected %d",
		    rp->rep_version, DB_REPVERSION);
		return (EINVAL);
	}
	if (rp->log_version != DB_LOGVERSION) {
		__db_err(dbenv,
		    "unexpected log record version %d, expected %d",
		    rp->log_version, DB_LOGVERSION);
		return (EINVAL);
	}

	/*
	 * Check for generation number matching.  Ignore any old messages
	 * except requests for ALIVE since the sender needs those to
	 * sync back up.  If the message is newer, then we are out of
	 * sync and need to catch up with the rest of the system.
	 */
	if (rp->gen < gen && rp->rectype != REP_ALIVE_REQ &&
	    rp->rectype != REP_NEWCLIENT)
		return (0);
	if (rp->gen > gen && rp->rectype != REP_ALIVE &&
	    rp->rectype != REP_NEWMASTER)
		return (__rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0));

	/*
	 * We need to check if we're in recovery and if we are
	 * then we need to ignore any messages except VERIFY, VOTE,
	 * ELECT (the master might fail while we are recovering), and
	 * ALIVE_REQ.
	 */
	if (recovering)
		switch(rp->rectype) {
			case REP_ALIVE:
			case REP_ALIVE_REQ:
			case REP_ELECT:
			case REP_NEWCLIENT:
			case REP_NEWMASTER:
			case REP_NEWSITE:
			case REP_VERIFY:
			case REP_VOTE1:
			case REP_VOTE2:
				break;
			default:
				return (0);
		}

	switch(rp->rectype) {
	case REP_ALIVE:
		ANYSITE(dbenv);
		if (rp->gen > gen && rp->flags)
			return (__rep_new_master(dbenv, rp, *eidp));
		break;
	case REP_ALIVE_REQ:
		ANYSITE(dbenv);
		dblp = dbenv->lg_handle;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (__rep_send_message(dbenv,
		    *eidp, REP_ALIVE, &lsn, NULL,
		    F_ISSET(dbenv, DB_ENV_REP_MASTER) ? 1 : 0));
	case REP_ALL_REQ:
		MASTER_ONLY(dbenv);
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			goto err;
		memset(&data_dbt, 0, sizeof(data_dbt));
		oldfilelsn = lsn = rp->lsn;
		for (ret = logc->get(logc, &rp->lsn, &data_dbt, DB_SET);
		    ret == 0;
		    ret = logc->get(logc, &lsn, &data_dbt, DB_NEXT)) {
			/*
			 * lsn.offset will only be 0 if this is the
			 * beginning of the log;  DB_SET, but not DB_NEXT,
			 * can set the log cursor to [n][0].
			 */
			if (lsn.offset == 0)
				ret = __rep_send_message(dbenv, *eidp,
				    REP_NEWFILE, &lsn, NULL, 0);
			else {
				/*
				 * DB_NEXT will never run into offsets
				 * of 0;  thus, when a log file changes,
				 * we'll have a real log record with
				 * some lsn [n][m], and we'll also want to send
				 * a NEWFILE message with lsn [n][0].
				 * So that the client can detect gaps,
				 * send in the rec parameter the
				 * last LSN in the old file.
				 */
				if (lsn.file != oldfilelsn.file) {
					newfilelsn.file = lsn.file;
					newfilelsn.offset = 0;

					memset(&lsndbt, 0, sizeof(DBT));
					lsndbt.size = sizeof(DB_LSN);
					lsndbt.data = &oldfilelsn;

					if ((ret = __rep_send_message(dbenv,
					    *eidp, REP_NEWFILE, &newfilelsn,
					    &lsndbt, 0)) != 0)
						break;
				}
				ret = __rep_send_message(dbenv, *eidp,
				    REP_LOG, &lsn, &data_dbt, 0);
			}

			/*
			 * In case we're about to change files and need it
			 * for a NEWFILE message, save the current LSN.
			 */
			oldfilelsn = lsn;
		}

		if (ret == DB_NOTFOUND)
			ret = 0;
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_ELECT:
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}
		MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
		ret = IN_ELECTION(rep) ? 0 : DB_REP_HOLDELECTION;
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		return (ret);
#ifdef NOTYET
	case REP_FILE: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_FILE_REQ:
		MASTER_ONLY(dbenv);
		return (__rep_send_file(dbenv, rec, *eidp));
		break;
#endif
	case REP_LOG:
		CLIENT_ONLY(dbenv);
		return (__rep_apply(dbenv, rp, rec));
	case REP_LOG_REQ:
		MASTER_ONLY(dbenv);
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			goto err;
		memset(&data_dbt, 0, sizeof(data_dbt));
		lsn = rp->lsn;
		if ((ret = logc->get(logc, &rp->lsn, &data_dbt, DB_SET)) == 0) {
			/*
			 * If the log file has changed, we may get back a
			 * log record with a later LSN than we requested.
			 * This most likely means that the log file
			 * changed, so we need to send a NEWFILE message.
			 */
			if (log_compare(&lsn, &rp->lsn) < 0 &&
			    rp->lsn.offset == 0)
				ret = __rep_send_message(dbenv, *eidp,
				    REP_NEWFILE, &lsn, NULL, 0);
			else
				ret = __rep_send_message(dbenv, *eidp,
				    REP_LOG, &rp->lsn, &data_dbt, 0);
		}
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_NEWSITE:
		/* This is a rebroadcast; simply tell the application. */
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			dblp = dbenv->lg_handle;
			lp = dblp->reginfo.primary;
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
		}
		return (DB_REP_NEWSITE);
	case REP_NEWCLIENT:
		/*
		 * This message was received and should have resulted in the
		 * application entering the machine ID in its machine table.
		 * We respond to this with an ALIVE to send relevant information
		 * to the new client.  But first, broadcast the new client's
		 * record to all the clients.
		 */
		if ((ret = __rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_NEWSITE, &rp->lsn, rec, 0)) != 0)
			goto err;

		if (F_ISSET(dbenv, DB_ENV_REP_CLIENT))
			return (0);

		 /* FALLTHROUGH */
	case REP_MASTER_REQ:
		MASTER_ONLY(dbenv);
		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = lp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (__rep_send_message(dbenv,
		    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
	case REP_NEWFILE:
		CLIENT_ONLY(dbenv);
		return (__rep_apply(dbenv, rp, rec));
	case REP_NEWMASTER:
		ANYSITE(dbenv);
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER) &&
		    *eidp != dbenv->rep_eid)
			return (DB_REP_DUPMASTER);
		return (__rep_new_master(dbenv, rp, *eidp));
	case REP_PAGE: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_PAGE_REQ: /* TODO */
		MASTER_ONLY(dbenv);
		break;
	case REP_PLIST: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_PLIST_REQ: /* TODO */
		MASTER_ONLY(dbenv);
		break;
	case REP_VERIFY:
		CLIENT_ONLY(dbenv);
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			goto err;
		memset(&mylog, 0, sizeof(mylog));
		if ((ret = logc->get(logc, &rp->lsn, &mylog, DB_SET)) != 0)
			goto rep_verify_err;
		db_rep = dbenv->rep_handle;
		rep = db_rep->region;
		if (mylog.size == rec->size &&
		    memcmp(mylog.data, rec->data, rec->size) == 0) {
			ret = __db_apprec(dbenv, &rp->lsn, 0);
			MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
			F_CLR(rep, REP_F_RECOVER);
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			ret = __rep_send_message(dbenv, rep->master_id,
			    REP_ALL_REQ, &rp->lsn, NULL, 0);
		} else if ((ret = logc->get(logc, &lsn, &mylog, DB_PREV)) == 0)
			ret = __rep_send_message(dbenv,
			    *eidp, REP_VERIFY_REQ, &lsn, NULL, 0);
rep_verify_err:	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		goto err;
	case REP_VERIFY_FAIL:
		return (DB_REP_OUTDATED);
	case REP_VERIFY_REQ:
		MASTER_ONLY(dbenv);
		type = REP_VERIFY;
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			goto err;
		d = &data_dbt;
		memset(d, 0, sizeof(data_dbt));
		ret = logc->get(logc, &rp->lsn, d, DB_SET);
		/*
		 * If the LSN was invalid, then we might get a not
		 * found, we might get an EIO, we could get anything.
		 * If we get a DB_NOTFOUND, then there is a chance that
		 * the LSN comes before the first file present in which
		 * case we need to return a fail so that the client can return
		 * a DB_OUTDATED.
		 */
		if (ret == DB_NOTFOUND &&
		    __log_is_outdated(dbenv, rp->lsn.file, &old) == 0 &&
		    old != 0)
			type = REP_VERIFY_FAIL;

		if (ret != 0)
			d = NULL;

		ret = __rep_send_message(dbenv, *eidp, type, &rp->lsn, d, 0);
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		goto err;
	case REP_VOTE1:
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Master received vote");
#endif
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}

		vi = (REP_VOTE_INFO *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);

		/*
		 * If you get a vote and you're not in an election, simply
		 * return an indicator to hold an election which will trigger
		 * this site to send its vote again.
		 */
		if (!IN_ELECTION(rep)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
				    "Not in election, but received vote1");
#endif
			ret = DB_REP_HOLDELECTION;
			goto unlock;
		}

		if (F_ISSET(rep, REP_F_EPHASE2))
			goto unlock;

		/* Check if this site knows about more sites than we do. */
		if (vi->nsites > rep->nsites)
			rep->nsites = vi->nsites;

		/* Check if we've heard from this site already. */
		tally = R_ADDR((REGINFO *)dbenv->reginfo, rep->tally_off);
		for (i = 0; i < rep->sites; i++) {
			if (tally[i] == *eidp)
				/* Duplicate vote. */
				goto unlock;
		}

		/*
		 * We are keeping vote, let's see if that changes our count of
		 * the number of sites.
		 */
		if (rep->sites + 1 > rep->nsites)
			rep->nsites = rep->sites + 1;
		if (rep->nsites > rep->asites &&
		    (ret = __rep_grow_sites(dbenv, rep->nsites)) != 0)
				goto unlock;

		tally[rep->sites] = *eidp;
		rep->sites++;

		/*
		 * Change winners if the incoming record has a higher
		 * priority, or an equal priority but a larger LSN.
		 */
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
			__db_err(dbenv,
			    "Existing vote: (eid)%d (pri)%d (gen)%d [%d,%d]",
			    rep->winner, rep->w_priority, rep->w_gen,
			    rep->w_lsn.file, rep->w_lsn.offset);
			__db_err(dbenv,
			    "Incoming vote: (eid)%d (pri)%d (gen)%d [%d,%d]",
			    *eidp, vi->priority, rp->gen, rp->lsn.file,
			    rp->lsn.offset);
		}
#endif
		if (vi->priority > rep->w_priority ||
		    (vi->priority != 0 && vi->priority == rep->w_priority &&
		    log_compare(&rp->lsn, &rep->w_lsn) > 0)) {
#ifdef DIABNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Accepting new vote");
#endif
			rep->winner = *eidp;
			rep->w_priority = vi->priority;
			rep->w_lsn = rp->lsn;
			rep->w_gen = rp->gen;
		}
		master = rep->winner;
		lsn = rep->w_lsn;
		done = rep->sites == rep->nsites && rep->w_priority != 0;
		if (done) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
				__db_err(dbenv, "Phase1 election done");
				__db_err(dbenv, "Voting for %d%s",
				    master, master == rep->eid ? "(self)" : "");
			}
#endif
			F_CLR(rep, REP_F_EPHASE1);
			F_SET(rep, REP_F_EPHASE2);
		}

		if (done && master == rep->eid) {
			rep->votes++;
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (0);
		}
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);

		/* Vote for someone else. */
		if (done)
			return (__rep_send_message(dbenv,
			    master, REP_VOTE2, NULL, NULL, 0));

		/* Election is still going on. */
		break;
	case REP_VOTE2:
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
			__db_err(dbenv, "We received a vote%s",
			    F_ISSET(dbenv, DB_ENV_REP_MASTER) ?
			    " (master)" : "");
#endif
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}

		MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);

		/* If we have priority 0, we should never get a vote. */
		DB_ASSERT(rep->priority != 0);

		if (!IN_ELECTION(rep) && rep->master_id != DB_EID_INVALID) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Not in election, got vote");
#endif
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (DB_REP_HOLDELECTION);
		}
		/* avoid counting duplicates. */
		rep->votes++;
		done = rep->votes > rep->nsites / 2;
		if (done) {
			rep->master_id = rep->eid;
			rep->gen = rep->w_gen + 1;
			ELECTION_DONE(rep);
			F_CLR(rep, REP_F_UPGRADE);
			F_SET(rep, REP_F_MASTER);
			*eidp = rep->master_id;
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			"Got enough votes to win; election done; winner is %d",
				    rep->master_id);
#endif
		}
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		if (done) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);

			/* Declare me the winner. */
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "I won, sending NEWMASTER");
#endif
			if ((ret = __rep_send_message(dbenv, DB_EID_BROADCAST,
			    REP_NEWMASTER, &lsn, NULL, 0)) != 0)
				break;
			return (DB_REP_NEWMASTER);
		}
		break;
	default:
		__db_err(dbenv,
	"DB_ENV->rep_process_message: unknown replication message: type %lu",
		   (u_long)rp->rectype);
		return (EINVAL);
	}

	return (0);

unlock:	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
err:	return (ret);
}

/*
 * __rep_apply --
 *
 * Handle incoming log records on a client, applying when possible and
 * entering into the bookkeeping table otherwise.  This is the guts of
 * the routine that handles the state machine that describes how we
 * process and manage incoming log records.
 */
static int
__rep_apply(dbenv, rp, rec)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
{
	__txn_ckp_args ckp_args;
	DB_REP *db_rep;
	DBT data_dbt, key_dbt;
	DB *dbp;
	DBC *dbc;
	DB_LOG *dblp;
	DB_LSN ckp_lsn, lsn, next_lsn;
	LOG *lp;
	int cmp, eid, ret, retry_count, t_ret;
	u_int32_t rectype;

	db_rep = dbenv->rep_handle;
	dbp = db_rep->rep_db;
	dbc = NULL;
	ret = 0;
	retry_count = 0;
	memset(&key_dbt, 0, sizeof(key_dbt));
	memset(&data_dbt, 0, sizeof(data_dbt));

	/*
	 * If this is a log record and it's the next one in line, simply
	 * write it to the log.  If it's a "normal" log record, i.e., not
	 * a COMMIT or CHECKPOINT or something that needs immediate processing,
	 * just return.  If it's a COMMIT, CHECKPOINT or LOG_REGISTER (i.e.,
	 * not SIMPLE), handle it now.  If it's a NEWFILE record, then we
	 * have to be prepared to deal with a logfile change.
	 */
	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	lp = dblp->reginfo.primary;
	cmp = log_compare(&rp->lsn, &lp->ready_lsn);

	/*
	 * This is written to assume that you don't end up with a lot of
	 * records after a hole.  That is, it optimizes for the case where
	 * there is only a record or two after a hole.  If you have a lot
	 * of records after a hole, what you'd really want to do is write
	 * all of them and then process all the commits, checkpoints, etc.
	 * together.  That is more complicated processing that we can add
	 * later if necessary.
	 *
	 * That said, I really don't want to do db operations holding the
	 * log mutex, so the synchronization here is tricky.
	 */
	if (cmp == 0) {
		if (rp->rectype == REP_NEWFILE) {
newfile:		CHANGE_FILES;
		} else {
			ret = __log_put_int(dbenv, &rp->lsn, rec, rp->flags);
			lp->ready_lsn = lp->lsn;
			memcpy(&rectype, rec->data, sizeof(rectype));
		}
		while (ret == 0 && IS_SIMPLE(rectype) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0) {
			/*
			 * We just filled in a gap in the log record stream.
			 * Write subsequent records to the log.
			 */
gap_check:		R_UNLOCK(dbenv, &dblp->reginfo);
			if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
				goto err;
			if ((ret = dbc->c_get(dbc,
			    &key_dbt, &data_dbt, DB_RMW | DB_FIRST)) != 0)
				goto err;
			rp = (REP_CONTROL *)key_dbt.data;
			rec = &data_dbt;
			memcpy(&rectype, rec->data, sizeof(rectype));
			R_LOCK(dbenv, &dblp->reginfo);
			/*
			 * We need to check again, because it's possible that
			 * some other thread of control changed the waiting_lsn
			 * or removed that record from the database.
			 */
			if (log_compare(&lp->ready_lsn, &rp->lsn) == 0) {
				if (rp->rectype != REP_NEWFILE) {
					ret = __log_put_int(dbenv,
					    &rp->lsn, &data_dbt, rp->flags);
					lp->ready_lsn = lp->lsn;
				} else
					CHANGE_FILES;
				R_UNLOCK(dbenv, &dblp->reginfo);
				if ((ret = dbc->c_del(dbc, 0)) != 0)
					goto err;

				/*
				 * If the current rectype is simple, we're
				 * ready for another record;  otherwise,
				 * don't get one, because we need to
				 * process the current one now.
				 */
				if (IS_SIMPLE(rectype)) {
					ret = dbc->c_get(dbc,
					    &key_dbt, &data_dbt, DB_NEXT);
					if (ret != DB_NOTFOUND && ret != 0)
						goto err;
					lsn =
					    ((REP_CONTROL *)key_dbt.data)->lsn;
					if ((ret = dbc->c_close(dbc)) != 0)
						goto err;
					R_LOCK(dbenv, &dblp->reginfo);
					if (ret == DB_NOTFOUND) {
						ZERO_LSN(lp->waiting_lsn);
						break;
					} else
						lp->waiting_lsn = lsn;
				} else {
					R_LOCK(dbenv, &dblp->reginfo);
					lp->waiting_lsn = lp->ready_lsn;
					break;
				}
			}
		}
	} else if (cmp > 0) {
		/*
		 * The LSN is higher than the one we were waiting for.
		 * If it is a NEWFILE message, this may not mean that
		 * there's a gap;  in some cases, NEWFILE messages contain
		 * the LSN of the beginning of the new file instead
		 * of the end of the old.
		 *
		 * In these cases, the rec DBT will contain the last LSN
		 * of the old file, so we can tell whether there's a gap.
		 */
		if (rp->rectype == REP_NEWFILE &&
		    rp->lsn.file == lp->ready_lsn.file + 1 &&
		    rp->lsn.offset == 0) {
			DB_ASSERT(rec != NULL && rec->data != NULL &&
			    rec->size == sizeof(DB_LSN));
			memcpy(&lsn, rec->data, sizeof(DB_LSN));
			if (log_compare(&lp->ready_lsn, &lsn) > 0)
				/*
				 * The last LSN in the old file is smaller
				 * than the one we're expecting, so there's
				 * no gap--the one we're expecting just
				 * doesn't exist.
				 */
				goto newfile;
		}

		/*
		 * This record isn't in sequence; add it to the table and
		 * update waiting_lsn if necessary.
		 */
		key_dbt.data = rp;
		key_dbt.size = sizeof(*rp);
		next_lsn = lp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		ret = dbp->put(dbp, NULL, &key_dbt, rec, 0);

		/* Request the LSN we are still waiting for. */
		MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
		eid = db_rep->region->master_id;
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		ret = __rep_send_message(dbenv, eid, REP_LOG_REQ,
		    &next_lsn, NULL, 0);
		R_LOCK(dbenv, &dblp->reginfo);
		if (ret == 0)
			if (IS_ZERO_LSN(lp->waiting_lsn) ||
			    log_compare(&rp->lsn, &lp->waiting_lsn) < 0)
				lp->waiting_lsn = rp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (ret);
	}
	R_UNLOCK(dbenv, &dblp->reginfo);
	if (ret != 0 || cmp < 0 || (cmp == 0 &&  IS_SIMPLE(rectype)))
		return (ret);

	/*
	 * If we got here, then we've got a log record in rp and rec that
	 * we need to process.
	 */
	switch(rectype) {
	case DB_txn_ckp:
		/* Sync the memory pool and write the log record. */
		memcpy(&ckp_lsn, (u_int8_t *)rec->data +
		    ((u_int8_t *)&ckp_args.ckp_lsn - (u_int8_t *)&ckp_args),
		    sizeof(DB_LSN));
retry:		if (!F_ISSET(dbenv, DB_ENV_REP_LOGSONLY)) {
			ret = dbenv->memp_sync(dbenv, &ckp_lsn);
			if (ret == DB_INCOMPLETE && retry_count < 4) {
				(void)__os_sleep(dbenv, 1 << retry_count, 0);
				retry_count++;
				goto retry;
			}
		}
		if (ret == 0) {
			ret = dbenv->log_put(dbenv, &lsn, rec, rp->flags);
		}
		break;
	case DB_log_register:
		/* Simply redo the operation. */
		if (!F_ISSET(dbenv, DB_ENV_REP_LOGSONLY))
			ret = __db_dispatch(dbenv,
			    NULL, rec, &rp->lsn, DB_TXN_APPLY, NULL);
		break;
	case DB_txn_regop:
		if (!F_ISSET(dbenv, DB_ENV_REP_LOGSONLY))
			ret = __rep_process_txn(dbenv, rec);
		break;
	default:
		goto err;
	}

	/* Check if we need to go back into the table. */
	if (ret == 0) {
		R_LOCK(dbenv, &dblp->reginfo);
		if (log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0)
			goto gap_check;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}

err:	if (dbc != NULL && (t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __rep_process_txn --
 *
 * This is the routine that actually gets a transaction ready for
 * processing.
 */
static int
__rep_process_txn(dbenv, commit_rec)
	DB_ENV *dbenv;
	DBT *commit_rec;
{
	DBT data_dbt;
	DB_LOCKREQ req, *lvp;
	DB_LOGC *logc;
	DB_LSN prev_lsn;
	LSN_PAGE *ap;
	TXN_RECS recs;
	__txn_regop_args *txn_args;
	u_int32_t op;
	int i, ret, t_ret;
	int (**dtab)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t dtabsize;

	/*
	 * There are three phases:  First, we have to traverse
	 * backwards through the log records gathering the list
	 * of all the pages accessed.  Once we have this information
	 * we can acquire all the locks we need.  Finally, we apply
	 * all the records in the transaction and release the locks.
	 */
	dtab = NULL;

	/* Make sure this is really a commit and not an abort! */
	if ((ret = __txn_regop_read(dbenv, commit_rec->data, &txn_args)) != 0)
		return (ret);
	op = txn_args->opcode;
	prev_lsn = txn_args->prev_lsn;
	__os_free(dbenv, txn_args, 0);
	if (op != TXN_COMMIT)
		return (0);

	memset(&recs, 0, sizeof(recs));
	recs.txnid = txn_args->txnid->txnid;
	if ((ret = dbenv->lock_id(dbenv, &recs.lockid)) != 0)
		return (ret);

	/* Initialize the getpgno dispatch table. */
	if ((ret = __rep_lockpgno_init(dbenv, &dtab, &dtabsize)) != 0)
		goto err;

	if ((ret = __rep_lockpages(dbenv,
	    dtab, NULL, &prev_lsn, &recs, recs.lockid)) != 0)
		goto err;
	if (recs.nalloc == 0)
		goto err;

	/* Phase 3: Apply updates and release locks. */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		goto err;
	memset(&data_dbt, 0, sizeof(data_dbt));
	for (ap = &recs.array[0], i = 0; i < recs.npages; i++, ap++) {
		if ((ret = logc->get(logc, &ap->lsn, &data_dbt, DB_SET)) != 0)
			goto err;
		if ((ret = __db_dispatch(dbenv, NULL,
		    &data_dbt, &ap->lsn, DB_TXN_APPLY, NULL)) != 0)
			goto err;
	}

err:	if (recs.nalloc != 0) {
		req.op = DB_LOCK_PUT_ALL;
		if ((t_ret = dbenv->lock_vec(dbenv, recs.lockid,
		    DB_LOCK_FREE_LOCKER, &req, 1, &lvp)) != 0 && ret == 0)
			ret = t_ret;
		__os_free(dbenv, recs.array, recs.nalloc * sizeof(LSN_PAGE));
	}

	if ((t_ret =
	    dbenv->lock_id_free(dbenv, recs.lockid)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (F_ISSET(&data_dbt, DB_DBT_REALLOC) && data_dbt.data != NULL)
		__os_free(dbenv, data_dbt.data, 0);

	if (dtab != NULL)
		__os_free(dbenv, dtab, 0);

	return (ret);
}

/*
 * __rep_client_dbinit --
 *
 * Initialize the LSN database on the client side.  This is called from the
 * client initialization code.  The startup flag value indicates if
 * this is the first thread/process starting up and therefore should create
 * the LSN database.  This routine must be called once by each process acting
 * as a client.
 *
 * PUBLIC:  int __rep_client_dbinit __P((DB_ENV *, int));
 */
int
__rep_client_dbinit(dbenv, startup)
	DB_ENV *dbenv;
	int startup;
{
	DB_REP *db_rep;
	DB *rep_db;
	int ret, t_ret;
	u_int32_t flags;

	PANIC_CHECK(dbenv);
	db_rep = dbenv->rep_handle;
	rep_db = NULL;

#define	REPDBNAME	"__db.rep.db"

	/* Check if this has already been called on this environment. */
	if (db_rep->rep_db != NULL)
		return (0);

	if (startup) {
		if ((ret = db_create(&rep_db, dbenv, 0)) != 0)
			goto err;
		/*
		 * Ignore errors, because if the file doesn't exist, this
		 * is perfectly OK.
		 */
		(void)rep_db->remove(rep_db, REPDBNAME, NULL, 0);
	}

	if ((ret = db_create(&rep_db, dbenv, 0)) != 0)
		goto err;
	if ((ret = rep_db->set_bt_compare(rep_db, __rep_bt_cmp)) != 0)
		goto err;

	flags = (F_ISSET(dbenv, DB_ENV_THREAD) ? DB_THREAD : 0) |
	    (startup ? DB_CREATE : 0);
	if ((ret = rep_db->open(rep_db,
	    "__db.rep.db", NULL, DB_BTREE, flags, 0)) != 0)
		goto err;

	/* Allow writes to this database on a client. */
	F_SET(rep_db, DB_CL_WRITER);

	db_rep->rep_db = rep_db;

	return (0);
err:
	if (rep_db != NULL &&
	    (t_ret = rep_db->close(rep_db, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;
	db_rep->rep_db = NULL;

	return (ret);
}

/*
 * __rep_bt_cmp --
 *
 * Comparison function for the LSN table.  We use the entire control
 * structure as a key (for simplicity, so we don't have to merge the
 * other fields in the control with the data field), but really only
 * care about the LSNs.
 */
static int
__rep_bt_cmp(dbp, dbt1, dbt2)
	DB *dbp;
	const DBT *dbt1, *dbt2;
{
	DB_LSN lsn1, lsn2;
	REP_CONTROL *rp1, *rp2;

	COMPQUIET(dbp, NULL);

	rp1 = dbt1->data;
	rp2 = dbt2->data;

	__ua_memcpy(&lsn1, &rp1->lsn, sizeof(DB_LSN));
	__ua_memcpy(&lsn2, &rp2->lsn, sizeof(DB_LSN));

	if (lsn1.file > lsn2.file)
		return (1);

	if (lsn1.file < lsn2.file)
		return (-1);

	if (lsn1.offset > lsn2.offset)
		return (1);

	if (lsn1.offset < lsn2.offset)
		return (-1);

	return (0);
}

