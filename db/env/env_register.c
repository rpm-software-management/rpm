/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004,2007 Oracle.  All rights reserved.
 *
 * $Id: env_register.c,v 1.35 2007/05/17 15:15:11 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#define	REGISTER_FILE	"__db.register"

#define	PID_EMPTY	"X                      0\n"	/* Unused PID entry */
#define	PID_FMT		"%24lu\n"			/* PID entry format */
							/* Unused PID test */
#define	PID_ISEMPTY(p)	(memcmp(p, PID_EMPTY, PID_LEN) == 0)
#define	PID_LEN		(25)				/* PID entry length */

#define	REGISTRY_LOCK(dbenv, pos, nowait)				\
	__os_fdlock(dbenv, (dbenv)->registry, (off_t)(pos), 1, nowait)
#define	REGISTRY_UNLOCK(dbenv, pos)					\
	__os_fdlock(dbenv, (dbenv)->registry, (off_t)(pos), 0, 0)
#define	REGISTRY_EXCL_LOCK(dbenv, nowait)				\
	REGISTRY_LOCK(dbenv, 1, nowait)
#define	REGISTRY_EXCL_UNLOCK(dbenv)					\
	REGISTRY_UNLOCK(dbenv, 1)

static  int __envreg_add __P((DB_ENV *, int *));

/*
 * Support for portable, multi-process database environment locking, based on
 * the Subversion SR (#11511).
 *
 * The registry feature is configured by specifying the DB_REGISTER flag to the
 * DbEnv.open method.  If DB_REGISTER is specified, DB opens the registry file
 * in the database environment home directory.  The registry file is formatted
 * as follows:
 *
 *	                    12345		# process ID slot 1
 *	X		# empty slot
 *	                    12346		# process ID slot 2
 *	X		# empty slot
 *	                    12347		# process ID slot 3
 *	                    12348		# process ID slot 4
 *	X                   12349		# empty slot
 *	X		# empty slot
 *
 * All lines are fixed-length.  All lines are process ID slots.  Empty slots
 * are marked with leading non-digit characters.
 *
 * To modify the file, you get an exclusive lock on the first byte of the file.
 *
 * While holding any DbEnv handle, each process has an exclusive lock on the
 * first byte of a process ID slot.  There is a restriction on having more
 * than one DbEnv handle open at a time, because Berkeley DB uses per-process
 * locking to implement this feature, that is, a process may never have more
 * than a single slot locked.
 *
 * This work requires that if a process dies or the system crashes, locks held
 * by the dying processes will be dropped.  (We can't use system shared
 * memory-backed or filesystem-backed locks because they're persistent when a
 * process dies.)  On POSIX systems, we use fcntl(2) locks; on Win32 we have
 * LockFileEx/UnlockFile, except for Win/9X and Win/ME which have to loop on
 * Lockfile/UnlockFile.
 *
 * We could implement the same solution with flock locking instead of fcntl,
 * but flock would require a separate file for each process of control (and
 * probably each DbEnv handle) in the database environment, which is fairly
 * ugly.
 *
 * Whenever a process opens a new DbEnv handle, it walks the registry file and
 * verifies it CANNOT acquire the lock for any non-empty slot.  If a lock for
 * a non-empty slot is available, we know a process died holding an open handle,
 * and recovery needs to be run.
 *
 * It's possible to get corruption in the registry file.  If a write system
 * call fails after partially completing, there can be corrupted entries in
 * the registry file, or a partial entry at the end of the file.  This is OK.
 * A corrupted entry will be flagged as a non-empty line during the registry
 * file walk.  Since the line was corrupted by process failure, no process will
 * hold a lock on the slot, which will lead to recovery being run.
 *
 * There can still be processes running in the environment when we recover it,
 * and, in fact, there can still be processes running in the old environment
 * after we're up and running in a new one.  This is safe because performing
 * recovery panics (and removes) the existing environment, so the window of
 * vulnerability is small.  Further, we check the panic flag in the DB API
 * methods, when waking from spinning on a mutex, and whenever we're about to
 * write to disk).  The only window of corruption is if the write check of the
 * panic were to complete, the region subsequently be recovered, and then the
 * write continues.  That's very, very unlikely to happen.  This vulnerability
 * already exists in Berkeley DB, too, the registry code doesn't make it any
 * worse than it already is.
 */
/*
 * __envreg_register --
 *	Register a DB_ENV handle.
 *
 * PUBLIC: int __envreg_register __P((DB_ENV *, int *));
 */
int
__envreg_register(dbenv, need_recoveryp)
	DB_ENV *dbenv;
	int *need_recoveryp;
{
	pid_t pid;
	u_int32_t bytes, mbytes;
	int ret;
	char *pp;

	*need_recoveryp = 0;
	dbenv->thread_id(dbenv, &pid, NULL);
	pp = NULL;

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv, "%lu: register environment", (u_long)pid);

	/* Build the path name and open the registry file. */
	if ((ret =
	    __db_appname(dbenv, DB_APP_NONE, REGISTER_FILE, 0, NULL, &pp)) != 0)
		goto err;
	if ((ret = __os_open(dbenv, pp, 0,
	    DB_OSO_CREATE, __db_omode("rw-rw----"), &dbenv->registry)) != 0)
		goto err;

	/*
	 * Wait for an exclusive lock on the file.
	 *
	 * !!!
	 * We're locking bytes that don't yet exist, but that's OK as far as
	 * I know.
	 */
	if ((ret = REGISTRY_EXCL_LOCK(dbenv, 0)) != 0)
		goto err;

	/*
	 * If the file size is 0, initialize the file.
	 *
	 * Run recovery if we create the file, that means we can clean up the
	 * system by removing the registry file and restarting the application.
	 */
	if ((ret = __os_ioinfo(
	    dbenv, pp, dbenv->registry, &mbytes, &bytes, NULL)) != 0)
		goto err;
	if (mbytes == 0 && bytes == 0) {
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
			__db_msg(dbenv, "%lu: creating %s", (u_long)pid, pp);
		*need_recoveryp = 1;
	}

	/* Register this process. */
	if ((ret = __envreg_add(dbenv, need_recoveryp)) != 0)
		goto err;

	/*
	 * Release our exclusive lock if we don't need to run recovery.  If
	 * we need to run recovery, DB_ENV->open will call back into register
	 * code once recovery has completed.
	 */
	if (*need_recoveryp == 0 && (ret = REGISTRY_EXCL_UNLOCK(dbenv)) != 0)
		goto err;

	if (0) {
err:		*need_recoveryp = 0;

		/*
		 * !!!
		 * Closing the file handle must release all of our locks.
		 */
		if (dbenv->registry != NULL)
			(void)__os_closehandle(dbenv, dbenv->registry);
		dbenv->registry = NULL;
	}

	if (pp != NULL)
		__os_free(dbenv, pp);

	return (ret);
}

/*
 * __envreg_add --
 *	Add the process' pid to the register.
 */
static int
__envreg_add(dbenv, need_recoveryp)
	DB_ENV *dbenv;
	int *need_recoveryp;
{
	pid_t pid;
	off_t end, pos;
	size_t nr, nw;
	u_int lcnt;
	u_int32_t bytes, mbytes;
	int need_recovery, ret;
	char *p, buf[PID_LEN + 10], pid_buf[PID_LEN + 10];

	need_recovery = 0;
	COMPQUIET(p, NULL);

	/* Get a copy of our process ID. */
	dbenv->thread_id(dbenv, &pid, NULL);
	snprintf(pid_buf, sizeof(pid_buf), PID_FMT, (u_long)pid);

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv, "%lu: adding self to registry", (u_long)pid);

	/*
	 * Read the file.  Skip empty slots, and check that a lock is held
	 * for any allocated slots.  An allocated slot which we can lock
	 * indicates a process died holding a handle and recovery needs to
	 * be run.
	 */
	for (lcnt = 0;; ++lcnt) {
		if ((ret = __os_read(
		    dbenv, dbenv->registry, buf, PID_LEN, &nr)) != 0)
			return (ret);
		if (nr == 0)
			break;

		/*
		 * A partial record at the end of the file is possible if a
		 * previously un-registered process was interrupted while
		 * registering.
		 */
		if (nr != PID_LEN) {
			need_recovery = 1;
			break;
		}

		if (PID_ISEMPTY(buf)) {
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv, "%02u: EMPTY", lcnt);
			continue;
		}

		/*
		 * !!!
		 * DB_REGISTER is implemented using per-process locking, only
		 * a single DB_ENV handle may be open per process.  Enforce
		 * that restriction.
		 */
		if (memcmp(buf, pid_buf, PID_LEN) == 0) {
			__db_errx(dbenv,
    "DB_REGISTER limits processes to one open DB_ENV handle per environment");
			return (EINVAL);
		}

		if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER)) {
			for (p = buf; *p == ' ';)
				++p;
			buf[nr - 1] = '\0';
		}

		pos = (off_t)lcnt * PID_LEN;
		if (REGISTRY_LOCK(dbenv, pos, 1) == 0) {
			if ((ret = REGISTRY_UNLOCK(dbenv, pos)) != 0)
				return (ret);

			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv, "%02u: %s: FAILED", lcnt, p);

			need_recovery = 1;
			break;
		} else
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv, "%02u: %s: LOCKED", lcnt, p);
	}

	/*
	 * If we have to perform recovery...
	 *
	 * Mark all slots empty.  Registry ignores empty slots we can't lock,
	 * so it doesn't matter if any of the processes are in the middle of
	 * exiting Berkeley DB -- they'll discard their lock when they exit.
	 */
	if (need_recovery) {
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
			__db_msg(dbenv, "%lu: recovery required", (u_long)pid);

		/* Figure out how big the file is. */
		if ((ret = __os_ioinfo(
		    dbenv, NULL, dbenv->registry, &mbytes, &bytes, NULL)) != 0)
			return (ret);
		end = (off_t)mbytes * MEGABYTE + bytes;

		/*
		 * Seek to the beginning of the file and overwrite slots to
		 * the end of the file.
		 *
		 * It's possible for there to be a partial entry at the end of
		 * the file if a process died when trying to register.  If so,
		 * correct for it and overwrite it as well.
		 */
		if ((ret = __os_seek(dbenv, dbenv->registry, 0, 0, 0)) != 0)
			return (ret);
		for (lcnt = (u_int)end / PID_LEN +
		    ((u_int)end % PID_LEN == 0 ? 0 : 1); lcnt > 0; --lcnt)
			if ((ret = __os_write(dbenv,
			    dbenv->registry, PID_EMPTY, PID_LEN, &nw)) != 0)
				return (ret);
	}

	/*
	 * Seek to the first process slot and add ourselves to the first empty
	 * slot we can lock.
	 */
	if ((ret = __os_seek(dbenv, dbenv->registry, 0, 0, 0)) != 0)
		return (ret);
	for (lcnt = 0;; ++lcnt) {
		if ((ret = __os_read(
		    dbenv, dbenv->registry, buf, PID_LEN, &nr)) != 0)
			return (ret);
		if (nr == PID_LEN && !PID_ISEMPTY(buf))
			continue;
		pos = (off_t)lcnt * PID_LEN;
		if (REGISTRY_LOCK(dbenv, pos, 1) == 0) {
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv,
				    "%lu: locking slot %02u at offset %lu",
				    (u_long)pid, lcnt, (u_long)pos);

			if ((ret = __os_seek(dbenv,
			    dbenv->registry, 0, 0, (u_int32_t)pos)) != 0 ||
			    (ret = __os_write(dbenv,
			    dbenv->registry, pid_buf, PID_LEN, &nw)) != 0)
				return (ret);
			dbenv->registry_off = (u_int32_t)pos;
			break;
		}
	}

	if (need_recovery)
		*need_recoveryp = 1;

	return (ret);
}

/*
 * __envreg_unregister --
 *	Unregister a DB_ENV handle.
 *
 * PUBLIC: int __envreg_unregister __P((DB_ENV *, int));
 */
int
__envreg_unregister(dbenv, recovery_failed)
	DB_ENV *dbenv;
	int recovery_failed;
{
	size_t nw;
	int ret, t_ret;

	ret = 0;

	/*
	 * If recovery failed, we want to drop our locks and return, but still
	 * make sure any subsequent process doesn't decide everything is just
	 * fine and try to get into the database environment.  In the case of
	 * an error, discard our locks, but leave our slot filled-in.
	 */
	if (recovery_failed)
		goto err;

	/*
	 * Why isn't an exclusive lock necessary to discard a DB_ENV handle?
	 *
	 * We mark our process ID slot empty before we discard the process slot
	 * lock, and threads of control reviewing the register file ignore any
	 * slots which they can't lock.
	 */
	if ((ret = __os_seek(dbenv,
	    dbenv->registry, 0, 0, dbenv->registry_off)) != 0 ||
	    (ret = __os_write(
	    dbenv, dbenv->registry, PID_EMPTY, PID_LEN, &nw)) != 0)
		goto err;

	/*
	 * !!!
	 * This code assumes that closing the file descriptor discards all
	 * held locks.
	 *
	 * !!!
	 * There is an ordering problem here -- in the case of a process that
	 * failed in recovery, we're unlocking both the exclusive lock and our
	 * slot lock.  If the OS unlocked the exclusive lock and then allowed
	 * another thread of control to acquire the exclusive lock before also
	 * also releasing our slot lock, we could race.  That can't happen, I
	 * don't think.
	 */
err:	if ((t_ret =
	    __os_closehandle(dbenv, dbenv->registry)) != 0 && ret == 0)
		ret = t_ret;

	dbenv->registry = NULL;
	return (ret);
}

/*
 * __envreg_xunlock --
 *	Discard the exclusive lock held by the DB_ENV handle.
 *
 * PUBLIC: int __envreg_xunlock __P((DB_ENV *));
 */
int
__envreg_xunlock(dbenv)
	DB_ENV *dbenv;
{
	pid_t pid;
	int ret;

	dbenv->thread_id(dbenv, &pid, NULL);

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv,
		    "%lu: recovery completed, unlocking", (u_long)pid);

	if ((ret = REGISTRY_EXCL_UNLOCK(dbenv)) == 0)
		return (ret);

	__db_err(dbenv, ret, "%s: exclusive file unlock", REGISTER_FILE);
	return (__db_panic(dbenv, ret));
}
