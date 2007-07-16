/* Do not edit: automatically built by gen_rec.awk. */

#include "db_config.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#include "ex_apprec.h"
/*
 * PUBLIC: int ex_apprec_mkdir_log __P((DB_ENV *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, const DBT *));
 */
int
ex_apprec_mkdir_log(dbenv, txnp, ret_lsnp, flags,
    dirname)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	const DBT *dirname;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn, *rlsnp;
	u_int32_t zero, rectype, txn_num;
	u_int npad;
	u_int8_t *bp;
	int ret;

	rectype = DB_ex_apprec_mkdir;
	npad = 0;
	rlsnp = ret_lsnp;

	ret = 0;

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
	    + sizeof(u_int32_t) + (dirname == NULL ? 0 : dirname->size);
	if ((logrec.data = malloc(logrec.size)) == NULL)
		return (ENOMEM);
	bp = logrec.data;

	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	if (dirname == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &dirname->size, sizeof(dirname->size));
		bp += sizeof(dirname->size);
		memcpy(bp, dirname->data, dirname->size);
		bp += dirname->size;
	}

	if ((ret = dbenv->log_put(dbenv, rlsnp, (DBT *)&logrec,
	    flags | DB_LOG_NOCOPY)) == 0 && txnp != NULL) {
		*lsnp = *rlsnp;
		if (rlsnp != ret_lsnp)
			 *ret_lsnp = *rlsnp;
	}
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)ex_apprec_mkdir_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, DB_TXN_PRINT, NULL);
#endif

	free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int ex_apprec_mkdir_read __P((DB_ENV *, void *,
 * PUBLIC:     ex_apprec_mkdir_args **));
 */
int
ex_apprec_mkdir_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	ex_apprec_mkdir_args **argpp;
{
	ex_apprec_mkdir_args *argp;
	u_int8_t *bp;
	/* Keep the compiler quiet. */

	dbenv = NULL;
	if ((argp = malloc(sizeof(ex_apprec_mkdir_args) + sizeof(DB_TXN))) == NULL)
		return (ENOMEM);
	bp = recbuf;
	argp->txnp = (DB_TXN *)&argp[1];
	memset(argp->txnp, 0, sizeof(DB_TXN));

	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnp->txnid, bp, sizeof(argp->txnp->txnid));
	bp += sizeof(argp->txnp->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memset(&argp->dirname, 0, sizeof(argp->dirname));
	memcpy(&argp->dirname.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->dirname.data = bp;
	bp += argp->dirname.size;

	*argpp = argp;
	return (0);
}

