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

const struct rpmdbOps_s *backends[] = {
#if defined(WITH_SQLITE)
    &sqlite_dbops,
#endif
#if defined(WITH_LMDB)
    &lmdb_dbops,
#endif
#ifdef ENABLE_NDB
    &ndb_dbops,
#endif
#if defined(WITH_BDB)
    &db3_dbops,
#endif
#if defined(WITH_BDB_RO)
    &bdbro_dbops,
#endif
    &dummydb_dbops,
    NULL
};

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
    const struct rpmdbOps_s **ops, *ops_config;

    rdb->db_ops = NULL;

    ops_config = NULL;
    for (ops = backends; *ops; ops++) {
	if (rstreq(db_backend, (*ops)->name)) {
	    ops_config = *ops;
	    break;
	}
    }

    /* if we have a configured backend, check it first */
    if (ops_config && ops_config->path) {
	path = rstrscat(NULL, dbhome, "/", ops_config->path, NULL);
	if (access(path, F_OK) == 0)
	    rdb->db_ops = ops_config;
	free(path);
    }

    /* if it did not match, check all available backends */
    if (rdb->db_ops == NULL) {
	for (ops = backends; *ops; ops++) {
	    if ((*ops)->path == NULL || *ops == ops_config)
		continue;

	    path = rstrscat(NULL, dbhome, "/", (*ops)->path, NULL);
	    if (access(path, F_OK) == 0) {
		rpmlog(RPMLOG_WARNING,
		    _("Found %s %s database while attempting %s backend: "
		    "using %s backend.\n"),
		    (*ops)->name, (*ops)->path, db_backend, (*ops)->name);
		rdb->db_ops = *ops;
	    }
	    free(path);
	    if (rdb->db_ops != NULL)
		break;
	}
    }

    /* if we did not find a match, use the configured backend */
    if (rdb->db_ops == NULL && ops_config)
	rdb->db_ops = ops_config;

    /* if everything failed fall back to dummydb */
    if (rdb->db_ops == NULL) {
	rdb->db_ops = &dummydb_dbops;
	rpmlog(RPMLOG_WARNING, "using dummy database, installs not possible\n");
    }

    rdb->db_descr = rdb->db_ops->name;
    rdb->db_ops_config = ops_config;

    if (db_backend)
	free(db_backend);
}

int dbiNeedConversion(rpmdb rdb)
{
    if (!rdb->db_ops)
	dbDetectBackend(rdb);
    return rdb->db_ops->readonly && rdb->db_ops_config
	&& rdb->db_ops_config->path && !rdb->db_ops_config->readonly;
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

rpmRC pkgdbPut(dbiIndex dbi, dbiCursor dbc, unsigned int *hdrNum, unsigned char *hdrBlob, unsigned int hdrLen)
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

unsigned int pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    return dbi->dbi_rpmdb->db_ops->pkgdbKey(dbi, dbc);
}

rpmRC idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int curFlags)
{
    return dbi->dbi_rpmdb->db_ops->idxdbGet(dbi, dbc, keyp, keylen, set, curFlags);
}

rpmRC idxdbPut(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h)
{
    return dbi->dbi_rpmdb->db_ops->idxdbPut(dbi, rpmtag, hdrNum, h);
}

rpmRC idxdbDel(dbiIndex dbi, rpmTagVal rpmtag, unsigned int hdrNum, Header h)
{
    return dbi->dbi_rpmdb->db_ops->idxdbDel(dbi, rpmtag, hdrNum, h);
}

const void * idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    return dbi->dbi_rpmdb->db_ops->idxdbKey(dbi, dbc, keylen);
}

