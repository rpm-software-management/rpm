/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005,2007 Oracle.  All rights reserved.
 *
 * $Id: repmgr_windows.c,v 1.22 2007/06/11 18:29:34 alanb Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

typedef struct __ack_waiter {
	HANDLE event;
	const DB_LSN *lsnp;
	struct __ack_waiter *next_free;
} ACK_WAITER;

#define	WAITER_SLOT_IN_USE(w) ((w)->lsnp != NULL)

/*
 * Array slots [0:next_avail-1] are initialized, and either in use or on the
 * free list.  Slots beyond that are virgin territory, whose memory contents
 * could be garbage.  In particular, note that slots [0:next_avail-1] have a
 * Win32 Event Object created for them, which have to be freed when cleaning up
 * this data structure.
 *
 * "first_free" points to a list of not-in-use slots threaded through the first
 * section of the array.
 */
struct __ack_waiters_table {
	struct __ack_waiter *array;
	int size;
	int next_avail;
	struct __ack_waiter *first_free;
};

static int allocate_wait_slot __P((DB_ENV *, ACK_WAITER **));
static void free_wait_slot __P((DB_ENV *, ACK_WAITER *));
static int handle_completion __P((DB_ENV *, REPMGR_CONNECTION *));
static int finish_connecting __P((DB_ENV *, REPMGR_CONNECTION *,
				     LPWSANETWORKEVENTS));

int
__repmgr_thread_start(dbenv, runnable)
	DB_ENV *dbenv;
	REPMGR_RUNNABLE *runnable;
{
	HANDLE thread_id;

	runnable->finished = FALSE;

	thread_id = CreateThread(NULL, 0,
	    (LPTHREAD_START_ROUTINE)runnable->run, dbenv, 0, NULL);
	if (thread_id == NULL)
		return (GetLastError());
	runnable->thread_id = thread_id;
	return (0);
}

int
__repmgr_thread_join(thread)
	REPMGR_RUNNABLE *thread;
{
	if (WaitForSingleObject(thread->thread_id, INFINITE) == WAIT_OBJECT_0)
		return (0);
	return (GetLastError());
}

int
__repmgr_set_nonblocking(s)
	SOCKET s;
{
	int ret;
	u_long arg;

	arg = 1;		/* any non-zero value */
	if ((ret = ioctlsocket(s, FIONBIO, &arg)) == SOCKET_ERROR)
		return (WSAGetLastError());
	return (0);
}

/*
 * Wake any send()-ing threads waiting for an acknowledgement.
 *
 * !!!
 * Caller must hold the repmgr->mutex, if this thread synchronization is to work
 * properly.
 */
int
__repmgr_wake_waiting_senders(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	ACK_WAITER *slot;
	int i, ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	for (i=0; i<db_rep->waiters->next_avail; i++) {
		 slot = &db_rep->waiters->array[i];
		 if (!WAITER_SLOT_IN_USE(slot))
			 continue;
		 if (__repmgr_is_permanent(dbenv, slot->lsnp))
			 if (!SetEvent(slot->event) && ret == 0)
				 ret = GetLastError();
	}
	return (ret);
}

/*
 * !!!
 * Caller must hold mutex.
 */
int
__repmgr_await_ack(dbenv, lsnp)
	DB_ENV *dbenv;
	const DB_LSN *lsnp;
{
	DB_REP *db_rep;
	ACK_WAITER *me;
	DWORD ret;
	DWORD timeout;

	db_rep = dbenv->rep_handle;

	if ((ret = allocate_wait_slot(dbenv, &me)) != 0)
		goto err;

	/* convert time-out from microseconds to milliseconds, rounding up */
	timeout = db_rep->ack_timeout > 0 ?
	    ((db_rep->ack_timeout + (US_PER_MS - 1)) / US_PER_MS) : INFINITE;
	me->lsnp = lsnp;
	if ((ret = SignalObjectAndWait(db_rep->mutex, me->event, timeout,
	    FALSE)) == WAIT_FAILED) {
		ret = GetLastError();
	} else if (ret == WAIT_TIMEOUT)
		ret = DB_REP_UNAVAIL;
	else
		DB_ASSERT(dbenv, ret == WAIT_OBJECT_0);

	LOCK_MUTEX(db_rep->mutex);
	free_wait_slot(dbenv, me);

err:
	return (ret);
}

/*
 * !!!
 * Caller must hold the mutex.
 */
static int
allocate_wait_slot(dbenv, resultp)
	DB_ENV *dbenv;
	ACK_WAITER **resultp;
{
	DB_REP *db_rep;
	ACK_WAITERS_TABLE *table;
	ACK_WAITER *w;
	int ret;

	db_rep = dbenv->rep_handle;
	table = db_rep->waiters;
	if (table->first_free == NULL) {
		if (table->next_avail >= table->size) {
			/*
			 * Grow the array.
			 */
			table->size *= 2;
			w = table->array;
			if ((ret = __os_realloc(dbenv, table->size * sizeof(*w),
			     &w)) != 0)
				return (ret);
			table->array = w;
		}
		/*
		 * Here if, one way or another, we're good to go for using the
		 * next slot (for the first time).
		 */
		w = &table->array[table->next_avail++];
		if ((w->event = CreateEvent(NULL, FALSE, FALSE, NULL)) ==
		    NULL) {
			/*
			 * Maintain the sanctity of our rule that
			 * [0:next_avail-1] contain valid Event Objects.
			 */
			--table->next_avail;
			return (GetLastError());
		}
	} else {
		w = table->first_free;
		table->first_free = w->next_free;
	}
	*resultp = w;
	return (0);
}

static void
free_wait_slot(dbenv, slot)
	DB_ENV *dbenv;
	ACK_WAITER *slot;
{
	DB_REP *db_rep;

	db_rep = dbenv->rep_handle;

	slot->lsnp = NULL;	/* show it's not in use */
	slot->next_free = db_rep->waiters->first_free;
	db_rep->waiters->first_free = slot;
}

/*
 * Make resource allocation an all-or-nothing affair, outside of this and the
 * close_sync function.  db_rep->waiters should be non-NULL iff all of these
 * resources have been created.
 */
int
__repmgr_init_sync(dbenv, db_rep)
     DB_ENV *dbenv;
     DB_REP *db_rep;
{
#define	INITIAL_ALLOCATION 5		/* arbitrary size */
	ACK_WAITERS_TABLE *table;
	int ret;

	db_rep->signaler = db_rep->queue_nonempty = db_rep->check_election =
	    db_rep->mutex = NULL;
	table = NULL;

	if ((db_rep->signaler = CreateEvent(NULL, /* security attr */
	    FALSE,	/* (not) of the manual reset variety  */
	    FALSE,		/* (not) initially signaled */
	    NULL)) == NULL)		/* name */
		goto geterr;

	if ((db_rep->queue_nonempty = CreateEvent(NULL, TRUE, FALSE, NULL))
	    == NULL)
		goto geterr;

	if ((db_rep->check_election = CreateEvent(NULL, FALSE, FALSE, NULL))
	    == NULL)
		goto geterr;

	if ((db_rep->mutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
		goto geterr;

	if ((ret = __os_calloc(dbenv, 1, sizeof(ACK_WAITERS_TABLE), &table))
	    != 0)
		goto err;

	if ((ret = __os_calloc(dbenv, INITIAL_ALLOCATION, sizeof(ACK_WAITER),
	    &table->array)) != 0)
		goto err;

	table->size = INITIAL_ALLOCATION;
	table->first_free = NULL;
	table->next_avail = 0;

	/* There's a restaurant joke in there somewhere. */
	db_rep->waiters = table;
	return (0);

geterr:
	ret = GetLastError();
err:
	if (db_rep->check_election != NULL)
		CloseHandle(db_rep->check_election);
	if (db_rep->queue_nonempty != NULL)
		CloseHandle(db_rep->queue_nonempty);
	if (db_rep->signaler != NULL)
		CloseHandle(db_rep->signaler);
	if (db_rep->mutex != NULL)
		CloseHandle(db_rep->mutex);
	if (table != NULL)
		__os_free(dbenv, table);
	db_rep->waiters = NULL;
	return (ret);
}

int
__repmgr_close_sync(dbenv)
     DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int i, ret;

	db_rep = dbenv->rep_handle;
	if (!(REPMGR_SYNC_INITED(db_rep)))
		return (0);

	ret = 0;
	for (i = 0; i < db_rep->waiters->next_avail; i++) {
		if (!CloseHandle(db_rep->waiters->array[i].event) && ret == 0)
			ret = GetLastError();
	}
	__os_free(dbenv, db_rep->waiters->array);
	__os_free(dbenv, db_rep->waiters);

	if (!CloseHandle(db_rep->check_election) && ret == 0)
		ret = GetLastError();

	if (!CloseHandle(db_rep->queue_nonempty) && ret == 0)
		ret = GetLastError();

	if (!CloseHandle(db_rep->signaler) && ret == 0)
		ret = GetLastError();

	if (!CloseHandle(db_rep->mutex) && ret == 0)
		ret = GetLastError();

	db_rep->waiters = NULL;
	return (ret);
}

/*
 * Performs net-related resource initialization other than memory initialization
 * and allocation.  A valid db_rep->listen_fd acts as the "all-or-nothing"
 * sentinel signifying that these resources are allocated (except that now the
 * new wsa_inited flag may be used to indicate that WSAStartup has already been
 * called).
 */
int
__repmgr_net_init(dbenv, db_rep)
	DB_ENV *dbenv;
	DB_REP *db_rep;
{
	int ret;

	/* Initialize the Windows sockets DLL. */
	if (!db_rep->wsa_inited && (ret = __repmgr_wsa_init(dbenv)) != 0)
		goto err;

	if ((ret = __repmgr_listen(dbenv)) == 0)
		return (0);

	if (WSACleanup() == SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "WSACleanup");
	}

err:	db_rep->listen_fd = INVALID_SOCKET;
	return (ret);
}

/*
 * __repmgr_wsa_init --
 *	Initialize the Windows sockets DLL.
 *
 * PUBLIC: int __repmgr_wsa_init __P((DB_ENV *));
 */
int
__repmgr_wsa_init(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	WSADATA wsaData;
	int ret;

	db_rep = dbenv->rep_handle;

	if ((ret = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		__db_err(dbenv, ret, "unable to initialize Windows networking");
		return (ret);
	}
	db_rep->wsa_inited = TRUE;

	return (0);
}

int
__repmgr_lock_mutex(mutex)
	mgr_mutex_t  *mutex;
{
	if (WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0)
		return (0);
	return (GetLastError());
}

int
__repmgr_unlock_mutex(mutex)
	mgr_mutex_t  *mutex;
{
	if (ReleaseMutex(*mutex))
		return (0);
	return (GetLastError());
}

int
__repmgr_signal(v)
	cond_var_t *v;
{
	return (SetEvent(*v) ? 0 : GetLastError());
}

int
__repmgr_wake_main_thread(dbenv)
	DB_ENV *dbenv;
{
	if (!SetEvent(dbenv->rep_handle->signaler))
		return (GetLastError());
	return (0);
}

int
__repmgr_writev(fd, iovec, buf_count, byte_count_p)
	socket_t fd;
	db_iovec_t *iovec;
	int buf_count;
	size_t *byte_count_p;
{
	DWORD bytes;

	if (WSASend(fd, iovec,
	    (DWORD)buf_count, &bytes, 0, NULL, NULL) == SOCKET_ERROR)
		return (net_errno);

	*byte_count_p = (size_t)bytes;
	return (0);
}

int
__repmgr_readv(fd, iovec, buf_count, xfr_count_p)
	socket_t fd;
	db_iovec_t *iovec;
	int buf_count;
	size_t *xfr_count_p;
{
	DWORD bytes, flags;

	flags = 0;
	if (WSARecv(fd, iovec,
	    (DWORD)buf_count, &bytes, &flags, NULL, NULL) == SOCKET_ERROR)
		return (net_errno);

	*xfr_count_p = (size_t)bytes;
	return (0);
}

int
__repmgr_select_loop(dbenv)
	DB_ENV *dbenv;
{
	DWORD select_timeout;
	DB_REP *db_rep;
	REPMGR_CONNECTION *conn, *next;
	REPMGR_RETRY *retry;
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	REPMGR_CONNECTION *connections[WSA_MAXIMUM_WAIT_EVENTS];
	DWORD nevents, ret;
	db_timespec timeout;
	WSAEVENT listen_event;
	WSANETWORKEVENTS net_events;
	int flow_control, i;

	db_rep = dbenv->rep_handle;

	if ((listen_event = WSACreateEvent()) == WSA_INVALID_EVENT) {
		__db_err(
		    dbenv, net_errno, "can't create event for listen socket");
		return (net_errno);
	}
	if (WSAEventSelect(db_rep->listen_fd, listen_event, FD_ACCEPT) ==
	    SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "can't enable event for listener");
		goto out;
	}

	LOCK_MUTEX(db_rep->mutex);
	if ((ret = __repmgr_first_try_connections(dbenv)) != 0)
		goto unlock;
	flow_control = FALSE;
	for (;;) {
		/* Start with the two events that we always wait for. */
		events[0] = db_rep->signaler;
		events[1] = listen_event;
		nevents = 2;

		/*
		 * Add an event for each surviving socket that we're interested
		 * in.  (For now [until we implement flow control], that's all
		 * of them, in one form or another.)
		 *     Note that even if we're suffering flow control, we
		 * nevertheless still read if we haven't even yet gotten a
		 * handshake.   Why?  (1) Handshakes are important; and (2) they
		 * don't hurt anything flow-control-wise.
		 */
		TAILQ_FOREACH(conn, &db_rep->connections, entries) {
			if (F_ISSET(conn, CONN_CONNECTING) ||
			    !STAILQ_EMPTY(&conn->outbound_queue) ||
			    (!flow_control || !IS_VALID_EID(conn->eid))) {
				events[nevents] = conn->event_object;
				connections[nevents++] = conn;
			}
		}

		/*
		 * Decide how long to wait based on when it will next be time to
		 * retry an idle connection.  (List items are in order, so we
		 * only have to examine the first one.)
		 */
		if (TAILQ_EMPTY(&db_rep->retries))
			select_timeout = WSA_INFINITE;
		else {
			retry = TAILQ_FIRST(&db_rep->retries);

			__repmgr_timespec_diff_now(
			    dbenv, &retry->time, &timeout);
			select_timeout =
			    (DWORD)(timeout.tv_sec * MS_PER_SEC +
			    timeout.tv_nsec / NS_PER_MS);
		}

		UNLOCK_MUTEX(db_rep->mutex);
		ret = WSAWaitForMultipleEvents(
		    nevents, events, FALSE, select_timeout, FALSE);
		if (db_rep->finished) {
			ret = 0;
			goto out;
		}
		LOCK_MUTEX(db_rep->mutex);

		/*
		 * The first priority thing we must do is to clean up any
		 * pending defunct connections.  Otherwise, if they have any
		 * lingering pending input, we get very confused if we try to
		 * process it.
		 *     Loop just like TAILQ_FOREACH, except that we need to be
		 * able to unlink a list entry.
		 */
		for (conn = TAILQ_FIRST(&db_rep->connections);
		     conn != NULL;
		     conn = next) {
			next = TAILQ_NEXT(conn, entries);
			if (F_ISSET(conn, CONN_DEFUNCT))
				__repmgr_cleanup_connection(dbenv, conn);
		}

		/*
		 * !!!
		 * Note that `ret' remains set as the return code from
		 * WSAWaitForMultipleEvents, above.
		 */
		if (ret >= WSA_WAIT_EVENT_0 &&
		    ret < WSA_WAIT_EVENT_0 + nevents) {
			switch (i = ret - WSA_WAIT_EVENT_0) {
			case 0:
				/* Another thread woke us. */
				break;
			case 1:
				if ((ret = WSAEnumNetworkEvents(
				    db_rep->listen_fd, listen_event,
				    &net_events)) == SOCKET_ERROR) {
					ret = net_errno;
					goto unlock;
				}
				DB_ASSERT(dbenv,
				    net_events.lNetworkEvents & FD_ACCEPT);
				if ((ret = net_events.iErrorCode[FD_ACCEPT_BIT])
				    != 0)
					goto unlock;
				if ((ret = __repmgr_accept(dbenv)) != 0)
					goto unlock;
				break;
			default:
				if ((ret = handle_completion(dbenv,
					 connections[i])) != 0)
					goto unlock;
				break;
			}
		} else if (ret == WSA_WAIT_TIMEOUT) {
			if ((ret = __repmgr_retry_connections(dbenv)) != 0)
				goto unlock;
		} else if (ret == WSA_WAIT_FAILED) {
			ret = net_errno;
			goto unlock;
		}
	}

unlock:
	UNLOCK_MUTEX(db_rep->mutex);
out:
	if (!CloseHandle(listen_event) && ret == 0)
		ret = GetLastError();
	return (ret);
}

/*
 * !!!
 * Only ever called on the select() thread, since we may call
 * __repmgr_bust_connection(..., TRUE).
 */
static int
handle_completion(dbenv, conn)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
{
	int ret;
	WSANETWORKEVENTS events;

	if ((ret = WSAEnumNetworkEvents(conn->fd, conn->event_object, &events))
	    == SOCKET_ERROR) {
		__db_err(dbenv, net_errno, "EnumNetworkEvents");
		STAT(dbenv->rep_handle->region->mstat.st_connection_drop++);
		ret = DB_REP_UNAVAIL;
		goto err;
	}

	if (F_ISSET(conn, CONN_CONNECTING)) {
		if ((ret = finish_connecting(dbenv, conn, &events)) != 0)
			goto err;
	} else {		/* Check both writing and reading. */
		if (events.lNetworkEvents & FD_CLOSE) {
			__db_err(dbenv,
			    events.iErrorCode[FD_CLOSE_BIT],
			    "connection closed");
			STAT(dbenv->rep_handle->
			    region->mstat.st_connection_drop++);
			ret = DB_REP_UNAVAIL;
			goto err;
		}

		if (events.lNetworkEvents & FD_WRITE) {
			if (events.iErrorCode[FD_WRITE_BIT] != 0) {
				__db_err(dbenv,
				    events.iErrorCode[FD_WRITE_BIT],
				    "error writing");
				STAT(dbenv->rep_handle->
				    region->mstat.st_connection_drop++);
				ret = DB_REP_UNAVAIL;
				goto err;
			} else if ((ret =
			    __repmgr_write_some(dbenv, conn)) != 0)
				goto err;
		}

		if (events.lNetworkEvents & FD_READ) {
			if (events.iErrorCode[FD_READ_BIT] != 0) {
				__db_err(dbenv,
				    events.iErrorCode[FD_READ_BIT],
				    "error reading");
				STAT(dbenv->rep_handle->
				    region->mstat.st_connection_drop++);
				ret = DB_REP_UNAVAIL;
				goto err;
			} else if ((ret =
			    __repmgr_read_from_site(dbenv, conn)) != 0)
				goto err;
		}
	}

	return (0);

err:	if (ret == DB_REP_UNAVAIL)
		return (__repmgr_bust_connection(dbenv, conn, TRUE));
	return (ret);
}

static int
finish_connecting(dbenv, conn, events)
	DB_ENV *dbenv;
	REPMGR_CONNECTION *conn;
	LPWSANETWORKEVENTS events;
{
	DB_REP *db_rep;
	u_int eid;
/*	char reason[100]; */
	int ret/*, t_ret*/;
/*	DWORD_PTR values[1]; */

	if (!(events->lNetworkEvents & FD_CONNECT))
		return (0);

	F_CLR(conn, CONN_CONNECTING);

	if ((ret = events->iErrorCode[FD_CONNECT_BIT]) != 0) {
/*		t_ret = FormatMessage( */
/*		    FORMAT_MESSAGE_IGNORE_INSERTS | */
/*		    FORMAT_MESSAGE_FROM_SYSTEM | */
/*		    FORMAT_MESSAGE_ARGUMENT_ARRAY, */
/*		    NULL, ret, 0, (LPTSTR)reason, sizeof(reason), values); */
/*		__db_err(dbenv/\*, ret*\/, "connecting: %s", */
/*		    reason); */
/*		LocalFree(reason); */
		__db_err(dbenv, ret, "connecting");
		goto err;
	}

	if (WSAEventSelect(conn->fd, conn->event_object, FD_READ | FD_CLOSE) ==
	    SOCKET_ERROR) {
		ret = net_errno;
		__db_err(dbenv, ret, "setting event bits for reading");
		return (ret);
	}

	return (__repmgr_send_handshake(dbenv, conn));

err:
	db_rep = dbenv->rep_handle;
	eid = conn->eid;
	DB_ASSERT(dbenv, IS_VALID_EID(eid));

	if (ADDR_LIST_NEXT(&SITE_FROM_EID(eid)->net_addr) == NULL) {
		STAT(db_rep->region->mstat.st_connect_fail++);
		return (DB_REP_UNAVAIL);
	}

	DB_ASSERT(dbenv, !TAILQ_EMPTY(&db_rep->connections));
	__repmgr_cleanup_connection(dbenv, conn);
	ret = __repmgr_connect_site(dbenv, eid);
	DB_ASSERT(dbenv, ret != DB_REP_UNAVAIL);
	return (ret);
}
