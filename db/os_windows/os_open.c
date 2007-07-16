/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_open.c,v 12.16 2006/09/12 01:49:36 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_open --
 *	Open a file descriptor.
 */
__os_open(dbenv, name, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags;
	int mode;
	DB_FH **fhpp;
{
	return (__os_open_extend(dbenv, name, 0, flags, mode, fhpp));
}

/*
 * __os_open_extend --
 *	Open a file descriptor (including page size and log size information).
 */
int
__os_open_extend(dbenv, name, page_size, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t page_size, flags;
	int mode;
	DB_FH **fhpp;
{
	DB_FH *fhp;
	DWORD cluster_size, sector_size, free_clusters, total_clusters;
	int access, attr, createflag, nrepeat, ret, share;
	_TCHAR *drive, *tname;
	_TCHAR dbuf[4]; /* <letter><colon><slash><nul> */

	fhp = NULL;
	tname = NULL;

#define	OKFLAGS								\
	(DB_OSO_ABSMODE | DB_OSO_CREATE | DB_OSO_DIRECT | DB_OSO_DSYNC |\
	DB_OSO_EXCL | DB_OSO_RDONLY | DB_OSO_REGION |	DB_OSO_SEQ |	\
	DB_OSO_TEMP | DB_OSO_TRUNC)
	if ((ret = __db_fchk(dbenv, "__os_open", flags, OKFLAGS)) != 0)
		return (ret);

	TO_TSTRING(dbenv, name, tname, ret);
	if (ret != 0)
		goto err;

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &fhp)) != 0)
		goto err;

	/*
	 * Otherwise, use the Windows/32 CreateFile interface so that we can
	 * play magic games with files to get data flush effects similar to
	 * the POSIX O_DSYNC flag.
	 *
	 * !!!
	 * We currently ignore the 'mode' argument.  It would be possible
	 * to construct a set of security attributes that we could pass to
	 * CreateFile that would accurately represents the mode.  In worst
	 * case, this would require looking up user and all group names and
	 * creating an entry for each.  Alternatively, we could call the
	 * _chmod (partial emulation) function after file creation, although
	 * this leaves us with an obvious race.  However, these efforts are
	 * largely meaningless on FAT, the most common file system, which
	 * only has a "readable" and "writeable" flag, applying to all users.
	 */
	access = GENERIC_READ;
	if (!LF_ISSET(DB_OSO_RDONLY))
		access |= GENERIC_WRITE;

	share = FILE_SHARE_READ | FILE_SHARE_WRITE;
	if (__os_is_winnt())
		share |= FILE_SHARE_DELETE;
	attr = FILE_ATTRIBUTE_NORMAL;

	/*
	 * Reproduce POSIX 1003.1 semantics: if O_CREATE and O_EXCL are both
	 * specified, fail, returning EEXIST, unless we create the file.
	 */
	if (LF_ISSET(DB_OSO_CREATE) && LF_ISSET(DB_OSO_EXCL))
		createflag = CREATE_NEW;	/* create only if !exist*/
	else if (!LF_ISSET(DB_OSO_CREATE) && LF_ISSET(DB_OSO_TRUNC))
		createflag = TRUNCATE_EXISTING; /* truncate, fail if !exist */
	else if (LF_ISSET(DB_OSO_TRUNC))
		createflag = CREATE_ALWAYS;	/* create and truncate */
	else if (LF_ISSET(DB_OSO_CREATE))
		createflag = OPEN_ALWAYS;	/* open or create */
	else
		createflag = OPEN_EXISTING;	/* open only if existing */

	if (LF_ISSET(DB_OSO_DSYNC)) {
		F_SET(fhp, DB_FH_NOSYNC);
		attr |= FILE_FLAG_WRITE_THROUGH;
	}

	if (LF_ISSET(DB_OSO_SEQ))
		attr |= FILE_FLAG_SEQUENTIAL_SCAN;
	else
		attr |= FILE_FLAG_RANDOM_ACCESS;

	if (LF_ISSET(DB_OSO_TEMP))
		attr |= FILE_FLAG_DELETE_ON_CLOSE;

	/*
	 * We can turn filesystem buffering off if the page size is a
	 * multiple of the disk's sector size. To find the sector size,
	 * we call GetDiskFreeSpace, which expects a drive name like "d:\\"
	 * or NULL for the current disk (i.e., a relative path)
	 */
	if (LF_ISSET(DB_OSO_DIRECT) && page_size != 0 && name[0] != '\0') {
		if (name[1] == ':') {
			drive = dbuf;
			_sntprintf(dbuf, sizeof(dbuf), _T("%c:\\"), tname[0]);
		} else
			drive = NULL;

		/*
		 * We ignore all results except sectorsize, but some versions
		 * of Windows require that the parameters are non-NULL.
		 */
		if (GetDiskFreeSpace(drive, &cluster_size,
		    &sector_size, &free_clusters, &total_clusters) &&
		    page_size % sector_size == 0)
			attr |= FILE_FLAG_NO_BUFFERING;
	}

	fhp->handle = fhp->trunc_handle = INVALID_HANDLE_VALUE;
	for (nrepeat = 1;; ++nrepeat) {
		if (fhp->handle == INVALID_HANDLE_VALUE)
			fhp->handle = CreateFile(
			    tname, access, share, NULL, createflag, attr, 0);

		/*
		 * Windows does not provide truncate directly.  There is no
		 * safe way to use a handle for truncate concurrently with
		 * reads or writes.  To deal with this, we open a second handle
		 * used just for truncating.
		 */
		if (fhp->handle != INVALID_HANDLE_VALUE &&
		    !LF_ISSET(DB_OSO_RDONLY | DB_OSO_TEMP) &&
		    fhp->trunc_handle == INVALID_HANDLE_VALUE)
			fhp->trunc_handle = CreateFile(
			    tname, access, share, NULL, OPEN_EXISTING, attr, 0);

		if (fhp->handle == INVALID_HANDLE_VALUE ||
		    (!LF_ISSET(DB_OSO_RDONLY | DB_OSO_TEMP) &&
		    fhp->trunc_handle == INVALID_HANDLE_VALUE)) {
			/*
			 * If it's a "temporary" error, we retry up to 3 times,
			 * waiting up to 12 seconds.  While it's not a problem
			 * if we can't open a database, an inability to open a
			 * log file is cause for serious dismay.
			 */
			ret = __os_posix_err(__os_get_syserr());
			if ((ret != ENFILE && ret != EMFILE && ret != ENOSPC) ||
			    nrepeat > 3)
				goto err;

			__os_sleep(dbenv, nrepeat * 2, 0);
		} else
			break;
	}

	FREE_STRING(dbenv, tname);

	F_SET(fhp, DB_FH_OPENED);
	*fhpp = fhp;
	return (0);

err:	FREE_STRING(dbenv, tname);
	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	return (ret);
}
