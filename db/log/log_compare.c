/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: log_compare.c,v 12.8 2006/09/14 15:00:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/log.h"

/*
 * log_compare --
 *	Compare two LSN's; return 1, 0, -1 if first is >, == or < second.
 *
 * EXTERN: int log_compare __P((const DB_LSN *, const DB_LSN *));
 */
int
log_compare(lsn0, lsn1)
	const DB_LSN *lsn0, *lsn1;
{
	return (LOG_COMPARE(lsn0, lsn1));
}

/*
 * __log_check_page_lsn --
 *	Panic if the page's lsn in past the end of the current log.
 *
 * PUBLIC: int __log_check_page_lsn __P((DB_ENV *, DB *, DB_LSN *));
 */
int
__log_check_page_lsn(dbenv, dbp, lsnp)
	DB_ENV *dbenv;
	DB *dbp;
	DB_LSN *lsnp;
{
	LOG *lp;
	int ret;

	lp = dbenv->lg_handle->reginfo.primary;
	LOG_SYSTEM_LOCK(dbenv);

	ret = LOG_COMPARE(lsnp, &lp->lsn);

	LOG_SYSTEM_UNLOCK(dbenv);

	if (ret >=0) {
		__db_errx(dbenv,
			"file %s has LSN %lu/%lu, past end of log at %lu/%lu",
		    dbp == NULL || dbp->fname == NULL ? "unknown" : dbp->fname,
		    (u_long)lsnp->file,
		    (u_long)lsnp->offset,
		    (u_long)lp->lsn.file,
		    (u_long)lp->lsn.offset);
		__db_errx(dbenv, "%s",
     "Commonly caused by moving a database from one transactional database");
		__db_errx(dbenv, "%s",
     "environment to another without clearing the database LSNs, or removing");
		__db_errx(dbenv, "%s",
		     "all of the log files from a database environment");
		return (EINVAL);
	}
	return (0);
}
