/* Do not edit: automatically built by gen_rec.awk. */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"
#include "dbinc/db_dispatch.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

/*
 * PUBLIC: int __crdel_metasub_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, const DBT *, DB_LSN *));
 */
int
__crdel_metasub_log(dbp, txnp, ret_lsnp, flags, pgno, page, lsn)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	const DBT *page;
	DB_LSN * lsn;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	COMPQUIET(lr, NULL);

	rectype = DB___crdel_metasub;
	npad = 0;
	rlsnp = ret_lsnp;

	ret = 0;

	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (txnp == NULL)
			return (0);
		is_durable = 0;
	} else
		is_durable = 1;

	if (txnp == NULL) {
		txn_num = 0;
		lsnp = &null_lsn;
		null_lsn.file = null_lsn.offset = 0;
	} else {
		if (TAILQ_FIRST(&txnp->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnp)) != 0)
			return (ret);
		/*
		 * We need to assign begin_lsn while holding region mutex.
		 * That assignment is done inside the DbEnv->log_put call,
		 * so pass in the appropriate memory location to be filled
		 * in by the log_put code.
		 */
		DB_SET_TXN_LSNP(txnp, &rlsnp, &lsnp);
		txn_num = txnp->txnid;
	}

	DB_ASSERT(dbenv, dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (page == NULL ? 0 : page->size)
	    + sizeof(*lsn);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (is_durable || txnp == NULL) {
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0)
			return (ret);
	} else {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
			__os_free(dbenv, lr);
			return (ret);
		}
#else
		logrec.data = lr->data;
#endif
	}
	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (page == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &page->size, sizeof(page->size));
		bp += sizeof(page->size);
		memcpy(bp, page->data, page->size);
		bp += page->size;
	}

	if (lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, lsn) != 0))
				return (ret);
		}
		memcpy(bp, lsn, sizeof(*lsn));
	} else
		memset(bp, 0, sizeof(*lsn));
	bp += sizeof(*lsn);

	DB_ASSERT(dbenv,
	    (u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

	if (is_durable || txnp == NULL) {
		if ((ret = __log_put(dbenv, rlsnp,(DBT *)&logrec,
		    flags | DB_LOG_NOCOPY)) == 0 && txnp != NULL) {
			*lsnp = *rlsnp;
			if (rlsnp != ret_lsnp)
				 *ret_lsnp = *rlsnp;
		}
	} else {
		ret = 0;
#ifdef DIAGNOSTIC
		/*
		 * Set the debug bit if we are going to log non-durable
		 * transactions so they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));

		if (!IS_REP_CLIENT(dbenv))
			ret = __log_put(dbenv,
			    rlsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
#endif
		STAILQ_INSERT_HEAD(&txnp->logs, lr, links);
		F_SET((TXN_DETAIL *)txnp->td, TXN_DTL_INMEMORY);
		LSN_NOT_LOGGED(*ret_lsnp);
	}

#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__crdel_metasub_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, DB_TXN_PRINT, NULL);
#endif

#ifdef DIAGNOSTIC
	__os_free(dbenv, logrec.data);
#else
	if (is_durable || txnp == NULL)
		__os_free(dbenv, logrec.data);
#endif
	return (ret);
}

/*
 * PUBLIC: int __crdel_metasub_read __P((DB_ENV *, void *,
 * PUBLIC:     __crdel_metasub_args **));
 */
int
__crdel_metasub_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__crdel_metasub_args **argpp;
{
	__crdel_metasub_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__crdel_metasub_args) + sizeof(DB_TXN), &argp)) != 0)
		return (ret);
	bp = recbuf;
	argp->txnp = (DB_TXN *)&argp[1];
	memset(argp->txnp, 0, sizeof(DB_TXN));

	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnp->txnid, bp, sizeof(argp->txnp->txnid));
	bp += sizeof(argp->txnp->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->page, 0, sizeof(argp->page));
	memcpy(&argp->page.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->page.data = bp;
	bp += argp->page.size;

	memcpy(&argp->lsn, bp,  sizeof(argp->lsn));
	bp += sizeof(argp->lsn);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_create_log __P((DB_ENV *, DB_TXN *,
 * PUBLIC:     DB_LSN *, u_int32_t, int32_t, const DBT *, const DBT *,
 * PUBLIC:     u_int32_t));
 */
int
__crdel_inmem_create_log(dbenv, txnp, ret_lsnp, flags,
    fileid, name, fid, pgsize)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	const DBT *name;
	const DBT *fid;
	u_int32_t pgsize;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	COMPQUIET(lr, NULL);

	rectype = DB___crdel_inmem_create;
	npad = 0;
	rlsnp = ret_lsnp;

	ret = 0;

	if (LF_ISSET(DB_LOG_NOT_DURABLE)) {
		if (txnp == NULL)
			return (0);
		if (txnp == NULL)
			return (0);
		is_durable = 0;
	} else
		is_durable = 1;

	if (txnp == NULL) {
		txn_num = 0;
		lsnp = &null_lsn;
		null_lsn.file = null_lsn.offset = 0;
	} else {
		if (TAILQ_FIRST(&txnp->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnp)) != 0)
			return (ret);
		/*
		 * We need to assign begin_lsn while holding region mutex.
		 * That assignment is done inside the DbEnv->log_put call,
		 * so pass in the appropriate memory location to be filled
		 * in by the log_put code.
		 */
		DB_SET_TXN_LSNP(txnp, &rlsnp, &lsnp);
		txn_num = txnp->txnid;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (name == NULL ? 0 : name->size)
	    + sizeof(u_int32_t) + (fid == NULL ? 0 : fid->size)
	    + sizeof(u_int32_t);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (is_durable || txnp == NULL) {
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0)
			return (ret);
	} else {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
			__os_free(dbenv, lr);
			return (ret);
		}
#else
		logrec.data = lr->data;
#endif
	}
	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	uinttmp = (u_int32_t)fileid;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (name == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &name->size, sizeof(name->size));
		bp += sizeof(name->size);
		memcpy(bp, name->data, name->size);
		bp += name->size;
	}

	if (fid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &fid->size, sizeof(fid->size));
		bp += sizeof(fid->size);
		memcpy(bp, fid->data, fid->size);
		bp += fid->size;
	}

	uinttmp = (u_int32_t)pgsize;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	DB_ASSERT(dbenv,
	    (u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

	if (is_durable || txnp == NULL) {
		if ((ret = __log_put(dbenv, rlsnp,(DBT *)&logrec,
		    flags | DB_LOG_NOCOPY)) == 0 && txnp != NULL) {
			*lsnp = *rlsnp;
			if (rlsnp != ret_lsnp)
				 *ret_lsnp = *rlsnp;
		}
	} else {
		ret = 0;
#ifdef DIAGNOSTIC
		/*
		 * Set the debug bit if we are going to log non-durable
		 * transactions so they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));

		if (!IS_REP_CLIENT(dbenv))
			ret = __log_put(dbenv,
			    rlsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
#endif
		STAILQ_INSERT_HEAD(&txnp->logs, lr, links);
		F_SET((TXN_DETAIL *)txnp->td, TXN_DTL_INMEMORY);
		LSN_NOT_LOGGED(*ret_lsnp);
	}

#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__crdel_inmem_create_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, DB_TXN_PRINT, NULL);
#endif

#ifdef DIAGNOSTIC
	__os_free(dbenv, logrec.data);
#else
	if (is_durable || txnp == NULL)
		__os_free(dbenv, logrec.data);
#endif
	return (ret);
}

/*
 * PUBLIC: int __crdel_inmem_create_read __P((DB_ENV *, void *,
 * PUBLIC:     __crdel_inmem_create_args **));
 */
int
__crdel_inmem_create_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__crdel_inmem_create_args **argpp;
{
	__crdel_inmem_create_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__crdel_inmem_create_args) + sizeof(DB_TXN), &argp)) != 0)
		return (ret);
	bp = recbuf;
	argp->txnp = (DB_TXN *)&argp[1];
	memset(argp->txnp, 0, sizeof(DB_TXN));

	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnp->txnid, bp, sizeof(argp->txnp->txnid));
	bp += sizeof(argp->txnp->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->name, 0, sizeof(argp->name));
	memcpy(&argp->name.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->name.data = bp;
	bp += argp->name.size;

	memset(&argp->fid, 0, sizeof(argp->fid));
	memcpy(&argp->fid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->fid.data = bp;
	bp += argp->fid.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgsize = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_rename_log __P((DB_ENV *, DB_TXN *,
 * PUBLIC:     DB_LSN *, u_int32_t, const DBT *, const DBT *, const DBT *));
 */
int
__crdel_inmem_rename_log(dbenv, txnp, ret_lsnp, flags,
    oldname, newname, fid)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	const DBT *oldname;
	const DBT *newname;
	const DBT *fid;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	COMPQUIET(lr, NULL);

	rectype = DB___crdel_inmem_rename;
	npad = 0;
	rlsnp = ret_lsnp;

	ret = 0;

	if (LF_ISSET(DB_LOG_NOT_DURABLE)) {
		if (txnp == NULL)
			return (0);
		if (txnp == NULL)
			return (0);
		is_durable = 0;
	} else
		is_durable = 1;

	if (txnp == NULL) {
		txn_num = 0;
		lsnp = &null_lsn;
		null_lsn.file = null_lsn.offset = 0;
	} else {
		if (TAILQ_FIRST(&txnp->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnp)) != 0)
			return (ret);
		/*
		 * We need to assign begin_lsn while holding region mutex.
		 * That assignment is done inside the DbEnv->log_put call,
		 * so pass in the appropriate memory location to be filled
		 * in by the log_put code.
		 */
		DB_SET_TXN_LSNP(txnp, &rlsnp, &lsnp);
		txn_num = txnp->txnid;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t) + (oldname == NULL ? 0 : oldname->size)
	    + sizeof(u_int32_t) + (newname == NULL ? 0 : newname->size)
	    + sizeof(u_int32_t) + (fid == NULL ? 0 : fid->size);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (is_durable || txnp == NULL) {
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0)
			return (ret);
	} else {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
			__os_free(dbenv, lr);
			return (ret);
		}
#else
		logrec.data = lr->data;
#endif
	}
	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	if (oldname == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &oldname->size, sizeof(oldname->size));
		bp += sizeof(oldname->size);
		memcpy(bp, oldname->data, oldname->size);
		bp += oldname->size;
	}

	if (newname == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &newname->size, sizeof(newname->size));
		bp += sizeof(newname->size);
		memcpy(bp, newname->data, newname->size);
		bp += newname->size;
	}

	if (fid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &fid->size, sizeof(fid->size));
		bp += sizeof(fid->size);
		memcpy(bp, fid->data, fid->size);
		bp += fid->size;
	}

	DB_ASSERT(dbenv,
	    (u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

	if (is_durable || txnp == NULL) {
		if ((ret = __log_put(dbenv, rlsnp,(DBT *)&logrec,
		    flags | DB_LOG_NOCOPY)) == 0 && txnp != NULL) {
			*lsnp = *rlsnp;
			if (rlsnp != ret_lsnp)
				 *ret_lsnp = *rlsnp;
		}
	} else {
		ret = 0;
#ifdef DIAGNOSTIC
		/*
		 * Set the debug bit if we are going to log non-durable
		 * transactions so they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));

		if (!IS_REP_CLIENT(dbenv))
			ret = __log_put(dbenv,
			    rlsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
#endif
		STAILQ_INSERT_HEAD(&txnp->logs, lr, links);
		F_SET((TXN_DETAIL *)txnp->td, TXN_DTL_INMEMORY);
		LSN_NOT_LOGGED(*ret_lsnp);
	}

#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__crdel_inmem_rename_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, DB_TXN_PRINT, NULL);
#endif

#ifdef DIAGNOSTIC
	__os_free(dbenv, logrec.data);
#else
	if (is_durable || txnp == NULL)
		__os_free(dbenv, logrec.data);
#endif
	return (ret);
}

/*
 * PUBLIC: int __crdel_inmem_rename_read __P((DB_ENV *, void *,
 * PUBLIC:     __crdel_inmem_rename_args **));
 */
int
__crdel_inmem_rename_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__crdel_inmem_rename_args **argpp;
{
	__crdel_inmem_rename_args *argp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__crdel_inmem_rename_args) + sizeof(DB_TXN), &argp)) != 0)
		return (ret);
	bp = recbuf;
	argp->txnp = (DB_TXN *)&argp[1];
	memset(argp->txnp, 0, sizeof(DB_TXN));

	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnp->txnid, bp, sizeof(argp->txnp->txnid));
	bp += sizeof(argp->txnp->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memset(&argp->oldname, 0, sizeof(argp->oldname));
	memcpy(&argp->oldname.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->oldname.data = bp;
	bp += argp->oldname.size;

	memset(&argp->newname, 0, sizeof(argp->newname));
	memcpy(&argp->newname.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->newname.data = bp;
	bp += argp->newname.size;

	memset(&argp->fid, 0, sizeof(argp->fid));
	memcpy(&argp->fid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->fid.data = bp;
	bp += argp->fid.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_remove_log __P((DB_ENV *, DB_TXN *,
 * PUBLIC:     DB_LSN *, u_int32_t, const DBT *, const DBT *));
 */
int
__crdel_inmem_remove_log(dbenv, txnp, ret_lsnp, flags,
    name, fid)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	const DBT *name;
	const DBT *fid;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	COMPQUIET(lr, NULL);

	rectype = DB___crdel_inmem_remove;
	npad = 0;
	rlsnp = ret_lsnp;

	ret = 0;

	if (LF_ISSET(DB_LOG_NOT_DURABLE)) {
		if (txnp == NULL)
			return (0);
		if (txnp == NULL)
			return (0);
		is_durable = 0;
	} else
		is_durable = 1;

	if (txnp == NULL) {
		txn_num = 0;
		lsnp = &null_lsn;
		null_lsn.file = null_lsn.offset = 0;
	} else {
		if (TAILQ_FIRST(&txnp->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnp)) != 0)
			return (ret);
		/*
		 * We need to assign begin_lsn while holding region mutex.
		 * That assignment is done inside the DbEnv->log_put call,
		 * so pass in the appropriate memory location to be filled
		 * in by the log_put code.
		 */
		DB_SET_TXN_LSNP(txnp, &rlsnp, &lsnp);
		txn_num = txnp->txnid;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t) + (name == NULL ? 0 : name->size)
	    + sizeof(u_int32_t) + (fid == NULL ? 0 : fid->size);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (is_durable || txnp == NULL) {
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0)
			return (ret);
	} else {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
			__os_free(dbenv, lr);
			return (ret);
		}
#else
		logrec.data = lr->data;
#endif
	}
	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	if (name == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &name->size, sizeof(name->size));
		bp += sizeof(name->size);
		memcpy(bp, name->data, name->size);
		bp += name->size;
	}

	if (fid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &fid->size, sizeof(fid->size));
		bp += sizeof(fid->size);
		memcpy(bp, fid->data, fid->size);
		bp += fid->size;
	}

	DB_ASSERT(dbenv,
	    (u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

	if (is_durable || txnp == NULL) {
		if ((ret = __log_put(dbenv, rlsnp,(DBT *)&logrec,
		    flags | DB_LOG_NOCOPY)) == 0 && txnp != NULL) {
			*lsnp = *rlsnp;
			if (rlsnp != ret_lsnp)
				 *ret_lsnp = *rlsnp;
		}
	} else {
		ret = 0;
#ifdef DIAGNOSTIC
		/*
		 * Set the debug bit if we are going to log non-durable
		 * transactions so they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));

		if (!IS_REP_CLIENT(dbenv))
			ret = __log_put(dbenv,
			    rlsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
#endif
		STAILQ_INSERT_HEAD(&txnp->logs, lr, links);
		F_SET((TXN_DETAIL *)txnp->td, TXN_DTL_INMEMORY);
		LSN_NOT_LOGGED(*ret_lsnp);
	}

#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__crdel_inmem_remove_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, DB_TXN_PRINT, NULL);
#endif

#ifdef DIAGNOSTIC
	__os_free(dbenv, logrec.data);
#else
	if (is_durable || txnp == NULL)
		__os_free(dbenv, logrec.data);
#endif
	return (ret);
}

/*
 * PUBLIC: int __crdel_inmem_remove_read __P((DB_ENV *, void *,
 * PUBLIC:     __crdel_inmem_remove_args **));
 */
int
__crdel_inmem_remove_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__crdel_inmem_remove_args **argpp;
{
	__crdel_inmem_remove_args *argp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__crdel_inmem_remove_args) + sizeof(DB_TXN), &argp)) != 0)
		return (ret);
	bp = recbuf;
	argp->txnp = (DB_TXN *)&argp[1];
	memset(argp->txnp, 0, sizeof(DB_TXN));

	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnp->txnid, bp, sizeof(argp->txnp->txnid));
	bp += sizeof(argp->txnp->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memset(&argp->name, 0, sizeof(argp->name));
	memcpy(&argp->name.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->name.data = bp;
	bp += argp->name.size;

	memset(&argp->fid, 0, sizeof(argp->fid));
	memcpy(&argp->fid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->fid.data = bp;
	bp += argp->fid.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __crdel_init_recover __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
__crdel_init_recover(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_metasub_recover, DB___crdel_metasub)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_create_recover, DB___crdel_inmem_create)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_rename_recover, DB___crdel_inmem_rename)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_remove_recover, DB___crdel_inmem_remove)) != 0)
		return (ret);
	return (0);
}
