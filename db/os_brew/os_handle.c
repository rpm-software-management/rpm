/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998,2007 Oracle.  All rights reserved.
 *
 * $Id: os_handle.c,v 1.5 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_openhandle --
 *      Open a file, using BREW open flags.
 */
int
__os_openhandle(dbenv, name, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	int flags, mode;
	DB_FH **fhpp;
{
	DB_FH *fhp;
	IFile *pIFile;
	IFileMgr *pIFileMgr;
	int f_exists, ret;

	COMPQUIET(mode, 0);

	FILE_MANAGER_CREATE(dbenv, pIFileMgr, ret);
	if (ret != 0)
		return (ret);

	/*
	 * Allocate the file handle and copy the file name.  We generally only
	 * use the name for verbose or error messages, but on systems where we
	 * can't unlink temporary files immediately, we use the name to unlink
	 * the temporary file when the file handle is closed.
	 */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &fhp)) != 0)
		return (ret);
	if ((ret = __os_strdup(dbenv, name, &fhp->name)) != 0)
		goto err;

	/*
	 * Test the file before opening.  BREW doesn't want to see the
	 * _OFM_CREATE flag if the file already exists, and it doesn't
	 * want to see any other flag if the file doesn't exist.
	 */
	f_exists = IFILEMGR_Test(pIFileMgr, name) == SUCCESS ? 1 : 0;
	if (f_exists)
		LF_CLR(_OFM_CREATE);		/* Clear _OFM_CREATE. */
	else
		LF_CLR(~_OFM_CREATE);		/* Leave only _OFM_CREATE. */

	if ((pIFile =
	    IFILEMGR_OpenFile(pIFileMgr, name, (OpenFileMode)flags)) == NULL) {
		FILE_MANAGER_ERR(dbenv,
		    pIFileMgr, name, "IFILEMGR_OpenFile", ret);
		goto err;
	}

	fhp->ifp = pIFile;
	IFILEMGR_Release(pIFileMgr);

	F_SET(fhp, DB_FH_OPENED);
	*fhpp = fhp;
	return (0);

err:	if (pIFile != NULL)
		IFILE_Release(pIFile);
	IFILEMGR_Release(pIFileMgr);

	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	return (ret);
}

/*
 * __os_closehandle --
 *      Close a file.
 */
int
__os_closehandle(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	/* Discard any underlying system file reference. */
	if (F_ISSET(fhp, DB_FH_OPENED))
		(void)IFILE_Release(fhp->ifp);

	/* Unlink the file if we haven't already done so. */
	if (F_ISSET(fhp, DB_FH_UNLINK))
		(void)__os_unlink(dbenv, fhp->name);

	if (fhp->name != NULL)
		__os_free(dbenv, fhp->name);
	__os_free(dbenv, fhp);

	return (0);
}
