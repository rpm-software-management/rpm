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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_addrem_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_addrem%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_big_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_big%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_ovref_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_ovref%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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
 * PUBLIC: int __db_relink_42_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_relink_42_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_relink_42_args *argp;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_relink_42_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_relink_42%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_debug_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_debug%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_noop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_noop%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tprevlsn: [%lu][%lu]\n",
	    (u_long)argp->prevlsn.file, (u_long)argp->prevlsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __db_pg_alloc_42_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_alloc_42_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_alloc_42_args *argp;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_alloc_42_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_alloc_42%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_alloc_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_alloc%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	(void)printf("\tpage_lsn: [%lu][%lu]\n",
	    (u_long)argp->page_lsn.file, (u_long)argp->page_lsn.offset);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tptype: %lu\n", (u_long)argp->ptype);
	(void)printf("\tnext: %lu\n", (u_long)argp->next);
	(void)printf("\tlast_pgno: %lu\n", (u_long)argp->last_pgno);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __db_pg_free_42_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_free_42_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_free_42_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_free_42_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_free_42%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_free_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_free%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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
	(void)printf("\tlast_pgno: %lu\n", (u_long)argp->last_pgno);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_cksum_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_cksum%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __db_pg_freedata_42_print __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__db_pg_freedata_42_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_freedata_42_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_freedata_42_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_freedata_42%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_freedata_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_freedata%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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
	(void)printf("\tlast_pgno: %lu\n", (u_long)argp->last_pgno);
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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_prepare_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_prepare%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

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

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_new_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_new%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
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
 * PUBLIC: int __db_pg_init_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_init_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_init_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_init_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_init%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\theader: ");
	for (i = 0; i < argp->header.size; i++) {
		ch = ((u_int8_t *)argp->header.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
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
 * PUBLIC: int __db_pg_sort_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__db_pg_sort_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__db_pg_sort_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __db_pg_sort_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__db_pg_sort%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tmeta: %lu\n", (u_long)argp->meta);
	(void)printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	(void)printf("\tlast_free: %lu\n", (u_long)argp->last_free);
	(void)printf("\tlast_lsn: [%lu][%lu]\n",
	    (u_long)argp->last_lsn.file, (u_long)argp->last_lsn.offset);
	(void)printf("\tlast_pgno: %lu\n", (u_long)argp->last_pgno);
	(void)printf("\tlist: ");
	for (i = 0; i < argp->list.size; i++) {
		ch = ((u_int8_t *)argp->list.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\n");
	__os_free(dbenv, argp);
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
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_init_print, DB___db_pg_init)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __db_pg_sort_print, DB___db_pg_sort)) != 0)
		return (ret);
	return (0);
}
