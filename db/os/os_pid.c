/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: os_pid.c,v 12.24 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#ifdef HAVE_MUTEX_SUPPORT
#include "dbinc/mutex_int.h"		/* Required to load appropriate
					   header files for thread functions. */
#endif

/*
 * __os_id --
 *	Return the current process ID.
 *
 * PUBLIC: void __os_id __P((DB_ENV *, pid_t *, db_threadid_t*));
 */
void
__os_id(dbenv, pidp, tidp)
	DB_ENV *dbenv;
	pid_t *pidp;
	db_threadid_t *tidp;
{
	/*
	 * We can't depend on dbenv not being NULL, this routine is called
	 * from places where there's no DB_ENV handle.  It takes a DB_ENV
	 * handle as an arg because it's the default DB_ENV->thread_id function.
	 *
	 * We cache the pid in the DB_ENV handle, it's a fairly slow call on
	 * lots of systems.
	 */
	if (pidp != NULL) {
		if (dbenv == NULL) {
#if defined(HAVE_VXWORKS)
			*pidp = taskIdSelf();
#else
			*pidp = getpid();
#endif
		} else
			*pidp = dbenv->pid_cache;
	}

	if (tidp != NULL) {
#if defined(DB_WIN32)
		*tidp = GetCurrentThreadId();
#elif defined(HAVE_MUTEX_UI_THREADS)
		*tidp = thr_self();
#elif defined(HAVE_MUTEX_SOLARIS_LWP) || \
	defined(HAVE_MUTEX_PTHREADS) || defined(HAVE_PTHREAD_API)
		*tidp = pthread_self();
#else
		/*
		 * Default to just getpid.
		 */
		*tidp = 0;
#endif
	}
}
