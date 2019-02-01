/** \ingroup rpmdb
 * \file lib/dbi.c
 */

#include "system.h"

#include <stdlib.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmlog.h>
#include "lib/rpmdb_internal.h"
#include "debug.h"


dbiIndex dbiFree(dbiIndex dbi)
{
    if (dbi) {
	free(dbi);
    }
    return NULL;
}

dbiIndex dbiNew(rpmdb rdb, rpmDbiTagVal rpmtag)
{
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    /* FIX: figger lib/dbi refcounts */
    dbi->dbi_rpmdb = rdb;
    dbi->dbi_file = rpmTagGetName(rpmtag);
    dbi->dbi_type = (rpmtag == RPMDBI_PACKAGES) ? DBI_PRIMARY : DBI_SECONDARY;
    dbi->dbi_byteswapped = -1;	/* -1 unknown, 0 native order, 1 alien order */
    return dbi;
}

static void
dbDetectBackend(rpmdb rdb)
{
    const char *dbhome = rpmdbHome(rdb);
    char *db_backend = rpmExpand("%{?_db_backend}", NULL);
    char *path = NULL;

#if defined(WITH_LMDB)
    if (!strcmp(db_backend, "lmdb")) {
	rdb->db_ops = &lmdb_dbops;
    } else
#endif
#ifdef ENABLE_NDB
    if (!strcmp(db_backend, "ndb")) {
	rdb->db_ops = &ndb_dbops;
    } else
#endif
#if defined(WITH_BDB)
    {
	rdb->db_ops = &db3_dbops;
	if (*db_backend == '\0') {
	    free(db_backend);
	    db_backend = xstrdup("bdb");
	}
    }
#endif

#if defined(WITH_LMDB)
    path = rstrscat(NULL, dbhome, "/data.mdb", NULL);
    if (access(path, F_OK) == 0 && rdb->db_ops != &lmdb_dbops) {
	rdb->db_ops = &lmdb_dbops;
	rpmlog(RPMLOG_WARNING, _("Found LMDB data.mdb database while attempting %s backend: using lmdb backend.\n"), db_backend);
    }
    free(path);
#endif

#ifdef ENABLE_NDB
    path = rstrscat(NULL, dbhome, "/Packages.db", NULL);
    if (access(path, F_OK) == 0 && rdb->db_ops != &ndb_dbops) {
	rdb->db_ops = &ndb_dbops;
	rpmlog(RPMLOG_WARNING, _("Found NDB Packages.db database while attempting %s backend: using ndb backend.\n"), db_backend);
    }
    free(path);
#endif

#if defined(WITH_BDB)
    path = rstrscat(NULL, dbhome, "/Packages", NULL);
    if (access(path, F_OK) == 0 && rdb->db_ops != &db3_dbops) {
	rdb->db_ops = &db3_dbops;
	rpmlog(RPMLOG_WARNING, _("Found BDB Packages database while attempting %s backend: using bdb backend.\n"), db_backend);
    }
    free(path);
#endif

    if (rdb->db_ops == NULL) {
	rdb->db_ops = &dummydb_dbops;
	rpmlog(RPMLOG_DEBUG, "using dummy database, installs not possible\n");
    }

    if (db_backend)
	free(db_backend);
}

const char * dbiName(dbiIndex dbi)
{
    return dbi->dbi_file;
}

int dbiFlags(dbiIndex dbi)
{
    return dbi->dbi_flags;
}

void dbSetFSync(rpmdb rdb, int enable)
{
    if (!rdb->db_ops)
	dbDetectBackend(rdb);
    rdb->db_ops->setFSync(rdb, enable);
}

int dbCtrl(rpmdb rdb, dbCtrlOp ctrl)
{
    if (!rdb->db_ops)
	dbDetectBackend(rdb);
    return rdb->db_ops->ctrl(rdb, ctrl);
}

int dbiOpen(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
{
    if (!rdb->db_ops)
	dbDetectBackend(rdb);
    return rdb->db_ops->open(rdb, rpmtag, dbip, flags);
}

int dbiClose(dbiIndex dbi, unsigned int flags)
{
    return dbi ? dbi->dbi_rpmdb->db_ops->close(dbi, flags) : 0;
}

int dbiVerify(dbiIndex dbi, unsigned int flags)
{
    return dbi->dbi_rpmdb->db_ops->verify(dbi, flags);
}

dbiCursor dbiCursorInit(dbiIndex dbi, unsigned int flags)
{
    return dbi->dbi_rpmdb->db_ops->cursorInit(dbi, flags);
}

dbiCursor dbiCursorFree(dbiIndex dbi, dbiCursor dbc)
{
    return dbi->dbi_rpmdb->db_ops->cursorFree(dbi, dbc);
}

rpmRC pkgdbPut(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char *hdrBlob, unsigned int hdrLen)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbPut(dbi, dbc, hdrNum, hdrBlob, hdrLen);
}

rpmRC pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbDel(dbi, dbc, hdrNum);
}

rpmRC pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char **hdrBlob, unsigned int *hdrLen)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbGet(dbi, dbc, hdrNum, hdrBlob, hdrLen);
}

rpmRC pkgdbNew(dbiIndex dbi, dbiCursor dbc,  unsigned int *hdrNum)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbNew(dbi, dbc, hdrNum);
}

unsigned int pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbKey(dbi, dbc);
}

rpmRC idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int curFlags)
{
    return dbi->dbi_rpmdb->db_ops->idxdbGet(dbi, dbc, keyp, keylen, set, curFlags);
}

rpmRC idxdbPut(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexItem rec)
{
    return dbi->dbi_rpmdb->db_ops->idxdbPut(dbi, dbc, keyp, keylen, rec);
}

rpmRC idxdbDel(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexItem rec)
{
    return dbi->dbi_rpmdb->db_ops->idxdbDel(dbi, dbc, keyp, keylen, rec);
}

const void * idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    return dbi->dbi_rpmdb->db_ops->idxdbKey(dbi, dbc, keylen);
}

