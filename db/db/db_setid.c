/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: db_setid.c,v 12.26 2007/05/26 00:13:14 ubell Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_swap.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"

static int __env_fileid_reset __P((DB_ENV *, const char *, int));

/*
 * __env_fileid_reset_pp --
 *	DB_ENV->fileid_reset pre/post processing.
 *
 * PUBLIC: int __env_fileid_reset_pp __P((DB_ENV *, const char *, u_int32_t));
 */
int
__env_fileid_reset_pp(dbenv, name, flags)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags;
{
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->fileid_reset");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline, outside of
	 * the replication block.
	 */
	if (flags != 0 && flags != DB_ENCRYPT)
		return (__db_ferr(dbenv, "DB_ENV->fileid_reset", 0));

	ENV_ENTER(dbenv, ip);

	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check && (ret = __env_rep_enter(dbenv, 1)) != 0)
		goto err;

	ret = __env_fileid_reset(dbenv, name, LF_ISSET(DB_ENCRYPT) ? 1 : 0);

	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

err:	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __env_fileid_reset --
 *	Reset the file IDs for every database in the file.
 */
static int
__env_fileid_reset(dbenv, name, encrypted)
	DB_ENV *dbenv;
	const char *name;
	int encrypted;
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	DB_FH *fhp;
	DB_MPOOLFILE *mpf;
	DB_PGINFO cookie;
	db_pgno_t pgno;
	int t_ret, ret;
	size_t n;
	char *real_name;
	u_int8_t fileid[DB_FILE_ID_LEN], mbuf[DBMETASIZE];
	void *pagep;

	dbp = NULL;
	dbcp = NULL;
	fhp = NULL;
	real_name = NULL;

	/* Get the real backing file name. */
	if ((ret =
	    __db_appname(dbenv, DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		return (ret);

	/* Get a new file ID. */
	if ((ret = __os_fileid(dbenv, real_name, 1, fileid)) != 0)
		goto err;

	/*
	 * The user may have physically copied a file currently open in the
	 * cache, which means if we open this file through the cache before
	 * updating the file ID on page 0, we might connect to the file from
	 * which the copy was made.
	 */
	if ((ret = __os_open(dbenv, real_name, 0, 0, 0, &fhp)) != 0) {
		__db_err(dbenv, ret, "%s", real_name);
		goto err;
	}
	if ((ret = __os_read(dbenv, fhp, mbuf, sizeof(mbuf), &n)) != 0)
		goto err;

	if (n != sizeof(mbuf)) {
		ret = EINVAL;
		__db_errx(dbenv,
		    "%s: unexpected file type or format", real_name);
		goto err;
	}

	/*
	 * Create the DB object.
	 */
	if ((ret = __db_create_internal(&dbp, dbenv, 0)) != 0)
		goto err;

	/* If configured with a password, the databases are encrypted. */
	if (encrypted && (ret = __db_set_flags(dbp, DB_ENCRYPT)) != 0)
		goto err;

	if ((ret = __db_meta_setup(dbenv,
	    dbp, real_name, (DBMETA *)mbuf, 0, DB_CHK_META)) != 0)
		goto err;
	memcpy(((DBMETA *)mbuf)->uid, fileid, DB_FILE_ID_LEN);
	cookie.db_pagesize = sizeof(mbuf);
	cookie.flags = dbp->flags;
	cookie.type = dbp->type;
	key.data = &cookie;

	if ((ret = __db_pgout(dbenv, 0, mbuf, &key)) != 0)
		goto err;
	if ((ret = __os_seek(dbenv, fhp, 0, 0, 0)) != 0)
		goto err;
	if ((ret = __os_write(dbenv, fhp, mbuf, sizeof(mbuf), &n)) != 0)
		goto err;
	if ((ret = __os_fsync(dbenv, fhp)) != 0)
		goto err;

	/*
	 * Page 0 of the file has an updated file ID, and we can open it in
	 * the cache without connecting to a different, existing file.  Open
	 * the file in the cache, and update the file IDs for subdatabases.
	 * (No existing code, as far as I know, actually uses the file ID of
	 * a subdatabase, but it's cleaner to get them all.)
	 */

	/*
	 * Open the DB file.
	 *
	 * !!!
	 * Note DB_RDWRMASTER flag, we need to open the master database file
	 * for writing in this case.
	 */
	if ((ret = __db_open(dbp, NULL,
	    name, NULL, DB_UNKNOWN, DB_RDWRMASTER, 0, PGNO_BASE_MD)) != 0)
		goto err;

	/*
	 * If the database file doesn't support subdatabases, we only have
	 * to update a single metadata page.  Otherwise, we have to open a
	 * cursor and step through the master database, and update all of
	 * the subdatabases' metadata pages.
	 */
	if (!F_ISSET(dbp, DB_AM_SUBDB))
		goto err;

	mpf = dbp->mpf;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	if ((ret = __db_cursor(dbp, NULL, &dbcp, 0)) != 0)
		goto err;
	while ((ret = __dbc_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		/*
		 * XXX
		 * We're handling actual data, not on-page meta-data, so it
		 * hasn't been converted to/from opposite endian architectures.
		 * Do it explicitly, now.
		 */
		memcpy(&pgno, data.data, sizeof(db_pgno_t));
		DB_NTOHL(&pgno);
		if ((ret = __memp_fget(mpf, &pgno, NULL,
		    DB_MPOOL_DIRTY, &pagep)) != 0)
			goto err;
		memcpy(((DBMETA *)pagep)->uid, fileid, DB_FILE_ID_LEN);
		if ((ret = __memp_fput(mpf, pagep, dbcp->priority)) != 0)
			goto err;
	}
	if (ret == DB_NOTFOUND)
		ret = 0;

err:	if (dbcp != NULL && (t_ret = __dbc_close(dbcp)) != 0 && ret == 0)
		ret = t_ret;
	if (dbp != NULL && (t_ret = __db_close(dbp, NULL, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (fhp != NULL &&
	    (t_ret = __os_closehandle(dbenv, fhp)) != 0 && ret == 0)
		ret = t_ret;
	if (real_name != NULL)
		__os_free(dbenv, real_name);

	return (ret);
}
