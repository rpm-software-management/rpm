/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_seek.c,v 12.11 2007/05/17 15:15:49 bostic Exp $
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
	/* Yes, this really is how Microsoft designed their API. */
	union {
		__int64 bigint;
		struct {
			unsigned long low;
			long high;
		};
	} offbytes;
	off_t offset;
	int ret;

	offset = (off_t)pgsize * pgno + relative;

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv,
		    "fileops: seek %s to %lu", fhp->name, (u_long)offset);

	offbytes.bigint = offset;
	ret = (SetFilePointer(fhp->handle, offbytes.low,
	    &offbytes.high, FILE_BEGIN) == (DWORD)-1) ? __os_get_syserr() : 0;

	if (ret == 0) {
		fhp->pgsize = pgsize;
		fhp->pgno = pgno;
		fhp->offset = relative;
	} else {
		__db_syserr(dbenv, ret,
		    "seek: %lu: (%lu * %lu) + %lu", (u_long)offset,
		    (u_long)pgno, (u_long)pgsize, (u_long)relative);
		ret = __os_posix_err(ret);
	}

	return (ret);
}
