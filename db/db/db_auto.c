/* Do not edit: automatically built by gen_rec.awk. */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <string.h>
#endif

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
__db_addrem_log(dbp, txnid, ret_lsnp, flags,
    opcode, pgno, indx, nbytes, hdr,
    dbt, pagelsn)
	DB *dbp;
	DB_TXN *txnid;
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
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_addrem;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

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

	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_addrem_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_addrem_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_addrem_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_addrem_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_addrem_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_addrem_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_addrem_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_addrem%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\topcode: %lu\n", (u_long)argp->opcode);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tindx: %lu\n", (u_long)argp->indx);
	(void)printf("\tnbytes: %lu\n", (u_long)argp->nbytes);
	(void)printf("\thdr: ");
	for (i = 0; i < argp->hdr.size; i++) {
		ch = ((u_int8_t *)argp->hdr.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tdbt: ");
	for (i = 0; i < argp->dbt.size; i++) {
		ch = ((u_int8_t *)argp->dbt.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
__db_big_log(dbp, txnid, ret_lsnp, flags,
    opcode, pgno, prev_pgno, next_pgno, dbt,
    pagelsn, prevlsn, nextlsn)
	DB *dbp;
	DB_TXN *txnid;
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
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_big;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

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

	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);

	if (prevlsn != NULL)
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);

	if (nextlsn != NULL)
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_big_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_big_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_big_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_big_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_big_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_big_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_big_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_big%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\topcode: %lu\n", (u_long)argp->opcode);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tprev_pgno: %lu\n", (u_long)argp->prev_pgno);
	(void)printf("\tnext_pgno: %lu\n", (u_long)argp->next_pgno);
	(void)printf("\tdbt: ");
	for (i = 0; i < argp->dbt.size; i++) {
		ch = ((u_int8_t *)argp->dbt.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	(void)printf("\tprevlsn: [%lu][%lu]\n",
	    (u_long)argp->prevlsn.file, (u_long)argp->prevlsn.offset);
	(void)printf("\tnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nextlsn.file, (u_long)argp->nextlsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
__db_ovref_log(dbp, txnid, ret_lsnp, flags, pgno, adjust, lsn)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	int32_t adjust;
	DB_LSN * lsn;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_ovref;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)adjust;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (lsn != NULL)
		memcpy(bp, lsn, sizeof(*lsn));
	else
		memset(bp, 0, sizeof(*lsn));
	bp += sizeof(*lsn);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_ovref_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_ovref_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_ovref_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_ovref_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_ovref_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_ovref_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_ovref_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_ovref%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tadjust: %ld\n", (long)argp->adjust);
	(void)printf("\tlsn: [%lu][%lu]\n",
	    (u_long)argp->lsn.file, (u_long)argp->lsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_relink_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *,
 * PUBLIC:     db_pgno_t, DB_LSN *));
 */
int
__db_relink_log(dbp, txnid, ret_lsnp, flags,
    opcode, pgno, lsn, prev, lsn_prev,
    next, lsn_next)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	db_pgno_t pgno;
	DB_LSN * lsn;
	db_pgno_t prev;
	DB_LSN * lsn_prev;
	db_pgno_t next;
	DB_LSN * lsn_next;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_relink;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(*lsn)
	    + sizeof(u_int32_t)
	    + sizeof(*lsn_prev)
	    + sizeof(u_int32_t)
	    + sizeof(*lsn_next);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (lsn != NULL)
		memcpy(bp, lsn, sizeof(*lsn));
	else
		memset(bp, 0, sizeof(*lsn));
	bp += sizeof(*lsn);

	uinttmp = (u_int32_t)prev;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (lsn_prev != NULL)
		memcpy(bp, lsn_prev, sizeof(*lsn_prev));
	else
		memset(bp, 0, sizeof(*lsn_prev));
	bp += sizeof(*lsn_prev);

	uinttmp = (u_int32_t)next;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (lsn_next != NULL)
		memcpy(bp, lsn_next, sizeof(*lsn_next));
	else
		memset(bp, 0, sizeof(*lsn_next));
	bp += sizeof(*lsn_next);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_relink_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_relink_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_relink_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_relink_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_relink_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_relink_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_relink_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_relink%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\topcode: %lu\n", (u_long)argp->opcode);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tlsn: [%lu][%lu]\n",
	    (u_long)argp->lsn.file, (u_long)argp->lsn.offset);
	(void)printf("\tprev: %lu\n", (u_long)argp->prev);
	(void)printf("\tlsn_prev: [%lu][%lu]\n",
	    (u_long)argp->lsn_prev.file, (u_long)argp->lsn_prev.offset);
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\tlsn_next: [%lu][%lu]\n",
	    (u_long)argp->lsn_next.file, (u_long)argp->lsn_next.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
}

/*
 * PUBLIC: int __db_relink_read __P((DB_ENV *, void *, __db_relink_args **));
 */
int
__db_relink_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__db_relink_args **argpp;
{
	__db_relink_args *argp;
	u_int32_t uinttmp;
	u_int8_t *bp;
	int ret;

	if ((ret = __os_malloc(dbenv,
	    sizeof(__db_relink_args) + sizeof(DB_TXN), &argp)) != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
__db_debug_log(dbenv, txnid, ret_lsnp, flags,
    op, fileid, key, data, arg_flags)
	DB_ENV *dbenv;
	DB_TXN *txnid;
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
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	rectype = DB___db_debug;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE)) {
		if (txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_debug_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_debug_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_debug_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_debug_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_debug_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_debug_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_debug_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_debug%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\top: ");
	for (i = 0; i < argp->op.size; i++) {
		ch = ((u_int8_t *)argp->op.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tkey: ");
	for (i = 0; i < argp->key.size; i++) {
		ch = ((u_int8_t *)argp->key.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tdata: ");
	for (i = 0; i < argp->data.size; i++) {
		ch = ((u_int8_t *)argp->data.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\targ_flags: %lu\n", (u_long)argp->arg_flags);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
__db_noop_log(dbp, txnid, ret_lsnp, flags, pgno, prevlsn)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * prevlsn;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_noop;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(*prevlsn);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (prevlsn != NULL)
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_noop_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_noop_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_noop_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_noop_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_noop_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_noop_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_noop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_noop%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tprevlsn: [%lu][%lu]\n",
	    (u_long)argp->prevlsn.file, (u_long)argp->prevlsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_pg_alloc_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, u_int32_t,
 * PUBLIC:     db_pgno_t));
 */
int
__db_pg_alloc_log(dbp, txnid, ret_lsnp, flags, meta_lsn, meta_pgno, page_lsn, pgno, ptype,
    next)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	DB_LSN * page_lsn;
	db_pgno_t pgno;
	u_int32_t ptype;
	db_pgno_t next;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_pg_alloc;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(*page_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL)
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);

	uinttmp = (u_int32_t)meta_pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (page_lsn != NULL)
		memcpy(bp, page_lsn, sizeof(*page_lsn));
	else
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_pg_alloc_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_pg_alloc_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_alloc_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_pg_alloc_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_alloc_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_alloc_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_pg_alloc_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_pg_alloc%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	(void)printf("\tpage_lsn: [%lu][%lu]\n",
	    (u_long)argp->page_lsn.file, (u_long)argp->page_lsn.offset);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tptype: %lu\n", (u_long)argp->ptype);
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_pg_free_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *,
 * PUBLIC:     db_pgno_t));
 */
int
__db_pg_free_log(dbp, txnid, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next)
	DB *dbp;
	DB_TXN *txnid;
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
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_pg_free;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL)
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	else
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_pg_free_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_pg_free_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_free_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_pg_free_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_free_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_free_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_pg_free_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_pg_free%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	(void)printf("\theader: ");
	for (i = 0; i < argp->header.size; i++) {
		ch = ((u_int8_t *)argp->header.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_cksum_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t));
 */
int
__db_cksum_log(dbenv, txnid, ret_lsnp, flags)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
{
	DBT logrec;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	rectype = DB___db_cksum;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE)) {
		if (txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_cksum_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_cksum_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_cksum_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_cksum_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_cksum_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_cksum_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_cksum_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_cksum%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __db_pg_freedata_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *,
 * PUBLIC:     db_pgno_t, const DBT *));
 */
int
__db_pg_freedata_log(dbp, txnid, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next,
    data)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
	DB_LSN * meta_lsn;
	db_pgno_t meta_pgno;
	const DBT *header;
	db_pgno_t next;
	const DBT *data;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_pg_freedata;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t)
	    + sizeof(*meta_lsn)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (header == NULL ? 0 : header->size)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL)
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	else
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_pg_freedata_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_pg_freedata_getpgnos __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__db_pg_freedata_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_pg_freedata_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_freedata_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_freedata_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_pg_freedata_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_pg_freedata%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	(void)printf("\theader: ");
	for (i = 0; i < argp->header.size; i++) {
		ch = ((u_int8_t *)argp->header.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\tdata: ");
	for (i = 0; i < argp->data.size; i++) {
		ch = ((u_int8_t *)argp->data.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_pg_prepare_log __P((DB *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, db_pgno_t));
 */
int
__db_pg_prepare_log(dbp, txnid, ret_lsnp, flags, pgno)
	DB *dbp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	db_pgno_t pgno;
{
	DBT logrec;
	DB_ENV *dbenv;
	DB_TXNLOGREC *lr;
	DB_LSN *lsnp, null_lsn;
	u_int32_t uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_pg_prepare;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t)
	    + sizeof(u_int32_t);
	if (CRYPTO_ON(dbenv)) {
		npad =
		    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);
		logrec.size += npad;
	}

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_pg_prepare_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_pg_prepare_getpgnos __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__db_pg_prepare_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_pg_prepare_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_prepare_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_prepare_args *argp;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_pg_prepare_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_pg_prepare%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
__db_pg_new_log(dbp, txnid, ret_lsnp, flags, pgno, meta_lsn, meta_pgno, header, next)
	DB *dbp;
	DB_TXN *txnid;
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
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero, uinttmp, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int is_durable, ret;

	dbenv = dbp->dbenv;
	rectype = DB___db_pg_new;
	npad = 0;

	is_durable = 1;
	if (LF_ISSET(DB_LOG_NOT_DURABLE) ||
	    F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) ||
	    F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		if (F_ISSET(dbenv, DB_ENV_TXN_NOT_DURABLE) && txnid == NULL)
			return (0);
		is_durable = 0;
	}
	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		if (TAILQ_FIRST(&txnid->kids) != NULL &&
		    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
			return (ret);
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

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

	if (!is_durable && txnid != NULL) {
		if ((ret = __os_malloc(dbenv,
		    logrec.size + sizeof(DB_TXNLOGREC), &lr)) != 0)
			return (ret);
#ifdef DIAGNOSTIC
		goto do_malloc;
#else
		logrec.data = &lr->data;
#endif
	} else {
#ifdef DIAGNOSTIC
do_malloc:
#endif
		if ((ret =
		    __os_malloc(dbenv, logrec.size, &logrec.data)) != 0) {
#ifdef DIAGNOSTIC
			if (!is_durable && txnid != NULL)
				(void)__os_free(dbenv, lr);
#endif
			return (ret);
		}
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

	DB_ASSERT(dbp->log_filename != NULL);
	if (dbp->log_filename->id == DB_LOGFILEID_INVALID &&
	    (ret = __dbreg_lazy_id(dbp)) != 0)
		return (ret);

	uinttmp = (u_int32_t)dbp->log_filename->id;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	uinttmp = (u_int32_t)pgno;
	memcpy(bp, &uinttmp, sizeof(uinttmp));
	bp += sizeof(uinttmp);

	if (meta_lsn != NULL)
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	else
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

	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) <= logrec.size);

#ifdef DIAGNOSTIC
	if (!is_durable && txnid != NULL) {
		 /*
		 * We set the debug bit if we are going
		 * to log non-durable transactions so
		 * they will be ignored by recovery.
		 */
		memcpy(lr->data, logrec.data, logrec.size);
		rectype |= DB_debug_FLAG;
		memcpy(logrec.data, &rectype, sizeof(rectype));
	}
#endif

	if (!is_durable && txnid != NULL) {
		ret = 0;
		STAILQ_INSERT_HEAD(&txnid->logs, lr, links);
#ifdef DIAGNOSTIC
		goto do_put;
#endif
	} else{
#ifdef DIAGNOSTIC
do_put:
#endif
		ret = __log_put(dbenv,
		    ret_lsnp, (DBT *)&logrec, flags | DB_LOG_NOCOPY);
		if (ret == 0 && txnid != NULL)
			txnid->last_lsn = *ret_lsnp;
	}

	if (!is_durable)
		LSN_NOT_LOGGED(*ret_lsnp);
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)__db_pg_new_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
#ifndef DIAGNOSTIC
	if (is_durable || txnid == NULL)
#endif
		__os_free(dbenv, logrec.data);

	return (ret);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_pg_new_getpgnos __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_new_getpgnos(dbenv, rec, lsnp, notused1, summary)
	DB_ENV *dbenv;
	DBT *rec;
	DB_LSN *lsnp;
	db_recops notused1;
	void *summary;
{
	TXN_RECS *t;
	int ret;
	COMPQUIET(rec, NULL);
	COMPQUIET(notused1, DB_TXN_ABORT);

	t = (TXN_RECS *)summary;

	if ((ret = __rep_check_alloc(dbenv, t, 1)) != 0)
		return (ret);

	t->array[t->npages].flags = LSN_PAGE_NOLOCK;
	t->array[t->npages].lsn = *lsnp;
	t->array[t->npages].fid = DB_LOGFILEID_INVALID;
	memset(&t->array[t->npages].pgdesc, 0,
	    sizeof(t->array[t->npages].pgdesc));

	t->npages++;

	return (0);
}
#endif /* HAVE_REPLICATION */

/*
 * PUBLIC: int __db_pg_new_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_new_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_new_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __db_pg_new_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]__db_pg_new%s: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	(void)printf("\theader: ");
	for (i = 0; i < argp->header.size; i++) {
		ch = ((u_int8_t *)argp->header.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\n");
	__os_free(dbenv, argp);

	return (0);
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
	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

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
 * PUBLIC: int __db_init_print __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
__db_init_print(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_addrem_print, DB___db_addrem)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_big_print, DB___db_big)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_ovref_print, DB___db_ovref)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_relink_print, DB___db_relink)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_debug_print, DB___db_debug)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_noop_print, DB___db_noop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_alloc_print, DB___db_pg_alloc)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_free_print, DB___db_pg_free)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_cksum_print, DB___db_cksum)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_freedata_print, DB___db_pg_freedata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_prepare_print, DB___db_pg_prepare)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_new_print, DB___db_pg_new)) != 0)
		return (ret);
	return (0);
}

#ifdef HAVE_REPLICATION
/*
 * PUBLIC: int __db_init_getpgnos __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
__db_init_getpgnos(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_addrem_getpgnos, DB___db_addrem)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_big_getpgnos, DB___db_big)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_ovref_getpgnos, DB___db_ovref)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_relink_getpgnos, DB___db_relink)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_debug_getpgnos, DB___db_debug)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_noop_getpgnos, DB___db_noop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_alloc_getpgnos, DB___db_pg_alloc)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_free_getpgnos, DB___db_pg_free)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_cksum_getpgnos, DB___db_cksum)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_freedata_getpgnos, DB___db_pg_freedata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_prepare_getpgnos, DB___db_pg_prepare)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_new_getpgnos, DB___db_pg_new)) != 0)
		return (ret);
	return (0);
}
#endif /* HAVE_REPLICATION */

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
	    __db_relink_recover, DB___db_relink)) != 0)
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
	return (0);
}
