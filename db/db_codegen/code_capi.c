/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: code_capi.c,v 1.8 2007/06/05 14:06:25 bostic Exp $
 */

#include "db_codegen.h"

static FILE *of;			/* Output stream. */

static void api_c_callback __P((DB_OBJ *));
static void api_c_compare __P((DB_OBJ *));
static void api_c_data __P((void));
static void api_c_db __P((void));
static void api_c_env __P((void));
static void api_c_header __P((void));
static void api_c_public_data __P((void));
static void api_c_public_func __P((void));
static void api_c_shutdown __P((void));

int
api_c(ofname)
	char *ofname;
{
	DB_OBJ *cur_db;
	ENV_OBJ *cur_env;

	/* Open the output file. */
	if (ofname == NULL)
		ofname = "application.c";
	if ((of = fopen(ofname, "w")) == NULL) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, ofname, strerror(errno));
		return (1);
	}

	/* File header. */
	api_c_header();

	/* Public information for the application. */
	api_c_public_data();

	/* Secondary callback functions. */
	TAILQ_FOREACH(cur_env, &env_tree, q)
		TAILQ_FOREACH(cur_db, &cur_env->dbq, q)
			if (cur_db->primary != NULL)
				api_c_callback(cur_db);

	/* Integral type comparison functions. */
	TAILQ_FOREACH(cur_env, &env_tree, q)
		TAILQ_FOREACH(cur_db, &cur_env->dbq, q)
			if (cur_db->key_type != NULL)
				api_c_compare(cur_db);

	/* Data structures. */
	api_c_data();

	/* Initialization functions. */
	api_c_public_func();

	/* Open the environments. */
	api_c_env();

	/* Open the databases. */
	api_c_db();

	/* Shutdown gracefully. */
	api_c_shutdown();

	return (0);
}

static void
api_c_header()
{
	/* Include files. */
	fprintf(of, "\
#include <sys/types.h>\n\
#include <sys/stat.h>\n\
\n\
#include <errno.h>\n\
#include <stdlib.h>\n\
#include <string.h>\n\
\n\
#ifdef _WIN32\n\
#include <direct.h>\n\
\n\
#define\tmkdir(dir, perm)\t_mkdir(dir)\n\
#endif\n\
\n\
#include \"db.h\"\n\n");
}

static void
api_c_public_data()
{
	ENV_OBJ *cur_env;
	DB_OBJ *cur_db;

	/*
	 * Global handles.
	 *
	 * XXX
	 * Maybe we should put these all into a structure with some
	 * access methods?
	 */
	fprintf(of, "\
/* Global environment and database handles for use by the application */\n");
	TAILQ_FOREACH(cur_env, &env_tree, q) {
		if (!cur_env->standalone)
			fprintf(of,
	    "DB_ENV\t*%s_dbenv;\t\t\t/* Database environment handle */\n",
			    cur_env->prefix);
		TAILQ_FOREACH(cur_db, &cur_env->dbq, q)
			if (cur_env->standalone)
				fprintf(of,
				    "DB\t*%s;\t\t\t/* Database handle */\n",
				    cur_db->name);
			else
				fprintf(of,
				    "DB\t*%s_%s;\t\t\t/* Database handle */\n",
				    cur_env->prefix, cur_db->name);
	}

	fprintf(of, "\
\n\
/* Public functions for use by the application */\n\
int bdb_startup(void);\n\
int bdb_shutdown(void);\n");
}

static void
api_c_data()
{
	DB_OBJ *cur_db;
	ENV_OBJ *cur_env;
	int first;

	fprintf(of, "\
\n\
/* DB_ENV initialization structures */\n\
typedef struct {\n\
\tDB_ENV **envpp;\n\
\tchar *home;\n\
\tu_int32_t gbytes;\n\
\tu_int32_t bytes;\n\
\tu_int32_t ncache;\n\
\tint private;\n\
\tint transaction;\n\
} env_list_t;\n\
static env_list_t env_list[] = {\n");

	first = 1;
	TAILQ_FOREACH(cur_env, &env_tree, q)
		if (!cur_env->standalone) {
			fprintf(of,
		"%s\t{ &%s_dbenv, \"%s\", %lu, %lu, %lu, %d, %d",
			    first ? "" : " },\n",
			    cur_env->prefix,
			    cur_env->home,
			    (u_long)cur_env->gbytes,
			    (u_long)cur_env->bytes,
			    (u_long)cur_env->ncache,
			    cur_env->private,
			    cur_env->transaction);
			first = 0;
		}
	fprintf(of, " }\n};\n\n");

	fprintf(of, "\
/* DB initialization structures */\n\
typedef struct db_list_t {\n\
\tDB_ENV **envpp;\n\
\tDB **dbpp;\n\
\tchar *name;\n\
\tDBTYPE type;\n\
\tu_int32_t extentsize;\n\
\tu_int32_t pagesize;\n\
\tu_int32_t re_len;\n\
\tint (*key_compare)(DB *, const DBT *, const DBT *);\n\
\tDB **primaryp;\n\
\tint (*secondary_callback)(DB *, const DBT *, const DBT *, DBT *);\n\
\tint dupsort;\n\
\tint recnum;\n\
\tint transaction;\n\
} db_list_t;\n\
static db_list_t db_list[] = {\n");

	first = 1;
	TAILQ_FOREACH(cur_env, &env_tree, q)
		TAILQ_FOREACH(cur_db, &cur_env->dbq, q) {
			fprintf(of, "\
%s\t{ %s%s%s, &%s%s%s, \"%s\", %s, %lu, %lu, %lu,\n\
\t\t%s%s%s, %s%s%s%s, %s%s%s, %d, %d, %d",
			    first ? "" : " },\n",
			    cur_env->standalone ? "" : "&",
			    cur_env->standalone ? "NULL" : cur_env->prefix,
			    cur_env->standalone ? "" : "_dbenv",
			    cur_env->prefix == NULL ?
				cur_db->name : cur_env->prefix,
			    cur_env->prefix == NULL ? "" : "_",
			    cur_env->prefix == NULL ? "" : cur_db->name,
			    cur_db->name,
			    cur_db->dbtype,
			    (u_long)cur_db->extentsize,
			    (u_long)cur_db->pagesize,
			    (u_long)cur_db->re_len,
			    cur_db->key_type == NULL ? "NULL" : "bdb_",
			    cur_db->key_type == NULL ? "" : cur_db->key_type,
			    cur_db->key_type == NULL ? "" : "_compare",
			    cur_db->primary == NULL ? "NULL" : "&",
			    cur_db->primary == NULL ? "" : cur_env->prefix,
			    cur_db->primary == NULL ? "" : "_",
			    cur_db->primary == NULL ? "" : cur_db->primary,
			    cur_db->primary == NULL ? "NULL" : "bdb_",
			    cur_db->primary == NULL ? "" : cur_db->name,
			    cur_db->primary == NULL ? "" : "_callback",
			    cur_db->dupsort,
			    cur_db->recnum,
			    cur_db->transaction);
			first = 0;
		}
	fprintf(of, " }\n};\n\n");
}

static void
api_c_public_func()
{
	fprintf(of, "\
#ifdef BUILD_STANDALONE\n\
int\n\
main()\n\
{\n\
\treturn (bdb_startup() && bdb_shutdown() ? EXIT_FAILURE : EXIT_SUCCESS);\n\
}\n\
#endif\n\n");

	/* Function prototypes. */
	fprintf(of, "\
static int bdb_env_startup(env_list_t *);\n\
static int bdb_env_shutdown(env_list_t *);\n\
static int bdb_db_startup(db_list_t *);\n\
static int bdb_db_shutdown(db_list_t *);\n\
\n");

	fprintf(of, "\
/*\n\
 * bdb_startup --\n\
 *\tStart up the environments and databases.\n\
 */\n\
int\nbdb_startup()\n{\n\
\tu_int i;\n\
\n\
\t/* Open environments. */\n\
\tfor (i = 0; i < sizeof(env_list) / sizeof(env_list[0]); ++i)\n\
\t\tif (bdb_env_startup(&env_list[i]))\n\
\t\t\treturn (1);\n\
\t/* Open primary databases. */\n\
\tfor (i = 0; i < sizeof(db_list) / sizeof(db_list[0]); ++i)\n\
\t\tif (db_list[i].primaryp == NULL &&\n\
\t\t    bdb_db_startup(&db_list[i]))\n\
\t\t\treturn (1);\n\
\t/* Open secondary databases. */\n\
\tfor (i = 0; i < sizeof(db_list) / sizeof(db_list[0]); ++i)\n\
\t\tif (db_list[i].primaryp != NULL &&\n\
\t\t    bdb_db_startup(&db_list[i]))\n\
\t\t\treturn (1);\n\
\treturn (0);\n\
}\n");

	fprintf(of, "\
\n\
/*\n\
 * bdb_shutdown --\n\
 *\tShut down the environments and databases.\n\
 */\n\
int\nbdb_shutdown()\n{\n\
\tu_int i;\n\
\n\
\t/* Close secondary databases. */\n\
\tfor (i = 0; i < sizeof(db_list) / sizeof(db_list[0]); ++i)\n\
\t\tif (db_list[i].primaryp != NULL &&\n\
\t\t    bdb_db_shutdown(&db_list[i]))\n\
\t\t\treturn (1);\n\
\t/* Close primary databases. */\n\
\tfor (i = 0; i < sizeof(db_list) / sizeof(db_list[0]); ++i)\n\
\t\tif (db_list[i].primaryp == NULL &&\n\
\t\t    bdb_db_shutdown(&db_list[i]))\n\
\t\t\treturn (1);\n\
\t/* Close environments. */\n\
\tfor (i = 0; i < sizeof(env_list) / sizeof(env_list[0]); ++i)\n\
\t\tif (bdb_env_shutdown(&env_list[i]))\n\
\t\t\treturn (1);\n\
\treturn (0);\n\
}\n");
}

static void
api_c_env()
{
	fprintf(of, "\
\n\
static int\nbdb_env_startup(env_list_t *ep)\n{\n\
\tstruct stat sb;\n\
\tDB_ENV *dbenv;\n\
\tu_int32_t open_flags;\n\
\tint ret;\n\
\n\
\t/*\n\
\t * If the directory doesn't exist, create it with permissions limited\n\
\t * to the owner.  Assume errors caused by the directory not existing;\n\
\t * we'd like to avoid interpreting system errors and it won't hurt to\n\
\t * attempt to create an existing directory.\n\
\t *\n\
\t * !!!\n\
\t * We use octal for the permissions, nothing else is portable.\n\
\t */\n\
\tif (stat(ep->home, &sb) != 0)\n\
\t\t(void)mkdir(ep->home,  0700);\n\
\n\
\t/*\n\
\t * If the environment is not transactional, remove and re-create it.\n\
\t */\n\
\tif (!ep->transaction) {\n\
\t\tif ((ret = db_env_create(&dbenv, 0)) != 0) {\n\
\t\t\tfprintf(stderr, \"db_env_create: %%s\", db_strerror(ret));\n\
\t\t\treturn (1);\n\
\t\t}\n\
\t\tif ((ret = dbenv->remove(dbenv, ep->home, DB_FORCE)) != 0) {\n\
\t\t\tdbenv->err(dbenv, ret,\n\
\t\t\t    \"DB_ENV->remove: %%s\", ep->home);\n\
\t\t\tgoto err;\n\
\t\t}\n\
\t}\n\n");

	fprintf(of, "\
\t/*\n\
\t * Create the DB_ENV handle and initialize error reporting.\n\
\t */\n\
\tif ((ret = db_env_create(&dbenv, 0)) != 0) {\n\
\t\tfprintf(stderr, \"db_env_create: %%s\", db_strerror(ret));\n\
\t\treturn (1);\n\
\t}\n");

	fprintf(of, "\
\tdbenv->set_errpfx(dbenv, ep->home);\n\
\tdbenv->set_errfile(dbenv, stderr);\n\n");

	fprintf(of, "\
\t /* Configure the cache size. */\n\
\tif ((ep->gbytes != 0 || ep->bytes != 0) &&\n\
\t    (ret = dbenv->set_cachesize(dbenv,\n\
\t    ep->gbytes, ep->bytes, ep->ncache)) != 0) {\n\
\t\tdbenv->err(dbenv, ret, \"DB_ENV->set_cachesize\");\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\t/*\n\
\t * Open the environment.\n\
\t */\n\
\topen_flags = DB_CREATE | DB_INIT_MPOOL | DB_THREAD;\n\
\tif (ep->private)\n\
\t	open_flags |= DB_PRIVATE;\n\
\tif (ep->transaction)\n\
\t	open_flags |= DB_INIT_LOCK |\n\
\t	    DB_INIT_LOG | DB_INIT_TXN | DB_RECOVER;\n\
\tif ((ret = dbenv->open(dbenv, ep->home, open_flags, 0)) != 0) {\n\
\t	dbenv->err(dbenv, ret, \"DB_ENV->open: %%s\",  ep->home);\n\
\t	goto err;\n\
\t}\n\
\n\
\t*ep->envpp = dbenv;\n\
\treturn (0);\n\
\n\
err:\t(void)dbenv->close(dbenv, 0);\n\
\treturn (1);\n\
}");
}

static void
api_c_db()
{
	fprintf(of, "\
\n\
\nstatic int\nbdb_db_startup(db_list_t *dp)\n\
{\n\
\tDB_ENV *dbenv;\n\
\tDB *dbp;\n\
\tint ret;\n\
\n\
\tdbenv = dp->envpp == NULL ? NULL : *dp->envpp;\n\
\n\
\t/*\n\
\t * If the database is not transactional, remove it and re-create it.\n\
\t */\n\
\tif (!dp->transaction) {\n\
\t\tif ((ret = db_create(&dbp, dbenv, 0)) != 0) {\n\
\t\t\tif (dbenv == NULL)\n\
\t\t\t\tfprintf(stderr,\n\
\t\t\t\t    \"db_create: %%s\\n\", db_strerror(ret));\n\
\t\t\telse\n\
\t\t\t\tdbenv->err(dbenv, ret, \"db_create\");\n\
\t\t\treturn (1);\n\
\t\t}\n\
\t\tif ((ret = dbp->remove(\n\
\t\t    dbp, dp->name, NULL, 0)) != 0 && ret != ENOENT) {\n\
\t\t\tif (dbenv == NULL)\n\
\t\t\t\tfprintf(stderr,\n\
\t\t\t\t    \"DB->remove: %%s: %%s\\n\",\n\
\t\t\t\t    dp->name, db_strerror(ret));\n\
\t\t\telse\n\
\t\t\t\tdbenv->err(\n\
\t\t\t\t    dbenv, ret, \"DB->remove: %%s\", dp->name);\n\
\t\t\treturn (1);\n\
\t\t}\n\
\t}\n");

	fprintf(of, "\
\n\
\tif ((ret = db_create(&dbp, dbenv, 0)) != 0) {\n\
\t\tif (dbenv == NULL)\n\
\t\t\tfprintf(stderr, \"db_create: %%s\\n\", db_strerror(ret));\n\
\t\telse\n\
\t\t\tdbenv->err(dbenv, ret, \"db_create\");\n\
\t\treturn (1);\n\
\t}\n\
\tif (dbenv == NULL) {\n\
\t\tdbp->set_errpfx(dbp, dp->name);\n\
\t\tdbp->set_errfile(dbp, stderr);\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->dupsort && (ret = dbp->set_flags(dbp, DB_DUPSORT)) != 0) {\n\
\t\tdbp->err(dbp, ret, \"DB->set_flags: DB_DUPSORT: %%s\", dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->recnum && (ret = dbp->set_flags(dbp, DB_RECNUM)) != 0) {\n\
\t\tdbp->err(dbp, ret, \"DB->set_flags: DB_RECNUM: %%s\", dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->extentsize != 0 &&\n\
\t    (ret = dbp->set_q_extentsize(dbp, dp->extentsize)) != 0) {\n\
\t\tdbp->err(dbp, ret,\n\
\t\t    \"DB->set_q_extentsize: %%lu: %%s\",\n\
\t\t    (u_long)dp->extentsize, dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->pagesize != 0 &&\n\
\t    (ret = dbp->set_pagesize(dbp, dp->pagesize)) != 0) {\n\
\t\tdbp->err(dbp, ret,\n\
\t\t    \"DB->set_pagesize: %%lu: %%s\",\n\
\t\t    (u_long)dp->pagesize, dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->re_len != 0 &&\n\
\t    (ret = dbp->set_re_len(dbp, dp->re_len)) != 0) {\n\
\t\tdbp->err(dbp, ret,\n\
\t\t    \"DB->set_re_len: %%lu: %%s\",\n\
\t\t    (u_long)dp->re_len, dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->key_compare != NULL &&\n\
\t    (ret = dbp->set_bt_compare(dbp, dp->key_compare)) != 0) {\n\
\t\tdbp->err(dbp, ret, \"DB->set_bt_compare\");\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif ((ret = dbp->open(dbp, NULL, dp->name, NULL, dp->type,\n\
\t    (dp->transaction ? DB_AUTO_COMMIT : 0) |\n\
\t    DB_CREATE | DB_THREAD, 0)) != 0) {\n\
\t\tdbp->err(dbp, ret, \"DB->open: %%s\", dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\tif (dp->primaryp != NULL &&\n\
\t    (ret = dbp->associate(*dp->primaryp,\n\
\t    NULL, dbp, dp->secondary_callback, DB_CREATE)) != 0) {\n\
\t\tdbp->err(dbp, ret, \"DB->associate: %%s\", dp->name);\n\
\t\tgoto err;\n\
\t}\n");

	fprintf(of, "\
\n\
\t*dp->dbpp = dbp;\n\
\treturn (0);\n\
\nerr:\t(void)dbp->close(dbp, 0);\n\
\treturn (1);\n\
}\n");
}

static void
api_c_callback(cur_db)
	DB_OBJ *cur_db;
{
	fprintf(of, "\
\n\
static int\nbdb_%s_callback(DB *secondary, const DBT *key, const DBT *data , DBT *result)\n\
{\n", cur_db->name);

	if (cur_db->custom) {
		fprintf(of, "\
\tsecondary->errx(secondary,\n\
\t    \"%s: missing callback comparison function\");\n\
\treturn (DB_DONOTINDEX);\n\
}\n", cur_db->name);
		fprintf(stderr,
    "Warning: you must write a comparison function for the %s database\n",
		    cur_db->name);
	} else
		fprintf(of, "\
\tresult->data = &((u_int8_t *)data->data)[%d];\n\
\tresult->size = %d;\n\
\treturn (0);\n\
}\n", cur_db->secondary_off, cur_db->secondary_len);
}

static void
api_c_compare(cur_db)
	DB_OBJ *cur_db;
{
	DB_OBJ *t_db;
	ENV_OBJ *t_env;

	/*
	 * Check to see if we've already written this one.
	 *
	 * !!!
	 * N^2, but like I care.
	 */
	TAILQ_FOREACH(t_env, &env_tree, q)
		TAILQ_FOREACH(t_db, &t_env->dbq, q) {
			if (t_db == cur_db)
				goto output;
			if (t_db->key_type == NULL)
				continue;
			if (strcasecmp(t_db->key_type, cur_db->key_type) == 0)
				return;
		}

	/* NOTREACHED */
	return;

output:	fprintf(of, "\
\n\
static int bdb_%s_compare(DB *, const DBT *, const DBT *);\n\
\n\
static int\nbdb_%s_compare(DB *dbp, const DBT *a, const DBT *b)\n\
{\n\
\t%s ai, bi;\n\
\n\
\tmemcpy(&ai, a->data, sizeof(ai));\n\
\tmemcpy(&bi, b->data, sizeof(bi));\n\
\treturn (ai < bi ? -1 : (ai > bi ? 1 : 0));\n\
}\n", cur_db->key_type, cur_db->key_type, cur_db->key_type);
}

static void
api_c_shutdown()
{
	fprintf(of, "\
\n\
static int\nbdb_env_shutdown(env_list_t *ep)\n\
{\n\
\tDB_ENV *dbenv;\n\
\tint ret;\n\
\n\
\tdbenv = ep->envpp == NULL ? NULL : *ep->envpp;\n\
\tret = 0;\n\
\n\
\tif (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0)\n\
\t\tfprintf(stderr,\n\
\t\t    \"DB_ENV->close: %%s: %%s\\n\", ep->home, db_strerror(ret));\n\
\treturn (ret == 0 ? 0 : 1);\n\
}\n");

	fprintf(of, "\
\n\
static int\nbdb_db_shutdown(db_list_t *dp)\n\
{\n\
\tDB_ENV *dbenv;\n\
\tDB *dbp;\n\
\tint ret;\n\
\n\
\tdbenv = dp->envpp == NULL ? NULL : *dp->envpp;\n\
\tdbp = *dp->dbpp;\n\
\tret = 0;\n\
\n\
\t/*\n\
\t * If the database is transactionally protected, close without writing;\n\
\t * dirty pages; otherwise, flush dirty pages to disk.\n\
\t */\n\
\tif (dbp != NULL &&\n\
\t    (ret = dbp->close(dbp, dp->transaction ? DB_NOSYNC : 0)) != 0) {\n\
\t\tif (dbenv == NULL)\n\
\t\t\tfprintf(stderr,\n\
\t\t\t    \"DB->close: %%s: %%s\\n\", dp->name, db_strerror(ret));\n\
\t\telse\n\
\t\t\tdbenv->err(dbenv, ret, \"DB->close: %%s\", dp->name);\n\
\t}\n\
\treturn (ret == 0 ? 0 : 1);\n\
}\n");
}
