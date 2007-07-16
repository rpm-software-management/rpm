/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: env_open.c,v 12.71 2006/08/24 14:45:39 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __db_tmp_open __P((DB_ENV *, u_int32_t, char *, DB_FH **));
static int __env_refresh __P((DB_ENV *, u_int32_t, int));

/*
 * db_version --
 *	Return version information.
 *
 * EXTERN: char *db_version __P((int *, int *, int *));
 */
char *
db_version(majverp, minverp, patchp)
	int *majverp, *minverp, *patchp;
{
	if (majverp != NULL)
		*majverp = DB_VERSION_MAJOR;
	if (minverp != NULL)
		*minverp = DB_VERSION_MINOR;
	if (patchp != NULL)
		*patchp = DB_VERSION_PATCH;
	return ((char *)DB_VERSION_STRING);
}

/*
 * __env_open_pp --
 *	DB_ENV->open pre/post processing.
 *
 * PUBLIC: int __env_open_pp __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__env_open_pp(dbenv, db_home, flags, mode)
	DB_ENV *dbenv;
	const char *db_home;
	u_int32_t flags;
	int mode;
{
	int ret;

#undef	OKFLAGS
#define	OKFLAGS								\
	(DB_CREATE | DB_INIT_CDB | DB_INIT_LOCK | DB_INIT_LOG |		\
	DB_INIT_MPOOL | DB_INIT_REP | DB_INIT_TXN | DB_LOCKDOWN |	\
	DB_PRIVATE | DB_RECOVER | DB_RECOVER_FATAL | DB_REGISTER |	\
	DB_SYSTEM_MEM | DB_THREAD | DB_USE_ENVIRON | DB_USE_ENVIRON_ROOT)
#undef	OKFLAGS_CDB
#define	OKFLAGS_CDB							\
	(DB_CREATE | DB_INIT_CDB | DB_INIT_MPOOL | DB_LOCKDOWN |	\
	DB_PRIVATE | DB_SYSTEM_MEM | DB_THREAD |			\
	DB_USE_ENVIRON | DB_USE_ENVIRON_ROOT)

	if ((ret = __db_fchk(dbenv, "DB_ENV->open", flags, OKFLAGS)) != 0)
		return (ret);
	if ((ret = __db_fcchk(
	    dbenv, "DB_ENV->open", flags, DB_INIT_CDB, ~OKFLAGS_CDB)) != 0)
		return (ret);
	if (LF_ISSET(DB_REGISTER)) {
		if (!__os_support_db_register()) {
			__db_errx(dbenv,
	     "Berkeley DB library does not support DB_REGISTER on this system");
			return (EINVAL);
		}
		if ((ret = __db_fcchk(dbenv, "DB_ENV->open", flags,
		    DB_PRIVATE, DB_REGISTER | DB_SYSTEM_MEM)) != 0)
			return (ret);
		if (!LF_ISSET(DB_INIT_TXN)) {
			__db_errx(
			    dbenv, "registration requires transaction support");
			return (EINVAL);
		}
	}
	if (LF_ISSET(DB_INIT_REP)) {
		if (!__os_support_replication()) {
			__db_errx(dbenv,
	     "Berkeley DB library does not support replication on this system");
			return (EINVAL);
		}
		if (!LF_ISSET(DB_INIT_LOCK)) {
			__db_errx(dbenv,
			    "replication requires locking support");
			return (EINVAL);
		}
		if (!LF_ISSET(DB_INIT_TXN)) {
			__db_errx(
			    dbenv, "replication requires transaction support");
			return (EINVAL);
		}
	}
	if (LF_ISSET(DB_RECOVER | DB_RECOVER_FATAL)) {
		if ((ret = __db_fcchk(dbenv,
		    "DB_ENV->open", flags, DB_RECOVER, DB_RECOVER_FATAL)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbenv,
		    "DB_ENV->open", flags, DB_REGISTER, DB_RECOVER_FATAL)) != 0)
			return (ret);
		if (!LF_ISSET(DB_CREATE)) {
			__db_errx(dbenv, "recovery requires the create flag");
			return (EINVAL);
		}
		if (!LF_ISSET(DB_INIT_TXN)) {
			__db_errx(
			    dbenv, "recovery requires transaction support");
			return (EINVAL);
		}
	}

#ifdef HAVE_MUTEX_THREAD_ONLY
	/*
	 * Currently we support one kind of mutex that is intra-process only,
	 * POSIX 1003.1 pthreads, because a variety of systems don't support
	 * the full pthreads API, and our only alternative is test-and-set.
	 */
	if (!LF_ISSET(DB_PRIVATE)) {
		__db_errx(dbenv,
	 "Berkeley DB library configured to support only private environments");
		return (EINVAL);
	}
#endif

#ifdef HAVE_MUTEX_FCNTL
	/*
	 * !!!
	 * We need a file descriptor for fcntl(2) locking.  We use the file
	 * handle from the REGENV file for this purpose.
	 *
	 * Since we may be using shared memory regions, e.g., shmget(2), and
	 * not a mapped-in regular file, the backing file may be only a few
	 * bytes in length.  So, this depends on the ability to call fcntl to
	 * lock file offsets much larger than the actual physical file.  I
	 * think that's safe -- besides, very few systems actually need this
	 * kind of support, SunOS is the only one still in wide use of which
	 * I'm aware.
	 *
	 * The error case is if an application lacks spinlocks and wants to be
	 * threaded.  That doesn't work because fcntl will lock the underlying
	 * process, including all its threads.
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		__db_errx(dbenv,
	    "architecture lacks fast mutexes: applications cannot be threaded");
		return (EINVAL);
	}
#endif

	return (__env_open(dbenv, db_home, flags, mode));
}

/*
 * __env_open --
 *	DB_ENV->open.
 *
 * PUBLIC: int __env_open __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__env_open(dbenv, db_home, flags, mode)
	DB_ENV *dbenv;
	const char *db_home;
	u_int32_t flags;
	int mode;
{
	DB_THREAD_INFO *ip;
	REGINFO *infop;
	u_int32_t init_flags, orig_flags;
	int register_recovery, rep_check, ret, t_ret;

	ip = NULL;
	register_recovery = rep_check = 0;

	/* Initial configuration. */
	if ((ret = __env_config(dbenv, db_home, flags, mode)) != 0)
		return (ret);

	/*
	 * Save the DB_ENV handle's configuration flags as set by user-called
	 * configuration methods and the environment directory's DB_CONFIG
	 * file.  If we use this DB_ENV structure to recover the existing
	 * environment or to remove an environment we created after failure,
	 * we'll restore the DB_ENV flags to these values.
	 */
	orig_flags = dbenv->flags;

	/*
	 * If we're going to register with the environment, that's the first
	 * thing we do.
	 */
	if (LF_ISSET(DB_REGISTER)) {
		if ((ret = __envreg_register(dbenv, &register_recovery)) != 0)
			goto err;
		if (register_recovery) {
			if (!LF_ISSET(DB_RECOVER)) {
				__db_errx(dbenv,
	    "The DB_RECOVER flag was not specified, and recovery is needed");
				ret = DB_RUNRECOVERY;
				goto err;
			}
		} else
			LF_CLR(DB_RECOVER);
	}

	/*
	 * If we're doing recovery, destroy the environment so that we create
	 * all the regions from scratch.  The major concern I have is if the
	 * application stomps the environment with a rogue pointer.  We have
	 * no way of detecting that, and we could be forced into a situation
	 * where we start up and then crash, repeatedly.
	 *
	 * We do not check any flags like DB_PRIVATE before calling remove.
	 * We don't care if the current environment was private or not, we
	 * want to remove files left over for any reason, from any session.
	 */
	if (LF_ISSET(DB_RECOVER | DB_RECOVER_FATAL))
		if ((ret = __db_e_remove(dbenv, DB_FORCE)) != 0 ||
		    (ret = __env_refresh(dbenv, orig_flags, 0)) != 0)
			goto err;

	/* Convert the DB_ENV->open flags to internal flags. */
	if (LF_ISSET(DB_CREATE))
		F_SET(dbenv, DB_ENV_CREATE);
	if (LF_ISSET(DB_LOCKDOWN))
		F_SET(dbenv, DB_ENV_LOCKDOWN);
	if (LF_ISSET(DB_PRIVATE))
		F_SET(dbenv, DB_ENV_PRIVATE);
	if (LF_ISSET(DB_RECOVER_FATAL))
		F_SET(dbenv, DB_ENV_FATAL);
	if (LF_ISSET(DB_SYSTEM_MEM))
		F_SET(dbenv, DB_ENV_SYSTEM_MEM);
	if (LF_ISSET(DB_THREAD))
		F_SET(dbenv, DB_ENV_THREAD);

	/*
	 * Flags saved in the init_flags field of the environment, representing
	 * flags to DB_ENV->set_flags and DB_ENV->open that need to be set.
	 */
#define	DB_INITENV_CDB		0x0001	/* DB_INIT_CDB */
#define	DB_INITENV_CDB_ALLDB	0x0002	/* DB_INIT_CDB_ALLDB */
#define	DB_INITENV_LOCK		0x0004	/* DB_INIT_LOCK */
#define	DB_INITENV_LOG		0x0008	/* DB_INIT_LOG */
#define	DB_INITENV_MPOOL	0x0010	/* DB_INIT_MPOOL */
#define	DB_INITENV_REP		0x0020	/* DB_INIT_REP */
#define	DB_INITENV_TXN		0x0040	/* DB_INIT_TXN */

	/*
	 * Create/join the environment.  We pass in the flags of interest to
	 * a thread subsequently joining an environment we create.  If we're
	 * not the ones to create the environment, our flags will be updated
	 * to match the existing environment.
	 */
	init_flags = 0;
	if (LF_ISSET(DB_INIT_CDB))
		FLD_SET(init_flags, DB_INITENV_CDB);
	if (F_ISSET(dbenv, DB_ENV_CDB_ALLDB))
		FLD_SET(init_flags, DB_INITENV_CDB_ALLDB);
	if (LF_ISSET(DB_INIT_LOCK))
		FLD_SET(init_flags, DB_INITENV_LOCK);
	if (LF_ISSET(DB_INIT_LOG))
		FLD_SET(init_flags, DB_INITENV_LOG);
	if (LF_ISSET(DB_INIT_MPOOL))
		FLD_SET(init_flags, DB_INITENV_MPOOL);
	if (LF_ISSET(DB_INIT_REP))
		FLD_SET(init_flags, DB_INITENV_REP);
	if (LF_ISSET(DB_INIT_TXN))
		FLD_SET(init_flags, DB_INITENV_TXN);
	if ((ret = __db_e_attach(dbenv, &init_flags)) != 0)
		goto err;

	/*
	 * __db_e_attach will return the saved init_flags field, which contains
	 * the DB_INIT_* flags used when the environment was created.
	 *
	 * We may be joining an environment -- reset our flags to match the
	 * ones in the environment.
	 */
	if (FLD_ISSET(init_flags, DB_INITENV_CDB))
		LF_SET(DB_INIT_CDB);
	if (FLD_ISSET(init_flags, DB_INITENV_LOCK))
		LF_SET(DB_INIT_LOCK);
	if (FLD_ISSET(init_flags, DB_INITENV_LOG))
		LF_SET(DB_INIT_LOG);
	if (FLD_ISSET(init_flags, DB_INITENV_MPOOL))
		LF_SET(DB_INIT_MPOOL);
	if (FLD_ISSET(init_flags, DB_INITENV_REP))
		LF_SET(DB_INIT_REP);
	if (FLD_ISSET(init_flags, DB_INITENV_TXN))
		LF_SET(DB_INIT_TXN);
	if (FLD_ISSET(init_flags, DB_INITENV_CDB_ALLDB) &&
	    (ret = __env_set_flags(dbenv, DB_CDB_ALLDB, 1)) != 0)
		goto err;

	/*
	 * Save the flags matching the database environment: we'll replace
	 * the argument flags with the flags corresponding to the existing,
	 * underlying set of subsystems.
	 */
	dbenv->open_flags = flags;

	/* Initialize for CDB product. */
	if (LF_ISSET(DB_INIT_CDB)) {
		LF_SET(DB_INIT_LOCK);
		F_SET(dbenv, DB_ENV_CDB);
	}

	/*
	 * The DB_ENV structure has now been initialized.  Turn off further
	 * use of the DB_ENV structure and most initialization methods, we're
	 * about to act on the values we currently have.
	 */
	F_SET(dbenv, DB_ENV_OPEN_CALLED);

	/*
	 * Initialize the subsystems.
	 *
	 * Initialize the mutex regions first.  There's no ordering requirement,
	 * but it's simpler to get this in place so we don't have to keep track
	 * of mutexes for later allocation, once the mutex region is created we
	 * can go ahead and do the allocation for real.
	 */
	if ((ret = __mutex_open(dbenv)) != 0)
		goto err;

	/* __mutex_open creates the thread info region, enter it now. */
	ENV_ENTER(dbenv, ip);

	/*
	 * Initialize the replication area next, so that we can lock out this
	 * call if we're currently running recovery for replication.
	 */
	if (LF_ISSET(DB_INIT_REP) && (ret = __rep_open(dbenv)) != 0)
		goto err;

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check && (ret = __env_rep_enter(dbenv, 0)) != 0)
		goto err;

	if (LF_ISSET(DB_INIT_MPOOL))
		if ((ret = __memp_open(dbenv)) != 0)
			goto err;
	/*
	 * Initialize the ciphering area prior to any running of recovery so
	 * that we can initialize the keys, etc. before recovery.
	 *
	 * !!!
	 * This must be after the mpool init, but before the log initialization
	 * because log_open may attempt to run log_recover during its open.
	 */
	if (LF_ISSET(DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_TXN) &&
	    (ret = __crypto_region_init(dbenv)) != 0)
		goto err;

	/*
	 * Transactions imply logging but do not imply locking.  While almost
	 * all applications want both locking and logging, it would not be
	 * unreasonable for a single threaded process to want transactions for
	 * atomicity guarantees, but not necessarily need concurrency.
	 */
	if (LF_ISSET(DB_INIT_LOG | DB_INIT_TXN))
		if ((ret = __log_open(dbenv)) != 0)
			goto err;
	if (LF_ISSET(DB_INIT_LOCK))
		if ((ret = __lock_open(dbenv)) != 0)
			goto err;

	if (LF_ISSET(DB_INIT_TXN)) {
		if ((ret = __txn_open(dbenv)) != 0)
			goto err;

		/*
		 * If the application is running with transactions, initialize
		 * the function tables.
		 */
		if ((ret = __env_init_rec(dbenv, DB_LOGVERSION)) != 0)
			goto err;
	}

	/*
	 * Initialize the DB list, and its mutex as necessary.  If the env
	 * handle isn't free-threaded we don't need a mutex because there
	 * will never be more than a single DB handle on the list.  If the
	 * mpool wasn't initialized, then we can't ever open a DB handle.
	 *
	 * We also need to initialize the MT mutex as necessary, so do them
	 * both.
	 *
	 * !!!
	 * This must come after the __memp_open call above because if we are
	 * recording mutexes for system resources, we will do it in the mpool
	 * region for environments and db handles.  So, the mpool region must
	 * already be initialized.
	 */
	TAILQ_INIT(&dbenv->dblist);
	if (LF_ISSET(DB_INIT_MPOOL)) {
		if ((ret = __mutex_alloc(dbenv, MTX_ENV_DBLIST,
		    DB_MUTEX_PROCESS_ONLY, &dbenv->mtx_dblist)) != 0)
			goto err;
		if ((ret = __mutex_alloc(dbenv, MTX_TWISTER,
		    DB_MUTEX_PROCESS_ONLY, &dbenv->mtx_mt)) != 0)
			goto err;

		/* Register DB's pgin/pgout functions.  */
		if ((ret = __memp_register(
		    dbenv, DB_FTYPE_SET, __db_pgin, __db_pgout)) != 0)
			goto err;
	}

	/* Perform recovery for any previous run. */
	if (LF_ISSET(DB_RECOVER | DB_RECOVER_FATAL) &&
	    (ret = __db_apprec(dbenv, NULL, NULL, 1,
	    LF_ISSET(DB_RECOVER | DB_RECOVER_FATAL))) != 0)
		goto err;

	/*
	 * If we've created the regions, are running with transactions, and did
	 * not just run recovery, we need to log the fact that the transaction
	 * IDs got reset.
	 *
	 * If we ran recovery, there may be prepared-but-not-yet-committed
	 * transactions that need to be resolved.  Recovery resets the minimum
	 * transaction ID and logs the reset if that's appropriate, so we
	 * don't need to do anything here in the recover case.
	 */
	infop = dbenv->reginfo;
	if (TXN_ON(dbenv) &&
	    !F_ISSET(dbenv, DB_ENV_LOG_INMEMORY) &&
	    F_ISSET(infop, REGION_CREATE) &&
	    !LF_ISSET(DB_RECOVER | DB_RECOVER_FATAL) &&
	    (ret = __txn_reset(dbenv)) != 0)
		goto err;

	/* The database environment is ready for business. */
	if ((ret = __db_e_golive(dbenv)) != 0)
		goto err;

	if (rep_check)
		ret = __env_db_rep_exit(dbenv);

err:	ENV_LEAVE(dbenv, ip);

	if (ret != 0) {
		/*
		 * If we fail after creating the regions, panic and remove them.
		 *
		 * !!!
		 * No need to call __env_db_rep_exit, that work is done by the
		 * calls to __env_refresh.
		 */
		infop = dbenv->reginfo;
		if (infop != NULL && F_ISSET(infop, REGION_CREATE)) {
			ret = __db_panic(dbenv, ret);

			/* Refresh the DB_ENV so can use it to call remove. */
			(void)__env_refresh(dbenv, orig_flags, rep_check);
			(void)__db_e_remove(dbenv, DB_FORCE);
			(void)__env_refresh(dbenv, orig_flags, 0);
		} else
			(void)__env_refresh(dbenv, orig_flags, rep_check);
	}

	if (register_recovery) {
		/*
		 * If recovery succeeded, release our exclusive lock, other
		 * processes can now proceed.
		 *
		 * If recovery failed, unregister now and let another process
		 * clean up.
		 */
		if (ret == 0 && (t_ret = __envreg_xunlock(dbenv)) != 0)
			ret = t_ret;
		if (ret != 0)
			(void)__envreg_unregister(dbenv, 1);
	}

	return (ret);
}

/*
 * __env_remove --
 *	DB_ENV->remove.
 *
 * PUBLIC: int __env_remove __P((DB_ENV *, const char *, u_int32_t));
 */
int
__env_remove(dbenv, db_home, flags)
	DB_ENV *dbenv;
	const char *db_home;
	u_int32_t flags;
{
	int ret, t_ret;

#undef	OKFLAGS
#define	OKFLAGS								\
	(DB_FORCE | DB_USE_ENVIRON | DB_USE_ENVIRON_ROOT)

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB_ENV->remove", flags, OKFLAGS)) != 0)
		return (ret);

	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->remove");

	if ((ret = __env_config(dbenv, db_home, flags, 0)) != 0)
		return (ret);

	ret = __db_e_remove(dbenv, flags);

	if ((t_ret = __env_close(dbenv, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __env_config --
 *	Argument-based initialization.
 *
 * PUBLIC: int __env_config __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__env_config(dbenv, db_home, flags, mode)
	DB_ENV *dbenv;
	const char *db_home;
	u_int32_t flags;
	int mode;
{
	int ret;
	char *home, home_buf[DB_MAXPATHLEN];

	/*
	 * Set the database home.
	 *
	 * Use db_home by default, this allows utilities to reasonably
	 * override the environment either explicitly or by using a -h
	 * option.  Otherwise, use the environment if it's permitted
	 * and initialized.
	 */
	home = (char *)db_home;
	if (home == NULL && (LF_ISSET(DB_USE_ENVIRON) ||
	    (LF_ISSET(DB_USE_ENVIRON_ROOT) && __os_isroot()))) {
		home = home_buf;
		if ((ret = __os_getenv(
		    dbenv, "DB_HOME", &home, sizeof(home_buf))) != 0)
			return (ret);
		/*
		 * home set to NULL if __os_getenv failed to find DB_HOME.
		 */
	}
	if (home != NULL &&
	    (ret = __os_strdup(dbenv, home, &dbenv->db_home)) != 0)
		return (ret);

	/* Default permissions are read-write for both owner and group. */
	dbenv->db_mode = mode == 0 ? __db_omode("rw-rw----") : mode;

	/* Read the DB_CONFIG file. */
	if ((ret = __env_read_db_config(dbenv)) != 0)
		return (ret);

	/*
	 * If no temporary directory path was specified in the config file,
	 * choose one.
	 */
	if (dbenv->db_tmp_dir == NULL && (ret = __os_tmpdir(dbenv, flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __env_close_pp --
 *	DB_ENV->close pre/post processor.
 *
 * PUBLIC: int __env_close_pp __P((DB_ENV *, u_int32_t));
 */
int
__env_close_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int rep_check, ret, t_ret;

	ret = 0;

	PANIC_CHECK(dbenv);

	ENV_ENTER(dbenv, ip);
	/*
	 * Validate arguments, but as a DB_ENV handle destructor, we can't
	 * fail.
	 */
	if (flags != 0 &&
	    (t_ret = __db_ferr(dbenv, "DB_ENV->close", 0)) != 0 && ret == 0)
		ret = t_ret;

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check) {
#ifdef HAVE_REPLICATION_THREADS
		/*
		 * Shut down Replication Manager threads first of all.  This
		 * must be done before __env_rep_enter to avoid a deadlock that
		 * could occur if repmgr's background threads try to do a rep
		 * operation that needs __rep_lockout.
		 */
		if ((t_ret = __repmgr_close(dbenv)) != 0 && ret == 0)
			ret = t_ret;
#endif
		if ((t_ret = __env_rep_enter(dbenv, 0)) != 0 && ret == 0)
			ret = t_ret;
	}

	if ((t_ret = __env_close(dbenv, rep_check)) != 0 && ret == 0)
		ret = t_ret;

	/* Don't ENV_LEAVE as we have already detached from the region. */
	return (ret);
}

/*
 * __env_close --
 *	DB_ENV->close.
 *
 * PUBLIC: int __env_close __P((DB_ENV *, int));
 */
int
__env_close(dbenv, rep_check)
	DB_ENV *dbenv;
	int rep_check;
{
	int ret, t_ret;
	char **p;

	ret = 0;

	/*
	 * Before checking the reference count, we have to see if we were in
	 * the middle of restoring transactions and need to close the open
	 * files.
	 */
	if (TXN_ON(dbenv) && (t_ret = __txn_preclose(dbenv)) != 0 && ret == 0)
		ret = t_ret;

#ifdef HAVE_REPLICATION
	if ((t_ret = __rep_close(dbenv)) != 0 && ret == 0)
		ret = t_ret;
#endif

	/*
	 * Detach from the regions and undo the allocations done by
	 * DB_ENV->open.
	 */
	if ((t_ret = __env_refresh(dbenv, 0, rep_check)) != 0 && ret == 0)
		ret = t_ret;

#ifdef HAVE_CRYPTO
	/*
	 * Crypto comes last, because higher level close functions need
	 * cryptography.
	 */
	if ((t_ret = __crypto_dbenv_close(dbenv)) != 0 && ret == 0)
		ret = t_ret;
#endif
	/* If we're registered, clean up. */
	if (dbenv->registry != NULL) {
		(void)__envreg_unregister(dbenv, 0);
		dbenv->registry = NULL;
	}

	/* Release any string-based configuration parameters we've copied. */
	if (dbenv->db_log_dir != NULL)
		__os_free(dbenv, dbenv->db_log_dir);
	dbenv->db_log_dir = NULL;
	if (dbenv->db_tmp_dir != NULL)
		__os_free(dbenv, dbenv->db_tmp_dir);
	dbenv->db_tmp_dir = NULL;
	if (dbenv->db_data_dir != NULL) {
		for (p = dbenv->db_data_dir; *p != NULL; ++p)
			__os_free(dbenv, *p);
		__os_free(dbenv, dbenv->db_data_dir);
		dbenv->db_data_dir = NULL;
		dbenv->data_next = 0;
	}
	if (dbenv->db_home != NULL) {
		__os_free(dbenv, dbenv->db_home);
		dbenv->db_home = NULL;
	}

	/* Discard the structure. */
	__db_env_destroy(dbenv);

	return (ret);
}

/*
 * __env_refresh --
 *	Refresh the DB_ENV structure.
 */
static int
__env_refresh(dbenv, orig_flags, rep_check)
	DB_ENV *dbenv;
	u_int32_t orig_flags;
	int rep_check;
{
	DB *ldbp;
	DB_THREAD_INFO *ip;
	int ret, t_ret;

	ret = 0;

	/*
	 * Release resources allocated by DB_ENV->open, and return it to the
	 * state it was in just before __env_open was called.  (This means
	 * state set by pre-open configuration functions must be preserved.)
	 *
	 * Refresh subsystems, in the reverse order they were opened (txn
	 * must be first, it may want to discard locks and flush the log).
	 *
	 * !!!
	 * Note that these functions, like all of __env_refresh, only undo
	 * the effects of __env_open.  Functions that undo work done by
	 * db_env_create or by a configuration function should go in
	 * __env_close.
	 */
	if (TXN_ON(dbenv) &&
	    (t_ret = __txn_dbenv_refresh(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	if (LOGGING_ON(dbenv) &&
	    (t_ret = __log_dbenv_refresh(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Locking should come after logging, because closing log results
	 * in files closing which may require locks being released.
	 */
	if (LOCKING_ON(dbenv)) {
		if (!F_ISSET(dbenv, DB_ENV_THREAD) &&
		    dbenv->env_lref != NULL && (t_ret = __lock_id_free(dbenv,
		    ((DB_LOCKER *)dbenv->env_lref)->id)) != 0 && ret == 0)
			ret = t_ret;
		dbenv->env_lref = NULL;

		if ((t_ret = __lock_dbenv_refresh(dbenv)) != 0 && ret == 0)
			ret = t_ret;
	}

	/*
	 * Discard DB list and its mutex.
	 * Discard the MT mutex.
	 *
	 * !!!
	 * This must be done before we close the mpool region because we
	 * may have allocated the DB handle mutex in the mpool region.
	 * It must be done *after* we close the log region, though, because
	 * we close databases and try to acquire the mutex when we close
	 * log file handles.  Ick.
	 */
	if (dbenv->db_ref != 0) {
		__db_errx(dbenv,
		    "Database handles still open at environment close");
		TAILQ_FOREACH(ldbp, &dbenv->dblist, dblistlinks)
			__db_errx(dbenv, "Open database handle: %s%s%s",
			    ldbp->fname == NULL ? "unnamed" : ldbp->fname,
			    ldbp->dname == NULL ? "" : "/",
			    ldbp->dname == NULL ? "" : ldbp->dname);
		if (ret == 0)
			ret = EINVAL;
	}
	TAILQ_INIT(&dbenv->dblist);

	if ((t_ret = __mutex_free(dbenv, &dbenv->mtx_dblist)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __mutex_free(dbenv, &dbenv->mtx_mt)) != 0 && ret == 0)
		ret = t_ret;

	if (dbenv->mt != NULL) {
		__os_free(dbenv, dbenv->mt);
		dbenv->mt = NULL;
	}

	if (MPOOL_ON(dbenv)) {
		/*
		 * If it's a private environment, flush the contents to disk.
		 * Recovery would have put everything back together, but it's
		 * faster and cleaner to flush instead.
		 */
		if (F_ISSET(dbenv, DB_ENV_PRIVATE) &&
		    (t_ret = __memp_sync(dbenv, NULL)) != 0 && ret == 0)
			ret = t_ret;
		if ((t_ret = __memp_dbenv_refresh(dbenv)) != 0 && ret == 0)
			ret = t_ret;
	}

	/*
	 * If we're included in a shared replication handle count, this
	 * is our last chance to decrement that count.
	 *
	 * !!!
	 * We can't afford to do anything dangerous after we decrement the
	 * handle count, of course, as replication may be proceeding with
	 * client recovery.  However, since we're discarding the regions
	 * as soon as we drop the handle count, there's little opportunity
	 * to do harm.
	 */
	if (rep_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Detach from the region.
	 *
	 * Must come after we call __env_db_rep_exit above.
	 */
	if (REP_ON(dbenv))
		__rep_dbenv_refresh(dbenv);

	/*
	 * Mark the thread as out of the env before we get rid of the handles
	 * needed to do so.
	 */
	if (dbenv->thr_hashtab != NULL &&
	    (t_ret = __env_set_state(dbenv, &ip, THREAD_OUT)) != 0 && ret == 0)
		ret = t_ret;

	if (MUTEX_ON(dbenv) &&
	    (t_ret = __mutex_dbenv_refresh(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	if (dbenv->reginfo != NULL) {
		if ((t_ret = __db_e_detach(dbenv, 0)) != 0 && ret == 0)
			ret = t_ret;
		/*
		 * !!!
		 * Don't free dbenv->reginfo or set the reference to NULL,
		 * that was done by __db_e_detach().
		 */
	}

	if (dbenv->mutex_iq != NULL) {
		__os_free(dbenv, dbenv->mutex_iq);
		dbenv->mutex_iq = NULL;
	}

	if (dbenv->recover_dtab != NULL) {
		__os_free(dbenv, dbenv->recover_dtab);
		dbenv->recover_dtab = NULL;
		dbenv->recover_dtab_size = 0;
	}

	dbenv->flags = orig_flags;

	return (ret);
}

#define	DB_ADDSTR(add) {						\
	/*								\
	 * The string might be NULL or zero-length, and the p[-1]	\
	 * might indirect to before the beginning of our buffer.	\
	 */								\
	if ((add) != NULL && (add)[0] != '\0') {			\
		/* If leading slash, start over. */			\
		if (__os_abspath(add)) {				\
			p = str;					\
			slash = 0;					\
		}							\
		/* Append to the current string. */			\
		len = strlen(add);					\
		if (slash)						\
			*p++ = PATH_SEPARATOR[0];			\
		memcpy(p, add, len);					\
		p += len;						\
		slash = strchr(PATH_SEPARATOR, p[-1]) == NULL;		\
	}								\
}

/*
 * __env_get_open_flags
 *	Retrieve the flags passed to DB_ENV->open.
 *
 * PUBLIC: int __env_get_open_flags __P((DB_ENV *, u_int32_t *));
 */
int
__env_get_open_flags(dbenv, flagsp)
	DB_ENV *dbenv;
	u_int32_t *flagsp;
{
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->get_open_flags");

	*flagsp = dbenv->open_flags;
	return (0);
}

/*
 * __db_appname --
 *	Given an optional DB environment, directory and file name and type
 *	of call, build a path based on the DB_ENV->open rules, and return
 *	it in allocated space.
 *
 * PUBLIC: int __db_appname __P((DB_ENV *, APPNAME,
 * PUBLIC:    const char *, u_int32_t, DB_FH **, char **));
 */
int
__db_appname(dbenv, appname, file, tmp_oflags, fhpp, namep)
	DB_ENV *dbenv;
	APPNAME appname;
	const char *file;
	u_int32_t tmp_oflags;
	DB_FH **fhpp;
	char **namep;
{
	enum { TRY_NOTSET, TRY_DATA_DIR, TRY_ENV_HOME, TRY_CREATE } try_state;
	size_t len, str_len;
	int data_entry, ret, slash, tmp_create;
	const char *a, *b;
	char *p, *str;

	try_state = TRY_NOTSET;
	a = b = NULL;
	data_entry = 0;
	tmp_create = 0;

	/*
	 * We don't return a name when creating temporary files, just a file
	 * handle.  Default to an error now.
	 */
	if (fhpp != NULL)
		*fhpp = NULL;
	if (namep != NULL)
		*namep = NULL;

	/*
	 * Absolute path names are never modified.  If the file is an absolute
	 * path, we're done.
	 */
	if (file != NULL && __os_abspath(file))
		return (__os_strdup(dbenv, file, namep));

	/* Everything else is relative to the environment home. */
	if (dbenv != NULL)
		a = dbenv->db_home;

retry:	/*
	 * DB_APP_NONE:
	 *      DB_HOME/file
	 * DB_APP_DATA:
	 *      DB_HOME/DB_DATA_DIR/file
	 * DB_APP_LOG:
	 *      DB_HOME/DB_LOG_DIR/file
	 * DB_APP_TMP:
	 *      DB_HOME/DB_TMP_DIR/<create>
	 */
	switch (appname) {
	case DB_APP_NONE:
		break;
	case DB_APP_DATA:
		if (dbenv == NULL || dbenv->db_data_dir == NULL) {
			try_state = TRY_CREATE;
			break;
		}

		/*
		 * First, step through the data_dir entries, if any, looking
		 * for the file.
		 */
		if ((b = dbenv->db_data_dir[data_entry]) != NULL) {
			++data_entry;
			try_state = TRY_DATA_DIR;
			break;
		}

		/* Second, look in the environment home directory. */
		if (try_state != TRY_ENV_HOME) {
			try_state = TRY_ENV_HOME;
			break;
		}

		/* Third, try creation in the first data_dir entry. */
		try_state = TRY_CREATE;
		b = dbenv->db_data_dir[0];
		break;
	case DB_APP_LOG:
		if (dbenv != NULL)
			b = dbenv->db_log_dir;
		break;
	case DB_APP_TMP:
		if (dbenv != NULL)
			b = dbenv->db_tmp_dir;
		tmp_create = 1;
		break;
	}

	len =
	    (a == NULL ? 0 : strlen(a) + 1) +
	    (b == NULL ? 0 : strlen(b) + 1) +
	    (file == NULL ? 0 : strlen(file) + 1);

	/*
	 * Allocate space to hold the current path information, as well as any
	 * temporary space that we're going to need to create a temporary file
	 * name.
	 */
#define	DB_TRAIL	"BDBXXXXX"
	str_len = len + sizeof(DB_TRAIL) + 10;
	if ((ret = __os_malloc(dbenv, str_len, &str)) != 0)
		return (ret);

	slash = 0;
	p = str;
	DB_ADDSTR(a);
	DB_ADDSTR(b);
	DB_ADDSTR(file);
	*p = '\0';

	/*
	 * If we're opening a data file, see if it exists.  If it does,
	 * return it, otherwise, try and find another one to open.
	 */
	if (appname == DB_APP_DATA &&
	    __os_exists(dbenv, str, NULL) != 0 && try_state != TRY_CREATE) {
		__os_free(dbenv, str);
		b = NULL;
		goto retry;
	}

	/* Create the file if so requested. */
	if (tmp_create &&
	    (ret = __db_tmp_open(dbenv, tmp_oflags, str, fhpp)) != 0) {
		__os_free(dbenv, str);
		return (ret);
	}

	if (namep == NULL)
		__os_free(dbenv, str);
	else
		*namep = str;
	return (0);
}

/*
 * __db_tmp_open --
 *	Create a temporary file.
 */
static int
__db_tmp_open(dbenv, tmp_oflags, path, fhpp)
	DB_ENV *dbenv;
	u_int32_t tmp_oflags;
	char *path;
	DB_FH **fhpp;
{
	pid_t pid;
	int filenum, i, isdir, ret;
	char *firstx, *trv;

	/*
	 * Check the target directory; if you have six X's and it doesn't
	 * exist, this runs for a *very* long time.
	 */
	if ((ret = __os_exists(dbenv, path, &isdir)) != 0) {
		__db_err(dbenv, ret, "%s", path);
		return (ret);
	}
	if (!isdir) {
		__db_err(dbenv, EINVAL, "%s", path);
		return (EINVAL);
	}

	/* Build the path. */
	(void)strncat(path, PATH_SEPARATOR, 1);
	(void)strcat(path, DB_TRAIL);

	/* Replace the X's with the process ID (in decimal). */
	__os_id(dbenv, &pid, NULL);
	for (trv = path + strlen(path); *--trv == 'X'; pid /= 10)
		*trv = '0' + (u_char)(pid % 10);
	firstx = trv + 1;

	/* Loop, trying to open a file. */
	for (filenum = 1;; filenum++) {
		if ((ret = __os_open(dbenv, path,
		    tmp_oflags | DB_OSO_CREATE | DB_OSO_EXCL | DB_OSO_TEMP,
		    __db_omode(OWNER_RW), fhpp)) == 0)
			return (0);

		/*
		 * !!!:
		 * If we don't get an EEXIST error, then there's something
		 * seriously wrong.  Unfortunately, if the implementation
		 * doesn't return EEXIST for O_CREAT and O_EXCL regardless
		 * of other possible errors, we've lost.
		 */
		if (ret != EEXIST) {
			__db_err(dbenv, ret, "temporary open: %s", path);
			return (ret);
		}

		/*
		 * Generate temporary file names in a backwards-compatible way.
		 * If pid == 12345, the result is:
		 *   <path>/DB12345 (tried above, the first time through).
		 *   <path>/DBa2345 ...  <path>/DBz2345
		 *   <path>/DBaa345 ...  <path>/DBaz345
		 *   <path>/DBba345, and so on.
		 *
		 * XXX
		 * This algorithm is O(n**2) -- that is, creating 100 temporary
		 * files requires 5,000 opens, creating 1000 files requires
		 * 500,000.  If applications open a lot of temporary files, we
		 * could improve performance by switching to timestamp-based
		 * file names.
		 */
		for (i = filenum, trv = firstx; i > 0; i = (i - 1) / 26)
			if (*trv++ == '\0')
				return (EINVAL);

		for (i = filenum; i > 0; i = (i - 1) / 26)
			*--trv = 'a' + ((i - 1) % 26);
	}
	/* NOTREACHED */
}
