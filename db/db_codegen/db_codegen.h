/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: db_codegen.h,v 1.4 2007/05/17 15:14:58 bostic Exp $
 */
#include "db_config.h"

#include "db_int.h"

typedef struct __db_obj {
	char	 *name;			/* Database name */
	char	 *dbtype;		/* Database type */

	u_int32_t extentsize;		/* Queue: extent size */
	u_int32_t pagesize;		/* Pagesize */
	u_int32_t re_len;		/* Queue/Recno: record length */

	char	 *key_type;		/* Key type */
	int	  custom;		/* Custom key comparison. */

	char	 *primary;		/* Secondary: primary's name */
	u_int32_t secondary_len;	/* secondary: length */
	u_int32_t secondary_off;	/* secondary: 0-based byte offset */

	int	  dupsort;		/* Sorted duplicates */
	int	  recnum;		/* Btree: record numbers */
	int	  transaction;		/* Database is transactional */

	TAILQ_ENTRY(__db_obj) q;	/* List of databases */
} DB_OBJ;

typedef struct __env_obj {
	char	 *prefix;		/* Name prefix */
	char	 *home;			/* Environment home */

	u_int32_t gbytes;		/* GB, B of cache */
	u_int32_t bytes;
	u_int32_t ncache;		/* Number of caches */

	int	  private;		/* Private environment */
	int	  standalone;		/* Standalone database */
	int	  transaction;		/* Database is transactional */

	TAILQ_ENTRY(__env_obj) q;	/* List of environments, databases */
	TAILQ_HEAD(__head_db, __db_obj) dbq;
} ENV_OBJ;

TAILQ_HEAD(__head_env, __env_obj);	/* List of environments */
extern struct __head_env env_tree;

extern const char *progname;		/* Program name */

int api_c __P((char *));
#ifdef DEBUG
int parse_dump __P((void));
#endif
int parse_input __P((FILE *));
