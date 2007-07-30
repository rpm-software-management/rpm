/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_rw.c,v 1.6 2007/05/17 15:15:47 bostic Exp $
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
	default:
		ret = EINVAL;
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
	FileInfo pInfo;
	int ret;

	ret = 0;
	if ((*nrp = (size_t)IFILE_Read(fhp->ifp, addr, len)) != len) {
		IFILE_GetInfo(fhp->ifp, &pInfo);
		if (pInfo.dwSize != 0) {
			ret = __os_get_syserr();
			__db_syserr(dbenv, ret, "IFILE_Read: %#lx, %lu",
			    P_TO_ULONG(addr), (u_long)len);
			ret = __os_posix_err(ret);
		}
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

#ifdef HAVE_FILESYSTEM_NOTZERO
	/* Zero-fill as necessary. */
	if (__os_fs_notzero()) {
		int ret;
		if ((ret = __os_zerofill(dbenv, fhp)) != 0)
			return (ret);
	}
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
	int ret;

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
	if ((*nwp = (size_t)IFILE_Write(fhp->ifp, addr, len)) != len) {
		ret = __os_get_syserr();
		__db_syserr(dbenv, ret, "IFILE_Write: %#lx, %lu",
		    P_TO_ULONG(addr), (u_long)len);
		ret = __os_posix_err(ret);
	}
	return (ret);
}
