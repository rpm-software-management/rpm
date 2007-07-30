/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rw.c,v 12.19 2007/05/17 15:15:49 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_io --
 *	Do an I/O.
 */
int
__os_io(dbenv, op, fhp, pgno, pgsize, relative, io_len, buf, niop)
	DB_ENV *dbenv;
	int op;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pgsize, relative, io_len;
	u_int8_t *buf;
	size_t *niop;
{
	int ret;

#ifndef DB_WINCE
	if (__os_is_winnt()) {
		ULONG64 off;
		OVERLAPPED over;
		DWORD nbytes;
		if ((off = relative) == 0)
			off = (ULONG64)pgsize * pgno;
		over.Offset = (DWORD)(off & 0xffffffff);
		over.OffsetHigh = (DWORD)(off >> 32);
		over.hEvent = 0; /* we don't want asynchronous notifications */

		if (dbenv != NULL &&
		    FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
			__db_msg(dbenv,
			    "fileops: %s %s: %lu bytes at offset %lu",
			    op == DB_IO_READ ? "read" : "write",
			    fhp->name, (u_long)io_len, (u_long)off);

		switch (op) {
		case DB_IO_READ:
			if (!ReadFile(fhp->handle,
			    buf, (DWORD)io_len, &nbytes, &over))
				goto slow;
			break;
		case DB_IO_WRITE:
#ifdef HAVE_FILESYSTEM_NOTZERO
			if (__os_fs_notzero())
				goto slow;
#endif
			if (!WriteFile(fhp->handle,
			    buf, (DWORD)io_len, &nbytes, &over))
				goto slow;
			break;
		}
		if (nbytes == io_len) {
			*niop = (size_t)nbytes;
			return (0);
		}
	}

slow:
#endif
	MUTEX_LOCK(dbenv, fhp->mtx_fh);

	if ((ret = __os_seek(dbenv, fhp, pgno, pgsize, relative)) != 0)
		goto err;

	switch (op) {
	case DB_IO_READ:
		ret = __os_read(dbenv, fhp, buf, io_len, niop);
		break;
	case DB_IO_WRITE:
		ret = __os_write(dbenv, fhp, buf, io_len, niop);
		break;
	}

err:	MUTEX_UNLOCK(dbenv, fhp->mtx_fh);

	return (ret);
}

/*
 * __os_read --
 *	Read from a file handle.
 */
int
__os_read(dbenv, fhp, addr, len, nrp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nrp;
{
	size_t offset, nr;
	DWORD count;
	int ret;
	u_int8_t *taddr;

	ret = 0;

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv,
		    "fileops: read %s: %lu bytes", fhp->name, (u_long)len);

	for (taddr = addr,
	    offset = 0; offset < len; taddr += nr, offset += nr) {
		RETRY_CHK((!ReadFile(fhp->handle,
		    taddr, (DWORD)(len - offset), &count, NULL)), ret);
		if (count == 0 || ret != 0)
			break;
		nr = (size_t)count;
	}
	*nrp = taddr - (u_int8_t *)addr;
	if (ret != 0) {
		__db_syserr(dbenv, ret, "read: 0x%lx, %lu",
		    P_TO_ULONG(taddr), (u_long)len - offset);
		ret = __os_posix_err(ret);
	}
	return (ret);
}

/*
 * __os_write --
 *	Write to a file handle.
 */
int
__os_write(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	int ret;

#ifdef HAVE_FILESYSTEM_NOTZERO
	/* Zero-fill as necessary. */
	if (__os_fs_notzero() && (ret = __os_zerofill(dbenv, fhp)) != 0)
		return (ret);
#endif
	return (__os_physwrite(dbenv, fhp, addr, len, nwp));
}

/*
 * __os_physwrite --
 *	Physical write to a file handle.
 */
int
__os_physwrite(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	size_t offset, nw;
	DWORD count;
	int ret;
	u_int8_t *taddr;

	if (dbenv != NULL && FLD_ISSET(dbenv->verbose, DB_VERB_FILEOPS_ALL))
		__db_msg(dbenv,
		    "fileops: write %s: %lu bytes", fhp->name, (u_long)len);

	/*
	 * Make a last "panic" check.  Imagine a thread of control running in
	 * Berkeley DB, going to sleep.  Another thread of control decides to
	 * run recovery because the environment is broken.  The first thing
	 * recovery does is panic the existing environment, but we only check
	 * the panic flag when crossing the public API.  If the sleeping thread
	 * wakes up and writes something, we could have two threads of control
	 * writing the log files at the same time.  So, before writing, make a
	 * last panic check.  Obviously, there's still a window, but it's very,
	 * very small.
	 */
	PANIC_CHECK(dbenv);

	ret = 0;
	for (taddr = addr,
	    offset = 0; offset < len; taddr += nw, offset += nw) {
		RETRY_CHK((!WriteFile(fhp->handle,
		    taddr, (DWORD)(len - offset), &count, NULL)), ret);
		if (ret != 0)
			break;
		nw = (size_t)count;
	}
	*nwp = len;
	if (ret != 0) {
		__db_syserr(dbenv, ret, "write: %#lx, %lu",
		    P_TO_ULONG(taddr), (u_long)len - offset);
		ret = __os_posix_err(ret);

		DB_EVENT(dbenv, DB_EVENT_WRITE_FAILED, NULL);
	}
	return (ret);
}
