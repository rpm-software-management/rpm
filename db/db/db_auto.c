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
 * PUBLIC: int __db_addrem_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, u_int32_t, u_int32_t,
 * PUBLIC:     const DBT *, const DBT *, DB_LSN *));
 */
int
__db_addrem_log(dbp, txnp, ret_lsnp, flags,
    opcode, pgno, indx, nbytes, hdr,
    dbt, pagelsn)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	db_pgno_t pgno;
	u_int32_t indx;
	u_int32_t nbytes;
	const DBT *hdr;
	const DBT *dbt;
	DB_LSN * pagelsn;
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

	rectype = DB___db_addrem;
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
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (hdr == NULL ? 0 : hdr->size)
	    + sizeof(u_int32_t) + (dbt == NULL ? 0 : dbt->size)
	    + sizeof(*pagelsn);
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

	uinttmp = (u_int32_t)opcode;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)indx;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)nbytes;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (hdr == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &hdr->size, sizeof(hdr->size));
		bp += sizeof(hdr->size);
		memcpy(bp, hdr->data, hdr->size);
		bp += hdr->size;
	}

	if (dbt == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &dbt->size, sizeof(dbt->size));
		bp += sizeof(dbt->size);
		memcpy(bp, dbt->data, dbt->size);
		bp += dbt->size;
	}

	if (pagelsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(pagelsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, pagelsn) != 0))
				return (ret);
		}
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	} else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);

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
		(void)__db_addrem_print(dbenv,
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
 * PUBLIC: int __db_addrem_read __P((DB_ENV *, void *, __db_addrem_args **));
 */
int
__db_addrem_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_addrem_args **argpp;
{
	__db_addrem_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_addrem_args) + sizeof(DB_TXN), &argp)) != 0)
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
	argp->opcode = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->indx = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->nbytes = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->hdr, 0, sizeof(argp->hdr));
	memcpy(&argp->hdr.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->hdr.data = bp;
	bp += argp->hdr.size;

	memset(&argp->dbt, 0, sizeof(argp->dbt));
	memcpy(&argp->dbt.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->dbt.data = bp;
	bp += argp->dbt.size;

	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_big_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, db_pgno_t, db_pgno_t,
 * PUBLIC:     const DBT *, DB_LSN *, DB_LSN *, DB_LSN *));
 */
int
__db_big_log(dbp, txnp, ret_lsnp, flags,
    opcode, pgno, prev_pgno, next_pgno, dbt,
    pagelsn, prevlsn, nextlsn)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	db_pgno_t pgno;
	db_pgno_t prev_pgno;
	db_pgno_t next_pgno;
	const DBT *dbt;
	DB_LSN * pagelsn;
	DB_LSN * prevlsn;
	DB_LSN * nextlsn;
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

	rectype = DB___db_big;
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
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (dbt == NULL ? 0 : dbt->size)
	    + sizeof(*pagelsn)
	    + sizeof(*prevlsn)
	    + sizeof(*nextlsn);
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

	uinttmp = (u_int32_t)opcode;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)prev_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)next_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (dbt == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &dbt->size, sizeof(dbt->size));
		bp += sizeof(dbt->size);
		memcpy(bp, dbt->data, dbt->size);
		bp += dbt->size;
	}

	if (pagelsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(pagelsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, pagelsn) != 0))
				return (ret);
		}
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	} else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);

	if (prevlsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(prevlsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, prevlsn) != 0))
				return (ret);
		}
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	} else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);

	if (nextlsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(nextlsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, nextlsn) != 0))
				return (ret);
		}
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	} else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);

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
		(void)__db_big_print(dbenv,
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
 * PUBLIC: int __db_big_read __P((DB_ENV *, void *, __db_big_args **));
 */
int
__db_big_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_big_args **argpp;
{
	__db_big_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_big_args) + sizeof(DB_TXN), &argp)) != 0)
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
	argp->opcode = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->prev_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->dbt, 0, sizeof(argp->dbt));
	memcpy(&argp->dbt.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->dbt.data = bp;
	bp += argp->dbt.size;

	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);

	memcpy(&argp->prevlsn, bp,  sizeof(argp->prevlsn));
	bp += sizeof(argp->prevlsn);

	memcpy(&argp->nextlsn, bp,  sizeof(argp->nextlsn));
	bp += sizeof(argp->nextlsn);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_ovref_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, int32_t, DB_LSN *));
 */
int
__db_ovref_log(dbp, txnp, ret_lsnp, flags, pgno, adjust, lsn)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	int32_t adjust;
	DB_LSN * lsn;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	COMPQUIET(lr, NULL);

	rectype = DB___db_ovref;
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
	    + sizeof(u_int32_t)
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

	uinttmp = (u_int32_t)adjust;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

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
		(void)__db_ovref_print(dbenv,
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
 * PUBLIC: int __db_ovref_read __P((DB_ENV *, void *, __db_ovref_args **));
 */
int
__db_ovref_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_ovref_args **argpp;
{
	__db_ovref_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_ovref_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->adjust = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->lsn, bp,  sizeof(argp->lsn));
	bp += sizeof(argp->lsn);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_relink_42_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_relink_42_args **));
 */
int
__db_relink_42_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_relink_42_args **argpp;
{
	__db_relink_42_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_relink_42_args) + sizeof(DB_TXN), &argp)) != 0)
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
	argp->opcode = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->lsn, bp,  sizeof(argp->lsn));
	bp += sizeof(argp->lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->prev = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->lsn_prev, bp,  sizeof(argp->lsn_prev));
	bp += sizeof(argp->lsn_prev);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->lsn_next, bp,  sizeof(argp->lsn_next));
	bp += sizeof(argp->lsn_next);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_debug_log __P((DB_ENV *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, const DBT *, int32_t, const DBT *, const DBT *,
 * PUBLIC:     u_int32_t));
 */
int
__db_debug_log(dbenv, txnp, ret_lsnp, flags,
    op, fileid, key, data, arg_flags)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	const DBT *op;
	int32_t fileid;
	const DBT *key;
	const DBT *data;
	u_int32_t arg_flags;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	COMPQUIET(lr, NULL);

	rectype = DB___db_debug;
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
	    + sizeof(u_int32_t) + (op == NULL ? 0 : op->size)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (key == NULL ? 0 : key->size)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size)
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

	if (op == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &op->size, sizeof(op->size));
		bp += sizeof(op->size);
		memcpy(bp, op->data, op->size);
		bp += op->size;
	}

	uinttmp = (u_int32_t)fileid;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (key == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &key->size, sizeof(key->size));
		bp += sizeof(key->size);
		memcpy(bp, key->data, key->size);
		bp += key->size;
	}

	if (data == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &data->size, sizeof(data->size));
		bp += sizeof(data->size);
		memcpy(bp, data->data, data->size);
		bp += data->size;
	}

	uinttmp = (u_int32_t)arg_flags;
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
		(void)__db_debug_print(dbenv,
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
 * PUBLIC: int __db_debug_read __P((DB_ENV *, void *, __db_debug_args **));
 */
int
__db_debug_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_debug_args **argpp;
{
	__db_debug_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_debug_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memset(&argp->op, 0, sizeof(argp->op));
	memcpy(&argp->op.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->op.data = bp;
	bp += argp->op.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->fileid = (int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->key, 0, sizeof(argp->key));
	memcpy(&argp->key.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->key.data = bp;
	bp += argp->key.size;

	memset(&argp->data, 0, sizeof(argp->data));
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->arg_flags = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_noop_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *));
 */
int
__db_noop_log(dbp, txnp, ret_lsnp, flags, pgno, prevlsn)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * prevlsn;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	COMPQUIET(lr, NULL);

	rectype = DB___db_noop;
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
	    + sizeof(*prevlsn);
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

	if (prevlsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(prevlsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, prevlsn) != 0))
				return (ret);
		}
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	} else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);

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
		(void)__db_noop_print(dbenv,
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
 * PUBLIC: int __db_noop_read __P((DB_ENV *, void *, __db_noop_args **));
 */
int
__db_noop_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_noop_args **argpp;
{
	__db_noop_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_noop_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->prevlsn, bp,  sizeof(argp->prevlsn));
	bp += sizeof(argp->prevlsn);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_alloc_42_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_alloc_42_args **));
 */
int
__db_pg_alloc_42_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_alloc_42_args **argpp;
{
	__db_pg_alloc_42_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_alloc_42_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->page_lsn, bp,  sizeof(argp->page_lsn));
	bp += sizeof(argp->page_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->ptype = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_alloc_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, u_int32_t,
 * PUBLIC:     db_pgno_t, db_pgno_t));
 */
int
__db_pg_alloc_log(dbp, txnp, ret_lsnp, flags, meta_lsn, meta_pgno, page_lsn, pgno, ptype,
    next, last_pgno)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	DB_LSN * page_lsn;
	db_pgno_t pgno;
	u_int32_t ptype;
	db_pgno_t next;
	db_pgno_t last_pgno;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	COMPQUIET(lr, NULL);

	rectype = DB___db_pg_alloc;
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
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(*page_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
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

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(meta_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, meta_lsn) != 0))
				return (ret);
		}
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	} else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)meta_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (page_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(page_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, page_lsn) != 0))
				return (ret);
		}
		memcpy(bp, page_lsn, sizeof(*page_lsn));
	} else
		memset(bp, 0, sizeof(*page_lsn));
	bp += sizeof(*page_lsn);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)ptype;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)next;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)last_pgno;
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
		(void)__db_pg_alloc_print(dbenv,
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
 * PUBLIC: int __db_pg_alloc_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_alloc_args **));
 */
int
__db_pg_alloc_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_alloc_args **argpp;
{
	__db_pg_alloc_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_alloc_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->page_lsn, bp,  sizeof(argp->page_lsn));
	bp += sizeof(argp->page_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->ptype = (u_int32_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->last_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_free_42_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_free_42_args **));
 */
int
__db_pg_free_42_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_free_42_args **argpp;
{
	__db_pg_free_42_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_free_42_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_free_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *,
 * PUBLIC:     db_pgno_t, db_pgno_t));
 */
int
__db_pg_free_log(dbp, txnp, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next,
    last_pgno)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	const DBT *header;
	db_pgno_t next;
	db_pgno_t last_pgno;
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

	rectype = DB___db_pg_free;
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
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (header == NULL ? 0 : header->size)
	    + sizeof(u_int32_t)
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

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(meta_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, meta_lsn) != 0))
				return (ret);
		}
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	} else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)meta_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (header == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &header->size, sizeof(header->size));
		bp += sizeof(header->size);
		memcpy(bp, header->data, header->size);
		bp += header->size;
	}

	uinttmp = (u_int32_t)next;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)last_pgno;
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
		(void)__db_pg_free_print(dbenv,
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
 * PUBLIC: int __db_pg_free_read __P((DB_ENV *, void *, __db_pg_free_args **));
 */
int
__db_pg_free_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_free_args **argpp;
{
	__db_pg_free_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_free_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->last_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_cksum_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t));
 */
int
__db_cksum_log(dbenv, txnp, ret_lsnp, flags)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	COMPQUIET(lr, NULL);

	rectype = DB___db_cksum;
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

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN);
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
		(void)__db_cksum_print(dbenv,
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
 * PUBLIC: int __db_cksum_read __P((DB_ENV *, void *, __db_cksum_args **));
 */
int
__db_cksum_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_cksum_args **argpp;
{
	__db_cksum_args *argp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_cksum_args) + sizeof(DB_TXN), &argp)) != 0)
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

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_freedata_42_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_freedata_42_args **));
 */
int
__db_pg_freedata_42_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_freedata_42_args **argpp;
{
	__db_pg_freedata_42_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_freedata_42_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->data, 0, sizeof(argp->data));
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_freedata_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *,
 * PUBLIC:     db_pgno_t, db_pgno_t, const DBT *));
 */
int
__db_pg_freedata_log(dbp, txnp, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next,
    last_pgno, data)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	const DBT *header;
	db_pgno_t next;
	db_pgno_t last_pgno;
	const DBT *data;
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

	rectype = DB___db_pg_freedata;
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
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (header == NULL ? 0 : header->size)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size);
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

	if (meta_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(meta_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, meta_lsn) != 0))
				return (ret);
		}
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	} else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)meta_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (header == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &header->size, sizeof(header->size));
		bp += sizeof(header->size);
		memcpy(bp, header->data, header->size);
		bp += header->size;
	}

	uinttmp = (u_int32_t)next;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)last_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (data == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &data->size, sizeof(data->size));
		bp += sizeof(data->size);
		memcpy(bp, data->data, data->size);
		bp += data->size;
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
		(void)__db_pg_freedata_print(dbenv,
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
 * PUBLIC: int __db_pg_freedata_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_freedata_args **));
 */
int
__db_pg_freedata_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_freedata_args **argpp;
{
	__db_pg_freedata_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_freedata_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->last_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->data, 0, sizeof(argp->data));
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_prepare_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t));
 */
int
__db_pg_prepare_log(dbp, txnp, ret_lsnp, flags, pgno)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	COMPQUIET(lr, NULL);

	rectype = DB___db_pg_prepare;
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

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
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
		(void)__db_pg_prepare_print(dbenv,
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
 * PUBLIC: int __db_pg_prepare_read __P((DB_ENV *, void *,
 * PUBLIC:     __db_pg_prepare_args **));
 */
int
__db_pg_prepare_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_prepare_args **argpp;
{
	__db_pg_prepare_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_prepare_args) + sizeof(DB_TXN), &argp)) != 0)
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

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_new_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *,
 * PUBLIC:     db_pgno_t));
 */
int
__db_pg_new_log(dbp, txnp, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	const DBT *header;
	db_pgno_t next;
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

	rectype = DB___db_pg_new;
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
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (header == NULL ? 0 : header->size)
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

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(meta_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, meta_lsn) != 0))
				return (ret);
		}
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	} else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)meta_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (header == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &header->size, sizeof(header->size));
		bp += sizeof(header->size);
		memcpy(bp, header->data, header->size);
		bp += header->size;
	}

	uinttmp = (u_int32_t)next;
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
		(void)__db_pg_new_print(dbenv,
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
 * PUBLIC: int __db_pg_new_read __P((DB_ENV *, void *, __db_pg_new_args **));
 */
int
__db_pg_new_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_new_args **argpp;
{
	__db_pg_new_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_new_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->meta_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->next = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_init_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, const DBT *, const DBT *));
 */
int
__db_pg_init_log(dbp, txnp, ret_lsnp, flags, pgno, header, data)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	const DBT *header;
	const DBT *data;
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

	rectype = DB___db_pg_init;
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
	    + sizeof(u_int32_t) + (header == NULL ? 0 : header->size)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size);
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

	if (header == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &header->size, sizeof(header->size));
		bp += sizeof(header->size);
		memcpy(bp, header->data, header->size);
		bp += header->size;
	}

	if (data == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &data->size, sizeof(data->size));
		bp += sizeof(data->size);
		memcpy(bp, data->data, data->size);
		bp += data->size;
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
		(void)__db_pg_init_print(dbenv,
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
 * PUBLIC: int __db_pg_init_read __P((DB_ENV *, void *, __db_pg_init_args **));
 */
int
__db_pg_init_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_init_args **argpp;
{
	__db_pg_init_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_init_args) + sizeof(DB_TXN), &argp)) != 0)
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

	memset(&argp->header, 0, sizeof(argp->header));
	memcpy(&argp->header.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->header.data = bp;
	bp += argp->header.size;

	memset(&argp->data, 0, sizeof(argp->data));
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_sort_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t,
 * PUBLIC:     const DBT *));
 */
int
__db_pg_sort_log(dbp, txnp, ret_lsnp, flags, meta, meta_lsn, last_free, last_lsn, last_pgno,
    list)
	DB *dbp;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t meta;
	DB_LSN * meta_lsn;
	db_pgno_t last_free;
	DB_LSN * last_lsn;
	db_pgno_t last_pgno;
	const DBT *list;
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

	rectype = DB___db_pg_sort;
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
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(*last_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (list == NULL ? 0 : list->size);
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

	uinttmp = (u_int32_t)meta;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(meta_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, meta_lsn) != 0))
				return (ret);
		}
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	} else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)last_free;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (last_lsn != NULL) {
		if (txnp != NULL) {
			LOG *lp = dbenv->lg_handle->reginfo.primary;
			if (LOG_COMPARE(last_lsn, &lp->lsn) >= 0 && (ret =
			    __log_check_page_lsn(dbenv, dbp, last_lsn) != 0))
				return (ret);
		}
		memcpy(bp, last_lsn, sizeof(*last_lsn));
	} else
		memset(bp, 0, sizeof(*last_lsn));
	bp += sizeof(*last_lsn);

	uinttmp = (u_int32_t)last_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (list == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &list->size, sizeof(list->size));
		bp += sizeof(list->size);
		memcpy(bp, list->data, list->size);
		bp += list->size;
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
		(void)__db_pg_sort_print(dbenv,
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
 * PUBLIC: int __db_pg_sort_read __P((DB_ENV *, void *, __db_pg_sort_args **));
 */
int
__db_pg_sort_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_pg_sort_args **argpp;
{
	__db_pg_sort_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_pg_sort_args) + sizeof(DB_TXN), &argp)) != 0)
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
	argp->meta = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->last_free = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memcpy(&argp->last_lsn, bp,  sizeof(argp->last_lsn));
	bp += sizeof(argp->last_lsn);

	memcpy(&uinttmp, bp, sizeof(uinttmp));
	argp->last_pgno = (db_pgno_t)uinttmp;
	bp += sizeof(uinttmp);

	memset(&argp->list, 0, sizeof(argp->list));
	memcpy(&argp->list.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->list.data = bp;
	bp += argp->list.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_init_recover __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
__db_init_recover(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_addrem_recover, DB___db_addrem)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_big_recover, DB___db_big)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_ovref_recover, DB___db_ovref)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_debug_recover, DB___db_debug)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_noop_recover, DB___db_noop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_alloc_recover, DB___db_pg_alloc)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_free_recover, DB___db_pg_free)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_cksum_recover, DB___db_cksum)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_freedata_recover, DB___db_pg_freedata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_prepare_recover, DB___db_pg_prepare)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_new_recover, DB___db_pg_new)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_init_recover, DB___db_pg_init)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_sort_recover, DB___db_pg_sort)) != 0)
		return (ret);
	return (0);
}
