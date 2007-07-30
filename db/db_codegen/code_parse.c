/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: code_parse.c,v 1.5 2007/05/17 15:14:58 bostic Exp $
 */

#include "db_codegen.h"

static enum					/* Parse state */
    { PS_UNSET, PS_ENV_SET, PS_DB_SET } parse_status;
static ENV_OBJ *cur_env;			/* Current objects */
static  DB_OBJ *cur_db;

static int parse_line __P((char *, int));

int
parse_input(fp)
	FILE *fp;
{
	int lc;
	char *p, *t, buf[256];

	parse_status = PS_UNSET;

	for (lc = 1; fgets(buf, sizeof(buf), fp) != NULL; ++lc) {
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		else if (strlen(buf) + 1 == sizeof(buf)) {
			fprintf(stderr, "%s: %d: line too long", progname, lc);
			return (1);
		}

		/* Skip leading whitespace. */
		for (p = buf; *p != '\0' && isspace((int)*p); ++p)
			;

		/*
		 * Any empty line or hash mark to the end of the line is
		 * a comment.
		 */
		if (*p == '\0' || *p == '#')
			continue;
		for (t = p; *t != '\0' && *t != '#'; ++t)
			;
		*t = '\0';

		if (parse_line(p, lc))
			return (1);
	}
	(void)fclose(fp);

	return (0);
}

#undef	CONFIG_SLOTS
#define	CONFIG_SLOTS	10

#undef	CONFIG_GET_UINT32
#define	CONFIG_GET_UINT32(s, vp) do {					\
	if (__db_getulong(NULL, progname, s, 0, UINT32_MAX, vp) != 0)	\
		return (EINVAL);					\
} while (0)

static int
parse_line(s, lc)
	char *s;
	int lc;
{
	u_long uv;
	int nf;
	char *argv[CONFIG_SLOTS], *p;

	nf = __config_split(s, argv);	/* Split the line by white-space. */

	/*
	 * Environment keywords.
	 */
	if (strcasecmp(argv[0], "environment") == 0) {
		if (nf != 3 ||
		    strcmp(argv[2], "{") != 0 || parse_status != PS_UNSET)
			goto format;

		if (__os_calloc(NULL, 1, sizeof(*cur_env), &cur_env) ||
		    __os_strdup(NULL, argv[1], &cur_env->prefix))
			goto memory;
		TAILQ_INIT(&cur_env->dbq);

		TAILQ_INSERT_TAIL(&env_tree, cur_env, q);

		/*
		 * Default settings.
		 */
		cur_env->home = ".";

		parse_status = PS_ENV_SET;
		return (0);
	}
	if (strcasecmp(argv[0], "home") == 0) {
		if (nf != 2 || parse_status != PS_ENV_SET)
			goto format;
		if (__os_strdup(NULL, argv[1], &cur_env->home))
			goto memory;
		return (0);
	}
	if (strcasecmp(argv[0], "cachesize") == 0) {
		if (nf != 4 || parse_status != PS_ENV_SET)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_env->gbytes = uv;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_env->bytes = uv;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_env->ncache = uv;
		return (0);
	}
	if (strcasecmp(argv[0], "private") == 0) {
		if (nf != 1 || parse_status != PS_ENV_SET)
			goto format;
		cur_env->private = 1;
		return (0);
	}

	/*
	 * Database keywords.
	 */
	if (strcasecmp(argv[0], "database") == 0) {
		if (nf != 3 ||
		    strcmp(argv[2], "{") != 0 || parse_status == PS_DB_SET)
			goto format;

		/*
		 * Databases can be specified standalone.   If we don't have an
		 * environment, create a fake one to hold the information.
		 */
		if (parse_status == PS_UNSET) {
			if (__os_calloc(NULL, 1, sizeof(*cur_env), &cur_env))
				goto memory;
			TAILQ_INIT(&cur_env->dbq);
			cur_env->standalone = 1;

			TAILQ_INSERT_TAIL(&env_tree, cur_env, q);
		}

		if (__os_calloc(NULL, 1, sizeof(*cur_db), &cur_db) ||
		    __os_strdup(NULL, argv[1], &cur_db->name))
			goto memory;
		TAILQ_INSERT_TAIL(&cur_env->dbq, cur_db, q);

		/*
		 * Default settings.
		 */
		cur_db->dbtype = "DB_BTREE";

		parse_status = PS_DB_SET;
		return (0);
	}
	if (strcasecmp(argv[0], "custom") == 0) {
		if (nf != 1 || parse_status != PS_DB_SET)
			goto format;
		cur_db->custom = 1;
		return (0);
	}
	if (strcasecmp(argv[0], "dupsort") == 0) {
		if (nf != 1 || parse_status != PS_DB_SET)
			goto format;
		cur_db->dupsort = 1;
		return (0);
	}
	if (strcasecmp(argv[0], "extentsize") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_db->extentsize = uv;
		return (0);
	}
	if (strcasecmp(argv[0], "key_type") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		if (__os_strdup(NULL, argv[1], &cur_db->key_type))
			goto memory;
		return (0);
	}
	if (strcasecmp(argv[0], "pagesize") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_db->pagesize = uv;
		return (0);
	}
	if (strcasecmp(argv[0], "primary") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		if (__os_strdup(NULL, argv[1], &cur_db->primary))
			goto memory;
		return (0);
	}
	if (strcasecmp(argv[0], "recnum") == 0) {
		if (nf != 1 || parse_status != PS_DB_SET)
			goto format;
		cur_db->recnum = 1;
		return (0);
	}
	if (strcasecmp(argv[0], "re_len") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_db->re_len = uv;
		return (0);
	}
	if (strcasecmp(argv[0], "secondary_offset") == 0) {
		if (nf != 3 || parse_status != PS_DB_SET)
			goto format;
		CONFIG_GET_UINT32(argv[1], &uv);
		cur_db->secondary_off = uv;
		CONFIG_GET_UINT32(argv[2], &uv);
		cur_db->secondary_len = uv;
		return (0);
	}
	if (strcasecmp(argv[0], "transaction") == 0) {
		if (nf != 1 || parse_status != PS_DB_SET)
			goto format;
		cur_env->transaction = cur_db->transaction = 1;
		return (0);
	}
	if (strcasecmp(argv[0], "type") == 0) {
		if (nf != 2 || parse_status != PS_DB_SET)
			goto format;
		if (strcasecmp(argv[1], "btree") == 0)
			p = "DB_BTREE";
		else if (strcasecmp(argv[1], "hash") == 0)
			p = "DB_HASH";
		else if (strcasecmp(argv[1], "queue") == 0)
			p = "DB_QUEUE";
		else if (strcasecmp(argv[1], "recno") == 0)
			p = "DB_RECNO";
		else
			goto format;
		if (__os_strdup(NULL, p, &cur_db->dbtype))
			goto memory;
		return (0);
	}

	/*
	 * End block.
	 */
	if (strcmp(argv[0], "}") == 0) {
		if (nf != 1)
			goto format;
		/*
		 * Pop up a level -- if we finished a database that's part of
		 * an environment, go back to the environment level; if we
		 * finished a standalone database or an environment, go back to
		 * unset.
		 */
		switch (parse_status) {
		case PS_UNSET:
			goto format;
		case PS_DB_SET:
			parse_status =
			    cur_env->standalone ? PS_UNSET : PS_ENV_SET;
			break;
		case PS_ENV_SET:
			parse_status = PS_UNSET;
		}
		return (0);
	}

format:	fprintf(stderr,
	    "%s: line %d: %s: invalid input\n", progname, lc, s);
	return (1);

memory:	fprintf(stderr, "%s: %s\n", progname, db_strerror(errno));
	return (1);
}

#ifdef DEBUG
int
parse_dump()
{
	TAILQ_FOREACH(cur_env, &env_tree, q) {
		printf("environment: %s\n",
		    cur_env->standalone ? "standalone" : cur_env->prefix);

		if (cur_env->home != NULL)
			printf("\thome: %s\n", cur_env->home);
		if (cur_env->gbytes != 0 || cur_env->bytes != 0)
			printf("\tcachesize: %luGB, %luB, %lu\n",
			    (u_long)cur_env->gbytes,
			    (u_long)cur_env->bytes,
			    (u_long)cur_env->ncache);

		if (cur_env->private)
			printf("\tprivate: yes\n");
		if (cur_env->transaction)
			printf("\ttransaction: yes\n");

		TAILQ_FOREACH(cur_db, &cur_env->dbq, q) {
			printf("\tdatabase: %s\n", cur_db->name);
			printf("\t\tdbtype: %s\n", cur_db->dbtype);

			if (cur_db->extentsize)
				printf("\t\textentsize: %lu\n",
				    (u_long)cur_db->extentsize);
			if (cur_db->pagesize)
				printf("\t\tpagesize: %lu\n",
				    (u_long)cur_db->pagesize);
			if (cur_db->re_len)
				printf("\t\tre_len: %lu\n",
				    (u_long)cur_db->re_len);

			if (cur_db->key_type != NULL)
				printf("\t\tkey_type: %s\n",
				    cur_db->key_type);

			if (cur_db->primary != NULL)
				printf("\t\tprimary: %s\n",
				    cur_db->primary);
			if (cur_db->custom)
				printf("\t\tcustom: yes\n");
			if (cur_db->secondary_off)
				printf("\t\tsecondary_offset: %lu/%lu\n",
				    (u_long)cur_db->secondary_off,
				    (u_long)cur_db->secondary_len);

			if (cur_db->dupsort)
				printf("\t\tdupsort: yes\n");
			if (cur_db->recnum)
				printf("\t\trecnum: yes\n");
			if (cur_db->transaction)
				printf("\t\ttransaction: yes\n");
		}
	}

	return (0);
}
#endif
