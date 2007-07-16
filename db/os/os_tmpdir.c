/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_tmpdir.c,v 12.11 2006/08/24 14:46:18 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#ifndef NO_SYSTEM_INCLUDES
#ifdef macintosh
#include <TFileSpec.h>
#endif
#endif

/*
 * __os_tmpdir --
 *	Set the temporary directory path.
 *
 * The order of items in the list structure and the order of checks in
 * the environment are documented.
 *
 * PUBLIC: int __os_tmpdir __P((DB_ENV *, u_int32_t));
 */
int
__os_tmpdir(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int isdir, ret;

	/*
	 * !!!
	 * Don't change this to:
	 *
	 *	static const char * const list[]
	 *
	 * because it creates a text relocation in position independent code.
	 */
	static const char * list[] = {
		"/var/tmp",
		"/usr/tmp",
		"/temp",		/* Windows. */
		"/tmp",
		"C:/temp",		/* Windows. */
		"C:/tmp",		/* Windows. */
		NULL
	};
	const char * const *lp;
	char *tdir, tdir_buf[DB_MAXPATHLEN];

	/* Use the environment if it's permitted and initialized. */
	if (LF_ISSET(DB_USE_ENVIRON) ||
	    (LF_ISSET(DB_USE_ENVIRON_ROOT) && __os_isroot())) {
		/* POSIX: TMPDIR */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    dbenv, "TMPDIR", &tdir, sizeof(tdir_buf))) != 0)
			return (ret);
		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		/*
		 * Windows: TEMP, TMP
		 */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    dbenv, "TEMP", &tdir, sizeof(tdir_buf))) != 0)
			return (ret);
		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    dbenv, "TMP", &tdir, sizeof(tdir_buf))) != 0)
			return (ret);
		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		/* Macintosh */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    dbenv, "TempFolder", &tdir, sizeof(tdir_buf))) != 0)
			return (ret);

		if (tdir != NULL && tdir[0] != '\0')
found:			return (__os_strdup(dbenv, tdir, &dbenv->db_tmp_dir));
	}

#ifdef macintosh
	/* Get the path to the temporary folder. */
	{FSSpec spec;

		if (!Special2FSSpec(kTemporaryFolderType,
		    kOnSystemDisk, 0, &spec))
			return (__os_strdup(dbenv,
			    FSp2FullPath(&spec), &dbenv->db_tmp_dir));
	}
#endif
#ifdef DB_WIN32
	/* Get the path to the temporary directory. */
	{
		_TCHAR tpath[DB_MAXPATHLEN + 1];
		char *path, *eos;

		if (GetTempPath(DB_MAXPATHLEN, tpath) > 2) {
			FROM_TSTRING(dbenv, tpath, path, ret);
			if (ret != 0)
				return (ret);

			eos = path + strlen(path) - 1;
			if (*eos == '\\' || *eos == '/')
				*eos = '\0';
			if (__os_exists(dbenv, path, &isdir) == 0 && isdir) {
				ret = __os_strdup(dbenv,
				    path, &dbenv->db_tmp_dir);
				FREE_STRING(dbenv, path);
				return (ret);
			}
			FREE_STRING(dbenv, path);
		}
	}
#endif

	/* Step through the static list looking for a possibility. */
	for (lp = list; *lp != NULL; ++lp)
		if (__os_exists(dbenv, *lp, &isdir) == 0 && isdir != 0)
			return (__os_strdup(dbenv, *lp, &dbenv->db_tmp_dir));
	return (0);
}
