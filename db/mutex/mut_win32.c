/*
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: mut_win32.c,v 1.6 2002/07/12 04:05:00 mjc Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

/*
 * This is where we load in the actual test-and-set mutex code.
 */
#define	LOAD_ACTUAL_MUTEX_CODE
#include "db_int.h"

#define	GET_HANDLE(mutexp, event) do {					\
	int i;								\
	char idbuf[13];							\
									\
	if (F_ISSET(mutexp, MUTEX_THREAD)) {				\
		event = mutexp->event;					\
		return (0);						\
	}								\
									\
	for (i = 0; i < 8; i++)						\
		idbuf[(sizeof(idbuf) - 1) - i] =			\
		    "0123456789abcdef"[(mutexp->id >> (i * 4)) & 0xf];	\
	event = CreateEvent(NULL, TRUE, FALSE, idbuf);			\
	if (event == NULL)						\
		return (__os_win32_errno());				\
} while (0)

#define	RELEASE_HANDLE(mutexp, event)					\
	if (!F_ISSET(mutexp, MUTEX_THREAD) && event != NULL) {		\
		CloseHandle(event);					\
		event = NULL;						\
	}

/*
 * __db_win32_mutex_init --
 *	Initialize a DB_MUTEX.
 *
 * PUBLIC: int __db_win32_mutex_init __P((DB_ENV *, DB_MUTEX *, u_int32_t));
 */
int
__db_win32_mutex_init(dbenv, mutexp, flags)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
	u_int32_t flags;
{
	u_int32_t save;

	/*
	 * The only setting/checking of the MUTEX_MPOOL flags is in the mutex
	 * mutex allocation code (__db_mutex_alloc/free).  Preserve only that
	 * flag.  This is safe because even if this flag was never explicitly
	 * set, but happened to be set in memory, it will never be checked or
	 * acted upon.
	 */
	save = F_ISSET(mutexp, MUTEX_MPOOL);
	memset(mutexp, 0, sizeof(*mutexp));
	F_SET(mutexp, save);

	/*
	 * If this is a thread lock or the process has told us that there are
	 * no other processes in the environment, use thread-only locks, they
	 * are faster in some cases.
	 *
	 * This is where we decide to ignore locks we don't need to set -- if
	 * the application isn't threaded, there aren't any threads to block.
	 */
	if (LF_ISSET(MUTEX_THREAD) || F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		if (!F_ISSET(dbenv, DB_ENV_THREAD)) {
			F_SET(mutexp, MUTEX_IGNORE);
			return (0);
		}
		F_SET(mutexp, MUTEX_THREAD);
		mutexp->event = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (mutexp->event == NULL)
			return (__os_win32_errno());
	} else
		mutexp->id = ((getpid() & 0xffff) << 16) ^ (u_int32_t)mutexp;

	mutexp->spins = __os_spin(dbenv);
	F_SET(mutexp, MUTEX_INITED);

	return (0);
}

/*
 * __db_win32_mutex_lock
 *	Lock on a mutex, logically blocking if necessary.
 *
 * PUBLIC: int __db_win32_mutex_lock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_win32_mutex_lock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	HANDLE event;
	u_long ms;
	int nspins;

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

	event = NULL;
	ms = 50;

loop:	/* Attempt to acquire the resource for N spins. */
	for (nspins = mutexp->spins; nspins > 0; --nspins) {
		if (!MUTEX_SET(&mutexp->tas))
			continue;

		if (mutexp->locked) {
			/*
			 * If we are about to block for the first time,
			 * increment the waiter count while we still hold
			 * the mutex.
			 */
			if (nspins == 1 && event == NULL)
				++mutexp->nwaiters;
			MUTEX_UNSET(&mutexp->tas);
			continue;
		}

		mutexp->locked = 1;

		if (event == NULL)
			++mutexp->mutex_set_nowait;
		else {
			++mutexp->mutex_set_wait;
			--mutexp->nwaiters;
		}
		MUTEX_UNSET(&mutexp->tas);
		RELEASE_HANDLE(mutexp, event);

		return (0);
	}

	/*
	 * Yield the processor; wait 50 ms initially, up to 1 second.
	 * This loop is needed to work around an unlikely race where the signal
	 * from the unlocking thread gets lost.
	 */
	if (event == NULL)
		GET_HANDLE(mutexp, event);
	if (WaitForSingleObject(event, ms) == WAIT_FAILED)
		return (__os_win32_errno());

	if ((ms <<= 1) > MS_PER_SEC)
		ms = MS_PER_SEC;

	goto loop;
}

/*
 * __db_win32_mutex_unlock --
 *	Release a lock.
 *
 * PUBLIC: int __db_win32_mutex_unlock __P((DB_ENV *, DB_MUTEX *));
 */
int
__db_win32_mutex_unlock(dbenv, mutexp)
	DB_ENV *dbenv;
	DB_MUTEX *mutexp;
{
	int ret;
	HANDLE event;

	if (F_ISSET(dbenv, DB_ENV_NOLOCKING) || F_ISSET(mutexp, MUTEX_IGNORE))
		return (0);

#ifdef DIAGNOSTIC
	if (!mutexp->locked)
		__db_err(dbenv,
		    "__db_win32_mutex_unlock: ERROR: lock already unlocked");
#endif

	ret = 0;

	/* We have to drop the mutex inside a critical section */
	while (!MUTEX_SET(&mutexp->tas))
		;
	mutexp->locked = 0;
	MUTEX_UNSET(&mutexp->tas);

	if (mutexp->nwaiters > 0) {
		GET_HANDLE(mutexp, event);

		if (!PulseEvent(event))
			ret = __os_win32_errno();

		RELEASE_HANDLE(mutexp, event);
	}

#ifdef DIAGNOSTIC
	if (ret)
		__db_err(dbenv,
		    "__db_win32_mutex_unlock: ERROR: unlock failed");
#endif

	return (ret);
}

/*
 * __db_win32_mutex_destroy --
 *	Destroy a DB_MUTEX.
 *
 * PUBLIC: int __db_win32_mutex_destroy __P((DB_MUTEX *));
 */
int
__db_win32_mutex_destroy(mutexp)
	DB_MUTEX *mutexp;
{
	int ret;

	if (F_ISSET(mutexp, MUTEX_IGNORE) || !F_ISSET(mutexp, MUTEX_THREAD))
		return (0);

	ret = 0;
	if (mutexp->event != NULL) {
		if (!CloseHandle(mutexp->event))
			ret = __os_win32_errno();
		mutexp->event = NULL;
	}

	return (ret);
}
