/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: rep_method.c,v 1.23 2001/10/10 02:57:41 margo Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

/* XXX */
#ifdef REP_DEBUG
#include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "log.h"
#include "rep.h"

#ifdef HAVE_RPC
#include "rpc_client_ext.h"
#endif

static int __rep_elect __P((DB_ENV *,
    int, int, u_int32_t, u_int32_t, int *, int *));
static int __rep_elect_init __P((DB_ENV *, DB_LSN *, int, int, int *));
static int __rep_set_rep_transport __P((DB_ENV *, int, void *, 
    int (*)(DB_ENV *, void *, const DBT *, DBT *, db_reptype_t, int)));
static int __rep_start __P((DB_ENV *, DBT *, u_int32_t));
static int __rep_wait __P((DB_ENV *,
   u_int32_t, u_int32_t, int *, int *, u_int32_t));

/*
 * __rep_dbenv_create --
 *	Replication-specific initialization of the DB_ENV structure.
 *
 * PUBLIC: int __rep_dbenv_create __P((DB_ENV *));
 */
int
__rep_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret;

	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 */

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_REP), &db_rep)) != 0)
		return (ret);
	dbenv->rep_handle = db_rep;

	/* Initialize the per-process replication structure. */
	db_rep->rep_send = NULL;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->rep_elect = __dbcl_rep_elect;
		dbenv->rep_start = __dbcl_rep_start;
		dbenv->rep_process_message = __dbcl_rep_process_message;
		dbenv->set_rep_transport = __dbcl_rep_set_rep_transport;
	} else
#endif
	{
		dbenv->rep_elect = __rep_elect;
		dbenv->rep_process_message = __rep_process_message;
		dbenv->rep_start = __rep_start;
		dbenv->set_rep_transport = __rep_set_rep_transport;
	}

	return (0);
}

/*
 * __rep_start --
 *	Become a master or client, and start sending messages to participate
 * in the replication environment.  Must be called after the environment
 * is open.
 */
static int
__rep_start(dbenv, dbt, flags)
	DB_ENV *dbenv;
	DBT *dbt;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	int announce, init_db, ret;

	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "rep_start");
	PANIC_CHECK(dbenv);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if ((ret = __db_fchk(dbenv, "DB_ENV->rep_start", flags,
	    DB_REP_CLIENT | DB_REP_LOGSONLY | DB_REP_MASTER)) != 0)
		return (ret);

	/* Exactly one of CLIENT and MASTER must be specified. */
	if ((!LF_ISSET(DB_REP_CLIENT) && (!LF_ISSET(DB_REP_MASTER))) ||
	    (LF_ISSET(DB_REP_CLIENT) && LF_ISSET(DB_REP_MASTER)))
		return (__db_ferr(dbenv, "DB_ENV->rep_start", 1));

	/* Masters can't be logs-only. */
	if (LF_ISSET(DB_REP_MASTER) && LF_ISSET(DB_REP_LOGSONLY))
		return (__db_ferr(dbenv, "DB_ENV->rep_start", 1));

	/* We need a transport function. */
	if (db_rep->rep_send == NULL) {
		__db_err(dbenv,
		    "DB_ENV->set_rep_transport must be called before %s",
		    "DB_ENV->rep_start");
		return (EINVAL);
	}

	MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
	if (rep->eid == DB_INVALID_EID)
		rep->eid = dbenv->rep_eid;

	if (LF_ISSET(DB_REP_MASTER)) {
		if (db_rep->rep_db != NULL) {
			ret = db_rep->rep_db->close(db_rep->rep_db, DB_NOSYNC);
			db_rep->rep_db = NULL;
		}

		F_CLR(dbenv, DB_ENV_REP_CLIENT);
		if (!F_ISSET(rep, REP_F_MASTER)) {
			/* Master is not yet set. */
			if (F_ISSET(rep, REP_ISCLIENT)) {
				F_CLR(rep, REP_ISCLIENT);
				rep->gen = ++rep->w_gen;
			} else if (rep->gen == 0)
				rep->gen = 1;
		}
		F_SET(rep, REP_F_MASTER);
		F_SET(dbenv, DB_ENV_REP_MASTER);
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
	} else {
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER) ||
		    F_ISSET(rep, REP_F_MASTER)) {
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (DB_REP_NEWMASTER);
		}

		F_SET(dbenv, DB_ENV_REP_CLIENT);
		if (LF_ISSET(DB_REP_LOGSONLY))
			F_SET(dbenv, DB_ENV_REP_LOGSONLY);

		announce = !F_ISSET(rep, REP_ISCLIENT) ||
		    rep->master_id == DB_INVALID_EID;
		init_db = 0;
		if (!F_ISSET(rep, REP_ISCLIENT)) {
			if (LF_ISSET(DB_REP_LOGSONLY))
				F_SET(rep, REP_F_LOGSONLY);
			else
				F_SET(rep, REP_F_UPGRADE);

			/*
			 * We initialize the client's generation number to 0.
			 * Upon startup, it looks for a master and updates the
			 * generation number as necessary, exactly as it does
			 * during normal operation and a master failure.
			 */
			rep->gen = 0;
			rep->master_id = DB_INVALID_EID;
			init_db = 1;
		}
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);

		if ((ret = __rep_client_dbinit(dbenv, init_db)) != 0)
			return (ret);

		/*
		 * If this client created a newly replicated environment,
		 * then announce the existence of this client.  The master
		 * should respond with a message that will tell this client
		 * the current generation number and the current LSN.  This
		 * will allow the client to either perform recovery or
		 * simply join in.
		 */
		if (announce)
			ret = __rep_send_message(dbenv,
			    DB_BROADCAST_EID, REP_NEWCLIENT, NULL, dbt, 0);
	}
	return (ret);
}

/*
 * __rep_set_transport --
 *	Set the transport function for replication.
 */
static int
__rep_set_rep_transport(dbenv, eid, send_cookie, f_send)
	DB_ENV *dbenv;
	int eid;
	void *send_cookie;
	int (*f_send)(DB_ENV *,
	    void *, const DBT *, DBT *, u_int32_t, int);
{
	DB_REP *db_rep;
	int ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	if (db_rep == NULL)
		return (EINVAL);

	db_rep->rep_send = f_send;
	db_rep->rep_send_data = send_cookie;

	dbenv->rep_eid = eid;
	return (ret);
}

/*
 * __rep_elect --
 *	Called after master failure to hold/participate in an election for
 *	a new master.
 */
static int
__rep_elect(dbenv, nsites, pri, wait, sleep, eidp, selfp)
	DB_ENV *dbenv;
	int nsites, pri;
	u_int32_t wait, sleep;
	int *eidp, *selfp;
{
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	REP *rep;
	int in_progress, ret, send_vote;

	*selfp = 0;

	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	lsn = ((LOG *)dblp->reginfo.primary)->lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	if ((ret = __rep_elect_init(dbenv,
	    &lsn, nsites, pri, &in_progress)) != 0)
		return (ret);

#ifdef REP_DEBUG
if (!in_progress)
	fprintf(stderr, "%lx Beginning an election\n", (long)pthread_self());
#endif
	if (!in_progress && (ret = __rep_send_message(dbenv,
	    DB_BROADCAST_EID, REP_ELECT, NULL, NULL, 0)) != 0)
		goto err;

	/* Now send vote */
	if ((ret = __rep_send_vote(dbenv, &lsn, nsites, pri)) != 0)
		goto err;

	ret = __rep_wait(dbenv, wait, sleep, selfp, eidp, REP_F_EPHASE1);
	switch (ret) {
		case 0:
			/* Check if election complete or phase complete. */
			if (*eidp != DB_INVALID_EID)
				return (0);
			goto phase2;
		case DB_TIMEOUT:
			break;
		default:
			goto err;
	}
	/*
	 * If we got here, we haven't heard from everyone, but we've
	 * run out of time, so it's time to decide if we have enough
	 * votes to pick a winner and if so, to send out a vote to
	 * the winner.
	 */
	MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
	send_vote = DB_INVALID_EID;
	if (rep->sites > rep->nsites / 2) {
		/* We think we've seen enough to cast a vote. */
		send_vote = rep->winner;
		if (rep->winner == rep->eid)
			rep->votes++;
		F_CLR(rep, REP_F_EPHASE1);
		F_SET(rep, REP_F_EPHASE2);
	}
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
	if (send_vote == DB_INVALID_EID) {
		/* We do not have enough votes to elect. */
#ifdef REP_DEBUG
fprintf(stderr, "%lx Not enough votes to elect: received %d of %d\n",
(long)pthread_self(), rep->sites, rep->nsites);
#endif
		ret = DB_REP_UNAVAIL;
		goto err;

	}
#ifdef REP_DEBUG
if (send_vote != rep->eid)
	fprintf(stderr, "%lx Sending vote\n", (long)pthread_self());
#endif

	if (send_vote != rep->eid && (ret = __rep_send_message(dbenv,
	    send_vote, REP_VOTE2, NULL, NULL, 0)) != 0)
		goto err;

phase2:	ret = __rep_wait(dbenv, wait, sleep, selfp, eidp, REP_F_EPHASE2);
	switch (ret) {
		case 0:
			return (0);
		case DB_TIMEOUT:
			ret = DB_REP_UNAVAIL;
			break;
		default:
			goto err;
	}

err:	MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
	ELECTION_DONE(rep);
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
#ifdef REP_DEBUG
fprintf(stderr, "%lx Ended election with %d\n", (long)pthread_self(), ret);
#endif
	return (ret);
}

/*
 * __rep_elect_init
 *	Initialize an election.  Sets beginp non-zero if the election is
 * already in progress; makes it 0 otherwise.
 */
int
__rep_elect_init(dbenv, lsnp, nsites, pri, beginp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	int nsites, pri, *beginp;
{
	DB_REP *db_rep;
	REP *rep;
	int ret, *tally;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	ret = 0;
	MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
	*beginp = IN_ELECTION(rep);
	if (!*beginp) {
		F_SET(rep, REP_F_EPHASE1);
		rep->master_id = DB_INVALID_EID;
		if (nsites > rep->asites &&
		    (ret = __rep_grow_sites(dbenv, nsites)) != 0)
			goto err;
		rep->nsites = nsites;
		rep->priority = pri;
		rep->votes = 0;

		/* We have always heard from ourselves. */
		rep->sites = 1;
		tally = R_ADDR((REGINFO *)dbenv->reginfo, rep->tally_off);
		tally[0] = rep->eid;

		if (pri != 0) {
			/* Make ourselves the winner to start. */
			rep->winner = rep->eid;
			rep->w_priority = pri;
			rep->w_gen = rep->gen;
			rep->w_lsn = *lsnp;
		} else {
			rep->winner = DB_INVALID_EID;
			rep->w_priority = 0;
			rep->w_gen = 0;
			ZERO_LSN(rep->w_lsn);
		}
	}
err:	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
	return (ret);
}

static int
__rep_wait(dbenv, wait, sleep, selfp, eidp, flags)
	DB_ENV *dbenv;
	u_int32_t wait, sleep;
	int *selfp, *eidp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	int done, ret;

	done = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	while (wait > 0) {
		if ((ret = __os_sleep(dbenv, 0, sleep)) != 0)
			return (ret);
		MUTEX_LOCK(dbenv, db_rep->mutexp, dbenv->lockfhp);
		done = !F_ISSET(rep, flags) && rep->master_id != DB_INVALID_EID;

		*eidp = rep->master_id;
		if (rep->eid == rep->master_id)
			*selfp = 1;
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);

		if (done)
			return (0);

		if (wait > sleep)
			wait -= sleep;
		else
			wait = 0;
	}
	return (DB_TIMEOUT);
}
