/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004,2007 Oracle.  All rights reserved.
 *
 * $Id: os_truncate.c,v 12.15 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_truncate --
 *	Truncate the file.
 */
int
__os_truncate(dbenv, fhp, pgno, pgsize)
	DB_ENV *dbenv;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pgsize;
{
	/* Yes, this really is how Microsoft have designed their API */
	union {
		__int64 bigint;
		struct {
			unsigned long low;
			long high;
		};
	} off;
	off_t offset;
	int ret;

	ret = 0;
	offset = (off_t)pgsize * pgno;

	if (dbenv != NULL &&
	    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS | DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv,
		    "fileops: truncate %s to %lu", fhp->name, (u_long)offset);

#ifdef HAVE_FILESYSTEM_NOTZERO
	/*
	 * If the filesystem doesn't zero fill, it isn't safe to extend the
	 * file, or we end up with junk blocks.  Just return in that case.
	 */
	if (__os_fs_notzero()) {
		off_t stat_offset;
		u_int32_t mbytes, bytes;

		/* Stat the file. */
		if ((ret =
		    __os_ioinfo(dbenv, NULL, fhp, &mbytes, &bytes, NULL)) != 0)
			return (ret);
		stat_offset = (off_t)mbytes * MEGABYTE + bytes;

		if (offset > stat_offset)
			return (0);
	}
#endif

	/*
	 * Windows doesn't provide truncate directly.  Instead, it has
	 * SetEndOfFile, which truncates to the current position.  To
	 * deal with that, we open a duplicate file handle for truncating.
	 *
	 * We want to retry the truncate call, which involves a SetFilePointer
	 * and a SetEndOfFile, but there are several complications:
	 *
	 * 1) since the Windows API deals in 32-bit values, it's possible that
	 *    the return from SetFilePointer (the low 32-bits) is
	 *    INVALID_SET_FILE_POINTER even when the call has succeeded.  So we
	 *    have to also check whether GetLastError() returns NO_ERROR.
	 *
	 * 2) when it returns, SetFilePointer overwrites the high bits of the
	 *    offset, so if we need to retry, we have to reset the offset each
	 *    time.
	 *
	 * We can't switch to SetFilePointerEx, which knows about 64-bit
	 * offsets, because it isn't supported on Win9x/ME.
	 */
	RETRY_CHK((off.bigint = (__int64)pgsize * pgno,
	    (SetFilePointer(fhp->trunc_handle, off.low, &off.high, FILE_BEGIN)
	    == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) ||
	    !SetEndOfFile(fhp->trunc_handle)), ret);

	if (ret != 0) {
		__db_syserr(dbenv, ret, "SetFilePointer: %lu", pgno * pgsize);
		ret = __os_posix_err(ret);
	}

	return (ret);
}
