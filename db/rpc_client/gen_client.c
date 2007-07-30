/* Do not edit: automatically built by gen_rpc.awk. */
#include "db_config.h"

#include "db_int.h"
#ifdef HAVE_SYSTEM_INCLUDE_FILES
#include <rpc/rpc.h>
#endif
#include "db_server.h"
#include "dbinc/txn.h"
#include "dbinc_auto/rpc_client_ext.h"

static int __dbcl_dbp_illegal __P((DB *));
static int __dbcl_noserver __P((DB_ENV *));
static int __dbcl_txn_illegal __P((DB_TXN *));

static int
__dbcl_noserver(dbenv)
	DB_ENV *dbenv;
{
	__db_errx(dbenv, "No Berkeley DB RPC server environment");
	return (DB_NOSERVER);
}

/*
 * __dbcl_dbenv_illegal --
 *	DB_ENV method not supported under RPC.
 *
 * PUBLIC: int __dbcl_dbenv_illegal __P((DB_ENV *));
 */
int
__dbcl_dbenv_illegal(dbenv)
	DB_ENV *dbenv;
{
	__db_errx(dbenv,
	    "Interface not supported by Berkeley DB RPC client environments");
	return (DB_OPNOTSUP);
}

/*
 * __dbcl_dbp_illegal --
 *	DB method not supported under RPC.
 */
static int
__dbcl_dbp_illegal(dbp)
	DB *dbp;
{
	return (__dbcl_dbenv_illegal(dbp->dbenv));
}

/*
 * __dbcl_txn_illegal --
 *	DB_TXN method not supported under RPC.
 */
static int
__dbcl_txn_illegal(txn)
	DB_TXN *txn;
{
	return (__dbcl_dbenv_illegal(txn->mgrp->dbenv));
}

/*
 * PUBLIC: int __dbcl_env_create __P((DB_ENV *, long));
 */
int
__dbcl_env_create(dbenv, timeout)
	DB_ENV * dbenv;
	long timeout;
{
	CLIENT *cl;
	__env_create_msg msg;
	__env_create_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.timeout = (u_int)timeout;

	replyp = __db_env_create_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_create_ret(dbenv, timeout, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_create_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_cdsgroup_begin __P((DB_ENV *, DB_TXN **));
 */
int
__dbcl_env_cdsgroup_begin(dbenv, txnpp)
	DB_ENV * dbenv;
	DB_TXN ** txnpp;
{
	CLIENT *cl;
	__env_cdsgroup_begin_msg msg;
	__env_cdsgroup_begin_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_cdsgroup_begin_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_cdsgroup_begin_ret(dbenv, txnpp, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_cdsgroup_begin_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_close __P((DB_ENV *, u_int32_t));
 */
int
__dbcl_env_close(dbenv, flags)
	DB_ENV * dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_close_msg msg;
	__env_close_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_env_close_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_dbremove __P((DB_ENV *, DB_TXN *, const char *,
 * PUBLIC:      const char *, u_int32_t));
 */
int
__dbcl_env_dbremove(dbenv, txnp, name, subdb, flags)
	DB_ENV * dbenv;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_dbremove_msg msg;
	__env_dbremove_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.flags = (u_int)flags;

	replyp = __db_env_dbremove_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_dbremove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_dbrename __P((DB_ENV *, DB_TXN *, const char *,
 * PUBLIC:      const char *, const char *, u_int32_t));
 */
int
__dbcl_env_dbrename(dbenv, txnp, name, subdb, newname, flags)
	DB_ENV * dbenv;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	const char * newname;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_dbrename_msg msg;
	__env_dbrename_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	if (newname == NULL)
		msg.newname = "";
	else
		msg.newname = (char *)newname;
	msg.flags = (u_int)flags;

	replyp = __db_env_dbrename_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_dbrename_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_cachesize __P((DB_ENV *, u_int32_t *,
 * PUBLIC:      u_int32_t *, int *));
 */
int
__dbcl_env_get_cachesize(dbenv, gbytesp, bytesp, ncachep)
	DB_ENV * dbenv;
	u_int32_t * gbytesp;
	u_int32_t * bytesp;
	int * ncachep;
{
	CLIENT *cl;
	__env_get_cachesize_msg msg;
	__env_get_cachesize_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_cachesize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (gbytesp != NULL)
		*gbytesp = (u_int32_t)replyp->gbytes;
	if (bytesp != NULL)
		*bytesp = (u_int32_t)replyp->bytes;
	if (ncachep != NULL)
		*ncachep = (int)replyp->ncache;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_cachesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_encrypt_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_encrypt_flags_msg msg;
	__env_get_encrypt_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_encrypt_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_encrypt_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_flags_msg msg;
	__env_get_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_home __P((DB_ENV *, const char * *));
 */
int
__dbcl_env_get_home(dbenv, homep)
	DB_ENV * dbenv;
	const char * * homep;
{
	CLIENT *cl;
	__env_get_home_msg msg;
	__env_get_home_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_home_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (homep != NULL)
		*homep = (const char *)replyp->home;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_home_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_get_open_flags __P((DB_ENV *, u_int32_t *));
 */
int
__dbcl_env_get_open_flags(dbenv, flagsp)
	DB_ENV * dbenv;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__env_get_open_flags_msg msg;
	__env_get_open_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;

	replyp = __db_env_get_open_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_get_open_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_open __P((DB_ENV *, const char *, u_int32_t, int));
 */
int
__dbcl_env_open(dbenv, home, flags, mode)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__env_open_msg msg;
	__env_open_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		msg.home = "";
	else
		msg.home = (char *)home;
	msg.flags = (u_int)flags;
	msg.mode = (u_int)mode;

	replyp = __db_env_open_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_open_ret(dbenv, home, flags, mode, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_open_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_remove __P((DB_ENV *, const char *, u_int32_t));
 */
int
__dbcl_env_remove(dbenv, home, flags)
	DB_ENV * dbenv;
	const char * home;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_remove_msg msg;
	__env_remove_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (home == NULL)
		msg.home = "";
	else
		msg.home = (char *)home;
	msg.flags = (u_int)flags;

	replyp = __db_env_remove_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_remove_ret(dbenv, home, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_remove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_set_cachesize __P((DB_ENV *, u_int32_t, u_int32_t,
 * PUBLIC:      int));
 */
int
__dbcl_env_set_cachesize(dbenv, gbytes, bytes, ncache)
	DB_ENV * dbenv;
	u_int32_t gbytes;
	u_int32_t bytes;
	int ncache;
{
	CLIENT *cl;
	__env_set_cachesize_msg msg;
	__env_set_cachesize_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	msg.gbytes = (u_int)gbytes;
	msg.bytes = (u_int)bytes;
	msg.ncache = (u_int)ncache;

	replyp = __db_env_set_cachesize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_set_cachesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
 */
int
__dbcl_env_set_encrypt(dbenv, passwd, flags)
	DB_ENV * dbenv;
	const char * passwd;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_set_encrypt_msg msg;
	__env_set_encrypt_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (passwd == NULL)
		msg.passwd = "";
	else
		msg.passwd = (char *)passwd;
	msg.flags = (u_int)flags;

	replyp = __db_env_set_encrypt_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_set_encrypt_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_set_flags __P((DB_ENV *, u_int32_t, int));
 */
int
__dbcl_env_set_flags(dbenv, flags, onoff)
	DB_ENV * dbenv;
	u_int32_t flags;
	int onoff;
{
	CLIENT *cl;
	__env_set_flags_msg msg;
	__env_set_flags_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = (u_int)flags;
	msg.onoff = (u_int)onoff;

	replyp = __db_env_set_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_set_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_txn_begin __P((DB_ENV *, DB_TXN *, DB_TXN **,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_env_txn_begin(dbenv, parent, txnpp, flags)
	DB_ENV * dbenv;
	DB_TXN * parent;
	DB_TXN ** txnpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_txn_begin_msg msg;
	__env_txn_begin_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	if (parent == NULL)
		msg.parentcl_id = 0;
	else
		msg.parentcl_id = parent->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_env_txn_begin_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_txn_begin_ret(dbenv, parent, txnpp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_txn_begin_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_env_txn_recover __P((DB_ENV *, DB_PREPLIST *, long,
 * PUBLIC:      long *, u_int32_t));
 */
int
__dbcl_env_txn_recover(dbenv, preplist, count, retp, flags)
	DB_ENV * dbenv;
	DB_PREPLIST * preplist;
	long count;
	long * retp;
	u_int32_t flags;
{
	CLIENT *cl;
	__env_txn_recover_msg msg;
	__env_txn_recover_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	msg.count = (u_int)count;
	msg.flags = (u_int)flags;

	replyp = __db_env_txn_recover_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_env_txn_recover_ret(dbenv, preplist, count, retp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___env_txn_recover_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_create __P((DB *, DB_ENV *, u_int32_t));
 */
int
__dbcl_db_create(dbp, dbenv, flags)
	DB * dbp;
	DB_ENV * dbenv;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_create_msg msg;
	__db_create_reply *replyp = NULL;
	int ret;

	ret = 0;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(dbenv));

	cl = (CLIENT *)dbenv->cl_handle;

	msg.dbenvcl_id = dbenv->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_create_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_create_ret(dbp, dbenv, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_create_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_associate __P((DB *, DB_TXN *, DB *, int (*)(DB *,
 * PUBLIC:      const DBT *, const DBT *, DBT *), u_int32_t));
 */
int
__dbcl_db_associate(dbp, txnp, sdbp, func0, flags)
	DB * dbp;
	DB_TXN * txnp;
	DB * sdbp;
	int (*func0) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	CLIENT *cl;
	__db_associate_msg msg;
	__db_associate_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (func0 != NULL) {
		__db_errx(dbenv, "User functions not supported in RPC");
		return (EINVAL);
	}
	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (sdbp == NULL)
		msg.sdbpcl_id = 0;
	else
		msg.sdbpcl_id = sdbp->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_associate_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_associate_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_close __P((DB *, u_int32_t));
 */
int
__dbcl_db_close(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_close_msg msg;
	__db_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_close_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_close_ret(dbp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
 */
int
__dbcl_db_cursor(dbp, txnp, dbcpp, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBC ** dbcpp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_cursor_msg msg;
	__db_cursor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_db_cursor_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_cursor_ret(dbp, txnp, dbcpp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_cursor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__dbcl_db_del(dbp, txnp, key, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_del_msg msg;
	__db_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.flags = (u_int)flags;

	replyp = __db_db_del_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_del_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_db_get(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_get_msg msg;
	__db_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_db_get_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_get_ret(dbp, txnp, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_bt_minkey __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_bt_minkey(dbp, minkeyp)
	DB * dbp;
	u_int32_t * minkeyp;
{
	CLIENT *cl;
	__db_get_bt_minkey_msg msg;
	__db_get_bt_minkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_bt_minkey_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (minkeyp != NULL)
		*minkeyp = (u_int32_t)replyp->minkey;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_bt_minkey_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_dbname __P((DB *, const char * *,
 * PUBLIC:      const char * *));
 */
int
__dbcl_db_get_dbname(dbp, filenamep, dbnamep)
	DB * dbp;
	const char * * filenamep;
	const char * * dbnamep;
{
	CLIENT *cl;
	__db_get_dbname_msg msg;
	__db_get_dbname_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_dbname_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (filenamep != NULL)
		*filenamep = (const char *)replyp->filename;
	if (dbnamep != NULL)
		*dbnamep = (const char *)replyp->dbname;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_dbname_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_encrypt_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_encrypt_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_encrypt_flags_msg msg;
	__db_get_encrypt_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_encrypt_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_encrypt_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_flags_msg msg;
	__db_get_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_h_ffactor __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_h_ffactor(dbp, ffactorp)
	DB * dbp;
	u_int32_t * ffactorp;
{
	CLIENT *cl;
	__db_get_h_ffactor_msg msg;
	__db_get_h_ffactor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_h_ffactor_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (ffactorp != NULL)
		*ffactorp = (u_int32_t)replyp->ffactor;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_h_ffactor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_h_nelem __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_h_nelem(dbp, nelemp)
	DB * dbp;
	u_int32_t * nelemp;
{
	CLIENT *cl;
	__db_get_h_nelem_msg msg;
	__db_get_h_nelem_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_h_nelem_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (nelemp != NULL)
		*nelemp = (u_int32_t)replyp->nelem;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_h_nelem_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_lorder __P((DB *, int *));
 */
int
__dbcl_db_get_lorder(dbp, lorderp)
	DB * dbp;
	int * lorderp;
{
	CLIENT *cl;
	__db_get_lorder_msg msg;
	__db_get_lorder_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_lorder_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (lorderp != NULL)
		*lorderp = (int)replyp->lorder;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_lorder_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_open_flags __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_open_flags(dbp, flagsp)
	DB * dbp;
	u_int32_t * flagsp;
{
	CLIENT *cl;
	__db_get_open_flags_msg msg;
	__db_get_open_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_open_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (flagsp != NULL)
		*flagsp = (u_int32_t)replyp->flags;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_open_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_pagesize __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_pagesize(dbp, pagesizep)
	DB * dbp;
	u_int32_t * pagesizep;
{
	CLIENT *cl;
	__db_get_pagesize_msg msg;
	__db_get_pagesize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_pagesize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (pagesizep != NULL)
		*pagesizep = (u_int32_t)replyp->pagesize;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_pagesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_priority __P((DB *, DB_CACHE_PRIORITY *));
 */
int
__dbcl_db_get_priority(dbp, priorityp)
	DB * dbp;
	DB_CACHE_PRIORITY * priorityp;
{
	CLIENT *cl;
	__db_get_priority_msg msg;
	__db_get_priority_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_priority_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (priorityp != NULL)
		*priorityp = (DB_CACHE_PRIORITY)replyp->priority;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_priority_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_q_extentsize __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_q_extentsize(dbp, extentsizep)
	DB * dbp;
	u_int32_t * extentsizep;
{
	CLIENT *cl;
	__db_get_q_extentsize_msg msg;
	__db_get_q_extentsize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_q_extentsize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (extentsizep != NULL)
		*extentsizep = (u_int32_t)replyp->extentsize;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_q_extentsize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_delim __P((DB *, int *));
 */
int
__dbcl_db_get_re_delim(dbp, delimp)
	DB * dbp;
	int * delimp;
{
	CLIENT *cl;
	__db_get_re_delim_msg msg;
	__db_get_re_delim_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_delim_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (delimp != NULL)
		*delimp = (int)replyp->delim;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_delim_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_len __P((DB *, u_int32_t *));
 */
int
__dbcl_db_get_re_len(dbp, lenp)
	DB * dbp;
	u_int32_t * lenp;
{
	CLIENT *cl;
	__db_get_re_len_msg msg;
	__db_get_re_len_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_len_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (lenp != NULL)
		*lenp = (u_int32_t)replyp->len;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_len_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_get_re_pad __P((DB *, int *));
 */
int
__dbcl_db_get_re_pad(dbp, padp)
	DB * dbp;
	int * padp;
{
	CLIENT *cl;
	__db_get_re_pad_msg msg;
	__db_get_re_pad_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;

	replyp = __db_db_get_re_pad_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (padp != NULL)
		*padp = (int)replyp->pad;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_get_re_pad_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_join __P((DB *, DBC **, DBC **, u_int32_t));
 */
int
__dbcl_db_join(dbp, curs, dbcp, flags)
	DB * dbp;
	DBC ** curs;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_join_msg msg;
	__db_join_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;
	DBC ** cursp;
	int cursi;
	u_int32_t * cursq;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	for (cursi = 0, cursp = curs; *cursp != 0;  cursi++, cursp++)
		;
	msg.curs.curs_len = (u_int)cursi;
	if ((ret = __os_calloc(dbenv,
	    msg.curs.curs_len, sizeof(u_int32_t), &msg.curs.curs_val)) != 0)
		return (ret);
	for (cursq = msg.curs.curs_val, cursp = curs; cursi--; cursq++, cursp++)
		*cursq = (*cursp)->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_join_4006(&msg, cl);
	__os_free(dbenv, msg.curs.curs_val);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_join_ret(dbp, curs, dbcp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_join_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_key_range __P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_key_range(dbp, txnp, key, range, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DB_KEY_RANGE * range;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_key_range_msg msg;
	__db_key_range_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.flags = (u_int)flags;

	replyp = __db_db_key_range_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_key_range_ret(dbp, txnp, key, range, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_key_range_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_open __P((DB *, DB_TXN *, const char *, const char *,
 * PUBLIC:      DBTYPE, u_int32_t, int));
 */
int
__dbcl_db_open(dbp, txnp, name, subdb, type, flags, mode)
	DB * dbp;
	DB_TXN * txnp;
	const char * name;
	const char * subdb;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	CLIENT *cl;
	__db_open_msg msg;
	__db_open_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.type = (u_int)type;
	msg.flags = (u_int)flags;
	msg.mode = (u_int)mode;

	replyp = __db_db_open_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_open_ret(dbp, txnp, name, subdb, type, flags, mode, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_open_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_pget __P((DB *, DB_TXN *, DBT *, DBT *, DBT *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_pget(dbp, txnp, skey, pkey, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_pget_msg msg;
	__db_pget_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.skeydlen = skey->dlen;
	msg.skeydoff = skey->doff;
	msg.skeyulen = skey->ulen;
	msg.skeyflags = skey->flags;
	msg.skeydata.skeydata_val = skey->data;
	msg.skeydata.skeydata_len = skey->size;
	msg.pkeydlen = pkey->dlen;
	msg.pkeydoff = pkey->doff;
	msg.pkeyulen = pkey->ulen;
	msg.pkeyflags = pkey->flags;
	msg.pkeydata.pkeydata_val = pkey->data;
	msg.pkeydata.pkeydata_len = pkey->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_db_pget_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_pget_ret(dbp, txnp, skey, pkey, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_pget_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_db_put(dbp, txnp, key, data, flags)
	DB * dbp;
	DB_TXN * txnp;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_put_msg msg;
	__db_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_db_put_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_put_ret(dbp, txnp, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_put_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_remove __P((DB *, const char *, const char *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_remove(dbp, name, subdb, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_remove_msg msg;
	__db_remove_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	msg.flags = (u_int)flags;

	replyp = __db_db_remove_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_remove_ret(dbp, name, subdb, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_remove_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_rename __P((DB *, const char *, const char *,
 * PUBLIC:      const char *, u_int32_t));
 */
int
__dbcl_db_rename(dbp, name, subdb, newname, flags)
	DB * dbp;
	const char * name;
	const char * subdb;
	const char * newname;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_rename_msg msg;
	__db_rename_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (name == NULL)
		msg.name = "";
	else
		msg.name = (char *)name;
	if (subdb == NULL)
		msg.subdb = "";
	else
		msg.subdb = (char *)subdb;
	if (newname == NULL)
		msg.newname = "";
	else
		msg.newname = (char *)newname;
	msg.flags = (u_int)flags;

	replyp = __db_db_rename_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_rename_ret(dbp, name, subdb, newname, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_rename_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_bt_minkey __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_bt_minkey(dbp, minkey)
	DB * dbp;
	u_int32_t minkey;
{
	CLIENT *cl;
	__db_set_bt_minkey_msg msg;
	__db_set_bt_minkey_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.minkey = (u_int)minkey;

	replyp = __db_db_set_bt_minkey_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_bt_minkey_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_encrypt __P((DB *, const char *, u_int32_t));
 */
int
__dbcl_db_set_encrypt(dbp, passwd, flags)
	DB * dbp;
	const char * passwd;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_set_encrypt_msg msg;
	__db_set_encrypt_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (passwd == NULL)
		msg.passwd = "";
	else
		msg.passwd = (char *)passwd;
	msg.flags = (u_int)flags;

	replyp = __db_db_set_encrypt_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_encrypt_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_flags __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_flags(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_set_flags_msg msg;
	__db_set_flags_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_set_flags_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_flags_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_h_ffactor __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_h_ffactor(dbp, ffactor)
	DB * dbp;
	u_int32_t ffactor;
{
	CLIENT *cl;
	__db_set_h_ffactor_msg msg;
	__db_set_h_ffactor_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.ffactor = (u_int)ffactor;

	replyp = __db_db_set_h_ffactor_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_h_ffactor_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_h_nelem __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_h_nelem(dbp, nelem)
	DB * dbp;
	u_int32_t nelem;
{
	CLIENT *cl;
	__db_set_h_nelem_msg msg;
	__db_set_h_nelem_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.nelem = (u_int)nelem;

	replyp = __db_db_set_h_nelem_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_h_nelem_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_lorder __P((DB *, int));
 */
int
__dbcl_db_set_lorder(dbp, lorder)
	DB * dbp;
	int lorder;
{
	CLIENT *cl;
	__db_set_lorder_msg msg;
	__db_set_lorder_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.lorder = (u_int)lorder;

	replyp = __db_db_set_lorder_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_lorder_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_pagesize __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_pagesize(dbp, pagesize)
	DB * dbp;
	u_int32_t pagesize;
{
	CLIENT *cl;
	__db_set_pagesize_msg msg;
	__db_set_pagesize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.pagesize = (u_int)pagesize;

	replyp = __db_db_set_pagesize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_pagesize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_priority __P((DB *, DB_CACHE_PRIORITY));
 */
int
__dbcl_db_set_priority(dbp, priority)
	DB * dbp;
	DB_CACHE_PRIORITY priority;
{
	CLIENT *cl;
	__db_set_priority_msg msg;
	__db_set_priority_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.priority = (u_int)priority;

	replyp = __db_db_set_priority_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_priority_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_q_extentsize __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_q_extentsize(dbp, extentsize)
	DB * dbp;
	u_int32_t extentsize;
{
	CLIENT *cl;
	__db_set_q_extentsize_msg msg;
	__db_set_q_extentsize_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.extentsize = (u_int)extentsize;

	replyp = __db_db_set_q_extentsize_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_q_extentsize_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_re_delim __P((DB *, int));
 */
int
__dbcl_db_set_re_delim(dbp, delim)
	DB * dbp;
	int delim;
{
	CLIENT *cl;
	__db_set_re_delim_msg msg;
	__db_set_re_delim_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.delim = (u_int)delim;

	replyp = __db_db_set_re_delim_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_re_delim_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_re_len __P((DB *, u_int32_t));
 */
int
__dbcl_db_set_re_len(dbp, len)
	DB * dbp;
	u_int32_t len;
{
	CLIENT *cl;
	__db_set_re_len_msg msg;
	__db_set_re_len_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.len = (u_int)len;

	replyp = __db_db_set_re_len_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_re_len_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_set_re_pad __P((DB *, int));
 */
int
__dbcl_db_set_re_pad(dbp, pad)
	DB * dbp;
	int pad;
{
	CLIENT *cl;
	__db_set_re_pad_msg msg;
	__db_set_re_pad_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.pad = (u_int)pad;

	replyp = __db_db_set_re_pad_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_set_re_pad_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_stat __P((DB *, DB_TXN *, void *, u_int32_t));
 */
int
__dbcl_db_stat(dbp, txnp, sp, flags)
	DB * dbp;
	DB_TXN * txnp;
	void * sp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_stat_msg msg;
	__db_stat_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_db_stat_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_stat_ret(dbp, txnp, sp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_stat_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_sync __P((DB *, u_int32_t));
 */
int
__dbcl_db_sync(dbp, flags)
	DB * dbp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_sync_msg msg;
	__db_sync_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_db_sync_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_sync_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_db_truncate __P((DB *, DB_TXN *, u_int32_t  *,
 * PUBLIC:      u_int32_t));
 */
int
__dbcl_db_truncate(dbp, txnp, countp, flags)
	DB * dbp;
	DB_TXN * txnp;
	u_int32_t  * countp;
	u_int32_t flags;
{
	CLIENT *cl;
	__db_truncate_msg msg;
	__db_truncate_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbp == NULL)
		msg.dbpcl_id = 0;
	else
		msg.dbpcl_id = dbp->cl_id;
	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_db_truncate_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_db_truncate_ret(dbp, txnp, countp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___db_truncate_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_close __P((DBC *));
 */
int
__dbcl_dbc_close(dbc)
	DBC * dbc;
{
	CLIENT *cl;
	__dbc_close_msg msg;
	__dbc_close_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;

	replyp = __db_dbc_close_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_close_ret(dbc, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_close_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_count __P((DBC *, db_recno_t *, u_int32_t));
 */
int
__dbcl_dbc_count(dbc, countp, flags)
	DBC * dbc;
	db_recno_t * countp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_count_msg msg;
	__dbc_count_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_count_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_count_ret(dbc, countp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_count_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_del __P((DBC *, u_int32_t));
 */
int
__dbcl_dbc_del(dbc, flags)
	DBC * dbc;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_del_msg msg;
	__dbc_del_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_del_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_del_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_dup __P((DBC *, DBC **, u_int32_t));
 */
int
__dbcl_dbc_dup(dbc, dbcp, flags)
	DBC * dbc;
	DBC ** dbcp;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_dup_msg msg;
	__dbc_dup_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_dup_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_dup_ret(dbc, dbcp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_dup_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_get __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_get(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_get_msg msg;
	__dbc_get_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_get_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_get_ret(dbc, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_get_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_get_priority __P((DBC *, DB_CACHE_PRIORITY *));
 */
int
__dbcl_dbc_get_priority(dbc, priorityp)
	DBC * dbc;
	DB_CACHE_PRIORITY * priorityp;
{
	CLIENT *cl;
	__dbc_get_priority_msg msg;
	__dbc_get_priority_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;

	replyp = __db_dbc_get_priority_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
	if (priorityp != NULL)
		*priorityp = (DB_CACHE_PRIORITY)replyp->priority;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_get_priority_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_pget __P((DBC *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_pget(dbc, skey, pkey, data, flags)
	DBC * dbc;
	DBT * skey;
	DBT * pkey;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_pget_msg msg;
	__dbc_pget_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.skeydlen = skey->dlen;
	msg.skeydoff = skey->doff;
	msg.skeyulen = skey->ulen;
	msg.skeyflags = skey->flags;
	msg.skeydata.skeydata_val = skey->data;
	msg.skeydata.skeydata_len = skey->size;
	msg.pkeydlen = pkey->dlen;
	msg.pkeydoff = pkey->doff;
	msg.pkeyulen = pkey->ulen;
	msg.pkeyflags = pkey->flags;
	msg.pkeydata.pkeydata_val = pkey->data;
	msg.pkeydata.pkeydata_len = pkey->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_pget_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_pget_ret(dbc, skey, pkey, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_pget_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_put __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__dbcl_dbc_put(dbc, key, data, flags)
	DBC * dbc;
	DBT * key;
	DBT * data;
	u_int32_t flags;
{
	CLIENT *cl;
	__dbc_put_msg msg;
	__dbc_put_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.keydlen = key->dlen;
	msg.keydoff = key->doff;
	msg.keyulen = key->ulen;
	msg.keyflags = key->flags;
	msg.keydata.keydata_val = key->data;
	msg.keydata.keydata_len = key->size;
	msg.datadlen = data->dlen;
	msg.datadoff = data->doff;
	msg.dataulen = data->ulen;
	msg.dataflags = data->flags;
	msg.datadata.datadata_val = data->data;
	msg.datadata.datadata_len = data->size;
	msg.flags = (u_int)flags;

	replyp = __db_dbc_put_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_dbc_put_ret(dbc, key, data, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_put_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_dbc_set_priority __P((DBC *, DB_CACHE_PRIORITY));
 */
int
__dbcl_dbc_set_priority(dbc, priority)
	DBC * dbc;
	DB_CACHE_PRIORITY priority;
{
	CLIENT *cl;
	__dbc_set_priority_msg msg;
	__dbc_set_priority_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = dbc->dbp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (dbc == NULL)
		msg.dbccl_id = 0;
	else
		msg.dbccl_id = dbc->cl_id;
	msg.priority = (u_int)priority;

	replyp = __db_dbc_set_priority_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___dbc_set_priority_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_abort __P((DB_TXN *));
 */
int
__dbcl_txn_abort(txnp)
	DB_TXN * txnp;
{
	CLIENT *cl;
	__txn_abort_msg msg;
	__txn_abort_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;

	replyp = __db_txn_abort_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_abort_ret(txnp, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_abort_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_commit __P((DB_TXN *, u_int32_t));
 */
int
__dbcl_txn_commit(txnp, flags)
	DB_TXN * txnp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_commit_msg msg;
	__txn_commit_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_txn_commit_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_commit_ret(txnp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_commit_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_discard __P((DB_TXN *, u_int32_t));
 */
int
__dbcl_txn_discard(txnp, flags)
	DB_TXN * txnp;
	u_int32_t flags;
{
	CLIENT *cl;
	__txn_discard_msg msg;
	__txn_discard_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	msg.flags = (u_int)flags;

	replyp = __db_txn_discard_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = __dbcl_txn_discard_ret(txnp, flags, replyp);
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_discard_reply, (void *)replyp);
	return (ret);
}

/*
 * PUBLIC: int __dbcl_txn_prepare __P((DB_TXN *, u_int8_t *));
 */
int
__dbcl_txn_prepare(txnp, gid)
	DB_TXN * txnp;
	u_int8_t * gid;
{
	CLIENT *cl;
	__txn_prepare_msg msg;
	__txn_prepare_reply *replyp = NULL;
	int ret;
	DB_ENV *dbenv;

	ret = 0;
	dbenv = txnp->mgrp->dbenv;
	if (dbenv == NULL || !RPC_ON(dbenv))
		return (__dbcl_noserver(NULL));

	cl = (CLIENT *)dbenv->cl_handle;

	if (txnp == NULL)
		msg.txnpcl_id = 0;
	else
		msg.txnpcl_id = txnp->txnid;
	memcpy(msg.gid, gid, 128);

	replyp = __db_txn_prepare_4006(&msg, cl);
	if (replyp == NULL) {
		__db_errx(dbenv, clnt_sperror(cl, "Berkeley DB"));
		ret = DB_NOSERVER;
		goto out;
	}
	ret = replyp->status;
out:
	if (replyp != NULL)
		xdr_free((xdrproc_t)xdr___txn_prepare_reply, (void *)replyp);
	return (ret);
}

/*
 * __dbcl_dbp_init --
 *	Initialize DB handle methods.
 *
 * PUBLIC: void __dbcl_dbp_init __P((DB *));
 */
void
__dbcl_dbp_init(dbp)
	DB *dbp;
{
	dbp->associate = __dbcl_db_associate;
	dbp->close = __dbcl_db_close;
	dbp->compact =
	    (int (*)(DB *, DB_TXN *, DBT *, DBT *, DB_COMPACT *, u_int32_t, DBT *))
	    __dbcl_dbp_illegal;
	dbp->cursor = __dbcl_db_cursor;
	dbp->del = __dbcl_db_del;
	dbp->fd =
	    (int (*)(DB *, int *))
	    __dbcl_dbp_illegal;
	dbp->get = __dbcl_db_get;
	dbp->get_bt_minkey = __dbcl_db_get_bt_minkey;
	dbp->get_cachesize =
	    (int (*)(DB *, u_int32_t *, u_int32_t *, int *))
	    __dbcl_dbp_illegal;
	dbp->get_dbname = __dbcl_db_get_dbname;
	dbp->get_encrypt_flags = __dbcl_db_get_encrypt_flags;
	dbp->get_flags = __dbcl_db_get_flags;
	dbp->get_h_ffactor = __dbcl_db_get_h_ffactor;
	dbp->get_h_nelem = __dbcl_db_get_h_nelem;
	dbp->get_lorder = __dbcl_db_get_lorder;
	dbp->get_mpf =
	    (DB_MPOOLFILE * (*)(DB *))
	    __dbcl_dbp_illegal;
	dbp->get_open_flags = __dbcl_db_get_open_flags;
	dbp->get_pagesize = __dbcl_db_get_pagesize;
	dbp->get_priority = __dbcl_db_get_priority;
	dbp->get_q_extentsize = __dbcl_db_get_q_extentsize;
	dbp->get_re_delim = __dbcl_db_get_re_delim;
	dbp->get_re_len = __dbcl_db_get_re_len;
	dbp->get_re_pad = __dbcl_db_get_re_pad;
	dbp->get_re_source =
	    (int (*)(DB *, const char **))
	    __dbcl_dbp_illegal;
	dbp->join = __dbcl_db_join;
	dbp->key_range = __dbcl_db_key_range;
	dbp->open = __dbcl_db_open;
	dbp->pget = __dbcl_db_pget;
	dbp->put = __dbcl_db_put;
	dbp->remove = __dbcl_db_remove;
	dbp->rename = __dbcl_db_rename;
	dbp->set_alloc =
	    (int (*)(DB *, void *(*)(size_t), void *(*)(void *, size_t), void (*)(void *)))
	    __dbcl_dbp_illegal;
	dbp->set_append_recno =
	    (int (*)(DB *, int (*)(DB *, DBT *, db_recno_t)))
	    __dbcl_dbp_illegal;
	dbp->set_bt_compare =
	    (int (*)(DB *, int (*)(DB *, const DBT *, const DBT *)))
	    __dbcl_dbp_illegal;
	dbp->set_bt_minkey = __dbcl_db_set_bt_minkey;
	dbp->set_bt_prefix =
	    (int (*)(DB *, size_t(*)(DB *, const DBT *, const DBT *)))
	    __dbcl_dbp_illegal;
	dbp->set_cachesize =
	    (int (*)(DB *, u_int32_t, u_int32_t, int))
	    __dbcl_dbp_illegal;
	dbp->set_dup_compare =
	    (int (*)(DB *, int (*)(DB *, const DBT *, const DBT *)))
	    __dbcl_dbp_illegal;
	dbp->set_encrypt = __dbcl_db_set_encrypt;
	dbp->set_feedback =
	    (int (*)(DB *, void (*)(DB *, int, int)))
	    __dbcl_dbp_illegal;
	dbp->set_flags = __dbcl_db_set_flags;
	dbp->set_h_compare =
	    (int (*)(DB *, int (*)(DB *, const DBT *, const DBT *)))
	    __dbcl_dbp_illegal;
	dbp->set_h_ffactor = __dbcl_db_set_h_ffactor;
	dbp->set_h_hash =
	    (int (*)(DB *, u_int32_t(*)(DB *, const void *, u_int32_t)))
	    __dbcl_dbp_illegal;
	dbp->set_h_nelem = __dbcl_db_set_h_nelem;
	dbp->set_lorder = __dbcl_db_set_lorder;
	dbp->set_pagesize = __dbcl_db_set_pagesize;
	dbp->set_paniccall =
	    (int (*)(DB *, void (*)(DB_ENV *, int)))
	    __dbcl_dbp_illegal;
	dbp->set_priority = __dbcl_db_set_priority;
	dbp->set_q_extentsize = __dbcl_db_set_q_extentsize;
	dbp->set_re_delim = __dbcl_db_set_re_delim;
	dbp->set_re_len = __dbcl_db_set_re_len;
	dbp->set_re_pad = __dbcl_db_set_re_pad;
	dbp->set_re_source =
	    (int (*)(DB *, const char *))
	    __dbcl_dbp_illegal;
	dbp->stat = __dbcl_db_stat;
	dbp->stat_print =
	    (int (*)(DB *, u_int32_t))
	    __dbcl_dbp_illegal;
	dbp->sync = __dbcl_db_sync;
	dbp->truncate = __dbcl_db_truncate;
	dbp->upgrade =
	    (int (*)(DB *, const char *, u_int32_t))
	    __dbcl_dbp_illegal;
	dbp->verify =
	    (int (*)(DB *, const char *, const char *, FILE *, u_int32_t))
	    __dbcl_dbp_illegal;
	return;
}

/*
 * __dbcl_dbc_init --
 *	Initialize DBC handle methods.
 *
 * PUBLIC: void __dbcl_dbc_init __P((DBC *));
 */
void
__dbcl_dbc_init(dbc)
	DBC *dbc;
{
	dbc->close = dbc->c_close = __dbcl_dbc_close;
	dbc->count = dbc->c_count = __dbcl_dbc_count;
	dbc->del = dbc->c_del = __dbcl_dbc_del;
	dbc->dup = dbc->c_dup = __dbcl_dbc_dup;
	dbc->get = dbc->c_get = __dbcl_dbc_get;
	dbc->get_priority = __dbcl_dbc_get_priority;
	dbc->pget = dbc->c_pget = __dbcl_dbc_pget;
	dbc->put = dbc->c_put = __dbcl_dbc_put;
	dbc->set_priority = __dbcl_dbc_set_priority;
	return;
}

/*
 * __dbcl_dbenv_init --
 *	Initialize DB_ENV handle methods.
 *
 * PUBLIC: void __dbcl_dbenv_init __P((DB_ENV *));
 */
void
__dbcl_dbenv_init(dbenv)
	DB_ENV *dbenv;
{
	dbenv->cdsgroup_begin = __dbcl_env_cdsgroup_begin;
	dbenv->close = __dbcl_env_close;
	dbenv->dbremove = __dbcl_env_dbremove;
	dbenv->dbrename = __dbcl_env_dbrename;
	dbenv->failchk =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->fileid_reset =
	    (int (*)(DB_ENV *, const char *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->get_cachesize = __dbcl_env_get_cachesize;
	dbenv->get_cache_max =
	    (int (*)(DB_ENV *, u_int32_t *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_data_dirs =
	    (int (*)(DB_ENV *, const char ***))
	    __dbcl_dbenv_illegal;
	dbenv->get_encrypt_flags = __dbcl_env_get_encrypt_flags;
	dbenv->get_flags = __dbcl_env_get_flags;
	dbenv->get_home = __dbcl_env_get_home;
	dbenv->get_lg_bsize =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lg_dir =
	    (int (*)(DB_ENV *, const char **))
	    __dbcl_dbenv_illegal;
	dbenv->get_lg_filemode =
	    (int (*)(DB_ENV *, int *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lg_max =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lg_regionmax =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lk_conflicts =
	    (int (*)(DB_ENV *, const u_int8_t **, int *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lk_detect =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lk_max_lockers =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lk_max_locks =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_lk_max_objects =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_mp_max_openfd =
	    (int (*)(DB_ENV *, int *))
	    __dbcl_dbenv_illegal;
	dbenv->get_mp_max_write =
	    (int (*)(DB_ENV *, int *, db_timeout_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_mp_mmapsize =
	    (int (*)(DB_ENV *, size_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_open_flags = __dbcl_env_get_open_flags;
	dbenv->get_shm_key =
	    (int (*)(DB_ENV *, long *))
	    __dbcl_dbenv_illegal;
	dbenv->get_thread_count =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_timeout =
	    (int (*)(DB_ENV *, db_timeout_t *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->get_tmp_dir =
	    (int (*)(DB_ENV *, const char **))
	    __dbcl_dbenv_illegal;
	dbenv->get_tx_max =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_tx_timestamp =
	    (int (*)(DB_ENV *, time_t *))
	    __dbcl_dbenv_illegal;
	dbenv->get_verbose =
	    (int (*)(DB_ENV *, u_int32_t, int *))
	    __dbcl_dbenv_illegal;
	dbenv->lock_detect =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t, int *))
	    __dbcl_dbenv_illegal;
	dbenv->lock_get =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t, const DBT *, db_lockmode_t, DB_LOCK *))
	    __dbcl_dbenv_illegal;
	dbenv->lock_id =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->lock_id_free =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->lock_put =
	    (int (*)(DB_ENV *, DB_LOCK *))
	    __dbcl_dbenv_illegal;
	dbenv->lock_stat =
	    (int (*)(DB_ENV *, DB_LOCK_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->lock_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->lock_vec =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t, DB_LOCKREQ *, int, DB_LOCKREQ **))
	    __dbcl_dbenv_illegal;
	dbenv->log_archive =
	    (int (*)(DB_ENV *, char ***, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->log_cursor =
	    (int (*)(DB_ENV *, DB_LOGC **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->log_file =
	    (int (*)(DB_ENV *, const DB_LSN *, char *, size_t))
	    __dbcl_dbenv_illegal;
	dbenv->log_flush =
	    (int (*)(DB_ENV *, const DB_LSN *))
	    __dbcl_dbenv_illegal;
	dbenv->log_printf =
	    (int (*)(DB_ENV *, DB_TXN *, const char *, ...))
	    __dbcl_dbenv_illegal;
	dbenv->log_put =
	    (int (*)(DB_ENV *, DB_LSN *, const DBT *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->log_stat =
	    (int (*)(DB_ENV *, DB_LOG_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->log_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->lsn_reset =
	    (int (*)(DB_ENV *, const char *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->memp_fcreate =
	    (int (*)(DB_ENV *, DB_MPOOLFILE **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->memp_register =
	    (int (*)(DB_ENV *, int, int (*)(DB_ENV *, db_pgno_t, void *, DBT *), int (*)(DB_ENV *, db_pgno_t, void *, DBT *)))
	    __dbcl_dbenv_illegal;
	dbenv->memp_stat =
	    (int (*)(DB_ENV *, DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->memp_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->memp_sync =
	    (int (*)(DB_ENV *, DB_LSN *))
	    __dbcl_dbenv_illegal;
	dbenv->memp_trickle =
	    (int (*)(DB_ENV *, int, int *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_alloc =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_free =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_get_align =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_get_increment =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_get_max =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_get_tas_spins =
	    (int (*)(DB_ENV *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_lock =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_set_align =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_set_increment =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_set_max =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_set_tas_spins =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_stat =
	    (int (*)(DB_ENV *, DB_MUTEX_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->mutex_unlock =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->open = __dbcl_env_open;
	dbenv->remove = __dbcl_env_remove;
	dbenv->rep_elect =
	    (int (*)(DB_ENV *, int, int, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_flush =
	    (int (*)(DB_ENV *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_get_config =
	    (int (*)(DB_ENV *, u_int32_t, int *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_get_limit =
	    (int (*)(DB_ENV *, u_int32_t *, u_int32_t *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_get_nsites =
	    (int (*)(DB_ENV *, int *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_get_priority =
	    (int (*)(DB_ENV *, int *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_get_timeout =
	    (int (*)(DB_ENV *, int, db_timeout_t *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_process_message =
	    (int (*)(DB_ENV *, DBT *, DBT *, int, DB_LSN *))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_config =
	    (int (*)(DB_ENV *, u_int32_t, int))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_lease =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_limit =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_nsites =
	    (int (*)(DB_ENV *, int))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_priority =
	    (int (*)(DB_ENV *, int))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_timeout =
	    (int (*)(DB_ENV *, int, db_timeout_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_set_transport =
	    (int (*)(DB_ENV *, int, int (*)(DB_ENV *, const DBT *, const DBT *, const DB_LSN *, int, u_int32_t)))
	    __dbcl_dbenv_illegal;
	dbenv->rep_start =
	    (int (*)(DB_ENV *, DBT *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_stat =
	    (int (*)(DB_ENV *, DB_REP_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->rep_sync =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_add_remote_site =
	    (int (*)(DB_ENV *, const char *, u_int, int *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_get_ack_policy =
	    (int (*)(DB_ENV *, int *))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_set_ack_policy =
	    (int (*)(DB_ENV *, int))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_set_local_site =
	    (int (*)(DB_ENV *, const char *, u_int, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_site_list =
	    (int (*)(DB_ENV *, u_int *, DB_REPMGR_SITE **))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_start =
	    (int (*)(DB_ENV *, int, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_stat =
	    (int (*)(DB_ENV *, DB_REPMGR_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->repmgr_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_alloc =
	    (int (*)(DB_ENV *, void *(*)(size_t), void *(*)(void *, size_t), void (*)(void *)))
	    __dbcl_dbenv_illegal;
	dbenv->set_app_dispatch =
	    (int (*)(DB_ENV *, int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)))
	    __dbcl_dbenv_illegal;
	dbenv->set_cachesize = __dbcl_env_set_cachesize;
	dbenv->set_cache_max =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_data_dir =
	    (int (*)(DB_ENV *, const char *))
	    __dbcl_dbenv_illegal;
	dbenv->set_encrypt = __dbcl_env_set_encrypt;
	dbenv->set_event_notify =
	    (int (*)(DB_ENV *, void (*)(DB_ENV *, u_int32_t, void *)))
	    __dbcl_dbenv_illegal;
	dbenv->set_feedback =
	    (int (*)(DB_ENV *, void (*)(DB_ENV *, int, int)))
	    __dbcl_dbenv_illegal;
	dbenv->set_flags = __dbcl_env_set_flags;
	dbenv->set_intermediate_dir =
	    (int (*)(DB_ENV *, int, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_isalive =
	    (int (*)(DB_ENV *, int (*)(DB_ENV *, pid_t, db_threadid_t, u_int32_t)))
	    __dbcl_dbenv_illegal;
	dbenv->set_lg_bsize =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lg_dir =
	    (int (*)(DB_ENV *, const char *))
	    __dbcl_dbenv_illegal;
	dbenv->set_lg_filemode =
	    (int (*)(DB_ENV *, int))
	    __dbcl_dbenv_illegal;
	dbenv->set_lg_max =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lg_regionmax =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lk_conflicts =
	    (int (*)(DB_ENV *, u_int8_t *, int))
	    __dbcl_dbenv_illegal;
	dbenv->set_lk_detect =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lk_max_lockers =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lk_max_locks =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_lk_max_objects =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_mp_max_openfd =
	    (int (*)(DB_ENV *, int))
	    __dbcl_dbenv_illegal;
	dbenv->set_mp_max_write =
	    (int (*)(DB_ENV *, int, db_timeout_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_mp_mmapsize =
	    (int (*)(DB_ENV *, size_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_paniccall =
	    (int (*)(DB_ENV *, void (*)(DB_ENV *, int)))
	    __dbcl_dbenv_illegal;
	dbenv->set_rep_request =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_rpc_server = __dbcl_env_set_rpc_server;
	dbenv->set_shm_key =
	    (int (*)(DB_ENV *, long))
	    __dbcl_dbenv_illegal;
	dbenv->set_thread_count =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_thread_id =
	    (int (*)(DB_ENV *, void (*)(DB_ENV *, pid_t *, db_threadid_t*)))
	    __dbcl_dbenv_illegal;
	dbenv->set_thread_id_string =
	    (int (*)(DB_ENV *, char *(*)(DB_ENV *, pid_t, db_threadid_t, char *)))
	    __dbcl_dbenv_illegal;
	dbenv->set_timeout =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_tmp_dir =
	    (int (*)(DB_ENV *, const char *))
	    __dbcl_dbenv_illegal;
	dbenv->set_tx_max =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->set_tx_timestamp =
	    (int (*)(DB_ENV *, time_t *))
	    __dbcl_dbenv_illegal;
	dbenv->set_verbose =
	    (int (*)(DB_ENV *, u_int32_t, int))
	    __dbcl_dbenv_illegal;
	dbenv->stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->txn_begin = __dbcl_env_txn_begin;
	dbenv->txn_checkpoint =
	    (int (*)(DB_ENV *, u_int32_t, u_int32_t, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->txn_recover = __dbcl_env_txn_recover;
	dbenv->txn_stat =
	    (int (*)(DB_ENV *, DB_TXN_STAT **, u_int32_t))
	    __dbcl_dbenv_illegal;
	dbenv->txn_stat_print =
	    (int (*)(DB_ENV *, u_int32_t))
	    __dbcl_dbenv_illegal;
	return;
}

/*
 * __dbcl_txn_init --
 *	Initialize DB_TXN handle methods.
 *
 * PUBLIC: void __dbcl_txn_init __P((DB_TXN *));
 */
void
__dbcl_txn_init(txn)
	DB_TXN *txn;
{
	txn->abort = __dbcl_txn_abort;
	txn->commit = __dbcl_txn_commit;
	txn->discard = __dbcl_txn_discard;
	txn->get_name =
	    (int (*)(DB_TXN *, const char **))
	    __dbcl_txn_illegal;
	txn->prepare = __dbcl_txn_prepare;
	txn->set_name =
	    (int (*)(DB_TXN *, const char *))
	    __dbcl_txn_illegal;
	return;
}

