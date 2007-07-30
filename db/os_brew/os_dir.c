/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_dir.c,v 1.5 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_dirlist --
 *	Return a list of the files in a directory.
 */
int
__os_dirlist(dbenv, dir, namesp, cntp)
	DB_ENV *dbenv;
	const char *dir;
	char ***namesp;
	int *cntp;
{
	FileInfo fi;
	IFileMgr *pIFileMgr;
	int arraysz, cnt, ret;
	char *filename, *p, **names;

	FILE_MANAGER_CREATE(dbenv, pIFileMgr, ret);
	if (ret != 0)
		return (ret);

	if ((ret = IFILEMGR_EnumInit(pIFileMgr, dir, FALSE)) != SUCCESS) {
		IFILEMGR_Release(pIFileMgr);
		__db_syserr(dbenv, ret, "IFILEMGR_EnumInit");
		return (__os_posix_err(ret));
	}

	names = NULL;
	arraysz = cnt = 0;
	while (IFILEMGR_EnumNext(pIFileMgr, &fi) != FALSE) {
		cnt ++;

		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = __os_realloc(dbenv,
			    (u_int)arraysz * sizeof(char *), &names)) != 0)
				goto nomem;
		}
		for (filename = fi.szName;
		    (p = strchr(filename, '\\')) != NULL; filename = p + 1)
			;
		for (; (p = strchr(filename, '/')) != NULL; filename = p + 1)
			;
		if ((ret = __os_strdup(dbenv, filename, &names[cnt - 1])) != 0)
			goto nomem;
	}
	IFILEMGR_Release(pIFileMgr);

	*namesp = names;
	*cntp = cnt;
	return (ret);

nomem:	if (names != NULL)
		__os_dirfree(dbenv, names, cnt);
	IFILEMGR_Release(pIFileMgr);

	return (ret);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 */
void
__os_dirfree(dbenv, names, cnt)
	DB_ENV *dbenv;
	char **names;
	int cnt;
{
	while (cnt > 0)
		__os_free(dbenv, names[--cnt]);
	__os_free(dbenv, names);
}
