#include "system.h"

static int _debug = 0;

#include <rpmlib.h>
#include <rpmurl.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/

#include "db0.h"
#include "db1.h"
#include "db2.h"

unsigned int dbiIndexSetCount(dbiIndexSet set) {
    return set->count;
}

dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber) {
    dbiIndexRecord rec = xmalloc(sizeof(*rec));
    rec->recOffset = recOffset;
    rec->fileNumber = fileNumber;
    return rec;
}

void dbiFreeIndexRecordInstance(dbiIndexRecord rec) {
    if (rec) free(rec);
}

unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set->recs[recno].recOffset;
}

unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set->recs[recno].fileNumber;
}

void dbiIndexRecordOffsetSave(dbiIndexSet set, int recno, unsigned int recoff) {
    set->recs[recno].recOffset = recoff;
}

static dbiIndex newDBI(void) {
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    return dbi;
}

static void freeDBI( /*@only@*/ /*@null@*/ dbiIndex dbi) {
    if (dbi) {
	if (dbi->dbi_dbenv)	free(dbi->dbi_dbenv);
	if (dbi->dbi_dbinfo)	free(dbi->dbi_dbinfo);
	if (dbi->dbi_file)	xfree(dbi->dbi_file);
	xfree(dbi);
    }
}

int _preferDbiMajor = 0;	/* XXX shared with rebuilddb.c */
int _useDbiMajor = -1;

typedef int (*_dbopen) (dbiIndex dbi);

static _dbopen mydbopens[] = {
#if HAVE_DB1_DB_H
    db0open,
#else
    NULL,
#endif
#if HAVE_DB_185_H
    db1open,
#else
    NULL,
#endif
    db2open,
    NULL,
    NULL
};

dbiIndex dbiOpenIndex(const char * urlfn, int flags, int perms, DBI_TYPE type) {
    dbiIndex dbi;
    const char * filename;
    int rc = 0;

    (void) urlPath(urlfn, &filename);
    if (*filename == '\0') {
	rpmError(RPMERR_DBOPEN, _("bad db file %s"), urlfn);
	return NULL;
    }

    dbi = newDBI();
    dbi->dbi_file = xstrdup(filename);
    dbi->dbi_flags = flags;
    dbi->dbi_perms = perms;
    dbi->dbi_type = type;
    dbi->dbi_openinfo = NULL;
    dbi->dbi_major = _useDbiMajor;

    switch (dbi->dbi_major) {
    case 3:
    case 2:
    case 1:
    case 0:
	errno = 0;
	rc = (*(mydbopens[dbi->dbi_major])) (dbi);
	if (rc == 0)
	    break;
	/*@fallthrough@*/
    case -1:
	dbi->dbi_major = 4;
	while (dbi->dbi_major-- > 0) {
	    if (mydbopens[dbi->dbi_major] == NULL)
		continue;
	    errno = 0;
	    rc = (*(mydbopens[dbi->dbi_major])) (dbi);
if (_debug)
fprintf(stderr, "*** loop db%d rc %d errno %d %s\n", dbi->dbi_major, rc, errno, strerror(errno));
	    if (rc == 0)
		break;
	    if (rc == 1 && dbi->dbi_major == 2) {
		fprintf(stderr, "*** FIXME: <message about how to convert db>\n");
		fprintf(stderr, _("\n\
--> Please run \"rpm --rebuilddb\" as root to convert your database from\n\
    db1 to db2 on-disk format.\n\
\n\
"));
		dbi->dbi_major--;	/* XXX don't bother with db_185 */
	    }
	}
	_useDbiMajor = dbi->dbi_major;
    	break;
    }

    if (rc == 0)
	return dbi;

    rpmError(RPMERR_DBOPEN, _("cannot open file %s: %s"), urlfn,
			      strerror(errno));

    freeDBI(dbi);
    return NULL;
}

int dbiCloseIndex(dbiIndex dbi) {
    int rc;

    switch (dbi->dbi_major) {
    case 2:
	rc = db2close(dbi, 0);
	break;
    case 1:
	rc = db1close(dbi, 0);
	break;
    default:
    case 0:
	rc = db0close(dbi, 0);
	break;
    }
    freeDBI(dbi);
    return rc;
}

int dbiSyncIndex(dbiIndex dbi) {
    int rc;

    switch (dbi->dbi_major) {
    case 2:
	rc = db2sync(dbi, 0);
	break;
    case 1:
	rc = db1sync(dbi, 0);
	break;
    default:
    case 0:
	rc = db0sync(dbi, 0);
	break;
    }
    return rc;
}

int dbiGetFirstKey(dbiIndex dbi, const char ** keyp) {
    int rc;

    if (dbi == NULL)
	return 1;

    switch (dbi->dbi_major) {
    case 2:
	rc = db2GetFirstKey(dbi, keyp);
	break;
    case 1:
	rc = db1GetFirstKey(dbi, keyp);
	break;
    default:
    case 0:
	rc = db0GetFirstKey(dbi, keyp);
	break;
    }
    return rc;
}

int dbiSearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set) {
    int rc;

    switch (dbi->dbi_major) {
    case 2:
	rc = db2SearchIndex(dbi, str, set);
	break;
    case 1:
	rc = db1SearchIndex(dbi, str, set);
	break;
    default:
    case 0:
	rc = db0SearchIndex(dbi, str, set);
	break;
    }

    switch (rc) {
    case -1:
	rpmError(RPMERR_DBGETINDEX, _("error getting record %s from %s"),
		str, dbi->dbi_file);
	break;
    }
    return rc;
}

int dbiUpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
    int rc;

    switch (dbi->dbi_major) {
    default:
    case 2:
	rc = db2UpdateIndex(dbi, str, set);
	break;
    case 1:
	rc = db1UpdateIndex(dbi, str, set);
	break;
    case 0:
	rc = db0UpdateIndex(dbi, str, set);
	break;
    }

    if (set->count) {
	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error storing record %s into %s"),
		    str, dbi->dbi_file);
	}
    } else {
	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error removing record %s into %s"),
		    str, dbi->dbi_file);
	}
    }

    return rc;
}

int dbiAppendIndexRecord(dbiIndexSet set,
	unsigned int recOffset, unsigned int fileNumber)
{
    set->count++;

    if (set->count == 1) {
	set->recs = xmalloc(set->count * sizeof(*(set->recs)));
    } else {
	set->recs = xrealloc(set->recs, set->count * sizeof(*(set->recs)));
    }
    set->recs[set->count - 1].recOffset = recOffset;
    set->recs[set->count - 1].fileNumber = fileNumber;

    return 0;
}

dbiIndexSet dbiCreateIndexSet(void) {
    dbiIndexSet set = xmalloc(sizeof(*set));

    set->recs = NULL;
    set->count = 0;
    return set;
}

void dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	if (set->recs) free(set->recs);
	free(set);
    }
}

/* returns 1 on failure */
int dbiRemoveIndexRecord(dbiIndexSet set, dbiIndexRecord rec) {
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;

    for (from = 0; from < num; from++) {
	if (rec->recOffset != set->recs[from].recOffset ||
	    rec->fileNumber != set->recs[from].fileNumber) {
	    /* structure assignment */
	    if (from != to) set->recs[to] = set->recs[from];
	    to++;
	    numCopied++;
	} else {
	    set->count--;
	}
    }

    return (numCopied == num);
}
