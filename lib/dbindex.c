#include "system.h"

static int _debug = 0;

#include <rpmlib.h>
#include <rpmurl.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/

#if HAVE_DB1_DB_H
extern struct _dbiVec db0vec;
#define	DB0vec		&db0vec
#else
#define	DB0vec		NULL
#endif

#if HAVE_DB_185_H
extern struct _dbiVec db1vec;
#define	DB1vec		&db1vec
#else
#define	DB1vec		NULL
#endif

extern struct _dbiVec db2vec;
#define	DB2vec		&db2vec

#if HAVE_DB3_DB_H
extern struct _dbiVec db3vec;
#define	DB3vec		&db3vec
#else
#define	DB3vec		NULL
#endif

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

static struct _dbiVec *mydbvecs[] = {
    DB0vec, DB1vec, DB2vec, DB3vec, NULL
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
	if (mydbvecs[dbi->dbi_major] != NULL) {
	    errno = 0;
	    rc = (*mydbvecs[dbi->dbi_major]->open) (dbi);
	    if (rc == 0) {
		dbi->dbi_vec = mydbvecs[dbi->dbi_major];
		break;
	    }
	}
	/*@fallthrough@*/
    case -1:
	dbi->dbi_major = 4;
	while (dbi->dbi_major-- > 0) {
if (_debug)
fprintf(stderr, "*** loop db%d mydbvecs %p\n", dbi->dbi_major, mydbvecs[dbi->dbi_major]);
	    if (mydbvecs[dbi->dbi_major] == NULL)
		continue;
	    errno = 0;
	    rc = (*mydbvecs[dbi->dbi_major]->open) (dbi);
if (_debug)
fprintf(stderr, "*** loop db%d rc %d errno %d %s\n", dbi->dbi_major, rc, errno, strerror(errno));
	    if (rc == 0) {
		dbi->dbi_vec = mydbvecs[dbi->dbi_major];
		break;
	    }
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

    if (rc) {
        rpmError(RPMERR_DBOPEN, _("cannot open file %s: %s"), urlfn, strerror(errno));
	freeDBI(dbi);
	dbi = NULL;
     }

    return dbi;
}

int dbiCloseIndex(dbiIndex dbi) {
    int rc;

    rc = (*dbi->dbi_vec->close) (dbi, 0);
    freeDBI(dbi);
    return rc;
}

int dbiSyncIndex(dbiIndex dbi) {
    int rc;

    rc = (*dbi->dbi_vec->sync) (dbi, 0);
    return rc;
}

int dbiGetFirstKey(dbiIndex dbi, const char ** keyp) {
    int rc;

    if (dbi == NULL)
	return 1;

    rc = (*dbi->dbi_vec->GetFirstKey) (dbi, keyp);
    return rc;
}

int dbiSearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set) {
    int rc;

    rc = (*dbi->dbi_vec->SearchIndex) (dbi, str, set);

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

    rc = (*dbi->dbi_vec->UpdateIndex) (dbi, str, set);

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
