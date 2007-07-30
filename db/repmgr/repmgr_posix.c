/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_posix.c,v 1.29 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#define	__INCLUDE_SELECT_H	1
#include "db_int.h"

/*
 * A very rough guess at the maximum stack space one of our threads could ever
 * need, which we hope is plenty conservative.  This can be patched in the field
 * if necessary.
 */
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
size_t __repmgr_guesstimated_max = (128 * 1024);
#endif

static int finish_connecting __P((DB_ENV *, REPMGR_CONNECTION *));

/*
 * Starts the thread described in the argument, and stores the resulting thread
 * ID therein.
 *
 * PUBLIC: int __repmgr_thread_start __P((DB_ENV *, REPMGR_RUNNABLE *));
 */
int
__repmgr_thread_start(dbenv, runnable)
	DB_ENV *dbenv;
	REPMGR_RUNNABLE *runnable;
{
	pthread_attr_t *attrp;
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
	pthread_attr_t attributes;
	size_t size;
	int ret;
#endif

	runnable->finished = FALSE;

#ifdef _POSIX_THREAD_ATTR_STACKSIZE
	attrp = &attributes;
	if ((ret = pthread_attr_init(&attributes)) != 0) {
		__db_err(dbenv,
		    ret, "pthread_attr_init in repmgr_thread_start");
		return (ret);
	}

	/*
	 * On a 64-bit machine it seems reasonable that we could need twice as
	 * much stack space as we did on a 32-bit machine.
	 */
	size = __repmgr_guesstimated_max;
	if (sizeof(size_t) > 4)
		size *= 2;
#ifdef PTHREAD_STACK_MIN
	if (size < PTHREAD_STACK_MIN)
		size = PTHREAD_STACK_MIN;
#endif
	if ((ret = pthread_attr_setstacksize(&attributes, size)) != 0) {
		__db_err(dbenv,
		    ret, "pthread_attr_setstacksize in repmgr_thread_start");
		return (ret);
	}
#else
	attrp = NULL;
#endif

	return (pthread_create(&runnable->thread_id, attrp,
		    runnable->run, dbenv));
}

/*
 * PUBLIC: int __repmgr_thread_join __P((REPMGR_RUNNABLE *));
 */
int
__repmgr_thread_join(thread)
	REPMGR_RUNNABLE *thread;
{
	return (pthread_join(thread->thread_id, NULL));
}

/*
 * PUBLIC: int __repmgr_set_nonblocking __P((socket_t));
 */
int
__repmgr_set_nonblocking(fd)
	socket_t fd;
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
		return (errno);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return (errno);
	return (0);
}

/*
 * PUBLIC: int __repmgr_wake_waiting_senders __P((DB_ENV *));
 *
 * Wake any send()-ing threads waiting for an acknowledgement.
 *
 * !!!
 * Caller must hold the db_rep->mutex, if this thread synchronization is to work
 * properly.
 */
int
__repmgr_wake_waiting_senders(dbenv)
	DB_ENV *dbenv;
{
	return (pthread_cond_broadcast(&dbenv->rep_handle->ack_condition));
}

/*
 * PUBLIC: int __repmgr_await_ack __P((DB_ENV *, const DB_LSN *));
 *
 * Waits (a limited time) for configured number of remote sites to ack the given
 * LSN.
 *
 * !!!
 * Caller must hold repmgr->mutex.
 */
int
__repmgr_await_ack(dbenv, lsnp)
	DB_ENV *dbenv;
	const DB_LSN *lsnp;
{
	DB_REP *db_rep;
	struct timespec deadline;
	int ret, timed;

	db_rep = dbenv->rep_handle;

	if ((timed = (db_rep->ack_timeout > 0)))
		__repmgr_compute_wait_deadline(dbenv, &deadline,
		    db_rep->ack_timeout);
	else
		COMPQUIET(deadline.tv_sec, 0);

	while (!__repmgr_is_permanent(dbenv, lsnp)) {
		if (timed)
			ret = pthread_cond_timedwait(&db_rep->ack_condition,
			    &db_rep->mutex, &deadline);
		else
			ret = pthread_cond_wait(&db_rep->ack_condition,
			    &db_rep->mutex);
		if (db_rep->finished)
			return (DB_REP_UNAVAIL);
		if (ret != 0)
			return (ret);
	}
	return (0);
}

/*
 * Computes a deadline time a certain distance into the future, in a form
 * suitable for the pthreads timed wait operation.  Curiously, that call uses
 * nano-second resolution; elsewhere we use microseconds.
 *
 * PUBLIC: void __repmgr_compute_wait_deadline __P((DB_ENV*,
 * PUBLIC:    struct timespec *, db_timeout_t));
 */
void
__repmgr_compute_wait_deadline(dbenv, result, wait)
	DB_ENV *dbenv;
	struct timespec *result;
	db_timeout_t wait;
{
	db_timespec v;

	/*
	 * Start with "now"; then add the "wait" offset.
	 *
	 * A db_timespec is the same as a "struct timespec" so we can pass
	 * result directly to the underlying Berkeley DB OS routine.
	 */
	__os_gettime(dbenv, (db_timespec *)result);

	/* Convert microsecond wait to a timespec. */
	DB_TIMEOUT_TO_TIMESPEC(wait, &v);

	timespecadd(result, &v);
}

/*
 * PUBLIC: int __repmgr_init_sync __P((DB_ENV *, DB_REP *));
 *
 * Allocate/initialize all data necessary for thread synchronization.  This
 * should be an all-or-nothing affair.  Other than here and in _close_sync there
 * should never be a time when these resources aren't either all allocated or
 * all freed.  If that's true, then we can safely use the values of the file
 * descriptor(s) to keep track of which it is.
 */
int
__repmgr_init_sync(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	int ret, mutex_inited, ack_inited, elect_inited, queue_inited,
	    file_desc[2];

	COMPQUIET(dbenv, NULL);

	mutex_inited = ack_inited = elect_inited = queue_inited = FALSE;

	if ((ret = pthread_mutex_init(&db_rep->mutex, NULL)) != 0)
		goto err;
	mutex_inited = TRUE;

	if ((ret = pthread_cond_init(&db_rep->ack_condition, NULL)) != 0)
		goto err;
	ack_inited = TRUE;

	if ((ret = pthread_cond_init(&db_rep->check_election, NULL)) != 0)
		goto err;
	elect_inited = TRUE;

	if ((ret = pthread_cond_init(&db_rep->queue_nonempty, NULL)) != 0)
		goto err;
	queue_inited = TRUE;

	if ((ret = pipe(file_desc)) == -1) {
		ret = errno;
		goto err;
	}

	db_rep->read_pipe = file_desc[0];
	db_rep->write_pipe = file_desc[1];
	return (0);
err:
	if (queue_inited)
		(void)pthread_cond_destroy(&db_rep->queue_nonempty);
	if (elect_inited)
		(void)pthread_cond_destroy(&db_rep->check_election);
	if (ack_inited)
		(void)pthread_cond_destroy(&db_rep->ack_condition);
	if (mutex_inited)
		(void)pthread_mutex_destroy(&db_rep->mutex);
	db_rep->read_pipe = db_rep->write_pipe = -1;

	return (ret);
}

/*
 * PUBLIC: int __repmgr_close_sync __P((DB_ENV *));
 *
 * Frees the thread synchronization data within a repmgr struct, in a
 * platform-specific way.
 */
int
__repmgr_close_sync(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret, t_ret;

	db_rep = dbenv->rep_handle;

	if (!(REPMGR_SYNC_INITED(db_rep)))
		return (0);

	ret = pthread_cond_destroy(&db_rep->queue_nonempty);

	if ((t_ret = pthread_cond_destroy(&db_rep->check_election)) != 0 &&
	    ret == 0)
		ret = t_ret;

	if ((t_ret = pthread_cond_destroy(&db_rep->ack_condition)) != 0 &&
	    ret == 0)
		ret = t_ret;

	if ((t_ret = pthread_mutex_destroy(&db_rep->mutex)) != 0 &&
	    ret == 0)
		ret = t_ret;

	if (close(db_rep->read_pipe) == -1 && ret == 0)
		ret = errno;
	if (close(db_rep->write_pipe) == -1 && ret == 0)
		ret = errno;

	db_rep->read_pipe = db_rep->write_pipe = -1;
	return (ret);
}

/*
 * Performs net-related resource initialization other than memory initialization
 * and allocation.  A valid db_rep->listen_fd acts as the "all-or-nothing"
 * sentinel signifying that these resources are allocated.
 *
 * PUBLIC: int __repmgr_net_init __P((DB_ENV *, DB_REP *));
 */
int
__repmgr_net_init(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	int ret;
	struct sigaction sigact;

	if ((ret = __repmgr_listen(dbenv)) != 0)
		return (ret);

	/*
	 * Make sure we're not ignoring SIGPIPE, 'cuz otherwise we'd be killed
	 * just for trying to write onto a socket that had been reset.
	 */
	if (sigaction(SIGPIPE, NULL, &sigact) == -1) {
		ret = errno;
		__db_err(dbenv, ret, "can't access signal handler");
		goto err;
	}
	/*
	 * If we need to change the sig handler, do so, and also set a flag so
	 * that we remember we did.
	 */
	if ((db_rep->chg_sig_handler = (sigact.sa_handler == SIG_DFL))) {
		sigact.sa_handler = SIG_IGN;
		sigact.sa_flags = 0;
		if (sigaction(SIGPIPE, &sigact, NULL) == -1) {
			ret = errno;
			__db_err(dbenv, ret, "can't access signal handler");
			goto err;
		}
	}
	return (0);

err:
	(void)closesocket(db_rep->listen_fd);
	db_rep->listen_fd = INVALID_SOCKET;
	return (ret);
}

/*
 * PUBLIC: int __repmgr_lock_mutex __P((mgr_mutex_t *));
 */
int
__repmgr_lock_mutex(mutex)
	mgr_mutex_t  *mutex;
{
	return (pthread_mutex_lock(mutex));
}

/*
 * PUBLIC: int __repmgr_unlock_mutex __P((mgr_mutex_t *));
 */
int
__repmgr_unlock_mutex(mutex)
	mgr_mutex_t  *mutex;
{
	return (pthread_mutex_unlock(mutex));
}

/*
 * Signals a condition variable.
 *
 * !!!
 * Caller must hold mutex.
 *
 * PUBLIC: int __repmgr_signal __P((cond_var_t *));
 */
int
__repmgr_signal(v)
	cond_var_t *v;
{
	return (pthread_cond_broadcast(v));
}

/*
 * PUBLIC: int __repmgr_wake_main_thread __P((DB_ENV*));
 */
int
__repmgr_wake_main_thread(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	u_int8_t any_value;

	COMPQUIET(any_value, 0);
	db_rep = dbenv->rep_handle;

	/*
	 * It doesn't matter what byte value we write.  Just the appearance of a
	 * byte in the stream is enough to wake up the select() thread reading
	 * the pipe.
	 */
	if (write(db_rep->write_pipe, &any_value, 1) == -1)
		return (errno);
	return (0);
}

/*
 * PUBLIC: int __repmgr_writev __P((socket_t, db_iovec_t *, int, size_t *));
 */
int
__repmgr_writev(fd, iovec, buf_count, byte_count_p)
	socket_t fd;
	db_iovec_t *iovec;
	int buf_count;
	size_t *byte_count_p;
{
	int nw;

	if ((nw = writev(fd, iovec, buf_count)) == -1)
		return (errno);
	*byte_count_p = (size_t)nw;
	return (0);
}

/*
 * PUBLIC: int __repmgr_readv __P((socket_t, db_iovec_t *, int, size_t *));
 */
int
__repmgr_readv(fd, iovec, buf_count, byte_count_p)
	socket_t fd;
	db_iovec_t *iovec;
	int buf_count;
	size_t *byte_count_p;
{
	ssize_t nw;

	if ((nw = readv(fd, iovec, buf_count)) == -1)
		return (errno);
	*byte_count_p = (size_t)nw;
	return (0);
}

/*
 * PUBLIC: int __repmgr_select_loop __P((DB_ENV *));
 */
int
__repmgr_select_loop(dbenv)
	DB_ENV *dbenv;
{
	struct timeval select_timeout, *select_timeout_p;
	DB_REP *db_rep;
	REPMGR_CONNECTION *conn, *next;
	REPMGR_RETRY *retry;
	db_timespec timeout;
	fd_set reads, writes;
	int ret, flow_control, maxfd, nready;
	u_int8_t buf[10];	/* arbitrary size */

	flow_control = FALSE;

	db_rep = dbenv->rep_handle;
	/*
	 * Almost this entire thread operates while holding the mutex.  But note
	 * that it never blocks, except in the call to select() (which is the
	 * one place we relinquish the mutex).
	 */
	LOCK_MUTEX(db_rep->mutex);
	if ((ret = __repmgr_first_try_connections(dbenv)) != 0)
		goto out;
	for (;;) {
		FD_ZERO(&reads);
		FD_ZERO(&writes);

		/*
		 * Always ask for input on listening socket and signalling
		 * pipe.
		 */
		FD_SET((u_int)db_rep->listen_fd, &reads);
		maxfd = db_rep->listen_fd;

		FD_SET((u_int)db_rep->read_pipe, &reads);
		if (db_rep->read_pipe > maxfd)
			maxfd = db_rep->read_pipe;

		/*
		 * Examine all connections to see what sort of I/O to ask for on
		 * each one.
		 */
		TAILQ_FOREACH(conn, &db_rep->connections, entries) {
			if (F_ISSET(conn, CONN_CONNECTING)) {
				FD_SET((u_int)conn->fd, &reads);
				FD_SET((u_int)conn->fd, &writes);
				if (conn->fd > maxfd)
					maxfd = conn->fd;
				continue;
			}

			if (!STAILQ_EMPTY(&conn->outbound_queue)) {
				FD_SET((u_int)conn->fd, &writes);
				if (conn->fd > maxfd)
					maxfd = conn->fd;
			}
			/*
			 * If we haven't yet gotten site's handshake, then read
			 * from it even if we're flow-controlling.
			 */
			if (!flow_control || !IS_VALID_EID(conn->eid)) {
				FD_SET((u_int)conn->fd, &reads);
				if (conn->fd > maxfd)
					maxfd = conn->fd;
			}
		}
		/*
		 * Decide how long to wait based on when it will next be time to
		 * retry an idle connection.  (List items are in order, so we
		 * only have to examine the first one.)
		 */
		if (TAILQ_EMPTY(&db_rep->retries))
			select_timeout_p = NULL;
		else {
			retry = TAILQ_FIRST(&db_rep->retries);

			__repmgr_timespec_diff_now(
			    dbenv, &retry->time, &timeout);

			/* Convert the timespec to a timeval. */
			select_timeout.tv_sec = timeout.tv_sec;
			select_timeout.tv_usec = timeout.tv_nsec / NS_PER_US;
			select_timeout_p = &select_timeout;
		}

		UNLOCK_MUTEX(db_rep->mutex);

		if ((ret = select(maxfd + 1,
		    &reads, &writes, NULL, select_timeout_p)) == -1) {
			switch (ret = errno) {
			case EINTR:
			case EWOULDBLOCK:
				LOCK_MUTEX(db_rep->mutex);
				continue; /* simply retry */
			default:
				__db_err(dbenv, ret, "select");
				return (ret);
			}
		}
		nready = ret;

		LOCK_MUTEX(db_rep->mutex);

		/*
		 * The first priority thing we must do is to clean up any
		 * pending defunct connections.  Otherwise, if they have any
		 * lingering pending input, we get very confused if we try to
		 * process it.
		 *
		 * The TAILQ_FOREACH macro would be suitable here, except that
		 * it doesn't allow unlinking the current element, which is
		 * needed for cleanup_connection.
		 */
		for (conn = TAILQ_FIRST(&db_rep->connections);
		     conn != NULL;
		     conn = next) {
			next = TAILQ_NEXT(conn, entries);
			if (F_ISSET(conn, CONN_DEFUNCT))
				__repmgr_cleanup_connection(dbenv, conn);
		}

		if ((ret = __repmgr_retry_connections(dbenv)) != 0)
			goto out;
		if (nready == 0)
			continue;

		/*
		 * Traverse the linked list.  (Again, like TAILQ_FOREACH, except
		 * that we need the ability to unlink an element along the way.)
		 */
		for (conn = TAILQ_FIRST(&db_rep->connections);
		     conn != NULL;
		     conn = next) {
			next = TAILQ_NEXT(conn, entries);
			if (F_ISSET(conn, CONN_CONNECTING)) {
				if (FD_ISSET((u_int)conn->fd, &reads) ||
				    FD_ISSET((u_int)conn->fd, &writes)) {
					if ((ret = finish_connecting(dbenv,
					    conn)) == DB_REP_UNAVAIL) {
						if ((ret =
						    __repmgr_bust_connection(
						    dbenv, conn, TRUE)) != 0)
							goto out;
					} else if (ret != 0)
						goto out;
				}
				continue;
			}

			/*
			 * Here, the site is connected, and the FD_SET's are
			 * valid.
			 */
			if (FD_ISSET((u_int)conn->fd, &writes)) {
				if ((ret = __repmgr_write_some(
				    dbenv, conn)) == DB_REP_UNAVAIL) {
					if ((ret =
					    __repmgr_bust_connection(dbenv,
					    conn, TRUE)) != 0)
						goto out;
					continue;
				} else if (ret != 0)
					goto out;
			}

			if (!flow_control &&
			    FD_ISSET((u_int)conn->fd, &reads)) {
				if ((ret = __repmgr_read_from_site(dbenv, conn))
				    == DB_REP_UNAVAIL) {
					if ((ret =
					    __repmgr_bust_connection(dbenv,
					    conn, TRUE)) != 0)
						goto out;
					continue;
				} else if (ret != 0)
					goto out;
			}
		}

		/*
		 * Read any bytes in the signalling pipe.  Note that we don't
		 * actually need to do anything with them; they're just there to
		 * wake us up when necessary.
		 */
		if (FD_ISSET((u_int)db_rep->read_pipe, &reads)) {
			if (read(db_rep->read_pipe, buf, sizeof(buf)) <= 0) {
				ret = errno;
				goto out;
			} else if (db_rep->finished) {
				ret = 0;
				goto out;
			}
		}
		if (FD_ISSET((u_int)db_rep->listen_fd, &reads) &&
		    (ret = __repmgr_accept(dbenv)) != 0)
			goto out;
	}
out:
	UNLOCK_MUTEX(db_rep->mutex);
	return (ret);
}

static int
finish_connecting(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	DB_REP *db_rep;
	REPMGR_SITE *site;
	socklen_t len;
	SITE_STRING_BUFFER buffer;
	u_int eid;
	int error, ret;

	len = sizeof(error);
	if (getsockopt(
	    conn->fd, SOL_SOCKET, SO_ERROR, (sockopt_t)&error, &len) < 0)
		goto err_rpt;
	if (error) {
		errno = error;
		goto err_rpt;
	}

	F_CLR(conn, CONN_CONNECTING);
	return (__repmgr_send_handshake(dbenv, conn));

err_rpt:
	db_rep = dbenv->rep_handle;

	DB_ASSERT(dbenv, IS_VALID_EID(conn->eid));
	eid = (u_int)conn->eid;

	site = SITE_FROM_EID(eid);
	__db_err(dbenv, errno,
	    "connecting to %s", __repmgr_format_site_loc(site, buffer));

	/* If we've exhausted the list of possible addresses, give up. */
	if (ADDR_LIST_NEXT(&site->net_addr) == NULL)
		return (DB_REP_UNAVAIL);

	/*
	 * This is just like a little mini-"bust_connection", except that we
	 * don't reschedule for later, 'cuz we're just about to try again right
	 * now.
	 *
	 * !!!
	 * Which means this must only be called on the select() thread, since
	 * only there are we allowed to actually close a connection.
	 */
	DB_ASSERT(dbenv, !TAILQ_EMPTY(&db_rep->connections));
	__repmgr_cleanup_connection(dbenv, conn);
	ret = __repmgr_connect_site(dbenv, eid);
	DB_ASSERT(dbenv, ret != DB_REP_UNAVAIL);
	return (ret);
}
