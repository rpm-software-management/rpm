/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: lock_stub.c,v 12.7 2007/05/17 15:15:43 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/lock.h"

/*
 * If the library wasn't compiled with locking support, various routines
 * aren't available.  Stub them here, returning an appropriate error.
 */
static int __db_nolocking __P((DB_ENV *));

/*
 * __db_nolocking --
 *	Error when a Berkeley DB build doesn't include the locking subsystem.
 */
static int
__db_nolocking(dbenv)
	DB_ENV *dbenv;
{
	__db_errx(dbenv,
	    "library build did not include support for locking");
	return (DB_OPNOTSUP);
}

int
__lock_env_create(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, 0);
	return (0);
}

void
__lock_env_destroy(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, 0);
}

int
__lock_get_lk_conflicts(dbenv, lk_conflictsp, lk_modesp)
	DB_ENV *dbenv;
	const u_int8_t **lk_conflictsp;
	int *lk_modesp;
{
	COMPQUIET(lk_conflictsp, NULL);
	COMPQUIET(lk_modesp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_lk_detect(dbenv, lk_detectp)
	DB_ENV *dbenv;
	u_int32_t *lk_detectp;
{
	COMPQUIET(lk_detectp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_lk_max_lockers(dbenv, lk_maxp)
	DB_ENV *dbenv;
	u_int32_t *lk_maxp;
{
	COMPQUIET(lk_maxp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_lk_max_locks(dbenv, lk_maxp)
	DB_ENV *dbenv;
	u_int32_t *lk_maxp;
{
	COMPQUIET(lk_maxp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_lk_max_objects(dbenv, lk_maxp)
	DB_ENV *dbenv;
	u_int32_t *lk_maxp;
{
	COMPQUIET(lk_maxp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_env_timeout(dbenv, timeoutp, flag)
	DB_ENV *dbenv;
	db_timeout_t *timeoutp;
	u_int32_t flag;
{
	COMPQUIET(timeoutp, NULL);
	COMPQUIET(flag, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_detect_pp(dbenv, flags, atype, abortp)
	DB_ENV *dbenv;
	u_int32_t flags, atype;
	int *abortp;
{
	COMPQUIET(flags, 0);
	COMPQUIET(atype, 0);
	COMPQUIET(abortp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_get_pp(dbenv, locker, flags, obj, lock_mode, lock)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	COMPQUIET(locker, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(obj, NULL);
	COMPQUIET(lock_mode, 0);
	COMPQUIET(lock, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_id_pp(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	COMPQUIET(idp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_id_free_pp(dbenv, id)
	DB_ENV *dbenv;
	u_int32_t id;
{
	COMPQUIET(id, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_put_pp(dbenv, lock)
	DB_ENV *dbenv;
	DB_LOCK *lock;
{
	COMPQUIET(lock, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_LOCK_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_vec_pp(dbenv, locker, flags, list, nlist, elistp)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	COMPQUIET(locker, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(list, NULL);
	COMPQUIET(nlist, 0);
	COMPQUIET(elistp, NULL);
	return (__db_nolocking(dbenv));
}

int
__lock_set_lk_conflicts(dbenv, lk_conflicts, lk_modes)
	DB_ENV *dbenv;
	u_int8_t *lk_conflicts;
	int lk_modes;
{
	COMPQUIET(lk_conflicts, NULL);
	COMPQUIET(lk_modes, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_set_lk_detect(dbenv, lk_detect)
	DB_ENV *dbenv;
	u_int32_t lk_detect;
{
	COMPQUIET(lk_detect, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_set_lk_max_locks(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	COMPQUIET(lk_max, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_set_lk_max_lockers(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	COMPQUIET(lk_max, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_set_lk_max_objects(dbenv, lk_max)
	DB_ENV *dbenv;
	u_int32_t lk_max;
{
	COMPQUIET(lk_max, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_set_env_timeout(dbenv, timeout, flags)
	DB_ENV *dbenv;
	db_timeout_t timeout;
	u_int32_t flags;
{
	COMPQUIET(timeout, 0);
	COMPQUIET(flags, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_open(dbenv, create_ok)
	DB_ENV *dbenv;
	int create_ok;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(create_ok, 0);
	return (__db_nolocking(dbenv));
}

int
__lock_id_free(dbenv, sh_locker)
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(sh_locker, 0);
	return (0);
}

int
__lock_env_refresh(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__lock_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(flags, 0);
	return (0);
}

int
__lock_put(dbenv, lock)
	DB_ENV *dbenv;
	DB_LOCK *lock;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(lock, NULL);
	return (0);
}

int
__lock_vec(dbenv, sh_locker, flags, list, nlist, elistp)
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	u_int32_t flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(sh_locker, 0);
	COMPQUIET(flags, 0);
	COMPQUIET(list, NULL);
	COMPQUIET(nlist, 0);
	COMPQUIET(elistp, NULL);
	return (0);
}

int
__lock_get(dbenv, locker, flags, obj, lock_mode, lock)
	DB_ENV *dbenv;
	DB_LOCKER *locker;
	u_int32_t flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(locker, NULL);
	COMPQUIET(flags, 0);
	COMPQUIET(obj, NULL);
	COMPQUIET(lock_mode, 0);
	COMPQUIET(lock, NULL);
	return (0);
}

int
__lock_id(dbenv, idp, lkp)
	DB_ENV *dbenv;
	u_int32_t *idp;
	DB_LOCKER **lkp;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(idp, NULL);
	COMPQUIET(lkp, NULL);
	return (0);
}

int
__lock_inherit_timeout(dbenv, parent, locker)
	DB_ENV *dbenv;
	DB_LOCKER *parent, *locker;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(parent, NULL);
	COMPQUIET(locker, NULL);
	return (0);
}

int
__lock_set_timeout(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	DB_LOCKER *locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(locker, NULL);
	COMPQUIET(timeout, 0);
	COMPQUIET(op, 0);
	return (0);
}

int
__lock_addfamilylocker(dbenv, pid, id)
	DB_ENV *dbenv;
	u_int32_t pid, id;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(pid, 0);
	COMPQUIET(id, 0);
	return (0);
}

int
__lock_freefamilylocker(lt, sh_locker)
	DB_LOCKTAB *lt;
	DB_LOCKER *sh_locker;
{
	COMPQUIET(lt, NULL);
	COMPQUIET(sh_locker, NULL);
	return (0);
}

int
__lock_downgrade(dbenv, lock, new_mode, flags)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	db_lockmode_t new_mode;
	u_int32_t flags;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(lock, NULL);
	COMPQUIET(new_mode, 0);
	COMPQUIET(flags, 0);
	return (0);
}

int
__lock_locker_is_parent(dbenv, locker, child, retp)
	DB_ENV *dbenv;
	DB_LOCKER *locker;
	DB_LOCKER *child;
	int *retp;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(locker, NULL);
	COMPQUIET(child, NULL);

	*retp = 1;
	return (0);
}

void
__lock_set_thread_id(lref, pid, tid)
	void *lref;
	pid_t pid;
	db_threadid_t tid;
{
	COMPQUIET(lref, NULL);
	COMPQUIET(pid, 0);
	COMPQUIET(tid, 0);
}

int
__lock_failchk(dbenv)
	DB_ENV *dbenv;
{
	COMPQUIET(dbenv, NULL);
	return (0);
}

int
__lock_get_list(dbenv, locker, flags, lock_mode, list)
	DB_ENV *dbenv;
	DB_LOCKER *locker;
	u_int32_t flags;
	db_lockmode_t lock_mode;
	DBT *list;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(locker, NULL);
	COMPQUIET(flags, 0);
	COMPQUIET(lock_mode, 0);
	COMPQUIET(list, NULL);
	return (0);
}

void
__lock_list_print(dbenv, list)
	DB_ENV *dbenv;
	DBT *list;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(list, NULL);
}

int
__lock_getlocker(lt, locker, create, retp)
	DB_LOCKTAB *lt;
	u_int32_t locker;
	int create;
	DB_LOCKER **retp;
{
	COMPQUIET(lt, NULL);
	COMPQUIET(locker, 0);
	COMPQUIET(create, 0);
	COMPQUIET(retp, NULL);
	return (__db_nolocking(lt->dbenv));
}

int
__lock_id_set(dbenv, cur_id, max_id)
	DB_ENV *dbenv;
	u_int32_t cur_id, max_id;
{
	COMPQUIET(dbenv, NULL);
	COMPQUIET(cur_id, 0);
	COMPQUIET(max_id, 0);
	return (0);
}
