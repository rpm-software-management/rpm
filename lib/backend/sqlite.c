#include "system.h"

#include <sqlite3.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "lib/rpmdb_internal.h"

#include "debug.h"

#define HASHTYPE stmtHash
#define HTKEYTYPE const char *
#define HTDATATYPE sqlite3_stmt *
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

static const int sleep_ms = 50;

struct dbiCursor_s {
    sqlite3 *sdb;
    sqlite3_stmt *stmt;
    stmtHash stcache;
    const char *fmt;
    int flags;
    rpmTagVal tag;
    int ctype;

    const void *key;
    unsigned int keylen;
};

static rpmRC sqlite_pkgdbNew(dbiIndex dbi, unsigned int *hdrNum);
static int sqlexec(sqlite3 *sdb, const char *fmt, ...);

static int dbiCursorResult(dbiCursor dbc)
{
    int rc = sqlite3_errcode(dbc->sdb);
    int err = (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW);
    if (err) {
	rpmlog(RPMLOG_ERR, "%s: %d: %s\n", sqlite3_sql(dbc->stmt),
		sqlite3_errcode(dbc->sdb), sqlite3_errmsg(dbc->sdb));
    }
    return err ? RPMRC_FAIL : RPMRC_OK;
}

static int dbiCursorReset(dbiCursor dbc)
{
    dbc->stmt = NULL;
    dbc->fmt = NULL;
    return 0;
}

static sqlite3_stmt *cachedStmt(dbiCursor dbc, const char *cmd)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt **sts = NULL;
    if (stmtHashGetEntry(dbc->stcache, cmd, &sts, NULL, NULL)) {
	stmt = sts[0];
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
    } else {
	if (sqlite3_prepare_v2(dbc->sdb, cmd, -1, &stmt, NULL) == SQLITE_OK) {
	    stmtHashAddEntry(dbc->stcache, xstrdup(cmd), stmt);
	}
    }

    return stmt;
}

static int dbiCursorPrep(dbiCursor dbc, const char *fmt, ...)
{
    char *cmd = NULL;
    /*
     * rpmdb recycles cursors for different purposes, reset if necessary.
     * This only works as long as fmt is always static (as it is now)
     */
    if (dbc->fmt && dbc->fmt != fmt)
	dbiCursorReset(dbc);

    if (dbc->fmt == NULL) {
	va_list ap;

	va_start(ap, fmt); 
	cmd = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	dbc->fmt = fmt;
    }

    if (dbc->stmt == NULL) {
	dbc->stmt = cachedStmt(dbc, cmd);
    } else {
	sqlite3_reset(dbc->stmt);
    }
    sqlite3_free(cmd);

    return dbiCursorResult(dbc);
}

static int dbiCursorBindPkg(dbiCursor dbc, unsigned int hnum,
				void *blob, unsigned int bloblen)
{
    int rc = sqlite3_bind_int(dbc->stmt, 1, hnum);
    if (blob) {
	if (!rc)
	    rc = sqlite3_bind_blob(dbc->stmt, 2, blob, bloblen, NULL);
    }
    return dbiCursorResult(dbc);
}

static int dbiCursorBindIdx(dbiCursor dbc, const void *key, int keylen,
				dbiIndexItem rec)
{
    int rc;
    if (dbc->ctype == SQLITE_TEXT) {
	rc = sqlite3_bind_text(dbc->stmt, 1, key, keylen, NULL);
    } else {
	rc = sqlite3_bind_blob(dbc->stmt, 1, key, keylen, NULL);
    }

    if (rec) {
	if (!rc)
	    rc = sqlite3_bind_int(dbc->stmt, 2, rec->hdrNum);
	if (!rc)
	    rc = sqlite3_bind_int(dbc->stmt, 3, rec->tagNum);
    }

    return dbiCursorResult(dbc);
}

static int sqlite_init(rpmdb rdb, const char * dbhome)
{
    int rc = 0;
    char *dbfile = NULL;

    if (rdb->db_dbenv == NULL) {
	dbfile = rpmGenPath(dbhome, rdb->db_ops->path, NULL);
	sqlite3 *sdb = NULL;
	int xx, flags = 0;
	int retry_open = 1;
	if ((rdb->db_mode & O_ACCMODE) == O_RDONLY)
	    flags |= SQLITE_OPEN_READONLY;
	else
	    flags |= (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

	while (retry_open--) {
	    xx = sqlite3_open_v2(dbfile, &sdb, flags, NULL);
	    /* Attempt to create if missing, discarding OPEN_READONLY (!) */
	    if (xx == SQLITE_CANTOPEN && (flags & SQLITE_OPEN_READONLY)) {
		flags &= ~SQLITE_OPEN_READONLY;
		flags |= (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
		retry_open++;
	    }
	}

	if (xx != SQLITE_OK) {
	    rpmlog(RPMLOG_ERR, _("Unable to open sqlite database %s: %s\n"),
		    dbfile, sqlite3_errmsg(sdb));
	    rc = 1;
	    goto exit;
	}
	sqlite3_busy_timeout(sdb, sleep_ms);

	rdb->db_cache = stmtHashCreate(31, rstrhash, strcmp,
				   (stmtHashFreeKey)rfree,
				   (stmtHashFreeData)sqlite3_finalize);

	if (rdb->db_flags & RPMDB_FLAG_REBUILD) {
	    sqlexec(sdb, "PRAGMA journal_mode = OFF");
	    sqlexec(sdb, "PRAGMA locking_mode = EXCLUSIVE");
	}

	rdb->db_dbenv = sdb;
	rdb->db_descr = xstrdup("sqlite");
    }
    rdb->db_opens++;

exit:
    free(dbfile);
    return rc;
}

static int sqlite_fini(rpmdb rdb)
{
    int rc = 0;
    if (rdb) {
	sqlite3 *sdb = rdb->db_dbenv;
	if (rdb->db_opens > 1) {
	    rdb->db_opens--;
	} else {
	    if (sqlite3_db_readonly(sdb, NULL) == 0) {
		sqlexec(sdb, "PRAGMA optimize");
	    }
	    rdb->db_cache = stmtHashFree(rdb->db_cache);
	    int xx = sqlite3_close(sdb);
	    rc = (xx != SQLITE_OK);
	}
    }

    return rc;
}

static int sqlexec(sqlite3 *sdb, const char *fmt, ...)
{
    int rc = 0;
    char *cmd = NULL;
    char *err = NULL;
    va_list ap;

    va_start(ap, fmt);
    cmd = sqlite3_vmprintf(fmt, ap);
    va_end(ap);

    /* sqlite3_exec() doesn't seeem to honor sqlite3_busy_timeout() */
    while ((rc = sqlite3_exec(sdb, cmd, NULL, NULL, &err)) == SQLITE_BUSY) {
	usleep(sleep_ms);
    }

    if (rc)
	rpmlog(RPMLOG_ERR, "sqlite failure: %s: %s\n", cmd, err);
    else
	rpmlog(RPMLOG_DEBUG, "%s: %d\n", cmd, rc);

    sqlite3_free(cmd);

    return rc ? RPMRC_FAIL : RPMRC_OK;
}

static int init_table(dbiIndex dbi, rpmTagVal tag)
{
    int rc = 0;

    if (dbi->dbi_type == DBI_PRIMARY) {
	rc = sqlexec(dbi->dbi_db,
			"CREATE TABLE IF NOT EXISTS '%q' ("
			    "hnum INTEGER PRIMARY KEY NOT NULL,"
			    "blob BLOB NOT NULL"
			")",
			dbi->dbi_file);
    } else {
	const char *keytype = (rpmTagGetClass(tag) == RPM_STRING_CLASS) ?
				"TEXT" : "BLOB";
	rc = sqlexec(dbi->dbi_db,
			"CREATE TABLE IF NOT EXISTS '%q' ("
			    "key '%q' NOT NULL, "
			    "hnum INTEGER NOT NULL, "
			    "idx INTEGER NOT NULL, "
			    "FOREIGN KEY (hnum) REFERENCES 'Packages'(hnum)"
			")",
			dbi->dbi_file, keytype);
    }

    return rc;
}

static int init_index(dbiIndex dbi, rpmTagVal tag)
{
    int rc = 0;
    if (dbi->dbi_type == DBI_SECONDARY) {
	rc = sqlexec(dbi->dbi_db,
		    "CREATE INDEX IF NOT EXISTS '%q_idx' ON '%q'(key ASC)",
		    dbi->dbi_file, dbi->dbi_file);
    }
    return rc;
}

static int sqlite_Open(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
{
    int rc = sqlite_init(rdb, rpmdbHome(rdb));

    dbiIndex dbi = dbiNew(rdb, rpmtag);
    dbi->dbi_db = rdb->db_dbenv;

    if (sqlite3_db_readonly(rdb->db_dbenv, NULL) == 0) {
	if (!rc)
	    rc = init_table(dbi, rpmtag);

	if (!rc)
	    rc = init_index(dbi, rpmtag);
    }

    if (!rc && dbi->dbi_type == DBI_PRIMARY) {
	unsigned int hnum = 0;
	if (sqlite_pkgdbNew(dbi, &hnum) == 0 && hnum == 1) {
	    dbi->dbi_flags |= DBI_CREATED;
	}
    }

    if (dbip)
	*dbip = dbi;
    else
	dbiFree(dbi);

    return rc;
}

static int sqlite_Close(dbiIndex dbi, unsigned int flags)
{
    sqlite_fini(dbi->dbi_rpmdb);
    dbiFree(dbi);
    return 0;
}

static int sqlite_Verify(dbiIndex dbi, unsigned int flags)
{
    int rc = RPMRC_FAIL;
    sqlite3_stmt *s = NULL;
    const char *cmd = "PRAGMA integrity_check";

    if (dbi->dbi_type == DBI_SECONDARY)
	return RPMRC_OK;

    if (sqlite3_prepare_v2(dbi->dbi_db, cmd, -1, &s, NULL) == SQLITE_OK) {
	while (sqlite3_step(s) == SQLITE_ROW) {
	    rc = RPMRC_OK;
	}
	sqlite3_finalize(s);
    } else {
	rpmlog(RPMLOG_ERR, "%s: %s\n", cmd, sqlite3_errmsg(dbi->dbi_db));
    }

    return rc;
}

static void sqlite_SetFSync(rpmdb rdb, int enable)
{
    sqlexec(rdb->db_dbenv,
	    "PRAGMA synchronous = %s", enable ? "NORMAL" : "OFF");
}

static int sqlite_Ctrl(rpmdb rdb, dbCtrlOp ctrl)
{
    int rc = 0;

    switch (ctrl) {
    case DB_CTRL_LOCK_RW:
	rc = sqlexec(rdb->db_dbenv, "SAVEPOINT 'rwlock'");
	break;
    case DB_CTRL_UNLOCK_RW:
	rc = sqlexec(rdb->db_dbenv, "RELEASE 'rwlock'");
	break;
    default:
	break;
    }

    return rc;
}

static dbiCursor sqlite_CursorInit(dbiIndex dbi, unsigned int flags)
{
    dbiCursor dbc = xcalloc(1, sizeof(*dbc));
    dbc->sdb = dbi->dbi_db;
    dbc->flags = flags;
    dbc->tag = rpmTagGetValue(dbi->dbi_file);
    dbc->stcache = dbi->dbi_rpmdb->db_cache;
    if (rpmTagGetClass(dbc->tag) == RPM_STRING_CLASS) {
	dbc->ctype = SQLITE_TEXT;
    } else {
	dbc->ctype = SQLITE_BLOB;
    }
    if (dbc->flags & DBC_WRITE)
	sqlexec(dbc->sdb, "SAVEPOINT '%s'", dbi->dbi_file);
    return dbc;
}

static dbiCursor sqlite_CursorFree(dbiIndex dbi, dbiCursor dbc)
{
    if (dbc) {
	if (dbc->flags & DBC_WRITE)
	    sqlexec(dbc->sdb, "RELEASE '%s'", dbi->dbi_file);
	free(dbc);
    }
    return NULL;
}

static rpmRC sqlite_pkgdbNew(dbiIndex dbi, unsigned int *hdrNum)
{
    dbiCursor c = dbiCursorInit(dbi, 0);
    int rc = dbiCursorPrep(c, "SELECT MAX(hnum) FROM '%q'", dbi->dbi_file);

    if (!rc) {
	while ((rc = sqlite3_step(c->stmt)) == SQLITE_ROW) {
	    *hdrNum = sqlite3_column_int(c->stmt, 0) + 1;
	}
    }
    rc = dbiCursorResult(c);
    dbiCursorFree(dbi, c);

    return rc;
}

static rpmRC sqlite_pkgdbPut(dbiIndex dbi, dbiCursor dbc,  unsigned int *hdrNum, unsigned char *hdrBlob, unsigned int hdrLen)
{
    int rc = 0;

    if (*hdrNum == 0)
	rc = sqlite_pkgdbNew(dbi, hdrNum);

    if (!rc) {
	rc = dbiCursorPrep(dbc, "INSERT OR REPLACE INTO '%q' VALUES(?, ?)",
			    dbi->dbi_file);
    }

    if (!rc)
	rc = dbiCursorBindPkg(dbc, *hdrNum, hdrBlob, hdrLen);

    if (!rc)
	while ((rc = sqlite3_step(dbc->stmt)) == SQLITE_ROW) {};

    return dbiCursorResult(dbc);
}

static rpmRC sqlite_pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum)
{
    int rc = dbiCursorPrep(dbc, "DELETE FROM '%q' WHERE hnum=?;",
			    dbi->dbi_file);

    if (!rc)
	rc = dbiCursorBindPkg(dbc, hdrNum, NULL, 0);

    if (!rc)
	while ((rc = sqlite3_step(dbc->stmt)) == SQLITE_ROW) {};

    return dbiCursorResult(dbc);
}

static rpmRC sqlite_stepPkg(dbiCursor dbc, unsigned char **hdrBlob, unsigned int *hdrLen)
{
    int rc = sqlite3_step(dbc->stmt);

    if (rc == SQLITE_ROW) {
	if (hdrLen)
	    *hdrLen = sqlite3_column_bytes(dbc->stmt, 1);
	if (hdrBlob)
	    *hdrBlob = (void *) sqlite3_column_blob(dbc->stmt, 1);
    }
    return rc;
}

static rpmRC sqlite_pkgdbByKey(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char **hdrBlob, unsigned int *hdrLen)
{
    int rc = dbiCursorPrep(dbc, "SELECT hnum, blob from '%q' where hnum=?",
				dbi->dbi_file);

    if (!rc)
	rc = dbiCursorBindPkg(dbc, hdrNum, NULL, 0);

    if (!rc)
	rc = sqlite_stepPkg(dbc, hdrBlob, hdrLen);

    return dbiCursorResult(dbc);
}

static rpmRC sqlite_pkgdbIter(dbiIndex dbi, dbiCursor dbc,
				unsigned char **hdrBlob, unsigned int *hdrLen)
{
    int rc = RPMRC_OK;
    if (dbc->stmt == NULL) {
	rc = dbiCursorPrep(dbc, "SELECT hnum, blob from '%q'", dbi->dbi_file);
    }

    if (!rc)
	rc = sqlite_stepPkg(dbc, hdrBlob, hdrLen);

    if (rc == SQLITE_DONE) {
	rc = RPMRC_NOTFOUND;
    } else {
	rc = dbiCursorResult(dbc);
    }

    return rc;
}

static rpmRC sqlite_pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char **hdrBlob, unsigned int *hdrLen)
{
    int rc;

    if (hdrNum) {
	rc = sqlite_pkgdbByKey(dbi, dbc, hdrNum, hdrBlob, hdrLen);
    } else {
	rc = sqlite_pkgdbIter(dbi, dbc, hdrBlob, hdrLen);
    }

    return rc;
}

static unsigned int sqlite_pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    return sqlite3_column_int(dbc->stmt, 0);
}

static rpmRC sqlite_idxdbByKey(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set)
{
    int rc = dbiCursorPrep(dbc, "SELECT key, hnum, idx from '%q' where key=?",
			dbi->dbi_file);

    if (!rc)
	rc = dbiCursorBindIdx(dbc, keyp, keylen, NULL);

    if (!rc) {
	while ((rc = sqlite3_step(dbc->stmt)) == SQLITE_ROW) {
	    unsigned int hnum = sqlite3_column_int(dbc->stmt, 1);
	    unsigned int tnum = sqlite3_column_int(dbc->stmt, 2);

	    if (*set == NULL)
		*set = dbiIndexSetNew(5);
	    dbiIndexSetAppendOne(*set, hnum, tnum, 0);
	}
    }

    if (rc == SQLITE_DONE) {
	if (*set)
	    dbiIndexSetSort(*set);
	rc = (*set) ? RPMRC_OK : RPMRC_NOTFOUND;
    } else {
	rc = dbiCursorResult(dbc);
    }

    return rc;
}

static rpmRC sqlite_idxdbIter(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int searchType)
{
    int rc = RPMRC_OK;

    if (dbc->stmt == NULL) {
	if (searchType == DBC_PREFIX_SEARCH) {
	    rc = dbiCursorPrep(dbc, "SELECT DISTINCT key from '%q' "
				    "WHERE key LIKE '%q%%' "
				    "AND LENGTH(key) >= %d "
				    "ORDER BY key",
				    dbi->dbi_file, keyp, keylen);
	} else {
	    rc = dbiCursorPrep(dbc, "SELECT DISTINCT key from '%q' "
				    "ORDER BY key",
				dbi->dbi_file);
	}
    }

    if (!rc)
	rc = sqlite3_step(dbc->stmt);

    if (rc == SQLITE_ROW) {
	dbiCursor kc = dbiCursorInit(dbi, 0);
	if (dbc->ctype == SQLITE_TEXT) {
	    dbc->key = sqlite3_column_text(dbc->stmt, 0);
	} else {
	    dbc->key = sqlite3_column_blob(dbc->stmt, 0);
	}
	dbc->keylen = sqlite3_column_bytes(dbc->stmt, 0);
	rc = sqlite_idxdbByKey(dbi, kc, dbc->key, dbc->keylen, set);
	dbiCursorFree(dbi, kc);
	rc = RPMRC_OK;
    } else if (rc == SQLITE_DONE) {
	if (searchType == DBC_PREFIX_SEARCH && (*set))
	    rc = RPMRC_OK;
	else
	    rc = RPMRC_NOTFOUND;
    } else {
	rc = dbiCursorResult(dbc);
    }

    return rc;
}

static rpmRC sqlite_idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int searchType)
{
    int rc;
    if (keyp && searchType != DBC_PREFIX_SEARCH) {
	rc = sqlite_idxdbByKey(dbi, dbc, keyp, keylen, set);
    } else {
	rc = sqlite_idxdbIter(dbi, dbc, keyp, keylen, set, searchType);
    }

    return rc;
}

static rpmRC sqlite_idxdbPutOne(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexItem rec)
{
    int rc = dbiCursorPrep(dbc, "INSERT INTO '%q' VALUES(?, ?, ?)",
			dbi->dbi_file);

    if (!rc)
	rc = dbiCursorBindIdx(dbc, keyp, keylen, rec);

    if (!rc)
	while ((rc = sqlite3_step(dbc->stmt)) == SQLITE_ROW) {};

    return dbiCursorResult(dbc);
}

static rpmRC sqlite_idxdbPut(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h)
{
    return tag2index(dbi, rpmtag, hdrNum, h, sqlite_idxdbPutOne);
}

static rpmRC sqlite_idxdbDel(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h)
{
    dbiCursor dbc = dbiCursorInit(dbi, DBC_WRITE);
    int rc = dbiCursorPrep(dbc, "DELETE FROM '%q' WHERE hnum=?", dbi->dbi_file);

    if (!rc)
	rc = dbiCursorBindPkg(dbc, hdrNum, NULL, 0);

    if (!rc)
	while ((rc = sqlite3_step(dbc->stmt)) == SQLITE_ROW) {};

    rc = dbiCursorResult(dbc);
    dbiCursorFree(dbi, dbc);
    return rc;
}

static const void * sqlite_idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    const void *key = NULL;
    if (dbc) {
	key = dbc->key;
	if (key && keylen)
	    *keylen = dbc->keylen;
    }
    return key;
}



struct rpmdbOps_s sqlite_dbops = {
    .name	= "sqlite",
    .path	= "rpmdb.sqlite",

    .open	= sqlite_Open,
    .close	= sqlite_Close,
    .verify	= sqlite_Verify,
    .setFSync	= sqlite_SetFSync,
    .ctrl	= sqlite_Ctrl,

    .cursorInit	= sqlite_CursorInit,
    .cursorFree	= sqlite_CursorFree,

    .pkgdbPut	= sqlite_pkgdbPut,
    .pkgdbDel	= sqlite_pkgdbDel,
    .pkgdbGet	= sqlite_pkgdbGet,
    .pkgdbKey	= sqlite_pkgdbKey,

    .idxdbGet	= sqlite_idxdbGet,
    .idxdbPut	= sqlite_idxdbPut,
    .idxdbDel	= sqlite_idxdbDel,
    .idxdbKey	= sqlite_idxdbKey
};

