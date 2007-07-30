/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_method.c,v 1.38 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

static int __repmgr_await_threads __P((DB_ENV *));

/*
 * PUBLIC: int __repmgr_start __P((DB_ENV *, int, u_int32_t));
 */
int
__repmgr_start(dbenv, nthreads, flags)
	DB_ENV *dbenv;
	int nthreads;
	u_int32_t flags;
{
	DB_REP *db_rep;
	DBT my_addr;
	REPMGR_RUNNABLE *selector, *messenger;
	int ret, i;

	db_rep = dbenv->rep_handle;

	/* Check that the required initialization has been done. */
	if (db_rep->my_addr.port == 0) {
		__db_errx(dbenv,
		    "repmgr_set_local_site must be called before repmgr_start");
		return (EINVAL);
	}

	if (db_rep->selector != NULL || db_rep->finished) {
		__db_errx(dbenv,
		    "DB_ENV->repmgr_start may not be called more than once");
		return (EINVAL);
	}

	switch (flags) {
	case DB_REP_CLIENT:
	case DB_REP_ELECTION:
	case DB_REP_MASTER:
		break;
	default:
		__db_errx(dbenv,
		    "repmgr_start: unrecognized flags parameter value");
		return (EINVAL);
	}

	if (nthreads <= 0) {
		__db_errx(dbenv,
		    "repmgr_start: nthreads parameter must be >= 1");
		return (EINVAL);
	}

	if ((ret =
	   __os_calloc(dbenv, (u_int)nthreads, sizeof(REPMGR_RUNNABLE *),
	    &db_rep->messengers)) != 0)
		return (ret);
	db_rep->nthreads = nthreads;

	if ((ret = __repmgr_net_init(dbenv, db_rep)) != 0 ||
	    (ret = __repmgr_init_sync(dbenv, db_rep)) != 0 ||
	    (ret = __rep_set_transport(dbenv, SELF_EID, __repmgr_send)) != 0)
		return (ret);

	/*
	 * Make some sort of call to rep_start before starting other threads, to
	 * ensure that incoming messages being processed always have a rep
	 * context properly configured.
	 */
	if ((db_rep->init_policy = flags) == DB_REP_MASTER)
		ret = __repmgr_become_master(dbenv);
	else {
		if ((ret = __repmgr_prepare_my_addr(dbenv, &my_addr)) != 0)
			return (ret);
		ret = __rep_start(dbenv, &my_addr, DB_REP_CLIENT);
		__os_free(dbenv, my_addr.data);
		if (ret == 0) {
			LOCK_MUTEX(db_rep->mutex);
			ret = __repmgr_init_election(dbenv, ELECT_SEEK_MASTER);
			UNLOCK_MUTEX(db_rep->mutex);
		}
	}
	if (ret != 0)
		return (ret);

	if ((ret = __os_calloc(dbenv, 1, sizeof(REPMGR_RUNNABLE), &selector))
	    != 0)
		return (ret);
	selector->dbenv = dbenv;
	selector->run = __repmgr_select_thread;
	if ((ret = __repmgr_thread_start(dbenv, selector)) != 0) {
		__db_err(dbenv, ret, "can't start selector thread");
		__os_free(dbenv, selector);
		return (ret);
	} else
		db_rep->selector = selector;

	for (i=0; i<nthreads; i++) {
		if ((ret = __os_calloc(dbenv, 1, sizeof(REPMGR_RUNNABLE),
		    &messenger)) != 0)
			return (ret);

		messenger->dbenv = dbenv;
		messenger->run = __repmgr_msg_thread;
		if ((ret = __repmgr_thread_start(dbenv, messenger)) != 0) {
			__os_free(dbenv, messenger);
			return (ret);
		}
		db_rep->messengers[i] = messenger;
	}

	return (ret);
}

/*
 * PUBLIC: int __repmgr_close __P((DB_ENV *));
 */
int
__repmgr_close(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret, t_ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	if (db_rep->selector != NULL) {
		RPRINT(dbenv, (dbenv, "Stopping repmgr threads"));
		ret = __repmgr_stop_threads(dbenv);
		if ((t_ret = __repmgr_await_threads(dbenv)) != 0 && ret == 0)
			ret = t_ret;
		RPRINT(dbenv, (dbenv, "Repmgr threads are finished"));
	}

	if ((t_ret = __repmgr_net_close(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = __repmgr_close_sync(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * PUBLIC: int __repmgr_set_ack_policy __P((DB_ENV *, int));
 */
int
__repmgr_set_ack_policy(dbenv, policy)
	DB_ENV *dbenv;
	int policy;
{
	switch (policy) {
	case DB_REPMGR_ACKS_ALL: /* FALLTHROUGH */
	case DB_REPMGR_ACKS_ALL_PEERS: /* FALLTHROUGH */
	case DB_REPMGR_ACKS_NONE: /* FALLTHROUGH */
	case DB_REPMGR_ACKS_ONE: /* FALLTHROUGH */
	case DB_REPMGR_ACKS_ONE_PEER: /* FALLTHROUGH */
	case DB_REPMGR_ACKS_QUORUM:
		dbenv->rep_handle->perm_policy = policy;
		return (0);
	default:
		__db_errx(dbenv,
		    "Unknown ack_policy in DB_ENV->repmgr_set_ack_policy");
		return (EINVAL);
	}
}

/*
 * PUBLIC: int __repmgr_get_ack_policy __P((DB_ENV *, int *));
 */
int
__repmgr_get_ack_policy(dbenv, policy)
	DB_ENV *dbenv;
	int *policy;
{
	*policy = dbenv->rep_handle->perm_policy;
	return (0);
}

/*
 * PUBLIC: int __repmgr_env_create __P((DB_ENV *, DB_REP *));
 */
int
__repmgr_env_create(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	int ret;

	/* Set some default values. */
	db_rep->ack_timeout = 1 * US_PER_SEC;			/*  1 second */
	db_rep->connection_retry_wait = 30 * US_PER_SEC;	/* 30 seconds */
	db_rep->election_retry_wait = 10 * US_PER_SEC;		/* 10 seconds */
	db_rep->config_nsites = 0;
	db_rep->peer = DB_EID_INVALID;
	db_rep->perm_policy = DB_REPMGR_ACKS_QUORUM;

#ifdef DB_WIN32
	db_rep->waiters = NULL;
#else
	db_rep->read_pipe = db_rep->write_pipe = -1;
#endif
	if ((ret = __repmgr_net_create(dbenv, db_rep)) == 0)
		ret = __repmgr_queue_create(dbenv, db_rep);

	return (ret);
}

/*
 * PUBLIC: void __repmgr_env_destroy __P((DB_ENV *, DB_REP *));
 */
void
__repmgr_env_destroy(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	__repmgr_queue_destroy(dbenv);
	__repmgr_net_destroy(dbenv, db_rep);
	if (db_rep->messengers != NULL) {
		__os_free(dbenv, db_rep->messengers);
		db_rep->messengers = NULL;
	}
}

/*
 * PUBLIC: int __repmgr_stop_threads __P((DB_ENV *));
 */
int
__repmgr_stop_threads(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret;

	db_rep = dbenv->rep_handle;

	/*
	 * Hold mutex for the purpose of waking up threads, but then get out of
	 * the way to let them clean up and exit.
	 */
	LOCK_MUTEX(db_rep->mutex);
	db_rep->finished = TRUE;
	if (db_rep->elect_thread != NULL &&
	    (ret = __repmgr_signal(&db_rep->check_election)) != 0)
		goto unlock;

	if ((ret = __repmgr_signal(&db_rep->queue_nonempty)) != 0)
		goto unlock;
	UNLOCK_MUTEX(db_rep->mutex);

	return (__repmgr_wake_main_thread(dbenv));

unlock:
	UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}

static int
__repmgr_await_threads(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REPMGR_RUNNABLE *messenger;
	int ret, t_ret, i;

	db_rep = dbenv->rep_handle;
	ret = 0;
	if (db_rep->elect_thread != NULL) {
		ret = __repmgr_thread_join(db_rep->elect_thread);
		__os_free(dbenv, db_rep->elect_thread);
		db_rep->elect_thread = NULL;
	}

	for (i=0; i<db_rep->nthreads && db_rep->messengers[i] != NULL; i++) {
		messenger = db_rep->messengers[i];
		if ((t_ret = __repmgr_thread_join(messenger)) != 0 && ret == 0)
			ret = t_ret;
		__os_free(dbenv, messenger);
		db_rep->messengers[i] = NULL; /* necessary? */
	}
	__os_free(dbenv, db_rep->messengers);
	db_rep->messengers = NULL;

	if (db_rep->selector != NULL) {
		if ((t_ret = __repmgr_thread_join(db_rep->selector)) != 0 &&
		    ret == 0)
			ret = t_ret;
		__os_free(dbenv, db_rep->selector);
		db_rep->selector = NULL;
	}

	return (ret);
}

/*
 * PUBLIC: int __repmgr_set_local_site __P((DB_ENV *, const char *, u_int,
 * PUBLIC:     u_int32_t));
 */
int
__repmgr_set_local_site(dbenv, host, port, flags)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
	u_int32_t flags;
{
	ADDRINFO *address_list;
	DB_REP *db_rep;
	repmgr_netaddr_t addr;
	int locked, ret;
	const char *sharable_host;
	char buffer[MAXHOSTNAMELEN];

	if (flags != 0)
		return (__db_ferr(dbenv, "DB_ENV->repmgr_set_local_site", 0));

	db_rep = dbenv->rep_handle;
	if (db_rep->my_addr.port != 0) {
		__db_errx(dbenv, "Listen address already set");
		return (EINVAL);
	}

	/*
	 * If we haven't been given a sharable local host name, we must get one
	 * for the purpose of sharing with our friends, but it's ok to use for
	 * the address look-up.  "Helpfully" converting special names such as
	 * "localhost" in this same way is tempting, but in practice turns out
	 * to be too tricky.
	 */
	if (host == NULL) {
		if ((ret = gethostname(buffer, sizeof(buffer))) != 0)
			return (net_errno);

		/* In case truncation leaves no terminating NUL byte: */
		buffer[sizeof(buffer) - 1] = '\0';
		sharable_host = buffer;
	} else
		sharable_host = host;

	if ((ret = __repmgr_getaddr(dbenv,
	    host, port, AI_PASSIVE, &address_list)) != 0)
		return (ret);

	if ((ret = __repmgr_pack_netaddr(dbenv,
	     sharable_host, port, address_list, &addr)) != 0) {
		__db_freeaddrinfo(dbenv, address_list);
		return (ret);
	}

	if (REPMGR_SYNC_INITED(db_rep)) {
		LOCK_MUTEX(db_rep->mutex);
		locked = TRUE;
	} else
		locked = FALSE;

	memcpy(&db_rep->my_addr, &addr, sizeof(addr));

	if (locked)
		UNLOCK_MUTEX(db_rep->mutex);
	return (0);
}

/*
 * If the application only calls this method from a single thread (e.g., during
 * its initialization), it will avoid the problems with the non-thread-safe host
 * name lookup.  In any case, if we relegate the blocking lookup to here it
 * won't affect our select() loop.
 *
 * PUBLIC: int __repmgr_add_remote_site __P((DB_ENV *, const char *, u_int,
 * PUBLIC: int *, u_int32_t));
 */
int
__repmgr_add_remote_site(dbenv, host, port, eidp, flags)
	DB_ENV *dbenv;
	const char *host;
	u_int port;
	int *eidp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	int eid, locked, ret;

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->repmgr_add_remote_site", flags, DB_REPMGR_PEER)) != 0)
		return (ret);

	if (host == NULL) {
		__db_errx(dbenv,
		    "repmgr_add_remote_site: host name is required");
		return (EINVAL);
	}

	db_rep = dbenv->rep_handle;

	if (REPMGR_SYNC_INITED(db_rep)) {
		LOCK_MUTEX(db_rep->mutex);
		locked = TRUE;
	} else
		locked = FALSE;

	if ((ret = __repmgr_add_site(dbenv, host, port, &site)) != 0)
		goto unlock;
	eid = EID_FROM_SITE(site);

	if (LF_ISSET(DB_REPMGR_PEER))
		db_rep->peer = eid;
	if (eidp != NULL)
		*eidp = eid;

unlock:	if (locked)
		UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}
