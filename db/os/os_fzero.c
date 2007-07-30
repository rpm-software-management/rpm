/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_fzero.c,v 12.20 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_zerofill --
 *	Zero out bytes in the file.
 *
 *	Pages allocated by writing pages past end-of-file are not zeroed,
 *	on some systems.  Recovery could theoretically be fooled by a page
 *	showing up that contained garbage.  In order to avoid this, we
 *	have to write the pages out to disk, and flush them.  The reason
 *	for the flush is because if we don't sync, the allocation of another
 *	page subsequent to this one might reach the disk first, and if we
 *	crashed at the right moment, leave us with this page as the one
 *	allocated by writing a page past it in the file.
 *
 * PUBLIC: int __os_zerofill __P((DB_ENV *, DB_FH *));
 */
int
__os_zerofill(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
#ifdef HAVE_FILESYSTEM_NOTZERO
	off_t stat_offset, write_offset;
	size_t blen, nw;
	u_int32_t bytes, mbytes;
	int group_sync, need_free, ret;
	u_int8_t buf[8 * 1024], *bp;

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv, "fileops: zero-fill %s", fhp->name);

	/* Calculate the byte offset of the next write. */
	write_offset = (off_t)fhp->pgno * fhp->pgsize + fhp->offset;

	/* Stat the file. */
	if ((ret = __os_ioinfo(dbenv, NULL, fhp, &mbytes, &bytes, NULL)) != 0)
		return (ret);
	stat_offset = (off_t)mbytes * MEGABYTE + bytes;

	/* Check if the file is large enough. */
	if (stat_offset >= write_offset)
		return (0);

	/* Get a large buffer if we're writing lots of data. */
#undef	ZF_LARGE_WRITE
#define	ZF_LARGE_WRITE	(64 * 1024)
	if (write_offset - stat_offset > ZF_LARGE_WRITE) {
		if ((ret = __os_calloc(dbenv, 1, ZF_LARGE_WRITE, &bp)) != 0)
			    return (ret);
		blen = ZF_LARGE_WRITE;
		need_free = 1;
	} else {
		bp = buf;
		blen = sizeof(buf);
		need_free = 0;
		memset(buf, 0, sizeof(buf));
	}

	/* Seek to the current end of the file. */
	if ((ret = __os_seek(dbenv, fhp, mbytes, MEGABYTE, bytes)) != 0)
		goto err;

	/*
	 * Hash is the only access method that allocates groups of pages.  Hash
	 * uses the existence of the last page in a group to signify the entire
	 * group is OK; so, write all the pages but the last one in the group,
	 * flush them to disk, then write the last one to disk and flush it.
	 */
	for (group_sync = 0; stat_offset < write_offset; group_sync = 1) {
		if (write_offset - stat_offset <= (off_t)blen) {
			blen = (size_t)(write_offset - stat_offset);
			if (group_sync && (ret = __os_fsync(dbenv, fhp)) != 0)
				goto err;
		}
		if ((ret = __os_physwrite(dbenv, fhp, bp, blen, &nw)) != 0)
			goto err;
		stat_offset += blen;
	}
	if ((ret = __os_fsync(dbenv, fhp)) != 0)
		goto err;

	/* Seek back to where we started. */
	mbytes = (u_int32_t)(write_offset / MEGABYTE);
	bytes = (u_int32_t)(write_offset % MEGABYTE);
	ret = __os_seek(dbenv, fhp, mbytes, MEGABYTE, bytes);

err:	if (need_free)
		__os_free(dbenv, bp);
	return (ret);
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(fhp, NULL);
	return (0);
#endif /* HAVE_FILESYSTEM_NOTZERO */
}
