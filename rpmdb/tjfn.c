#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <db3/db.h>

#define RPMDB_HOME	"/tmp/rpm"
#define	RPMDB_Packages	"Packages"
#define	RPMDB_Dirnames	"Dirnames"
#define	RPMDB_Basenames	"Basenames"

static int nentries = 100;

static int
rnum(int rmin, int rmax)
{
    static int initialized = 0;

    if (!initialized) {
	srand((unsigned)getpid());
	initialized = 1;
    }

    return (rmin + (rand() % rmax));
}

static int
db_init(const char * home, DB_ENV ** dbenvp)
{
    DB_ENV * dbenv;
    u_int32_t flags;
    int ret;

    if ((ret = db_env_create(&dbenv, 0)) != 0)
	goto exit;
    dbenv->set_errfile(dbenv, stderr);
    dbenv->set_errpfx(dbenv, "tjfn");

    flags = DB_CREATE | DB_INIT_MPOOL;
    if ((ret = dbenv->open(dbenv, home, flags, 0)) != 0)
	goto exit;

exit:
    if (dbenvp != NULL && ret == 0)
	*dbenvp = dbenv;
    else if (dbenv)
	(void) dbenv->close(dbenv, 0);

    return ret;
}

static int
db_open(const char * dbfn, DB_ENV * dbenv, DB ** dbp)
{
    DB * db = NULL;
    int ret;

    if ((ret = db_create(&db, dbenv, 0)) != 0) {
	fprintf(stderr, "db_create: %s\n", db_strerror(ret));
	goto err;
    }

    if ((ret = db->set_flags(db, DB_DUP)) != 0) {
	db->err(db, ret, "db->set_flags(%s)", dbfn);
	goto err;
    }

    if ((ret = db->open(db, dbfn, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
	db->err(db, ret, "db->open(%s)", dbfn);
	goto err;
    }

err:
    if (dbp != NULL && ret == 0)
	*dbp = db;
    else if (db)
	(void) db->close(db, 0);
    return ret;
}

static int
db_put(DB * db, const char * kstr, int klen, const char * dstr, int dlen)
{
    DBT key, data;
    int ret;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if (klen <= 0)
	klen = strlen(kstr) + 1;
    if (dlen <= 0)
	dlen = strlen(dstr) + 1;
    key.data = (void *)kstr;
    key.size = klen;
    data.data = (void *)dstr;
    data.size = dlen;

    ret = db->put(db, NULL, &key, &data, 0);
    if (ret == DB_KEYEXIST)
	ret = 0;

    return ret;
}

static int
db_join(DB * pkgdb, DB * dndb, DB * bndb)
{
    char kstr[256], dstr[256];
    DBC *dndbc = NULL;
    DBC *bndbc = NULL;
    DBC *dbc = NULL;
    DBC *carray[3];
    DBT bnkey, bndata;
    DBT dnkey, dndata;
    DBT key, data;
    int ret, tret;
    int i, j, k;

    if ((ret = dndb->cursor(dndb, NULL, &dndbc, 0)) != 0)
	goto err;

    i = rnum(0, nentries);
    j = i;
    k = i;

    sprintf(kstr, "DN%d", i);
    sprintf(dstr, "%04d", k);
    memset(&dnkey, 0, sizeof(dnkey));
    memset(&dndata, 0, sizeof(dndata));
    dnkey.data = kstr;
    dnkey.size = strlen(kstr) + 1;
    dnkey.ulen = 0;
    dnkey.dlen = 0;
    dnkey.doff = 0;
    dnkey.flags = 0;
    if ((ret = dndbc->c_get(dndbc, &dnkey, &dndata, DB_SET_RANGE)) != 0)
	goto err;

    if ((ret = bndb->cursor(bndb, NULL, &bndbc, 0)) != 0)
	goto err;
    sprintf(kstr, "BN%d", i);
    sprintf(dstr, "%04d", i);
    memset(&bnkey, 0, sizeof(bnkey));
    memset(&bndata, 0, sizeof(bndata));
    bnkey.data = kstr;
    bnkey.size = strlen(kstr) + 1;
    bnkey.ulen = 0;
    bnkey.dlen = 0;
    bnkey.doff = 0;
    bnkey.flags = 0;
    if ((ret = bndbc->c_get(bndbc, &bnkey, &bndata, DB_SET_RANGE)) != 0)
	goto err;

    carray[0] = dndbc;
    carray[1] = bndbc;
    carray[2] = NULL;

    if ((ret = pkgdb->join(pkgdb, carray, &dbc, 0)) != 0)
	goto err;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    while ((ret = dbc->c_get(dbc, &key, &data, DB_JOIN_ITEM)) == 0) {
	/* Process record returned in key/data. */
fprintf(stderr, "*** Get db %p key (%s,%d) data (%s,%d)\n", pkgdb, (char *)key.data, key.size, (char *)data.data, data.size);
    }

    /*
     * If we exited the loop because we ran out of records,
     * then it has completed successfully.
     */
    if (ret == DB_NOTFOUND)
	ret = 0;

err:
    if (dbc != NULL && (tret = dbc->c_close(dbc)) != 0 && ret == 0)
	ret = tret;
    if (dndbc != NULL && (tret = dndbc->c_close(dndbc)) != 0 && ret == 0)
	ret = tret;
    if (bndbc != NULL && (tret = bndbc->c_close(bndbc)) != 0 && ret == 0)
	ret = tret;

    return (ret);
}

int
main(int argc, char *argv[])
{
    char kstr[256], dstr[256];
    DB_ENV * dbenv = NULL;
    DB * pkgdb = NULL;
    DB * dndb = NULL;
    DB * bndb = NULL;
    int ret, t_ret;
    int i;

    (void) mkdir(RPMDB_HOME, 0755);

    if ((ret = db_init(RPMDB_HOME, &dbenv)) != 0) {
	fprintf(stderr, "db_init(%s): %s\n", RPMDB_HOME, db_strerror(ret));
	goto err;
    }

    if ((ret = db_open(RPMDB_Packages, dbenv, &pkgdb)) != 0) {
	fprintf(stderr, "db_open(%s): %s\n", RPMDB_Packages, db_strerror(ret));
	goto err;
    }
    for (i = 0; i < nentries; i++) {
	sprintf(kstr, "%08d", i);
	sprintf(dstr, "PKG%d", i);
	if ((ret = db_put(pkgdb, kstr, 0, dstr, 0)) != 0) {
	    fprintf(stderr, "db_put(%s): %s\n", RPMDB_Packages, db_strerror(ret));
	    goto err;
	}
    }

    if ((ret = db_open(RPMDB_Dirnames, dbenv, &dndb)) != 0) {
	fprintf(stderr, "db_open(%s): %s\n", RPMDB_Dirnames, db_strerror(ret));
	goto err;
    }
    for (i = 0; i < nentries; i++) {
	sprintf(kstr, "DN%d", i);
	sprintf(dstr, "%08d", i);
	if ((ret = db_put(dndb, kstr, 0, dstr, 0)) != 0) {
	    fprintf(stderr, "db_put(%s): %s\n", RPMDB_Dirnames, db_strerror(ret));
	    goto err;
	}
    }

    if ((ret = db_open(RPMDB_Basenames, dbenv, &bndb)) != 0) {
	fprintf(stderr, "db_open(%s): %s\n", RPMDB_Basenames, db_strerror(ret));
	goto err;
    }
    for (i = 0; i < nentries; i++) {
	sprintf(kstr, "BN%d", i);
	sprintf(dstr, "%08d", i);
	if ((ret = db_put(bndb, kstr, 0, dstr, 0)) != 0) {
	    fprintf(stderr, "db_put(%s): %s\n", RPMDB_Basenames, db_strerror(ret));
	    goto err;
	}
    }

    if ((ret = db_join(pkgdb, dndb, bndb)) != 0) {
	fprintf(stderr, "db_join: %s\n", db_strerror(ret));
	goto err;
    }

err:
    if (dbenv != NULL) {
	if (pkgdb != NULL) {
	    if ((t_ret = pkgdb->close(pkgdb, 0)) != 0 && ret == 0)
		ret = t_ret; 
	}
	if (dndb != NULL) {
	    if ((t_ret = dndb->close(dndb, 0)) != 0 && ret == 0)
		ret = t_ret; 
	}
	if (bndb != NULL) {
	    if ((t_ret = bndb->close(bndb, 0)) != 0 && ret == 0)
		ret = t_ret; 
	}
	(void) dbenv->close(dbenv, 0);
    }

    return 0;
}
