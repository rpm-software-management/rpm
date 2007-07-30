/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_qnx_open.c,v 12.23 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_qnx_region_open --
 *	Open a shared memory region file using POSIX shm_open.
 *
 * PUBLIC: #ifdef HAVE_QNX
 * PUBLIC: int __os_qnx_region_open
 * PUBLIC:     __P((DB_ENV *, const char *, int, int, DB_FH *));
 * PUBLIC: #endif
 */
int
__os_qnx_region_open(dbenv, name, oflags, mode, fhp)
	DB_ENV *dbenv;
	const char *name;
	int oflags, mode;
	DB_FH *fhp;
{
	int ret;
	char *newname;

	if ((ret = __os_qnx_shmname(dbenv, name, &newname)) != 0)
		return (ret);

	/*
	 * Once we have created the object, we don't need the name
	 * anymore.  Other callers of this will convert themselves.
	 */
	fhp->fd = shm_open(newname, oflags, mode);
	if (fhp->fd == -1)
		ret = __os_posix_err(__os_get_syserr());
	__os_free(dbenv, newname);
	if (fhp->fd == -1)
		return (ret);

	F_SET(fhp, DB_FH_OPENED);

#ifdef HAVE_FCNTL_F_SETFD
	/* Deny file descriptor access to any child process. */
	if (fcntl(fhp->fd, F_SETFD, 1) == -1) {
		ret = __os_get_syserr();
		__db_syserr(dbenv, ret, "fcntl(F_SETFD)");
		return (__os_posix_err(ret));
	}
#endif
	return (0);
}

/*
 * __os_qnx_shmname --
 *	Translate a pathname into a shm_open memory object name.
 *
 * PUBLIC: #ifdef HAVE_QNX
 * PUBLIC: int __os_qnx_shmname __P((DB_ENV *, const char *, char **));
 * PUBLIC: #endif
 */
int
__os_qnx_shmname(dbenv, name, newnamep)
	DB_ENV *dbenv;
	const char *name;
	char **newnamep;
{
	int ret;
	size_t size;
	char *p, *q, *tmpname;

	*newnamep = NULL;

	/*
	 * POSIX states that the name for a shared memory object
	 * may begin with a slash '/' and support for subsequent
	 * slashes is implementation-dependent.  The one implementation
	 * we know of right now, QNX, forbids subsequent slashes.
	 * We don't want to be parsing pathnames for '.' and '..' in
	 * the middle.  In order to allow easy conversion, just take
	 * the last component as the shared memory name.  This limits
	 * the namespace a bit, but makes our job a lot easier.
	 *
	 * We should not be modifying user memory, so we use our own.
	 * Caller is responsible for freeing the memory we give them.
	 */
	if ((ret = __os_strdup(dbenv, name, &tmpname)) != 0)
		return (ret);
	/*
	 * Skip over filename component.
	 * We set that separator to '\0' so that we can do another
	 * __db_rpath.  However, we immediately set it then to ':'
	 * so that we end up with the tailing directory:filename.
	 * We require a home directory component.  Return an error
	 * if there isn't one.
	 */
	p = __db_rpath(tmpname);
	if (p == NULL)
		return (EINVAL);
	if (p != tmpname) {
		*p = '\0';
		q = p;
		p = __db_rpath(tmpname);
		*q = ':';
	}
	if (p != NULL) {
		/*
		 * If we have a path component, copy and return it.
		 */
		ret = __os_strdup(dbenv, p, newnamep);
		__os_free(dbenv, tmpname);
		return (ret);
	}

	/*
	 * We were given just a directory name with no path components.
	 * Add a leading slash, and copy the remainder.
	 */
	size = strlen(tmpname) + 2;
	if ((ret = __os_malloc(dbenv, size, &p)) != 0)
		return (ret);
	p[0] = '/';
	memcpy(&p[1], tmpname, size-1);
	__os_free(dbenv, tmpname);
	*newnamep = p;
	return (0);
}
