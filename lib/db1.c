#include "system.h"

#ifdef HAVE_DB_185_H

#include <db_185.h>

#define	DB_VERSION_MAJOR	1
#define	DB_VERSION_MINOR	85
#define	DB_VERSION_PATCH	0

#define	_mymemset(_a, _b, _c)

#include <rpmlib.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

static inline DBTYPE dbi_to_dbtype(DBI_TYPE dbitype)
{
    switch(dbitype) {
    case DBI_BTREE:	return DB_BTREE;
    case DBI_HASH:	return DB_HASH;
    case DBI_RECNO:	return DB_RECNO;
    }
    /*@notreached@*/ return DB_HASH;
}

static inline /*@observer@*/ /*@null@*/ DB * GetDB(dbiIndex dbi) {
    return ((DB *)dbi->dbi_db);
}

#if defined(__USE_DB2)
static int db_init(const char *home, int dbflags,
			DB_ENV **dbenvp, DB_INFO **dbinfop)
{
    DB_ENV *dbenv = xcalloc(1, sizeof(*dbenv));
    DB_INFO *dbinfo = xcalloc(1, sizeof(*dbinfo));
    int rc;

    if (dbenvp) *dbenvp = NULL;
    if (dbinfop) *dbinfop = NULL;

    dbenv->db_errfile = stderr;
    dbenv->db_errpfx = "rpmdb";
    dbenv->mp_size = 1024 * 1024;

    rc = db_appinit(home, NULL, dbenv, dbflags);
    if (rc)
	goto errxit;

    dbinfo->db_pagesize = 1024;

    if (dbenvp)
	*dbenvp = dbenv;
    else
	free(dbenv);

    if (dbinfop)
	*dbinfop = dbinfo;
    else
	free(dbinfo);

    return 0;

errxit:
    if (dbenv)	free(dbenv);
    if (dbinfo)	free(dbinfo);
    return rc;
}
#endif

static int db1close(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2)
    DB_ENV * dbenv = (DB_ENV *)dbi->dbi_dbenv;
    DB_INFO * dbinfo = (DB_INFO *)dbi->dbi_dbinfo;
    DBC * dbcursor = (DBC *)dbi->dbi_cursor;

    if (dbcursor) {
	dbcursor->c_close(dbcursor)
	dbi->dbi_cursor = NULL;
    }

    rc = db->close(db, 0);
    dbi->dbi_db = NULL;

    if (dbinfo) {
	free(dbinfo);
	dbi->dbi_dbinfo = NULL;
    }
    if (dbenv) {
	(void) db_appexit(dbenv);
	free(dbenv);
	dbi->dbi_dbenv = NULL;
    }
#else
    rc = db->close(db);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	break;
    }

    return rc;
}

static int db1sync(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2)
    rc = db->sync(db, flags);
#else
    rc = db->sync(db, flags);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	break;
    }

    return rc;
}

static int db1GetFirstKey(dbiIndex dbi, const char ** keyp) {
    DBT key, data;
    DB * db;
    int rc;

    if (dbi == NULL || dbi->dbi_db == NULL)
	return 1;

    db = GetDB(dbi);
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = NULL;
    key.size = 0;

#if defined(__USE_DB2)
    {	DBC * dbcursor = NULL;
	rc = dbp->cursor(dbp, NULL, &dbcursor, 0);
	if (rc == 0)
	    rc = dbc->c_get(dbcp, &key, &data, DB_FIRST)
	if (dbcursor)
	    dbcp->c_close(dbcursor)
    }
#else
    rc = db->seq(db, &key, &data, R_FIRST);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	if (keyp) {
    	    char *k = xmalloc(key.size + 1);
	    memcpy(k, key.data, key.size);
	    k[key.size] = '\0';
	    *keyp = k;
	}
	break;
    }

    return rc;
}

static int db1SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set) {
    DBT key, data;
    DB * db = GetDB(dbi);
    int rc;

    if (set) *set = NULL;
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = (void *)str;
    key.size = strlen(str);
    data.data = NULL;
    data.size = 0;

#if defined(__USE_DB2)
    rc = db->get(db, NULL, &key, &data, 0);
#else
    rc = db->get(db, &key, &data, 0);
#endif

    switch (rc) {
    default:
    case RET_ERROR:	/* -1 */
	rc = -1;
	break;
    case RET_SPECIAL:	/* 1 */
	rc = 1;
	break;
    case RET_SUCCESS:	/* 0 */
	rc = 0;
	if (set) {
	    *set = dbiCreateIndexSet();
	    (*set)->recs = xmalloc(data.size);
	    memcpy((*set)->recs, data.data, data.size);
	    (*set)->count = data.size / sizeof(*(*set)->recs);
	}
	break;
    }
    return rc;
}

/*@-compmempass@*/
static int db1UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
    DBT key;
    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));
    key.data = (void *)str;
    key.size = strlen(str);

    if (set->count) {
	DBT data;

	_mymemset(&data, 0, sizeof(data));
	data.data = set->recs;
	data.size = set->count * sizeof(*(set->recs));

#if defined(__USE_DB2)
	rc = db->put(db, NULL, &key, &data, 0);
#else
	rc = db->put(db, &key, &data, 0);
#endif

	switch (rc) {
	default:
	case RET_ERROR:		/* -1 */
	case RET_SPECIAL:	/* 1 */
	    rc = 1;
	    break;
	case RET_SUCCESS:	/* 0 */
	    rc = 0;
	    break;
	}
    } else {

#if defined(__USE_DB2)
	rc = db->del(db, NULL, &key, 0);
#else
	rc = db->del(db, &key, 0);
#endif

	switch (rc) {
	default:
	case RET_ERROR:		/* -1 */
	case RET_SPECIAL:	/* 1 */
	    rc = 1;
	    break;
	case RET_SUCCESS:	/* 0 */
	    rc = 0;
	    break;
	}
    }

    return rc;
}
/*@=compmempass@*/

static int db1open(dbiIndex dbi)
{
    int rc;

#if defined(__USE_DB2)
    char * dbhome = NULL;
    DB_ENV * dbenv = NULL;
    DB_INFO * dbinfo = NULL;
    u_int32_t dbflags;

    dbflags = (	!(dbi->dbi_flags & O_RDWR) ? DB_RDONLY :
		((dbi->dbi_flags & O_CREAT) ? DB_CREATE : 0));

    rc = db_init(dbhome, dbflags, &dbenv, &dbinfo);
    dbi->dbi_dbenv = dbenv;
    dbi->dbi_dbinfo = dbinfo;

    if (rc == 0)
	rc = db_open(dbi->dbi_file, dbi_to_dbtype(dbi->dbi_type), dbflags,
			dbi->dbi_perms, dbenv, dbinfo, &dbi->dbi_db);

    if (rc)
	dbi->dbi_db = NULL;
#else
    dbi->dbi_db = dbopen(dbi->dbi_file, dbi->dbi_flags, dbi->dbi_perms,
		dbi_to_dbtype(dbi->dbi_type), dbi->dbi_openinfo);
#endif

    if (dbi->dbi_db) {
	rc = 0;
fprintf(stderr, "*** db%dopen: %s\n", dbi->dbi_major, dbi->dbi_file);
    } else {
	rc = 1;
    }

    return rc;
}

struct _dbiVec db1vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db1open, db1close, db1sync, db1GetFirstKey, db1SearchIndex, db1UpdateIndex
};

#endif	/* HABE_DB_185_H */
