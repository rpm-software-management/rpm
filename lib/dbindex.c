#include <db.h>
#include <stdlib.h>
#include <strings.h>

#include "dbindex.h"
#include "rpmerr.h"

dbIndex * openDBIndex(char * filename, int flags, int perms) {
    dbIndex * db;
        
    db = malloc(sizeof(*db));
    db->indexname = strdup(filename);
    db->db = dbopen(filename, flags, perms, DB_HASH, NULL);
    if (!db) {
	free(db->indexname);
	free(db);
	error(RPMERR_DBOPEN, "cannot open file %s filename");
	return NULL;
    }

    return db;
}

void closeDBIndex(dbIndex * dbi) {
    dbi->db->close(dbi->db);
    free(dbi->indexname);
    free(dbi);
}

int searchDBIndex(dbIndex * dbi, char * str, dbIndexSet * set) {
    DBT key, data;
    int rc;

    key.data = str;
    key.size = strlen(str);

    rc = dbi->db->get(dbi->db, &key, &data, 0);
    if (rc == -1) {
	error(RPMERR_DBGETINDEX, "error getting record %s from %s",
		str, dbi->indexname);
	return -1;
    } else if (rc == 1) {
	return 1;
    } else {
	set->recs = data.data;
	set->recs = malloc(data.size);
	memcpy(set->recs, data.data, data.size);
	set->count = data.size / sizeof(dbIndexRecord);
	return 0;
    }
}

int updateDBIndex(dbIndex * dbi, char * str, dbIndexSet * set) {
   /* 0 on success */
    DBT key, data;
    int rc;

    key.data = str;
    key.size = strlen(str);

    data.data = set->recs;
    data.size = set->count * sizeof(dbIndexRecord);

    rc = dbi->db->put(dbi->db, &key, &data, 0);
    if (rc) {
	error(RPMERR_DBPUTINDEX, "error storing record %s into %s",
		str, dbi->indexname);
	return 1;
    }

    return 0;
}

int appendDBIndexRecord(dbIndexSet * set, dbIndexRecord rec) {
    set->count++;

    if (set->count == 1) {
	set->recs = malloc(set->count * sizeof(dbIndexRecord));
    } else {
	set->recs = realloc(set->recs, set->count * sizeof(dbIndexRecord));
    }
    set->recs[set->count - 1] = rec;

    return 0;
}

dbIndexSet createDBIndexRecord(void) {
    dbIndexSet set;

    set.count = 0;
    return set;
}
    
void freeDBIndexRecord(dbIndexSet set) {
    free(set.recs);
}
