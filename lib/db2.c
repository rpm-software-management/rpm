#include "system.h"

#include <db.h>


#include <rpmlib.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#include "db2.h"

#if DB_VERSION_MAJOR == 2
#define	__USE_DB2	1
#define	_mymemset(_a, _b, _c)	memset((_a), (_b), (_c))

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
static int db_init(dbiIndex dbi, const char *home, int dbflags,
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

#define _DBFMASK	(DB_CREATE|DB_NOMMAP|DB_THREAD)
    rc = db_appinit(home, NULL, dbenv, (dbflags & _DBFMASK));
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

int db2open(dbiIndex dbi)
{
    int rc = 0;

#if defined(__USE_DB2)
    char * dbhome = NULL;
    DB * db = NULL;
    DB_ENV * dbenv = NULL;
    DB_INFO * dbinfo = NULL;
    u_int32_t dbflags;

    dbflags = (	!(dbi->dbi_flags & O_RDWR) ? DB_RDONLY :
		((dbi->dbi_flags & O_CREAT) ? DB_CREATE : 0));

    rc = db_init(dbi, dbhome, dbflags, &dbenv, &dbinfo);

    if (rc == 0)
	rc = db_open(dbi->dbi_file, dbi_to_dbtype(dbi->dbi_type), dbflags,
			dbi->dbi_perms, dbenv, dbinfo, &db);

    dbi->dbi_db = db;
    dbi->dbi_dbenv = dbenv;
    dbi->dbi_dbinfo = dbinfo;

#else
    dbi->dbi_db = dbopen(dbi->dbi_file, dbi->dbi_flags, dbi->dbi_perms,
		dbi_to_dbtype(dbi->dbi_type), dbi->dbi_openinfo);
#endif

    if (rc == 0 && dbi->dbi_db != NULL) {
	rc = 0;
	dbi->dbi_major = DB_VERSION_MAJOR;
	dbi->dbi_minor = DB_VERSION_MINOR;
	dbi->dbi_patch = DB_VERSION_PATCH;
    } else
	rc = 1;

    return rc;
}

int db2close(dbiIndex dbi, unsigned int flags) {
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2)
    DB_ENV * dbenv = (DB_ENV *)dbi->dbi_dbenv;
    DB_INFO * dbinfo = (DB_INFO *)dbi->dbi_dbinfo;
    DBC * dbcursor = (DBC *)dbi->dbi_dbcursor;

    if (dbcursor) {
	(void)dbcursor->c_close(dbcursor);
	dbi->dbi_dbcursor = NULL;
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

int db2sync(dbiIndex dbi, unsigned int flags) {
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

int db2GetFirstKey(dbiIndex dbi, const char ** keyp) {
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
	rc = db->cursor(db, NULL, &dbcursor);
	if (rc == 0)
	    rc = dbcursor->c_get(dbcursor, &key, &data, DB_FIRST);
	if (dbcursor)
	    (void)dbcursor->c_close(dbcursor);
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

int db2SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set) {
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
int db2UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
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
#endif	/* DB_VERSION_MAJOR == 2 */
