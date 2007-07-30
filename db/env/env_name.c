/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: env_name.c,v 12.87 2007/05/17 15:15:11 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

static int __db_tmp_open __P((DB_ENV *, u_int32_t, char *, DB_FH **));

#define	DB_ADDSTR(add) {						\
	/*								\
	 * The string might be NULL or zero-length, and the p[-1]	\
	 * might indirect to before the beginning of our buffer.	\
	 */								\
	if ((add) != NULL && (add)[0] != '\0') {			\
		/* If leading slash, start over. */			\
		if (__os_abspath(add)) {				\
			p = str;					\
			slash = 0;					\
		}							\
		/* Append to the current string. */			\
		len = strlen(add);					\
		if (slash)						\
			*p++ = PATH_SEPARATOR[0];			\
		memcpy(p, add, len);					\
		p += len;						\
		slash = strchr(PATH_SEPARATOR, p[-1]) == NULL;		\
	}								\
}

/*
 * __db_appname --
 *	Given an optional DB environment, directory and file name and type
 *	of call, build a path based on the DB_ENV->open rules, and return
 *	it in allocated space.
 *
 * PUBLIC: int __db_appname __P((DB_ENV *, APPNAME,
 * PUBLIC:    const char *, u_int32_t, DB_FH **, char **));
 */
int
__db_appname(dbenv, appname, file, tmp_oflags, fhpp, namep)
	DB_ENV *dbenv;
	APPNAME appname;
	const char *file;
	u_int32_t tmp_oflags;
	DB_FH **fhpp;
	char **namep;
{
	enum { TRY_NOTSET, TRY_DATA_DIR, TRY_ENV_HOME, TRY_CREATE } try_state;
	size_t len, str_len;
	int data_entry, ret, slash, tmp_create;
	const char *a, *b;
	char *p, *str;

	try_state = TRY_NOTSET;
	a = b = NULL;
	data_entry = 0;
	tmp_create = 0;

	/*
	 * We don't return a name when creating temporary files, just a file
	 * handle.  Default to an error now.
	 */
	if (fhpp != NULL)
		*fhpp = NULL;
	if (namep != NULL)
		*namep = NULL;

	/*
	 * Absolute path names are never modified.  If the file is an absolute
	 * path, we're done.
	 */
	if (file != NULL && __os_abspath(file))
		return (__os_strdup(dbenv, file, namep));

	/* Everything else is relative to the environment home. */
	if (dbenv != NULL)
		a = dbenv->db_home;

retry:	/*
	 * DB_APP_NONE:
	 *      DB_HOME/file
	 * DB_APP_DATA:
	 *      DB_HOME/DB_DATA_DIR/file
	 * DB_APP_LOG:
	 *      DB_HOME/DB_LOG_DIR/file
	 * DB_APP_TMP:
	 *      DB_HOME/DB_TMP_DIR/<create>
	 */
	switch (appname) {
	case DB_APP_NONE:
		break;
	case DB_APP_DATA:
		if (dbenv == NULL || dbenv->db_data_dir == NULL) {
			try_state = TRY_CREATE;
			break;
		}

		/*
		 * First, step through the data_dir entries, if any, looking
		 * for the file.
		 */
		if ((b = dbenv->db_data_dir[data_entry]) != NULL) {
			++data_entry;
			try_state = TRY_DATA_DIR;
			break;
		}

		/* Second, look in the environment home directory. */
		if (try_state != TRY_ENV_HOME) {
			try_state = TRY_ENV_HOME;
			break;
		}

		/* Third, try creation in the first data_dir entry. */
		try_state = TRY_CREATE;
		b = dbenv->db_data_dir[0];
		break;
	case DB_APP_LOG:
		if (dbenv != NULL)
			b = dbenv->db_log_dir;
		break;
	case DB_APP_TMP:
		if (dbenv != NULL)
			b = dbenv->db_tmp_dir;
		tmp_create = 1;
		break;
	}

	len =
	    (a == NULL ? 0 : strlen(a) + 1) +
	    (b == NULL ? 0 : strlen(b) + 1) +
	    (file == NULL ? 0 : strlen(file) + 1);

	/*
	 * Allocate space to hold the current path information, as well as any
	 * temporary space that we're going to need to create a temporary file
	 * name.
	 */
#define	DB_TRAIL	"BDBXXXXX"
	str_len = len + sizeof(DB_TRAIL) + 10;
	if ((ret = __os_malloc(dbenv, str_len, &str)) != 0)
		return (ret);

	slash = 0;
	p = str;
	DB_ADDSTR(a);
	DB_ADDSTR(b);
	DB_ADDSTR(file);
	*p = '\0';

	/*
	 * If we're opening a data file, see if it exists.  If it does,
	 * return it, otherwise, try and find another one to open.
	 */
	if (appname == DB_APP_DATA &&
	    __os_exists(dbenv, str, NULL) != 0 && try_state != TRY_CREATE) {
		__os_free(dbenv, str);
		b = NULL;
		goto retry;
	}

	/* Create the file if so requested. */
	if (tmp_create &&
	    (ret = __db_tmp_open(dbenv, tmp_oflags, str, fhpp)) != 0) {
		__os_free(dbenv, str);
		return (ret);
	}

	if (namep == NULL)
		__os_free(dbenv, str);
	else
		*namep = str;
	return (0);
}

/*
 * __db_tmp_open --
 *	Create a temporary file.
 */
static int
__db_tmp_open(dbenv, tmp_oflags, path, fhpp)
	DB_ENV *dbenv;
	u_int32_t tmp_oflags;
	char *path;
	DB_FH **fhpp;
{
	pid_t pid;
	int filenum, i, isdir, ret;
	char *firstx, *trv;

	/*
	 * Check the target directory; if you have six X's and it doesn't
	 * exist, this runs for a *very* long time.
	 */
	if ((ret = __os_exists(dbenv, path, &isdir)) != 0) {
		__db_err(dbenv, ret, "%s", path);
		return (ret);
	}
	if (!isdir) {
		__db_err(dbenv, EINVAL, "%s", path);
		return (EINVAL);
	}

	/* Build the path. */
	(void)strncat(path, PATH_SEPARATOR, 1);
	(void)strcat(path, DB_TRAIL);

	/* Replace the X's with the process ID (in decimal). */
	__os_id(dbenv, &pid, NULL);
	for (trv = path + strlen(path); *--trv == 'X'; pid /= 10)
		*trv = '0' + (u_char)(pid % 10);
	firstx = trv + 1;

	/* Loop, trying to open a file. */
	for (filenum = 1;; filenum++) {
		if ((ret = __os_open(dbenv, path, 0,
		    tmp_oflags | DB_OSO_CREATE | DB_OSO_EXCL | DB_OSO_TEMP,
		    __db_omode(OWNER_RW), fhpp)) == 0)
			return (0);

		/*
		 * !!!:
		 * If we don't get an EEXIST error, then there's something
		 * seriously wrong.  Unfortunately, if the implementation
		 * doesn't return EEXIST for O_CREAT and O_EXCL regardless
		 * of other possible errors, we've lost.
		 */
		if (ret != EEXIST) {
			__db_err(dbenv, ret, "temporary open: %s", path);
			return (ret);
		}

		/*
		 * Generate temporary file names in a backwards-compatible way.
		 * If pid == 12345, the result is:
		 *   <path>/DB12345 (tried above, the first time through).
		 *   <path>/DBa2345 ...  <path>/DBz2345
		 *   <path>/DBaa345 ...  <path>/DBaz345
		 *   <path>/DBba345, and so on.
		 *
		 * XXX
		 * This algorithm is O(n**2) -- that is, creating 100 temporary
		 * files requires 5,000 opens, creating 1000 files requires
		 * 500,000.  If applications open a lot of temporary files, we
		 * could improve performance by switching to timestamp-based
		 * file names.
		 */
		for (i = filenum, trv = firstx; i > 0; i = (i - 1) / 26)
			if (*trv++ == '\0')
				return (EINVAL);

		for (i = filenum; i > 0; i = (i - 1) / 26)
			*--trv = 'a' + ((i - 1) % 26);
	}
	/* NOTREACHED */
}
