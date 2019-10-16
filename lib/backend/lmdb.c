/** \ingroup rpmdb
 * \file lib/lmdb.c
 */

#include "system.h"

#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#include <popt.h>
#include <lmdb.h>
#include <signal.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "lib/rpmdb_internal.h"

#include "debug.h"

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

struct dbiCursor_s {
    dbiIndex dbi;
    const void *key;
    unsigned int keylen;
    int flags;
    MDB_cursor * cursor;
    MDB_txn * txn;
};

static const char * _EnvF(unsigned eflags)
{
    static char t[256];
    char *te = t;

    *te = '\0';
#define	_EF(_v) if (eflags & MDB_##_v) te = stpcpy(stpcpy(te,"|"),#_v)
    _EF(FIXEDMAP);
    _EF(NOSUBDIR);
    _EF(NOSYNC);
    _EF(RDONLY);
    _EF(NOMETASYNC);
    _EF(WRITEMAP);
    _EF(MAPASYNC);
    _EF(NOTLS);
    _EF(NOLOCK);
    _EF(NORDAHEAD);
    _EF(NOMEMINIT);
#undef	_EF
    if (t[0] == '\0') te += sprintf(te, "|0x%x", eflags);
    *te = '\0';
    return t+1;
}

static const char * _OpenF(unsigned oflags)
{
    static char t[256];
    char *te = t;

    *te = '\0';
#define	_OF(_v) if (oflags & MDB_##_v) te = stpcpy(stpcpy(te,"|"),#_v)
    _OF(REVERSEKEY);
    _OF(DUPSORT);
    _OF(INTEGERKEY);
    _OF(DUPFIXED);
    _OF(INTEGERDUP);
    _OF(REVERSEDUP);
    _OF(CREATE);
#undef	_OF
    if (t[0] == '\0') te += sprintf(te, "|0x%x", oflags);
    *te = '\0';
    return t+1;
}

static int dbapi_err(rpmdb rdb, const char * msg, int rc, int printit)
{
    if (printit && rc) {
	int lvl = RPMLOG_ERR;
	if (msg)
	    rpmlog(lvl, _("%s:\trc(%d) = %s(): %s\n"),
		rdb->db_descr, rc, msg, (rc ? mdb_strerror(rc) : ""));
	else
	    rpmlog(lvl, _("%s:\trc(%d) = %s()\n"),
		rdb->db_descr, rc, (rc ? mdb_strerror(rc) : ""));
    }
    return rc;
}

static int cvtdberr(dbiIndex dbi, const char * msg, int rc, int printit)
{
    return dbapi_err(dbi->dbi_rpmdb, msg, rc, printit);
}

static void lmdb_assert(MDB_env *env, const char *msg)
{
    rpmlog(RPMLOG_ERR, "%s: %s\n", __FUNCTION__, msg);
}

static void lmdb_dbSetFSync(rpmdb rdb, int enable)
{
}

static int lmdb_Ctrl(rpmdb rdb, dbCtrlOp ctrl)
{
    return 0;
}

static int db_fini(rpmdb rdb, const char * dbhome)
{
    int rc = 0;
    MDB_env * env = rdb->db_dbenv;

    if (env == NULL)
	goto exit;
    if (--rdb->db_opens > 0)
	goto exit;

    mdb_env_close(env);
    rdb->db_dbenv = env = NULL;

    rpmlog(RPMLOG_DEBUG, "closed   db environment %s\n", dbhome);

exit:
    return rc;
}

static int db_init(rpmdb rdb, const char * dbhome)
{
    int rc = EINVAL;
    MDB_env * env = NULL;
    int retry_open = 2;
    uint32_t eflags = 0;

    if (rdb->db_dbenv != NULL) {
	rdb->db_opens++;
	return 0;
    } else {
	/* On first call, set backend description to something... */
	free(rdb->db_descr);
	rdb->db_descr = xstrdup("lmdb");
    }

    MDB_dbi maxdbs = 32;
    unsigned int maxreaders = 16;
    size_t mapsize = 1024 * 1024 * 1024;

    if ((rc = mdb_env_create(&env))
     || (rc = mdb_env_set_maxreaders(env, maxreaders))
     || (rc = mdb_env_set_mapsize(env, mapsize))
     || (rc = mdb_env_set_maxdbs(env, maxdbs))
     || (rc = mdb_env_set_assert(env, lmdb_assert))
     || (rc = mdb_env_set_userctx(env, rdb))
    ) {
	rc = dbapi_err(rdb, "mdb_env_create", rc, _debug);
	goto exit;
    }

    /*
     * Actually open the environment. Fall back to private environment
     * if we dont have permission to join/create shared environment or
     * system doesn't support it..
     */
    while (retry_open) {
	rpmlog(RPMLOG_DEBUG, "opening  db environment %s eflags=%s perms=0%o\n", dbhome, _EnvF(eflags), rdb->db_perms);

	eflags = 0;
	eflags |= MDB_WRITEMAP;
	eflags |= MDB_MAPASYNC;
	eflags |= MDB_NOTLS;

	if (access(dbhome, W_OK) && (rdb->db_mode & O_ACCMODE) == O_RDONLY)
	    eflags |= MDB_RDONLY;

	rc = mdb_env_open(env, dbhome, eflags, rdb->db_perms);
	if (rc) {
	    rc = dbapi_err(rdb, "mdb_env_open", rc, _debug);
	    if (rc == EPERM)
		rpmlog(RPMLOG_ERR, "lmdb: %s(%s/lock.mdb): %s\n", __FUNCTION__, dbhome, mdb_strerror(rc));
	}
	retry_open = 0;		/* XXX EAGAIN might need a retry */
    }
    if (rc)
	goto exit;

    rdb->db_dbenv = env;
    rdb->db_opens = 1;

exit:
    if (rc && env) {
	mdb_env_close(env);
	rdb->db_dbenv = env = NULL;
    }
    return rc;
}

static int dbiSync(dbiIndex dbi, unsigned int flags)
{
    int rc = 0;
    MDB_dbi db = (unsigned long) dbi->dbi_db;

    if (db != 0xdeadbeef && !dbi->cfg.dbi_no_dbsync) {
	MDB_env * env = dbi->dbi_rpmdb->db_dbenv;
	unsigned eflags = 0;
	rc = mdb_env_get_flags(env, &eflags);
	if (rc) {
	    rc = cvtdberr(dbi, "mdb_env_get_flags", rc, _debug);
	    eflags |= MDB_RDONLY;
	}
	if (!(eflags & MDB_RDONLY)) {
	    int force = 0;
	    rc = mdb_env_sync(env, force);
	    if (rc)
		rc = cvtdberr(dbi, "mdb_env_sync", rc, _debug);
	}
    }
    return rc;
}

static dbiCursor lmdb_dbiCursorInit(dbiIndex dbi, unsigned int flags)
{
    dbiCursor dbc = NULL;

    if (dbi && dbi->dbi_db != (void *)0xdeadbeefUL) {
	MDB_env * env = dbi->dbi_rpmdb->db_dbenv;
	MDB_txn * parent = NULL;
	unsigned tflags = !(flags & DBC_WRITE) ? MDB_RDONLY : 0;
	MDB_txn * txn = NULL;
	MDB_cursor * cursor = NULL;
	int rc = EINVAL;

	rc = mdb_txn_begin(env, parent, tflags, &txn);
	if (rc)
	    rc = cvtdberr(dbi, "mdb_txn_begin", rc, _debug);

	if (rc == 0) {
	    MDB_dbi db = (unsigned long) dbi->dbi_db;
	    rc = mdb_cursor_open(txn, db, &cursor);
	    if (rc)
		rc = cvtdberr(dbi, "mdb_cursor_open", rc, _debug);
	}

	if (rc == 0) {
	    dbc = xcalloc(1, sizeof(*dbc));
	    dbc->dbi = dbi;
	    dbc->flags = flags;
	    dbc->cursor = cursor;
	    dbc->txn = txn;
	}
    }

    return dbc;
}

static dbiCursor lmdb_dbiCursorFree(dbiIndex dbi, dbiCursor dbc)
{
    if (dbc) {
	int rc = 0;
	MDB_cursor * cursor = dbc->cursor;
	MDB_txn * txn = dbc->txn;
	dbiIndex dbi = dbc->dbi;
	unsigned flags = dbc->flags;

	mdb_cursor_close(cursor);
	dbc->cursor = cursor = NULL;
	if (rc)
	    cvtdberr(dbc->dbi, "mdb_cursor_close", rc, _debug);

	/* Automatically commit close */
	if (txn) {
	    rc = mdb_txn_commit(txn);
	    dbc->txn = txn = NULL;
	    if (rc)
		rc = cvtdberr(dbc->dbi, "mdb_txn_commit", rc, _debug);
	}

	/* Automatically sync on write-cursor close */
	if (flags & DBC_WRITE)
	    dbiSync(dbi, 0);

	free(dbc);
	rc = 0;
    }
    return NULL;
}

static int dbiCursorPut(dbiCursor dbc, MDB_val * key, MDB_val * data, unsigned flags)
{
    int rc = EINVAL;
    int sane = (key->mv_data != NULL && key->mv_size > 0 &&
		data->mv_data != NULL && data->mv_size > 0);

    if (dbc && sane) {
	MDB_cursor * cursor = dbc->cursor;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;
	rpmswEnter(&rdb->db_putops, (ssize_t) 0);

	rc = mdb_cursor_put(cursor, key, data, flags);
	if (rc) {
	    rc = cvtdberr(dbc->dbi, "mdb_cursor_put", rc, _debug);
	    if (dbc->txn) {
		mdb_txn_abort(dbc->txn);
		dbc->txn = NULL;
	    }
	}

	rpmswExit(&rdb->db_putops, (ssize_t) data->mv_size);
    }
    return rc;
}

static int dbiCursorGet(dbiCursor dbc, MDB_val *key, MDB_val *data, unsigned op)
{
    int rc = EINVAL;
    int sane = ((op == MDB_NEXT) || (key->mv_data != NULL && key->mv_size > 0));

    if (dbc && sane) {
	MDB_cursor * cursor = dbc->cursor;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;

	rpmswEnter(&rdb->db_getops, 0);

	/* XXX db4 does DB_FIRST on uninitialized cursor */
	rc = mdb_cursor_get(cursor, key, data, op);
	if (rc && rc != MDB_NOTFOUND) {
	    rc = cvtdberr(dbc->dbi, "mdb_cursor_get", rc, _debug);
	    if (dbc->txn) {
		mdb_txn_abort(dbc->txn);
		dbc->txn = NULL;
	    }
	}

	/* Remember the last key fetched */
	if (rc == 0) {
	    dbc->key = key->mv_data;
	    dbc->keylen = key->mv_size;
	} else {
	    dbc->key = NULL;
	    dbc->keylen = 0;
	}

	rpmswExit(&rdb->db_getops, data->mv_size);
    }
    return rc;
}

static int dbiCursorDel(dbiCursor dbc, MDB_val *key, MDB_val *data, unsigned int flags)
{
    int rc = EINVAL;
    int sane = (key->mv_data != NULL && key->mv_size > 0);

    if (dbc && sane) {
	MDB_cursor * cursor = dbc->cursor;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;
	rpmswEnter(&rdb->db_delops, 0);

	/* XXX TODO: ensure that cursor is positioned with duplicates */
	rc = mdb_cursor_get(cursor, key, data, MDB_SET);
	if (rc && rc != MDB_NOTFOUND) {
	    rc = cvtdberr(dbc->dbi, "mdb_cursor_get", rc, _debug);
	    if (dbc->txn)
		dbc->txn = NULL;
	}

	if (rc == 0) {
	    rc = mdb_cursor_del(cursor, flags);
	    if (rc)
		rc = cvtdberr(dbc->dbi, "mdb_cursor_del", rc, _debug);
	}
	rpmswExit(&rdb->db_delops, data->mv_size);
    }
    return rc;
}

static int lmdb_dbiVerify(dbiIndex dbi, unsigned int flags)
{
    return 0;
}

static int lmdb_dbiClose(dbiIndex dbi, unsigned int flags)
{
    int rc = 0;
    rpmdb rdb = dbi->dbi_rpmdb;
    const char * dbhome = rpmdbHome(rdb);
    MDB_dbi db = (unsigned long) dbi->dbi_db;

    if (db != 0xdeadbeef) {
	MDB_env * env = dbi->dbi_rpmdb->db_dbenv;
	mdb_dbi_close(env, db);
	dbi->dbi_db = (void *) 0xdeadbeefUL;

	rpmlog(RPMLOG_DEBUG, "closed   db index       %s/%s\n",
		dbhome, dbi->dbi_file);
    }

    db_fini(rdb, dbhome ? dbhome : "");

    dbi = dbiFree(dbi);

    return rc;
}

static int lmdb_dbiOpen(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
{
    int rc = 1;
    const char *dbhome = rpmdbHome(rdb);
    dbiIndex dbi = NULL;
    int retry_open;

    MDB_dbi db = 0;
    uint32_t oflags;

    if (dbip)
	*dbip = NULL;

    if ((dbi = dbiNew(rdb, rpmtag)) == NULL)
	goto exit;
    dbi->dbi_flags = 0;
    dbi->dbi_db = (void *) 0xdeadbeefUL;

    rc = db_init(rdb, dbhome);

    retry_open = (rc == 0) ? 2 : 0;

    do {
	MDB_env * env = rdb->db_dbenv;
	MDB_txn * parent = NULL;
	unsigned tflags = access(dbhome, W_OK) ? MDB_RDONLY : 0;
	MDB_txn * txn = NULL;

	if (tflags & MDB_RDONLY)
	    dbi->dbi_flags |= DBI_RDONLY;

	rc = mdb_txn_begin(env, parent, tflags, &txn);
	if (rc)
	    rc = cvtdberr(dbi, "mdb_txn_begin", rc, _debug);

	const char * name = dbi->dbi_file;
	oflags = 0;
	if (!(tflags & MDB_RDONLY))
	    oflags |= MDB_CREATE;
	if (!strcmp(dbi->dbi_file, "Packages"))
	    oflags |= MDB_INTEGERKEY;

	rpmlog(RPMLOG_DEBUG, "opening  db index       %s/%s oflags=%s\n",
		dbhome, dbi->dbi_file, _OpenF(oflags));

	db = 0xdeadbeef;
	rc = mdb_dbi_open(txn, name, oflags, &db);
	if (rc && rc != MDB_NOTFOUND) {
	    rc = cvtdberr(dbi, "mdb_dbi_open", rc, _debug);
	    if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
		db = 0xdeadbeef;
	    }
	}

	if (txn) {
	    rc = mdb_txn_commit(txn);
	    if (rc)
		rc = cvtdberr(dbi, "mdb_txn_commit", rc, _debug);
	}
	retry_open = 0;
    } while (--retry_open > 0);

    dbi->dbi_db = (void *) ((unsigned long)db);

    if (!rc && dbip)
	*dbip = dbi;
    else
	(void) dbiClose(dbi, 0);

exit:
    return rc;
}

/* The LMDB btree implementation needs BIGENDIAN primary keys. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static int _dbibyteswapped = 1;
#else
static int _dbibyteswapped = 0;
#endif

/**
 * Convert retrieved data to index set.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @retval setp		(malloc'ed) index set
 * @return		0 on success
 */
static rpmRC dbt2set(dbiIndex dbi, MDB_val * data, dbiIndexSet * setp)
{
    rpmRC rc = RPMRC_FAIL;
    const char * sdbir;
    dbiIndexSet set = NULL;
    unsigned int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	goto exit;

    rc = RPMRC_OK;
    if ((sdbir = data->mv_data) == NULL) {
	*setp = NULL;
	goto exit;
    }

    set = dbiIndexSetNew(data->mv_size / (2 * sizeof(int32_t)));
    set->count = data->mv_size / (2 * sizeof(int32_t));

    for (i = 0; i < set->count; i++) {
	union _dbswap hdrNum, tagNum;

	memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
	sdbir += sizeof(hdrNum.ui);
	memcpy(&tagNum.ui, sdbir, sizeof(tagNum.ui));
	sdbir += sizeof(tagNum.ui);
	if (_dbibyteswapped) {
	    _DBSWAP(hdrNum);
	    _DBSWAP(tagNum);
	}
	set->recs[i].hdrNum = hdrNum.ui;
	set->recs[i].tagNum = tagNum.ui;
    }
    *setp = set;

exit:
    return rc;
}

/**
 * Convert index set to database representation.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @param set		index set
 * @return		0 on success
 */
static rpmRC set2dbt(dbiIndex dbi, MDB_val * data, dbiIndexSet set)
{
    rpmRC rc = RPMRC_FAIL;
    char * tdbir;
    unsigned int i;

    if (dbi == NULL || data == NULL || set == NULL)
	goto exit;

    rc = RPMRC_OK;
    data->mv_size = set->count * (2 * sizeof(int32_t));
    if (data->mv_size == 0) {
	data->mv_data = NULL;
	goto exit;
    }
    tdbir = data->mv_data = xmalloc(data->mv_size);

    for (i = 0; i < set->count; i++) {
	union _dbswap hdrNum, tagNum;

	memset(&hdrNum, 0, sizeof(hdrNum));
	memset(&tagNum, 0, sizeof(tagNum));
	hdrNum.ui = set->recs[i].hdrNum;
	tagNum.ui = set->recs[i].tagNum;
	if (_dbibyteswapped) {
	    _DBSWAP(hdrNum);
	    _DBSWAP(tagNum);
	}
	memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
	tdbir += sizeof(hdrNum.ui);
	memcpy(tdbir, &tagNum.ui, sizeof(tagNum.ui));
	tdbir += sizeof(tagNum.ui);
    }
exit:
    return rc;
}

static rpmRC lmdb_idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
			  dbiIndexSet *set, int searchType)
{
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    if (dbi != NULL && dbc != NULL && set != NULL) {
	int cflags = MDB_NEXT;
	int dbrc;
	MDB_val key = { 0, NULL };
	MDB_val data = { 0, NULL };

	if (keyp) {
	    if (keylen == 0) {		/* XXX "/" fixup */ 
		keyp = "";
		keylen = 1;
	    }
	    key.mv_data = (void *) keyp; /* discards const */
	    key.mv_size = keylen;
	    cflags = searchType == DBC_PREFIX_SEARCH ? MDB_SET_RANGE : MDB_SET;
	}

	for (;;) {
	    dbiIndexSet newset = NULL;
	    dbrc = dbiCursorGet(dbc, &key, &data, cflags);
	    if (dbrc != 0)
		break;
	    if (searchType == DBC_PREFIX_SEARCH &&
		    (key.mv_size < keylen || memcmp(key.mv_data, keyp, keylen) != 0))
		break;
	    dbt2set(dbi, &data, &newset);
	    if (*set == NULL) {
		*set = newset;
	    } else {
		dbiIndexSetAppendSet(*set, newset, 0);
		dbiIndexSetFree(newset);
	    }
	    if (searchType != DBC_PREFIX_SEARCH)
		break;
	    key.mv_data = NULL;
	    key.mv_size = 0;
	    cflags = MDB_NEXT;
	}

	/* fixup result status for prefix search */
	if (searchType == DBC_PREFIX_SEARCH) {
	    if (dbrc == MDB_NOTFOUND && *set != NULL && (*set)->count > 0)
		dbrc = 0;
	    else if (dbrc == 0 && (*set == NULL || (*set)->count == 0))
		dbrc = MDB_NOTFOUND;
	}

	if (dbrc == 0) {
	    rc = RPMRC_OK;
	} else if (dbrc == MDB_NOTFOUND) {
	    rc = RPMRC_NOTFOUND;
	} else {
	    rpmlog(RPMLOG_ERR,
		   _("rc(%d) getting \"%s\" records from %s index: %s\n"),
		   dbrc, keyp ? keyp : "???", dbiName(dbi), mdb_strerror(dbrc));
	}
    }
    return rc;
}

/* Update secondary index. NULL set deletes the key */
static rpmRC updateIndex(dbiCursor dbc, const char *keyp, unsigned int keylen,
			 dbiIndexSet set)
{
    rpmRC rc = RPMRC_FAIL;

    if (dbc && keyp) {
	dbiIndex dbi = dbc->dbi;
	int dbrc;
	MDB_val key = { 0, NULL };
	MDB_val data = { 0, NULL };

	key.mv_data = (void *) keyp; /* discards const */
	key.mv_size = keylen;

	if (set)
	    set2dbt(dbi, &data, set);

	if (dbiIndexSetCount(set) > 0) {
	    dbrc = dbiCursorPut(dbc, &key, &data, 0);
	    if (dbrc) {
		rpmlog(RPMLOG_ERR,
		       _("rc(%d) storing record \"%s\" into %s index: %s\n"),
		       dbrc, (char*)key.mv_data, dbiName(dbi), mdb_strerror(dbrc));
	    }
	    free(data.mv_data);
	} else {
	    dbrc = dbiCursorDel(dbc, &key, &data, 0);
	    if (dbrc) {
		rpmlog(RPMLOG_ERR,
		       _("rc(%d) removing record \"%s\" from %s index: %s\n"),
		       dbrc, (char*)key.mv_data, dbiName(dbi), mdb_strerror(dbrc));
	    }
	}

	if (dbrc == 0)
	    rc = RPMRC_OK;
    }

    return rc;
}

static rpmRC lmdb_idxdbPut(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
	       dbiIndexItem rec)
{
    dbiIndexSet set = NULL;
    rpmRC rc;

    if (keyp && keylen == 0) {		/* XXX "/" fixup */
	keyp = "";
	keylen++;
    }
    rc = idxdbGet(dbi, dbc, keyp, keylen, &set, DBC_NORMAL_SEARCH);

    /* Not found means a new key and is not an error. */
    if (rc && rc != RPMRC_NOTFOUND)
	goto exit;

    if (set == NULL)
	set = dbiIndexSetNew(1);
    dbiIndexSetAppend(set, rec, 1, 0);

    rc = updateIndex(dbc, keyp, keylen, set);

    dbiIndexSetFree(set);

exit:
    return rc;
}

static rpmRC lmdb_idxdbDel(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
	       dbiIndexItem rec)
{
    rpmRC rc = RPMRC_FAIL;
    dbiIndexSet set = NULL;

    if (keyp && keylen == 0) {		/* XXX "/" fixup */
	keyp = "";
	keylen++;
    }
    rc = idxdbGet(dbi, dbc, keyp, keylen, &set, DBC_NORMAL_SEARCH);
    if (rc)
	goto exit;

    if (dbiIndexSetPrune(set, rec, 1, 1)) {
	/* Nothing was pruned. XXX: Can this actually happen? */
	rc = RPMRC_OK;
    } else {
	/* If there's data left, update data. Otherwise delete the key. */
	if (dbiIndexSetCount(set) > 0) {
	    rc = updateIndex(dbc, keyp, keylen, set);
	} else {
	    rc = updateIndex(dbc, keyp, keylen, NULL);
	}
    };
    dbiIndexSetFree(set);

exit:
    return rc;
}

static const void * lmdb_idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    const void *key = NULL;
    if (dbc) {
	key = dbc->key;
	if (key && keylen)
	    *keylen = dbc->keylen;
    }
    return key;
}

/* Update primary Packages index. NULL hdr means remove */
static rpmRC updatePackages(dbiCursor dbc, unsigned int hdrNum, MDB_val *hdr)
{
    int rc = RPMRC_FAIL;
    int dbrc = EINVAL;

    if (dbc == NULL || hdrNum == 0)
	goto exit;

    union _dbswap mi_offset;
    mi_offset.ui = hdrNum;
    if (_dbibyteswapped)
	_DBSWAP(mi_offset);

    MDB_val key = { 0, NULL };
    key.mv_data = (void *) &mi_offset;
    key.mv_size = sizeof(mi_offset.ui);

    MDB_val data = { 0, NULL };

    dbrc = dbiCursorGet(dbc, &key, &data, MDB_SET);
    if (dbrc && dbrc != MDB_NOTFOUND) {
	rpmlog(RPMLOG_ERR,
		   _("rc(%d) positioning header #%d record: %s\n"), dbrc, hdrNum, mdb_strerror(dbrc));
	goto exit;
    }

    if (hdr) {
	dbrc = dbiCursorPut(dbc, &key, hdr, 0);
	if (dbrc) {
	    rpmlog(RPMLOG_ERR,
		   _("rc(%d) adding header #%d record: %s\n"), dbrc, hdrNum, mdb_strerror(dbrc));
	}
    } else {
	dbrc = dbiCursorDel(dbc, &key, &data, 0);
	if (dbrc) {
	    rpmlog(RPMLOG_ERR,
		   _("rc(%d) deleting header #%d record: %s\n"), dbrc, hdrNum, mdb_strerror(dbrc));
	}
    }

exit:
    rc = dbrc == 0 ? RPMRC_OK : RPMRC_FAIL;
    return rc;
}

/* Get current header instance number or try to allocate a new one */
static unsigned int pkgInstance(dbiCursor dbc, int alloc)
{
    unsigned int hdrNum = 0;

    MDB_val key = { 0, NULL };
    MDB_val data = { 0, NULL };
    unsigned int firstkey = 0;
    union _dbswap mi_offset;
    int rc;

    /* Key 0 holds the current largest instance, fetch it */
    key.mv_data = &firstkey;
    key.mv_size = sizeof(firstkey);
    rc = dbiCursorGet(dbc, &key, &data, MDB_SET);

    if (!rc && data.mv_data) {
	memcpy(&mi_offset, data.mv_data, sizeof(mi_offset.ui));
	if (_dbibyteswapped)
	    _DBSWAP(mi_offset);
	hdrNum = mi_offset.ui;
    }

    if (alloc) {
	/* Rather complicated "increment by one", bswapping as needed */
	++hdrNum;
	mi_offset.ui = hdrNum;
	if (_dbibyteswapped)
	    _DBSWAP(mi_offset);
	data.mv_data = &mi_offset;
	data.mv_size = sizeof(mi_offset.ui);

	/* Unless we manage to insert the new instance number, we failed */
	rc = dbiCursorPut(dbc, &key, &data, 0);
	if (rc) {
	    hdrNum = 0;
	    rpmlog(RPMLOG_ERR,
		    _("rc(%d) allocating new package instance: %s\n"), rc, mdb_strerror(rc));
	}
    }

    return hdrNum;
}

static rpmRC lmdb_pkgdbPut(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum,
               unsigned char *hdrBlob, unsigned int hdrLen)
{
    MDB_val hdr;
    hdr.mv_data = hdrBlob;
    hdr.mv_size = hdrLen;
    return updatePackages(dbc, hdrNum, &hdr);
}

static rpmRC lmdb_pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum)
{
    return updatePackages(dbc, hdrNum, NULL);
}

static rpmRC lmdb_pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum,
	     unsigned char **hdrBlob, unsigned int *hdrLen)
{
    union _dbswap mi_offset;
    MDB_val key = { 0, NULL };
    MDB_val data = { 0, NULL };
    rpmRC rc = RPMRC_FAIL;

    if (dbc == NULL)
	goto exit;

    if (hdrNum) {
	mi_offset.ui = hdrNum;
	if (_dbibyteswapped)
	    _DBSWAP(mi_offset);
	key.mv_data = (void *) &mi_offset;
	key.mv_size = sizeof(mi_offset.ui);
    }

    rc = dbiCursorGet(dbc, &key, &data, hdrNum ? MDB_SET : MDB_NEXT);
    if (rc == 0) {
	if (hdrBlob)
	    *hdrBlob = data.mv_data;
	if (hdrLen)
	    *hdrLen = data.mv_size;
	rc = RPMRC_OK;
    } else if (rc == MDB_NOTFOUND)
	rc = RPMRC_NOTFOUND;
    else
	rc = RPMRC_FAIL;
exit:
    return rc;
}

static unsigned int lmdb_pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    union _dbswap mi_offset;

    if (dbc == NULL || dbc->key == NULL)
	return 0;
    memcpy(&mi_offset, dbc->key, sizeof(mi_offset.ui));
    if (_dbibyteswapped)
	_DBSWAP(mi_offset);
    return mi_offset.ui;
}

static rpmRC lmdb_pkgdbNew(dbiIndex dbi, dbiCursor dbc, unsigned int *hdrNum)
{
    unsigned int num;
    rpmRC rc = RPMRC_FAIL;

    if (dbc == NULL)
	goto exit;
    num = pkgInstance(dbc, 1);
    if (num) {
	*hdrNum = num;
	rc = RPMRC_OK;
    }
exit:
    return rc;
}

struct rpmdbOps_s lmdb_dbops = {
    .open   = lmdb_dbiOpen,
    .close  = lmdb_dbiClose,
    .verify = lmdb_dbiVerify,

    .setFSync = lmdb_dbSetFSync,
    .ctrl = lmdb_Ctrl,

    .cursorInit = lmdb_dbiCursorInit,
    .cursorFree = lmdb_dbiCursorFree,

    .pkgdbGet = lmdb_pkgdbGet,
    .pkgdbPut = lmdb_pkgdbPut,
    .pkgdbDel = lmdb_pkgdbDel,
    .pkgdbNew = lmdb_pkgdbNew,
    .pkgdbKey = lmdb_pkgdbKey,

    .idxdbGet = lmdb_idxdbGet,
    .idxdbPut = lmdb_idxdbPut,
    .idxdbDel = lmdb_idxdbDel,
    .idxdbKey = lmdb_idxdbKey
};
