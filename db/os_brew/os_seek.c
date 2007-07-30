/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_seek.c,v 1.4 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_seek --
 *	Seek to a page/byte offset in the file.
 */
int
__os_seek(dbenv, fhp, pgno, pgsize, relative)
	DB_ENV *dbenv;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pgsize;
	u_int32_t relative;
{
	off_t offset;
	int ret;

	offset = (off_t)pgsize * pgno + relative;

	/*
	 * Use BREW's lseek function IFILE_Seek.  If the seek fails, the source
	 * returns EBADSEEKPOS.
	 */
	ret = IFILE_Seek(fhp->ifp, _SEEK_START, offset);

	if (ret == SUCCESS) {
		fhp->pgsize = pgsize;
		fhp->pgno = pgno;
		fhp->offset = relative;
		ret = 0;
	} else {
		__db_syserr(dbenv, ret,
		    "seek: %lu: (%lu * %lu) + %lu", (u_long)offset,
		    (u_long)pgno, (u_long)pgsize, (u_long)relative);
		ret = __os_posix_err(ret);
	}

	return (ret);
}
