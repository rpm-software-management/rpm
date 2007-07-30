/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: env_config.c,v 12.73 2007/05/17 15:15:11 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __config_parse __P((DB_ENV *, char *, int));

/*
 * __env_read_db_config --
 *	Read the DB_CONFIG file.
 *
 * PUBLIC: int __env_read_db_config __P((DB_ENV *));
 */
int
__env_read_db_config(dbenv)
	DB_ENV *dbenv;
{
	FILE *fp;
	int lc, ret;
	char *p, buf[256];

	/* Parse the config file. */
	p = NULL;
	if ((ret =
	    __db_appname(dbenv, DB_APP_NONE, "DB_CONFIG", 0, NULL, &p)) != 0)
		return (ret);
	if (p == NULL)
		fp = NULL;
	else {
		fp = fopen(p, "r");
		__os_free(dbenv, p);
	}

	if (fp == NULL)
		return (0);

	for (lc = 1; fgets(buf, sizeof(buf), fp) != NULL; ++lc) {
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		else if (strlen(buf) + 1 == sizeof(buf)) {
			__db_errx(dbenv, "DB_CONFIG: line too long");
			ret = EINVAL;
			break;
		}
		for (p = buf; *p != '\0' || isspace((int)*p); ++p)
			;
		if (buf[0] == '\0' || buf[0] == '#')
			continue;

		if ((ret = __config_parse(dbenv, buf, lc)) != 0)
			break;
	}
	(void)fclose(fp);

	return (ret);
}

#undef	CONFIG_GET_INT
#define	CONFIG_GET_INT(s, vp) do {					\
	int __ret;							\
	if ((__ret =							\
	    __db_getlong(dbenv, NULL, s, 0, INT_MAX, vp)) != 0)		\
		return (__ret);						\
} while (0)
#undef	CONFIG_GET_LONG
#define	CONFIG_GET_LONG(s, vp) do {					\
	int __ret;							\
	if ((__ret =							\
	    __db_getlong(dbenv, NULL, s, 0, LONG_MAX, vp)) != 0)	\
		return (__ret);						\
} while (0)
#undef	CONFIG_INT
#define	CONFIG_INT(s, f) do {						\
	if (strcasecmp(s, argv[0]) == 0) {				\
		long __v;						\
		if (nf != 2)						\
			goto format;					\
		CONFIG_GET_INT(argv[1], &__v);				\
		return (f(dbenv, (int)__v));				\
	}								\
} while (0)
#undef	CONFIG_GET_UINT32
#define	CONFIG_GET_UINT32(s, vp) do {					\
	if (__db_getulong(dbenv, NULL, s, 0, UINT32_MAX, vp) != 0)	\
		return (EINVAL);					\
} while (0)
#undef	CONFIG_UINT32
#define	CONFIG_UINT32(s, f) do {					\
	if (strcasecmp(s, argv[0]) == 0) {				\
		u_long __v;						\
		if (nf != 2)						\
			goto format;					\
		CONFIG_GET_UINT32(argv[1], &__v);			\
		return (f(dbenv, (u_int32_t)__v));			\
	}								\
} while (0)

#undef	CONFIG_SLOTS
#define	CONFIG_SLOTS	10

/*
 * __config_parse --
 *	Parse a single NAME VALUE pair.
 */
static int
__config_parse(dbenv, s, lc)
	DB_ENV *dbenv;
	char *s;
	int lc;
{
	u_long uv1, uv2;
	u_int32_t flags;
	long lv1, lv2;
	int nf;
	char *argv[CONFIG_SLOTS];
					/* Split the line by white-space. */
	if ((nf = __config_split(s, argv)) < 2) {
format:		__db_errx(dbenv,
		    "line %d: %s: incorrect name-value pair", lc, argv[0]);
		return (EINVAL);
	}

	CONFIG_UINT32("mutex_set_align", __mutex_set_align);
	CONFIG_UINT32("mutex_set_increment", __mutex_set_increment);
	CONFIG_UINT32("mutex_set_max", __mutex_set_max);
	CONFIG_UINT32("mutex_set_tas_spins", __mutex_set_tas_spins);

	if (strcasecmp(argv[0], "rep_set_config") == 0) {
		if (nf != 2)
			goto format;
		if (strcasecmp(argv[1], "rep_bulk") == 0)
			return (__rep_set_config(dbenv,
			    DB_REP_CONF_BULK, 1));
		if (strcasecmp(argv[1], "rep_delayclient") == 0)
			return (__rep_set_config(dbenv,
			    DB_REP_CONF_DELAYCLIENT, 1));
		if (strcasecmp(argv[1], "rep_noautoinit") == 0)
			return (__rep_set_config(dbenv,
			    DB_REP_CONF_NOAUTOINIT, 1));
		if (strcasecmp(argv[1], "rep_nowait") == 0)
			return (__rep_set_config(dbenv, DB_REP_CONF_NOWAIT, 1));
		goto format;
	}

	if (strcasecmp(argv[0], "set_cachesize") == 0) {
		if (nf != 4)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv1);
		CONFIG_GET_UINT32(argv[2], &uv2);
		CONFIG_GET_INT(argv[3], &lv1);
		return (__memp_set_cachesize(
		    dbenv, (u_int32_t)uv1, (u_int32_t)uv2, (int)lv1));
	}

	if (strcasecmp(argv[0], "set_data_dir") == 0 ||
	    strcasecmp(argv[0], "db_data_dir") == 0) {	/* Compatibility. */
		if (nf != 2)
			goto format;
		return (__env_set_data_dir(dbenv, argv[1]));
	}
							/* Undocumented. */
	if (strcasecmp(argv[0], "set_intermediate_dir") == 0) {
		if (nf != 2)
			goto format;
		CONFIG_GET_INT(argv[1], &lv1);
		return (__env_set_intermediate_dir(dbenv, (int)lv1, 0));
	}

	if (strcasecmp(argv[0], "set_flags") == 0) {
		if (nf != 2)
			goto format;
		if (strcasecmp(argv[1], "db_auto_commit") == 0)
			return (__env_set_flags(dbenv, DB_AUTO_COMMIT, 1));
		if (strcasecmp(argv[1], "db_cdb_alldb") == 0)
			return (__env_set_flags(dbenv, DB_CDB_ALLDB, 1));
		if (strcasecmp(argv[1], "db_direct_db") == 0)
			return (__env_set_flags(dbenv, DB_DIRECT_DB, 1));
		if (strcasecmp(argv[1], "db_direct_log") == 0)
			return (__env_set_flags(dbenv, DB_DIRECT_LOG, 1));
		if (strcasecmp(argv[1], "db_dsync_db") == 0)
			return (__env_set_flags(dbenv, DB_DSYNC_DB, 1));
		if (strcasecmp(argv[1], "db_dsync_log") == 0)
			return (__env_set_flags(dbenv, DB_DSYNC_LOG, 1));
		if (strcasecmp(argv[1], "db_log_autoremove") == 0)
			return (__env_set_flags(dbenv, DB_LOG_AUTOREMOVE, 1));
		if (strcasecmp(argv[1], "db_log_inmemory") == 0)
			return (__env_set_flags(dbenv, DB_LOG_INMEMORY, 1));
		if (strcasecmp(argv[1], "db_multiversion") == 0)
			return (__env_set_flags(dbenv, DB_MULTIVERSION, 1));
		if (strcasecmp(argv[1], "db_nolocking") == 0)
			return (__env_set_flags(dbenv, DB_NOLOCKING, 1));
		if (strcasecmp(argv[1], "db_nommap") == 0)
			return (__env_set_flags(dbenv, DB_NOMMAP, 1));
		if (strcasecmp(argv[1], "db_nopanic") == 0)
			return (__env_set_flags(dbenv, DB_NOPANIC, 1));
		if (strcasecmp(argv[1], "db_overwrite") == 0)
			return (__env_set_flags(dbenv, DB_OVERWRITE, 1));
		if (strcasecmp(argv[1], "db_region_init") == 0)
			return (__env_set_flags(dbenv, DB_REGION_INIT, 1));
		if (strcasecmp(argv[1], "db_txn_nosync") == 0)
			return (__env_set_flags(dbenv, DB_TXN_NOSYNC, 1));
		if (strcasecmp(argv[1], "db_txn_nowait") == 0)
			return (__env_set_flags(dbenv, DB_TXN_NOWAIT, 1));
		if (strcasecmp(argv[1], "db_txn_snapshot") == 0)
			return (__env_set_flags(dbenv, DB_TXN_SNAPSHOT, 1));
		if (strcasecmp(argv[1], "db_txn_write_nosync") == 0)
			return (
			    __env_set_flags(dbenv, DB_TXN_WRITE_NOSYNC, 1));
		if (strcasecmp(argv[1], "db_yieldcpu") == 0)
			return (__env_set_flags(dbenv, DB_YIELDCPU, 1));
		goto format;
	}

	CONFIG_UINT32("set_lg_bsize", __log_set_lg_bsize);
	CONFIG_INT("set_lg_filemode", __log_set_lg_filemode);
	CONFIG_UINT32("set_lg_max", __log_set_lg_max);
	CONFIG_UINT32("set_lg_regionmax", __log_set_lg_regionmax);

	if (strcasecmp(argv[0], "set_lg_dir") == 0 ||
	    strcasecmp(argv[0], "db_log_dir") == 0) {	/* Compatibility. */
		if (nf != 2)
			goto format;
		return (__log_set_lg_dir(dbenv, argv[1]));
	}

	if (strcasecmp(argv[0], "set_lk_detect") == 0) {
		if (nf != 2)
			goto format;
		if (strcasecmp(argv[1], "db_lock_default") == 0)
			flags = DB_LOCK_DEFAULT;
		else if (strcasecmp(argv[1], "db_lock_expire") == 0)
			flags = DB_LOCK_EXPIRE;
		else if (strcasecmp(argv[1], "db_lock_maxlocks") == 0)
			flags = DB_LOCK_MAXLOCKS;
		else if (strcasecmp(argv[1], "db_lock_maxwrite") == 0)
			flags = DB_LOCK_MAXWRITE;
		else if (strcasecmp(argv[1], "db_lock_minlocks") == 0)
			flags = DB_LOCK_MINLOCKS;
		else if (strcasecmp(argv[1], "db_lock_minwrite") == 0)
			flags = DB_LOCK_MINWRITE;
		else if (strcasecmp(argv[1], "db_lock_oldest") == 0)
			flags = DB_LOCK_OLDEST;
		else if (strcasecmp(argv[1], "db_lock_random") == 0)
			flags = DB_LOCK_RANDOM;
		else if (strcasecmp(argv[1], "db_lock_youngest") == 0)
			flags = DB_LOCK_YOUNGEST;
		else
			goto format;
		return (__lock_set_lk_detect(dbenv, flags));
	}

	CONFIG_UINT32("set_lk_max_locks", __lock_set_lk_max_locks);
	CONFIG_UINT32("set_lk_max_lockers", __lock_set_lk_max_lockers);
	CONFIG_UINT32("set_lk_max_objects", __lock_set_lk_max_objects);

	if (strcasecmp(argv[0], "set_lock_timeout") == 0) {
		if (nf != 2)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv1);
		return (__lock_set_env_timeout(
		    dbenv, (u_int32_t)uv1, DB_SET_LOCK_TIMEOUT));
	}

	CONFIG_INT("set_mp_max_openfd", __memp_set_mp_max_openfd);

	if (strcasecmp(argv[0], "set_mp_max_write") == 0) {
		if (nf != 3)
			goto format;
		CONFIG_GET_INT(argv[1], &lv1);
		CONFIG_GET_INT(argv[2], &lv2);
		return (__memp_set_mp_max_write(
		    dbenv, (int)lv1, (db_timeout_t)lv2));
	}

	CONFIG_UINT32("set_mp_mmapsize", __memp_set_mp_mmapsize);

	if (strcasecmp(argv[0], "set_region_init") == 0) {
		if (nf != 2)
			goto format;
		CONFIG_GET_INT(argv[1], &lv1);
		if (lv1 != 0 && lv1 != 1)
			goto format;
		return (__env_set_flags(
		    dbenv, DB_REGION_INIT, lv1 == 0 ? 0 : 1));
	}

	if (strcasecmp(argv[0], "set_shm_key") == 0) {
		if (nf != 2)
			goto format;
		CONFIG_GET_LONG(argv[1], &lv1);
		return (__env_set_shm_key(dbenv, lv1));
	}

	/*
	 * The set_tas_spins method has been replaced by mutex_set_tas_spins.
	 * The set_tas_spins argv[0] remains for DB_CONFIG compatibility.
	 */
	CONFIG_UINT32("set_tas_spins", __mutex_set_tas_spins);

	if (strcasecmp(argv[0], "set_tmp_dir") == 0 ||
	    strcasecmp(argv[0], "db_tmp_dir") == 0) {	/* Compatibility.*/
		if (nf != 2)
			goto format;
		return (__env_set_tmp_dir(dbenv, argv[1]));
	}

	CONFIG_UINT32("set_tx_max", __txn_set_tx_max);

	if (strcasecmp(argv[0], "set_txn_timeout") == 0) {
		if (nf != 2)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv1);
		return (__lock_set_env_timeout(
		    dbenv, (u_int32_t)uv1, DB_SET_TXN_TIMEOUT));
	}

	if (strcasecmp(argv[0], "set_verbose") == 0) {
		if (nf != 2)
			goto format;
		if (strcasecmp(argv[1], "db_verb_deadlock") == 0)
			flags = DB_VERB_DEADLOCK;
		else if (strcasecmp(argv[1], "db_verb_fileops") == 0)
			flags = DB_VERB_FILEOPS;
		else if (strcasecmp(argv[1], "db_verb_fileops_all") == 0)
			flags = DB_VERB_FILEOPS_ALL;
		else if (strcasecmp(argv[1], "db_verb_recovery") == 0)
			flags = DB_VERB_RECOVERY;
		else if (strcasecmp(argv[1], "db_verb_register") == 0)
			flags = DB_VERB_REGISTER;
		else if (strcasecmp(argv[1], "db_verb_replication") == 0)
			flags = DB_VERB_REPLICATION;
		else if (strcasecmp(argv[1], "db_verb_waitsfor") == 0)
			flags = DB_VERB_WAITSFOR;
		else
			goto format;
		return (__env_set_verbose(dbenv, flags, 1));
	}

	__db_errx(dbenv, "unrecognized name-value pair: %s", s);
	return (EINVAL);
}

/*
 * __config_split --
 *	Split lines into white-space separated fields, returning the count of
 *	fields.
 *
 * PUBLIC: int __config_split __P((char *, char *[]));
 */
int
__config_split(input, argv)
	char *input, *argv[CONFIG_SLOTS];
{
	int count;
	char **ap;

	for (count = 0, ap = argv; (*ap = strsep(&input, " \t\n")) != NULL;)
		if (**ap != '\0') {
			++count;
			if (++ap == &argv[CONFIG_SLOTS - 1]) {
				*ap = NULL;
				break;
			}
		}
	return (count);
}
