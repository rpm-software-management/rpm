#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <db3/db.h>

static char * dbfile = "/var/lib/rpm/Dirnames";

int
main(int argc, char *argv[])
{
    DB * db = NULL;
    DB_TXN * txnid = NULL;
    DBC * dbcursor = NULL;
    DBT key;
    DBT data;
    unsigned int flags;
    int rc, ec = 0;

    if (argc > 1)
	dbfile = argv[1];

    if ((rc = db_create(&db, NULL, 0)) != 0) {
	fprintf(stderr, "db_create: %s\n", db_strerror(rc));
	exit (1);
    }

    if ((rc = db->open(db, dbfile, NULL, DB_UNKNOWN, DB_RDONLY, 0664)) != 0) {
	db->err(db, rc, "db->open");
	if (!ec) ec = rc;
	goto err;
    }

    if ((rc = db->cursor(db, txnid, &dbcursor, 0)) != 0) {
	db->err(db, rc, "db->cursor");
	if (!ec) ec = rc;
	goto err;
    }

    flags = DB_NEXT;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((rc = dbcursor->c_get(dbcursor, &key, &data, flags)) == 0) {
	const char * kval;
	size_t klen;
	db_recno_t nrecs;

	nrecs = 0;
	if ((rc = dbcursor->c_count(dbcursor, &nrecs, 0)) != 0) {
	    db->err(db, rc, "dbcursor->c_count");
	    if (!ec) ec = rc;
	}
	kval = key.data;
	klen = key.size;
	printf("%3d %.*s\t%p[%d]\n", nrecs, klen, kval, data.data, data.size);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
    }

    if (rc != DB_NOTFOUND) {
	db->err(db, rc, "db->c_get");
	if (!ec) ec = rc;
	goto err;
    }

err:
    if (dbcursor && (rc = dbcursor->c_close(dbcursor)) != 0) {
	db->err(db, rc, "dbcursor->c_close");
	if (!ec) ec = rc;
    }
    if ((rc = db->close(db, 0)) != 0) {
	db->err(db, rc, "db->close");
	if (!ec) ec = rc;
    }
    return ec;
}
