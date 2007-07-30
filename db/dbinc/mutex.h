/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: mutex.h,v 12.28 2007/05/17 15:15:05 bostic Exp $
 */

#ifndef _DB_MUTEX_H_
#define	_DB_MUTEX_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Mutexes are represented by unsigned, 32-bit integral values.  As the
 * OOB value is 0, mutexes can be initialized by zero-ing out the memory
 * in which they reside.
 */
#define	MUTEX_INVALID	0

/*
 * We track mutex allocations by ID.
 */
#define	MTX_APPLICATION		 1
#define	MTX_DB_HANDLE		 2
#define	MTX_ENV_DBLIST		 3
#define	MTX_ENV_HANDLE		 4
#define	MTX_ENV_REGION		 5
#define	MTX_LOCK_REGION		 6
#define	MTX_LOGICAL_LOCK	 7
#define	MTX_LOG_FILENAME	 8
#define	MTX_LOG_FLUSH		 9
#define	MTX_LOG_HANDLE		10
#define	MTX_LOG_REGION		11
#define	MTX_MPOOLFILE_HANDLE	12
#define	MTX_MPOOL_FH		13
#define	MTX_MPOOL_FILE_BUCKET	14
#define	MTX_MPOOL_HANDLE	15
#define	MTX_MPOOL_HASH_BUCKET	16
#define	MTX_MPOOL_IO		17
#define	MTX_MPOOL_REGION	18
#define	MTX_MUTEX_REGION	19
#define	MTX_MUTEX_TEST		20
#define	MTX_REP_CKP		21
#define	MTX_REP_DATABASE	22
#define	MTX_REP_EVENT		23
#define	MTX_REP_REGION		24
#define	MTX_SEQUENCE		25
#define	MTX_TWISTER		26
#define	MTX_TXN_ACTIVE		27
#define	MTX_TXN_CHKPT		28
#define	MTX_TXN_COMMIT		29
#define	MTX_TXN_MVCC		30
#define	MTX_TXN_REGION		31

#define	MTX_MAX_ENTRY		31

/* Redirect mutex calls to the correct functions. */
#if !defined(HAVE_MUTEX_HYBRID) && (					\
    defined(HAVE_MUTEX_PTHREADS) ||					\
    defined(HAVE_MUTEX_SOLARIS_LWP) ||					\
    defined(HAVE_MUTEX_UI_THREADS))
#define	__mutex_init(a, b, c)		__db_pthread_mutex_init(a, b, c)
#define	__mutex_lock(a, b)		__db_pthread_mutex_lock(a, b)
#define	__mutex_unlock(a, b)		__db_pthread_mutex_unlock(a, b)
#define	__mutex_destroy(a, b)		__db_pthread_mutex_destroy(a, b)
#elif defined(HAVE_MUTEX_WIN32) || defined(HAVE_MUTEX_WIN32_GCC)
#define	__mutex_init(a, b, c)		__db_win32_mutex_init(a, b, c)
#define	__mutex_lock(a, b)		__db_win32_mutex_lock(a, b)
#define	__mutex_unlock(a, b)		__db_win32_mutex_unlock(a, b)
#define	__mutex_destroy(a, b)		__db_win32_mutex_destroy(a, b)
#elif defined(HAVE_MUTEX_FCNTL)
#define	__mutex_init(a, b, c)		__db_fcntl_mutex_init(a, b, c)
#define	__mutex_lock(a, b)		__db_fcntl_mutex_lock(a, b)
#define	__mutex_unlock(a, b)		__db_fcntl_mutex_unlock(a, b)
#define	__mutex_destroy(a, b)		__db_fcntl_mutex_destroy(a, b)
#else
#define	__mutex_init(a, b, c)		__db_tas_mutex_init(a, b, c)
#define	__mutex_lock(a, b)		__db_tas_mutex_lock(a, b)
#define	__mutex_unlock(a, b)		__db_tas_mutex_unlock(a, b)
#define	__mutex_destroy(a, b)		__db_tas_mutex_destroy(a, b)
#endif

/*
 * Lock/unlock a mutex.  If the mutex was never required, the thread of
 * control can proceed without it.
 *
 * We never fail to acquire or release a mutex without panicing.  Simplify
 * the macros to always return a panic value rather than saving the actual
 * return value of the mutex routine.
 */
#ifdef HAVE_MUTEX_SUPPORT
#define	MUTEX_LOCK(dbenv, mutex) do {					\
	if ((mutex) != MUTEX_INVALID &&					\
	    __mutex_lock(dbenv, mutex) != 0)				\
		return (DB_RUNRECOVERY);				\
} while (0)
#define	MUTEX_UNLOCK(dbenv, mutex) do {					\
	if ((mutex) != MUTEX_INVALID &&					\
	    __mutex_unlock(dbenv, mutex) != 0)				\
		return (DB_RUNRECOVERY);				\
} while (0)
#else
/*
 * XXX
 * There are calls to lock/unlock mutexes outside of #ifdef's -- replace
 * the call with something the compiler can discard, but which will make
 * if-then-else blocks work correctly.
 */
#define	MUTEX_LOCK(dbenv, mutex)					\
	(mutex) = (mutex);
#define	MUTEX_UNLOCK(dbenv, mutex)					\
	(mutex) = (mutex);
#endif

/*
 * Berkeley DB ports may require single-threading at places in the code.
 */
#ifdef HAVE_MUTEX_VXWORKS
#include "taskLib.h"
/*
 * Use the taskLock() mutex to eliminate a race where two tasks are
 * trying to initialize the global lock at the same time.
 */
#define	DB_BEGIN_SINGLE_THREAD do {					\
	if (DB_GLOBAL(db_global_init))					\
		(void)semTake(DB_GLOBAL(db_global_lock), WAIT_FOREVER);	\
	else {								\
		taskLock();						\
		if (DB_GLOBAL(db_global_init)) {			\
			taskUnlock();					\
			(void)semTake(DB_GLOBAL(db_global_lock),	\
			    WAIT_FOREVER);				\
			continue;					\
		}							\
		DB_GLOBAL(db_global_lock) =				\
		    semBCreate(SEM_Q_FIFO, SEM_EMPTY);			\
		if (DB_GLOBAL(db_global_lock) != NULL)			\
			DB_GLOBAL(db_global_init) = 1;			\
		taskUnlock();						\
	}								\
} while (DB_GLOBAL(db_global_init) == 0)
#define	DB_END_SINGLE_THREAD	(void)semGive(DB_GLOBAL(db_global_lock))
#endif

/*
 * Single-threading defaults to a no-op.
 */
#ifndef DB_BEGIN_SINGLE_THREAD
#define	DB_BEGIN_SINGLE_THREAD
#endif
#ifndef DB_END_SINGLE_THREAD
#define	DB_END_SINGLE_THREAD
#endif

#if defined(__cplusplus)
}
#endif

#include "dbinc_auto/mutex_ext.h"
#endif /* !_DB_MUTEX_H_ */
