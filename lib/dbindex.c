#include "system.h"

#include <rpmlib.h>

int dbiIndexSetCount(dbiIndexSet set) {
    return set.count;
}

/* structure return */
dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber) {
    dbiIndexRecord rec;
    rec.recOffset = recOffset;
    rec.fileNumber = fileNumber;
    return rec;
}

unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set.recs[recno].recOffset;
}

unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set.recs[recno].fileNumber;
}

dbiIndex * dbiOpenIndex(const char * filename, int flags, int perms, DBTYPE type) {
    dbiIndex * dbi;
        
    dbi = malloc(sizeof(*dbi));
    dbi->db = dbopen(filename, flags, perms, type, NULL);
    if (!dbi->db) {
	free(dbi);
	rpmError(RPMERR_DBOPEN, _("cannot open file %s: %s"), filename, 
			      strerror(errno));
	return NULL;
    }
    dbi->indexname = strdup(filename);
    return dbi;
}

void dbiCloseIndex(dbiIndex * dbi) {
    dbi->db->close(dbi->db);
    free(dbi->indexname);
    free(dbi);
}

void dbiSyncIndex(dbiIndex * dbi) {
    dbi->db->sync(dbi->db, 0);
}

int dbiGetFirstKey(dbiIndex * dbi, const char ** keyp) {
    DBT key, data;
    int rc;

    if (dbi == NULL || dbi->db == NULL)
	return 1;
    
    rc = dbi->db->seq(dbi->db, &key, &data, R_FIRST);
    if (rc) {
	return 1;
    }

    {	char *k = malloc(key.size + 1);
	memcpy(k, key.data, key.size);
	k[key.size] = '\0';
	*keyp = k;
    }

    return 0;
}

int dbiSearchIndex(dbiIndex * dbi, const char * str, dbiIndexSet * set) {
    DBT key, data;
    int rc;

    key.data = (void *)str;
    key.size = strlen(str);

    rc = dbi->db->get(dbi->db, &key, &data, 0);
    if (rc == -1) {
	rpmError(RPMERR_DBGETINDEX, _("error getting record %s from %s"),
		str, dbi->indexname);
	return -1;
    } else if (rc == 1) {
	return 1;
    } 

    set->recs = malloc(data.size);
    memcpy(set->recs, data.data, data.size);
    set->count = data.size / sizeof(dbiIndexRecord);
    return 0;
}

int dbiUpdateIndex(dbiIndex * dbi, const char * str, dbiIndexSet * set) {
   /* 0 on success */
    DBT key, data;
    int rc;

    key.data = (void *)str;
    key.size = strlen(str);

    if (set->count) {
	data.data = set->recs;
	data.size = set->count * sizeof(dbiIndexRecord);

	rc = dbi->db->put(dbi->db, &key, &data, 0);
	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error storing record %s into %s"),
		    str, dbi->indexname);
	    return 1;
	}
    } else {
	rc = dbi->db->del(dbi->db, &key, 0);
	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error removing record %s into %s"),
		    str, dbi->indexname);
	    return 1;
	}
    }

    return 0;
}

int dbiAppendIndexRecord(dbiIndexSet * set, dbiIndexRecord rec) {
    set->count++;

    if (set->count == 1) {
	set->recs = malloc(set->count * sizeof(dbiIndexRecord));
    } else {
	set->recs = realloc(set->recs, set->count * sizeof(dbiIndexRecord));
    }
    set->recs[set->count - 1] = rec;

    return 0;
}

/* structure return */
dbiIndexSet dbiCreateIndexRecord(void) {
    dbiIndexSet set;

    set.recs = NULL;
    set.count = 0;
    return set;
}
    
void dbiFreeIndexRecord(dbiIndexSet set) {
    if (set.recs) free(set.recs);
}

/* returns 1 on failure */
int dbiRemoveIndexRecord(dbiIndexSet * set, dbiIndexRecord rec) {
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;
  
    for (from = 0; from < num; from++) {
	if (rec.recOffset != set->recs[from].recOffset ||
	    rec.fileNumber != set->recs[from].fileNumber) {
	    if (from != to) set->recs[to] = set->recs[from];
	    to++;
	    numCopied++;
	} else {
	    set->count--;
	}
    }

    return (numCopied == num);
}
