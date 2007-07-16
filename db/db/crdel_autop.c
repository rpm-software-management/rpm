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
 * PUBLIC: int __crdel_metasub_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
__crdel_metasub_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__crdel_metasub_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __crdel_metasub_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__crdel_metasub%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tpgno: %lu\n", (u_long)argp->pgno);
	(void)printf("\tpage: ");
	for (i = 0; i < argp->page.size; i++) {
		ch = ((u_int8_t *)argp->page.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tlsn: [%lu][%lu]\n",
	    (u_long)argp->lsn.file, (u_long)argp->lsn.offset);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_create_print __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_create_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__crdel_inmem_create_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __crdel_inmem_create_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__crdel_inmem_create%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tfileid: %ld\n", (long)argp->fileid);
	(void)printf("\tname: ");
	for (i = 0; i < argp->name.size; i++) {
		ch = ((u_int8_t *)argp->name.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tfid: ");
	for (i = 0; i < argp->fid.size; i++) {
		ch = ((u_int8_t *)argp->fid.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tpgsize: %lu\n", (u_long)argp->pgsize);
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_rename_print __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_rename_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__crdel_inmem_rename_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __crdel_inmem_rename_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__crdel_inmem_rename%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\toldname: ");
	for (i = 0; i < argp->oldname.size; i++) {
		ch = ((u_int8_t *)argp->oldname.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tnewname: ");
	for (i = 0; i < argp->newname.size; i++) {
		ch = ((u_int8_t *)argp->newname.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tfid: ");
	for (i = 0; i < argp->fid.size; i++) {
		ch = ((u_int8_t *)argp->fid.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __crdel_inmem_remove_print __P((DB_ENV *, DBT *,
 * PUBLIC:     DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_remove_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__crdel_inmem_remove_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_PRINT;
	notused3 = NULL;

	if ((ret = __crdel_inmem_remove_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
    "[%lu][%lu]__crdel_inmem_remove%s: rec: %lu txnp %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset,
	    (argp->type & DB_debug_FLAG) ? "_debug" : "",
	    (u_long)argp->type,
	    (u_long)argp->txnp->txnid,
	    (u_long)argp->prev_lsn.file, (u_long)argp->prev_lsn.offset);
	(void)printf("\tname: ");
	for (i = 0; i < argp->name.size; i++) {
		ch = ((u_int8_t *)argp->name.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\tfid: ");
	for (i = 0; i < argp->fid.size; i++) {
		ch = ((u_int8_t *)argp->fid.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\n");
	__os_free(dbenv, argp);
	return (0);
}

/*
 * PUBLIC: int __crdel_init_print __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
__crdel_init_print(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_metasub_print, DB___crdel_metasub)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_create_print, DB___crdel_inmem_create)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_rename_print, DB___crdel_inmem_rename)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    __crdel_inmem_remove_print, DB___crdel_inmem_remove)) != 0)
		return (ret);
	return (0);
}
