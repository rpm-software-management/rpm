/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: db_cds.c,v 12.10 2007/07/17 07:29:04 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/txn.h"

static int __cdsgroup_abort __P((DB_TXN *txn));
static int __cdsgroup_commit __P((DB_TXN *txn, u_int32_t flags));
static int __cdsgroup_discard __P((DB_TXN *txn, u_int32_t flags));
static u_int32_t __cdsgroup_id __P((DB_TXN *txn));
static int __cdsgroup_notsup __P((DB_ENV *dbenv, const char *meth));
static int __cdsgroup_prepare __P((DB_TXN *txn, u_int8_t *gid));
static int __cdsgroup_set_name __P((DB_TXN *txn, const char *name));
static int __cdsgroup_set_timeout
    __P((DB_TXN *txn, db_timeout_t timeout, u_int32_t flags));

/*
 * __cdsgroup_notsup --
 *	Error when CDS groups don't support a method.
 */
static int
__cdsgroup_notsup(dbenv, meth)
	DB_ENV *dbenv;
	const char *meth;
{
	__db_errx(dbenv, "CDS groups do not support %s", meth);
	return (DB_OPNOTSUP);
}

static int __cdsgroup_abort(txn)
	DB_TXN *txn;
{
	return (__cdsgroup_notsup(txn->mgrp->dbenv, "abort"));
}

static int __cdsgroup_commit(txn, flags)
	DB_TXN *txn;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_LOCKREQ lreq;
	DB_LOCKER *locker;
	int ret, t_ret;

	COMPQUIET(flags, 0);
	dbenv = txn->mgrp->dbenv;

	/* Check for live cursors. */
	if (txn->cursors != 0) {
		__db_errx(dbenv, "CDS group has active cursors");
		return (EINVAL);
	}

	/* We may be holding handle locks; release them. */
	lreq.op = DB_LOCK_PUT_ALL;
	lreq.obj = NULL;
	ret = __lock_vec(dbenv, txn->locker, 0, &lreq, 1, NULL);

	dbenv = txn->mgrp->dbenv;
	locker = txn->locker;
	__os_free(dbenv, txn->mgrp);
	__os_free(dbenv, txn);
	if ((t_ret = __lock_id_free(dbenv, locker)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

static int __cdsgroup_discard(txn, flags)
	DB_TXN *txn;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);
	return (__cdsgroup_notsup(txn->mgrp->dbenv, "discard"));
}

static u_int32_t __cdsgroup_id(txn)
	DB_TXN *txn;
{
	return (txn->txnid);
}

static int __cdsgroup_prepare(txn, gid)
	DB_TXN *txn;
	u_int8_t *gid;
{
	COMPQUIET(gid, NULL);
	return (__cdsgroup_notsup(txn->mgrp->dbenv, "prepare"));
}

static int __cdsgroup_set_name(txn, name)
	DB_TXN *txn;
	const char *name;
{
	COMPQUIET(name, NULL);
	return (__cdsgroup_notsup(txn->mgrp->dbenv, "set_name"));
}

static int __cdsgroup_set_timeout(txn, timeout, flags)
	DB_TXN *txn;
	db_timeout_t timeout;
	u_int32_t flags;
{
	COMPQUIET(timeout, 0);
	COMPQUIET(flags, 0);
	return (__cdsgroup_notsup(txn->mgrp->dbenv, "set_timeout"));
}

/*
 * __cds_txn_begin --
 *	DB_ENV->cdsgroup_begin
 *
 * PUBLIC: int __cdsgroup_begin __P((DB_ENV *, DB_TXN **));
 */
int
__cdsgroup_begin(dbenv, txnpp)
	DB_ENV *dbenv;
	DB_TXN **txnpp;
{
	DB_THREAD_INFO *ip;
	DB_TXN *txn;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "cdsgroup_begin");
	if (!CDB_LOCKING(dbenv))
		return (__db_env_config(dbenv, "cdsgroup_begin", DB_INIT_CDB));

	ENV_ENTER(dbenv, ip);
	*txnpp = txn = NULL;
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXN), &txn)) != 0)
		goto err;
	/*
	 * We need a dummy DB_TXNMGR -- it's the only way to get from a
	 * transaction handle to the environment handle.
	 */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXNMGR), &txn->mgrp)) != 0)
		goto err;
	txn->mgrp->dbenv = dbenv;

	if ((ret = __lock_id(dbenv, &txn->txnid, &txn->locker)) != 0)
		goto err;

	txn->flags = TXN_CDSGROUP;
	txn->abort = __cdsgroup_abort;
	txn->commit = __cdsgroup_commit;
	txn->discard = __cdsgroup_discard;
	txn->id = __cdsgroup_id;
	txn->prepare = __cdsgroup_prepare;
	txn->set_name = __cdsgroup_set_name;
	txn->set_timeout = __cdsgroup_set_timeout;

	*txnpp = txn;

	if (0) {
err:		if (txn != NULL) {
			if (txn->mgrp != NULL)
				__os_free(dbenv, txn->mgrp);
			__os_free(dbenv, txn);
		}
	}
	ENV_LEAVE(dbenv, ip);
	return (ret);
}
