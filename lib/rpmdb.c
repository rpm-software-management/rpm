#include "system.h"

static int _debug = 0;

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include <rpmlib.h>
#include <rpmurl.h>	/* XXX for assert.h */
#include <rpmmacro.h>	/* XXX for rpmGetPath/rpmGenPath */

#include "rpmdb.h"
/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/
/*@access rpmdbMatchIterator@*/

#include "fprint.h"
#include "misc.h"

extern int _noDirTokens;

int _useDbiMajor = 3;		/* XXX shared with rebuilddb.c */

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

struct _dbiIndex rpmdbi[] = {
    { "packages.rpm", 0, 0*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "nameindex.rpm", RPMTAG_NAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "fileindex.rpm", RPMTAG_BASENAMES, 2*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "groupindex.rpm", RPMTAG_GROUP, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "requiredby.rpm", RPMTAG_REQUIRENAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "providesindex.rpm", RPMTAG_PROVIDENAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "conflictsindex.rpm", RPMTAG_CONFLICTNAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "triggerindex.rpm", RPMTAG_TRIGGERNAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "obsoletesindex.rpm", RPMTAG_OBSOLETENAME, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "versionindex.rpm", RPMTAG_VERSION, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "releaseindex.rpm", RPMTAG_RELEASE, 1*sizeof(int_32),
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL }
#define	RPMDBI_MIN		0
#define	RPMDBI_MAX		11
};

/**
 * Return dbi index used for rpm tag.
 * @param rpmtag	rpm header tag
 * @return dbi index, -1 on error
 */
static int dbiTagToDbix(int rpmtag)
{
    int dbix;

    for (dbix = RPMDBI_MIN; dbix < RPMDBI_MAX; dbix++) {
	if (rpmtag == rpmdbi[dbix].dbi_rpmtag)
	    return dbix;
    }
    return -1;
}

#define	dbiSyncIndex(_dbi)	(*(_dbi)->dbi_vec->sync) ((_dbi), 0);

/**
 * Create and initialize element of index database set.
 * @param recOffset	byte offset of header in db
 * @param fileNumber	file array index
 * @return	new element
 */
static inline dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber) {
    dbiIndexRecord rec = xmalloc(sizeof(*rec));
    rec->recOffset = recOffset;
    rec->fileNumber = fileNumber;
    return rec;
}

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
static int dbiSearchIndex(dbiIndex dbi, const char * str, size_t len,
		dbiIndexSet * set)
{
    int rc;

    rc = (*dbi->dbi_vec->SearchIndex) (dbi, str, len, set);

    switch (rc) {
    case -1:
	rpmError(RPMERR_DBGETINDEX, _("error getting record %s from %s"),
		str, dbi->dbi_file);
	break;
    }
    return rc;
}

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param str	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
static int dbiUpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
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

/**
 * Append element to set of index database items.
 * @param set	set of index database items
 * @param rec	item to append to set
 * @return	0 success (always)
 */
static inline int dbiAppendIndexRecord(dbiIndexSet set, dbiIndexRecord rec)
{
    set->count++;

    if (set->count == 1) {
	set->recs = xmalloc(set->count * sizeof(*(set->recs)));
    } else {
	set->recs = xrealloc(set->recs, set->count * sizeof(*(set->recs)));
    }
    set->recs[set->count - 1].recOffset = rec->recOffset;
    set->recs[set->count - 1].fileNumber = rec->fileNumber;

    return 0;
}

/* returns 1 on failure */
/**
 * Remove element from set of index database items.
 * @param set	set of index database items
 * @param rec	item to remove from set
 * @return	0 success, 1 failure
 */
static inline int dbiRemoveIndexRecord(dbiIndexSet set, dbiIndexRecord rec) {
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

#if USE_DB0
extern struct _dbiVec db0vec;
#define	DB0vec		&db0vec
#else
#define	DB0vec		NULL
#endif

#if USE_DB1
extern struct _dbiVec db1vec;
#define	DB1vec		&db1vec
#else
#define	DB1vec		NULL
#endif

#if USE_DB2
extern struct _dbiVec db2vec;
#define	DB2vec		&db2vec
#else
#define	DB2vec		NULL
#endif

#if USE_DB3
extern struct _dbiVec db3vec;
#define	DB3vec		&db3vec
#else
#define	DB3vec		NULL
#endif

int __do_dbenv_remove = -1;	/* XXX in dbindex.c, shared with rebuilddb.c */

/* XXX rpminstall.c, transaction.c */
unsigned int dbiIndexSetCount(dbiIndexSet set) {
    return set->count;
}

/* XXX rpminstall.c, transaction.c */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set->recs[recno].recOffset;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set->recs[recno].fileNumber;
}

/**
 * Change record offset of header within element in index database set.
 * @param set	set of index database items
 * @param recno	index of item in set
 * @param recoff new record offset
 */
static inline void dbiIndexRecordOffsetSave(dbiIndexSet set, int recno, unsigned int recoff) {
    set->recs[recno].recOffset = recoff;
}

static dbiIndex newDBI(const dbiIndex dbiTemplate) {
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    
    *dbi = *dbiTemplate;	/* structure assignment */
    if (dbiTemplate->dbi_basename)
	dbi->dbi_basename = xstrdup(dbiTemplate->dbi_basename);
    return dbi;
}

static void freeDBI( /*@only@*/ /*@null@*/ dbiIndex dbi) {
    if (dbi) {
	if (dbi->dbi_dbenv)	free(dbi->dbi_dbenv);
	if (dbi->dbi_dbinfo)	free(dbi->dbi_dbinfo);
	if (dbi->dbi_file)	xfree(dbi->dbi_file);
	if (dbi->dbi_basename)	xfree(dbi->dbi_basename);
	xfree(dbi);
    }
}

static struct _dbiVec *mydbvecs[] = {
    DB0vec, DB1vec, DB2vec, DB3vec, NULL
};

/**
 * Return handle for an index database.
 * @param rpmdb		rpm database
 * @param dbix		dbi template to use
 * @return		index database handle
 */
static int dbiOpenIndex(rpmdb rpmdb, int dbix)
{
    dbiIndex dbiTemplate = rpmdbi + dbix;
    const char * urlfn;
    const char * filename = NULL;
    dbiIndex dbi = NULL;
    int rc = 0;

    /* Is this index already open ? */
    if (rpmdb->_dbi[dbix])
	return 0;
    if (dbix < 0 || dbix >= RPMDBI_MAX)
	return 1;

    urlfn = rpmGenPath(rpmdb->db_root, rpmdb->db_home, dbiTemplate->dbi_basename);
    (void) urlPath(urlfn, &filename);
    if (!(filename && *filename != '\0')) {
	rpmError(RPMERR_DBOPEN, _("bad db file %s"), urlfn);
	goto exit;
    }

    dbi = newDBI(dbiTemplate);
    dbi->dbi_file = xstrdup(filename);
    dbi->dbi_mode = rpmdb->db_mode;
    dbi->dbi_major = rpmdb->db_major;
    dbi->dbi_rpmdb = rpmdb;

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
	if (rpmdb->db_major == -1)
	    rpmdb->db_major = dbi->dbi_major;
    	break;
    }

    if (rc == 0) {
	rpmdb->_dbi[dbix] = dbi;
    } else {
        rpmError(RPMERR_DBOPEN, _("cannot open file %s: %s"), urlfn, strerror(errno));
	freeDBI(dbi);
	dbi = NULL;
     }

exit:
    if (urlfn) {
	xfree(urlfn);
	urlfn = NULL;
    }
    return rc;
}

/**
 * Close index database.
 * @param dbi	index database handle
 */
static int dbiCloseIndex(dbiIndex dbi) {
    int rc;

    rc = (*dbi->dbi_vec->close) (dbi, 0);
    freeDBI(dbi);
    return rc;
}

/* XXX depends.c, install.c, query.c, rpminstall.c, transaction.c */
void dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	if (set->recs) free(set->recs);
	free(set);
    }
}

/* XXX the signal handling in here is not thread safe */

static sigset_t signalMask;

static void blockSignals(void)
{
    sigset_t newMask;

    sigfillset(&newMask);		/* block all signals */
    sigprocmask(SIG_BLOCK, &newMask, &signalMask);
}

static void unblockSignals(void)
{
    sigprocmask(SIG_SETMASK, &signalMask, NULL);
}

#define	_DB_ROOT	""
#define	_DB_HOME	"%{_dbpath}"
#define	_DB_FLAGS	0
#define	_DB_TYPE	DBI_UNKNOWN
#define _DB_MODE	0
#define _DB_PERMS	0644
#define _DB_MAJOR	-1

#define	_DB_LORDER	0
#define	_DB_ERRCALL	NULL
#define	_DB_ERRFILE	NULL
#define	_DB_ERRPFX	"rpmdb"
#define	_DB_VERBOSE	1

#define	_DB_MP_MMAPSIZE	16 * 1024 * 1024
#define	_DB_MP_SIZE	2 * 1024 * 1024
#define	_DB_CACHESIZE	0
#define	_DB_PAGESIZE	0
#define	_DB_MALLOC	NULL
#define	_DB_H_FFACTOR	0
#define	_DB_H_HASH_FCN	NULL
#define	_DB_H_NELEM	0
#define	_DB_H_FLAGS	0		/* DB_DUP, DB_DUPSORT */
#define	_DB_H_DUP_COMPARE_FCN NULL

#define	_DB_NDBI	0

static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS,
    _DB_TYPE,	_DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_LORDER, _DB_ERRCALL, _DB_ERRFILE, _DB_ERRPFX, _DB_VERBOSE,
		_DB_MP_MMAPSIZE, _DB_MP_SIZE,
		_DB_CACHESIZE, _DB_PAGESIZE, _DB_MALLOC,
		_DB_H_FFACTOR, _DB_H_HASH_FCN, _DB_H_NELEM, _DB_H_FLAGS,
    _DB_NDBI
};

/* XXX query.c, rebuilddb.c, rpminstall.c, verify.c */
void rpmdbClose (rpmdb db)
{
    int dbix;

    for (dbix = db->db_ndbi; --dbix >= RPMDBI_MIN; ) {
	if (db->_dbi[dbix] == NULL)
	    continue;
    	dbiCloseIndex(db->_dbi[dbix]);
    	db->_dbi[dbix] = NULL;
    }
    if (db->db_errpfx) {
	xfree(db->db_errpfx);
	db->db_root = NULL;
    }
    if (db->db_root) {
	xfree(db->db_root);
	db->db_root = NULL;
    }
    if (db->db_home) {
	xfree(db->db_home);
	db->db_home = NULL;
    }
    free(db);
}

static /*@only@*/ rpmdb newRpmdb(const char * root, const char * home,
		int mode, int perms, int flags)
{
    rpmdb db = xcalloc(sizeof(*db), 1);

    *db = dbTemplate;	/* structure assignment */

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (root)
	db->db_root = (*root ? root : _DB_ROOT);
    if (home)
	db->db_home = (*home ? home : _DB_HOME);
    if (mode >= 0)	db->db_mode = mode;
    if (perms >= 0)	db->db_perms = perms;
    if (flags >= 0)	db->db_flags = flags;

    if (db->db_root)
	db->db_root = rpmGetPath(db->db_root, NULL);
    if (db->db_home) {
	db->db_home = rpmGetPath(db->db_home, NULL);
	if (!(db->db_home && db->db_home[0] != '%')) {
	    rpmError(RPMERR_DBOPEN, _("no dbpath has been set"));
	   goto errxit;
	}
    }
    if (db->db_errpfx)
	db->db_errpfx = xstrdup(db->db_errpfx);
    db->db_major = _useDbiMajor;
    db->db_ndbi = RPMDBI_MAX;
    return db;

errxit:
    if (db)
	rpmdbClose(db);
    return db;
}

/* XXX rebuilddb.c */
int openDatabase(const char * prefix, const char * dbpath, rpmdb *dbp,
		int mode, int perms, int flags)
{
    rpmdb rpmdb;
    int rc;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
	return 1;

    rpmdb = newRpmdb(prefix, dbpath, mode, perms, flags);

    {	int dbix;

	rc = 0;
	for (dbix = RPMDBI_MIN; rc == 0 && dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbi;

	    if (!justCheck)
		(void) dbiOpenIndex(rpmdb, dbix);

	    if ((dbi = rpmdb->_dbi[dbix]) == NULL)
		continue;

	    switch (dbix) {
	    case 0:
		if (rpmdb->db_major == 3)
		    goto exit;
		break;
	    case 1:
		if (minimal)
		    goto exit;
		break;
	    case 2:
	    {	void * keyp = NULL;
		int xx;

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
		if (justCheck)
		    break;
		xx = (*dbi->dbi_vec->cget) (dbi, &keyp, NULL, NULL, NULL);
		if (xx == 0) {
		    const char * akey = keyp;
		    if (strchr(akey, '/')) {
			rpmError(RPMERR_OLDDB, _("old format database is present; "
				"use --rebuilddb to generate a new format database"));
			rc |= 1;
		    }
		}
		xx = (*dbi->dbi_vec->cclose) (dbi);
	    }	break;
	    default:
		break;
	    }
	}
    }

exit:
    if (!(rc || justCheck || dbp == NULL))
	*dbp = rpmdb;
    else
	rpmdbClose(rpmdb);

    return rc;
}

/* XXX python/upgrade.c */
int rpmdbOpenForTraversal(const char * prefix, rpmdb * dbp)
{
    return openDatabase(prefix, NULL, dbp, O_RDONLY, 0644, RPMDB_FLAG_MINIMAL);
}

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    return openDatabase(prefix, NULL, dbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb rpmdb = NULL;
    int rc;

    rc = openDatabase(prefix, NULL, &rpmdb, (O_CREAT | O_RDWR), perms, RPMDB_FLAG_JUSTCHECK);
    if (rpmdb) {
	rpmdbClose(rpmdb);
	rpmdb = NULL;
    }
    return rc;
}

/* XXX depends.c, install.c, query.c, transaction.c, uninstall.c */
Header rpmdbGetRecord(rpmdb rpmdb, unsigned int offset)
{
    int dbix;
    dbiIndex dbi;
    void * uh;
    size_t uhlen;
    void * keyp = &offset;
    size_t keylen = sizeof(offset);
    int rc;

    dbix = 0;	/* RPMDBI_PACKAGES */
    (void) dbiOpenIndex(rpmdb, dbix);
    dbi = rpmdb->_dbi[dbix];
    rc = (*dbi->dbi_vec->get) (dbi, keyp, keylen, &uh, &uhlen);
    if (rc)
	return NULL;
    return headerLoad(uh);
}

static int rpmdbFindByFile(rpmdb rpmdb, const char * filespec,
			/*@out@*/ dbiIndexSet * matches)
{
    const char * dirName;
    const char * baseName;
    fingerPrintCache fpc;
    fingerPrint fp1;
    dbiIndexSet allMatches = NULL;
    dbiIndexRecord rec = NULL;
    int dbix;
    int i;
    int rc;

    *matches = NULL;
    if ((baseName = strrchr(filespec, '/')) != NULL) {
    	char * t;
	size_t len;

    	len = baseName - filespec + 1;
	t = strncpy(alloca(len + 1), filespec, len);
	t[len] = '\0';
	dirName = t;
	baseName++;
    } else {
	dirName = "";
	baseName = filespec;
    }

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    dbix = dbiTagToDbix(RPMTAG_BASENAMES);
    rc = dbiSearchIndex(rpmdb->_dbi[dbix], baseName, 0, &allMatches);
    if (rc) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
	fpCacheFree(fpc);
	return rc;
    }

    *matches = xcalloc(1, sizeof(**matches));
    rec = dbiReturnIndexRecordInstance(0, 0);
    i = 0;
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	int_32 * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

	if ((h = rpmdbGetRecord(rpmdb, offset)) == NULL) {
	    i++;
	    continue;
	}

	headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, 
				(void **) &baseNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, 
				(void **) &dirIndexes, NULL);
	headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
				(void **) &dirNames, NULL);

	do {
	    fingerPrint fp2;
	    int num = dbiIndexRecordFileNumber(allMatches, i);

	    fp2 = fpLookup(fpc, dirNames[dirIndexes[num]], baseNames[num], 1);
	    if (FP_EQUAL(fp1, fp2)) {
		rec->recOffset = dbiIndexRecordOffset(allMatches, i);
		rec->fileNumber = dbiIndexRecordFileNumber(allMatches, i);
		dbiAppendIndexRecord(*matches, rec);
	    }

	    prevoff = offset;
	    i++;
	    offset = dbiIndexRecordOffset(allMatches, i);
	} while (i < allMatches->count && 
		(i == 0 || offset == prevoff));

	free(baseNames);
	free(dirNames);
	headerFree(h);
    }

    if (rec) {
	free(rec);
	rec = NULL;
    }
    if (allMatches) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
    }

    fpCacheFree(fpc);

    if ((*matches)->count == 0) {
	dbiFreeIndexSet(*matches);
	*matches = NULL; 
	return 1;
    }

    return 0;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb rpmdb, const char * name)
{
    dbiIndexSet matches = NULL;
    int dbix;
    int rc;

    dbix = dbiTagToDbix(RPMTAG_NAME);
    rc = dbiSearchIndex(rpmdb->_dbi[dbix], name, 0, &matches);

    switch (rc) {
    default:
    case -1:		/* error */
	rpmError(RPMERR_DBCORRUPT, _("cannot retrieve package \"%s\" from db"),
                name);
	rc = -1;
	break;
    case 1:		/* not found */
	rc = 0;
	break;
    case 0:		/* success */
	rc = dbiIndexSetCount(matches);
	break;
    }

    if (matches)
	dbiFreeIndexSet(matches);

    return rc;
}

struct _rpmdbMatchIterator {
    const void *	mi_key;
    size_t		mi_keylen;
    rpmdb		mi_rpmdb;
    dbiIndex		mi_dbi;
    int			mi_dbix;
    dbiIndexSet		mi_set;
    int			mi_setx;
    Header		mi_h;
    unsigned int	mi_prevoffset;
    unsigned int	mi_offset;
    unsigned int	mi_filenum;
    const char *	mi_version;
    const char *	mi_release;
};

void rpmdbFreeIterator(rpmdbMatchIterator mi)
{
    if (mi == NULL)
	return;

    if (mi->mi_release) {
	xfree(mi->mi_release);
	mi->mi_release = NULL;
    }
    if (mi->mi_version) {
	xfree(mi->mi_version);
	mi->mi_version = NULL;
    }
    if (mi->mi_h) {
	headerFree(mi->mi_h);
	mi->mi_h = NULL;
    }
    if (mi->mi_set) {
	dbiFreeIndexSet(mi->mi_set);
	mi->mi_set = NULL;
    } else {
	int dbix = 0;	/* RPMDBI_PACKAGES */
	dbiIndex dbi = mi->mi_rpmdb->_dbi[dbix];
	(void) (*dbi->dbi_vec->cclose) (dbi);
    }
    if (mi->mi_key) {
	xfree(mi->mi_key);
	mi->mi_key = NULL;
    }
    free(mi);
}

unsigned int rpmdbGetIteratorOffset(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return 0;
    return mi->mi_offset;
}

unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return 0;
    return mi->mi_filenum;
}

int rpmdbGetIteratorCount(rpmdbMatchIterator mi) {
    if (!(mi && mi->mi_set))
	return 0;	/* XXX W2DO? */
    return mi->mi_set->count;
}

void rpmdbSetIteratorRelease(rpmdbMatchIterator mi, const char * release) {
    if (mi == NULL)
	return;
    if (mi->mi_release) {
	xfree(mi->mi_release);
	mi->mi_release = NULL;
    }
    mi->mi_release = (release ? xstrdup(release) : NULL);
}

void rpmdbSetIteratorVersion(rpmdbMatchIterator mi, const char * version) {
    if (mi == NULL)
	return;
    if (mi->mi_version) {
	xfree(mi->mi_version);
	mi->mi_version = NULL;
    }
    mi->mi_version = (version ? xstrdup(version) : NULL);
}

Header rpmdbNextIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi;
    int dbix;
    void * uh;
    size_t uhlen;
    void * keyp;
    size_t keylen;
    int rc;

    if (mi == NULL)
	return NULL;

    dbix = 0;	/* RPMDBI_PACKAGES */
    (void) dbiOpenIndex(mi->mi_rpmdb, dbix);
    dbi = mi->mi_rpmdb->_dbi[dbix];
    keyp = &mi->mi_offset;
    keylen = sizeof(mi->mi_offset);

top:
    /* XXX skip over instances with 0 join key */
    do {
	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    if (mi->mi_dbix != 0) {	/* RPMDBI_PACKAGES */
		mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
		mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	    }
	} else {
	    rc = (*dbi->dbi_vec->cget) (dbi, &keyp, &keylen, NULL, NULL);
	    if (keyp)
		memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));

	    /* Terminate on error or end of keys */
	    if (rc || (mi->mi_setx && mi->mi_offset == 0))
		return NULL;
	}
	mi->mi_setx++;
    } while (mi->mi_offset == 0);

    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset)
	return mi->mi_h;

    /* Retrieve header */
    rc = (*dbi->dbi_vec->get) (dbi, keyp, keylen, &uh, &uhlen);
    if (rc)
	return NULL;

    if (mi->mi_h) {
	headerFree(mi->mi_h);
	mi->mi_h = NULL;
    }

    mi->mi_h = headerLoad(uh);

    if (mi->mi_release) {
	const char *release;
	headerNVR(mi->mi_h, NULL, NULL, &release);
	if (strcmp(mi->mi_release, release))
	    goto top;
    }

    if (mi->mi_version) {
	const char *version;
	headerNVR(mi->mi_h, NULL, &version, NULL);
	if (strcmp(mi->mi_version, version))
	    goto top;
    }

    mi->mi_prevoffset = mi->mi_offset;
    return mi->mi_h;
}

static int intMatchCmp(const void * one, const void * two)
{
    const struct _dbiIndexRecord * a = one;
    const struct _dbiIndexRecord * b = two;

#ifdef	DYING
    if (a->recOffset < b->recOffset)
	return -1;
    else if (a->recOffset > b->recOffset)
	return 1;

    return 0;
#else
    return (a->recOffset - b->recOffset);
#endif
}

static void rpmdbSortIterator(rpmdbMatchIterator mi)
{
    if (!(mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0))
	return;

    qsort(mi->mi_set->recs, mi->mi_set->count, sizeof(*mi->mi_set->recs),
		intMatchCmp);
}

static int rpmdbGrowIterator(rpmdbMatchIterator mi,
	const void * key, size_t keylen, int fpNum)
{
    dbiIndex dbi = NULL;
    dbiIndexSet set = NULL;
    int i;
    int rc;

    if (!(mi && key))
	return 1;

    dbi = mi->mi_rpmdb->_dbi[mi->mi_dbix];

    if (keylen == 0)
	keylen = strlen(key);

    rc = dbiSearchIndex(dbi, key, keylen, &set);

    switch (rc) {
    default:
    case -1:		/* error */
    case 1:		/* not found */
	break;
    case 0:		/* success */
	for (i = 0; i < set->count; i++)
	    set->recs[i].fpNum = fpNum;

	if (mi->mi_set == NULL) {
	    mi->mi_set = set;
	    set = NULL;
	} else {
	    mi->mi_set->recs = xrealloc(mi->mi_set->recs,
		(mi->mi_set->count + set->count) * sizeof(*(mi->mi_set->recs)));
	    memcpy(mi->mi_set->recs + mi->mi_set->count, set->recs,
		set->count * sizeof(*(mi->mi_set->recs)));
	    mi->mi_set->count += set->count;
	}
	break;
    }

    if (set)
	dbiFreeIndexSet(set);
    return rc;
}

rpmdbMatchIterator rpmdbInitIterator(rpmdb rpmdb, int rpmtag,
	const void * key, size_t keylen)
{
    rpmdbMatchIterator mi = NULL;
    dbiIndexSet set = NULL;
    dbiIndex dbi;
    int dbix = dbiTagToDbix(rpmtag);

    if (dbix < 0)
	return NULL;
    (void) dbiOpenIndex(rpmdb, dbix);
    if ((dbi = rpmdb->_dbi[dbix]) == NULL)
	return NULL;

    if (key) {
	int rc;
	if (rpmtag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(rpmdb, key, &set);
	} else {
	    rc = dbiSearchIndex(dbi, key, keylen, &set);
	}
	switch (rc) {
	default:
	case -1:	/* error */
	case 1:		/* not found */
	    if (set)
		dbiFreeIndexSet(set);
	    return NULL;
	    /*@notreached@*/ break;
	case 0:		/* success */
	    break;
	}
    }

    mi = xcalloc(sizeof(*mi), 1);
    if (key) {
	if (keylen == 0)
	    keylen = strlen(key);

	{   char * k = xmalloc(keylen + 1);
	    memcpy(k, key, keylen);
	    k[keylen] = '\0';	/* XXX for strings */
	    mi->mi_key = k;
	}

	mi->mi_keylen = keylen;
    } else {
	mi->mi_key = NULL;
	mi->mi_keylen = 0;
    }
    mi->mi_rpmdb = rpmdb;
    mi->mi_dbi = dbi;

    /* XXX falloc has dbi == NULL ) */
    assert(!(dbi && dbi->dbi_dbcursor));

    mi->mi_dbix = dbix;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_h = NULL;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_filenum = 0;
    mi->mi_version = NULL;
    mi->mi_release = NULL;
    return mi;
}

static inline int removeIndexEntry(dbiIndex dbi, const char * key, dbiIndexRecord rec,
		             int tolerant, const char * idxName)
{
    dbiIndexSet set = NULL;
    int rc;
    
    rc = dbiSearchIndex(dbi, key, 0, &set);

    switch (rc) {
    case -1:			/* error */
	rc = 1;
	break;   /* error message already generated from dbindex.c */
    case 1:			/* not found */
	rc = 0;
	if (!tolerant) {
	    rpmError(RPMERR_DBCORRUPT, _("key \"%s\" not found in %s"), 
			key, idxName);
	    rc = 1;
	}
	break;
    case 0:			/* success */
	if (dbiRemoveIndexRecord(set, rec)) {
	    if (!tolerant) {
		rpmError(RPMERR_DBCORRUPT, _("key \"%s\" not removed from %s"),
				key, idxName);
		rc = 1;
	    }
	    break;
	}
	if (dbiUpdateIndex(dbi, key, set))
	    rc = 1;
	break;
    }

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return rc;
}

/* XXX uninstall.c */
int rpmdbRemove(rpmdb rpmdb, unsigned int offset, int tolerant)
{
    Header h;

    h = rpmdbGetRecord(rpmdb, offset);
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("rpmdbRemove: cannot read header at 0x%x"),
	      offset);
	return 1;
    }

    {	const char *n, *v, *r;
	headerNVR(h, &n, &v, &r);
	rpmMessage(RPMMESS_VERBOSE, "  --- %s-%s-%s\n", n, v, r);
    }

    blockSignals();

    {	int dbix;
	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < rpmdb->db_ndbi; dbix++) {
	    dbiIndex dbi;
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    /* XXX FIXME: this forces all indices open */
	    (void) dbiOpenIndex(rpmdb, dbix);
	    dbi = rpmdb->_dbi[dbix];

	    if (dbi->dbi_rpmtag == 0) {
		(void) (*dbi->dbi_vec->del) (dbi, &offset, sizeof(offset));
		continue;
	    }
	
	    if (!headerGetEntry(h, dbi->dbi_rpmtag, &rpmtype,
		(void **) &rpmvals, &rpmcnt)) {
#if 0
		rpmMessage(RPMMESS_DEBUG, _("removing 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
#endif
		continue;
	    }

	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("removing \"%s\" from %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		(void) removeIndexEntry(dbi, (const char *)rpmvals,
			rec, tolerant, dbi->dbi_basename);
	    } else {
		int i, mytolerant;

		rpmMessage(RPMMESS_DEBUG, _("removing %d entries in %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

		for (i = 0; i < rpmcnt; i++) {
		    rpmMessage(RPMMESS_DEBUG, _("\t%6d %s\n"),
			i, rpmvals[i]);

		    mytolerant = tolerant;
		    rec->fileNumber = 0;

		    switch (dbi->dbi_rpmtag) {
		    case RPMTAG_BASENAMES:
			rec->fileNumber = i;
			break;
		    /*
		     * There could be dups in the sorted list. Rather then
		     * sort the list, be tolerant of missing entries as they
		     * should just indicate duplicated entries.
		     */
		    case RPMTAG_REQUIRENAME:
		    case RPMTAG_TRIGGERNAME:
			mytolerant = 1;
			break;
		    }

		    (void) removeIndexEntry(dbi, rpmvals[i],
			rec, mytolerant, dbi->dbi_basename);
		}
	    }

	    dbiSyncIndex(dbi);

	    switch (rpmtype) {
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		xfree(rpmvals);
		rpmvals = NULL;
		break;
	    }
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	if (rec) {
	    free(rec);
	    rec = NULL;
	}
    }

    unblockSignals();

    headerFree(h);

    return 0;
}

static inline int addIndexEntry(dbiIndex dbi, const char *index, dbiIndexRecord rec)
{
    dbiIndexSet set = NULL;
    int rc;

    rc = dbiSearchIndex(dbi, index, 0, &set);

    switch (rc) {
    default:
    case -1:			/* error */
	rc = 1;
	break;
    case 1:			/* not found */
	rc = 0;
	set = xcalloc(1, sizeof(*set));
	/*@fallthrough@*/
    case 0:			/* success */
	dbiAppendIndexRecord(set, rec);
	if (dbiUpdateIndex(dbi, index, set))
	    rc = 1;
	break;
    }

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return 0;
}

/* XXX install.c, rebuilddb.c */
int rpmdbAdd(rpmdb rpmdb, Header h)
{
    const char ** baseNames;
    int count = 0;
    int type;
    dbiIndex dbi;
    int dbix;
    unsigned int offset;
    int rc = 0;

    /*
     * If old style filename tags is requested, the basenames need to be
     * retrieved early, and the header needs to be converted before
     * being written to the package header database.
     */

    headerGetEntry(h, RPMTAG_BASENAMES, &type, (void **) &baseNames, &count);

    if (_noDirTokens)
	expandFilelist(h);

    blockSignals();

    {
	unsigned int firstkey = 0;
	void * keyp = &firstkey;
	size_t keylen = sizeof(firstkey);
	void * datap = NULL;
	size_t datalen = 0;
	int rc;

	dbix = 0;	/* RPMDBI_PACKAGES */
	(void) dbiOpenIndex(rpmdb, dbix);
	dbi = rpmdb->_dbi[dbix];

	/* XXX hack to pass sizeof header to fadAlloc */
	datap = h;
	datalen = headerSizeof(h, HEADER_MAGIC_NO);

	/* Retrieve join key for next header instance. */

	rc = (*dbi->dbi_vec->get) (dbi, keyp, keylen, (void *)&datap, &datalen);

	offset = 0;
	if (rc == 0 && datap)
	    memcpy(&offset, datap, sizeof(offset));

	++offset;
	if (datap) {
	    memcpy(datap, &offset, sizeof(offset));
	} else {
	    datap = &offset;
	    datalen = sizeof(offset);
	}

	rc = (*dbi->dbi_vec->put) (dbi, keyp, keylen, datap, datalen);
    }

    if (rc) {
	rpmError(RPMERR_DBCORRUPT, _("cannot allocate new instance in database"));
	goto exit;
    }

    /* Now update the indexes */

    {	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < rpmdb->db_ndbi; dbix++) {
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    /* XXX FIXME: this forces all indices open */
	    (void) dbiOpenIndex(rpmdb, dbix);
	    dbi = rpmdb->_dbi[dbix];

	    if (dbi->dbi_rpmtag == 0) {
		size_t uhlen = headerSizeof(h, HEADER_MAGIC_NO);
		void * uh = headerUnload(h);
		(void) (*dbi->dbi_vec->put) (dbi, &offset, sizeof(offset), uh, uhlen);
		free(uh);

		{   const char *n, *v, *r;
		    headerNVR(h, &n, &v, &r);
		    rpmMessage(RPMMESS_VERBOSE, "  +++ %8d %s-%s-%s\n", offset, n, v, r);
		}

		continue;
	    }
	
	    /* XXX preserve legacy behavior */
	    switch (dbi->dbi_rpmtag) {
	    case RPMTAG_BASENAMES:
		rpmtype = type;
		rpmvals = baseNames;
		rpmcnt = count;
		break;
	    default:
		headerGetEntry(h, dbi->dbi_rpmtag, &rpmtype,
			(void **) &rpmvals, &rpmcnt);
		break;
	    }

	    if (rpmcnt <= 0) {
		if (dbi->dbi_rpmtag != RPMTAG_GROUP) {
#if 0
		    rpmMessage(RPMMESS_DEBUG, _("adding 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
#endif
		    continue;
		}

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }

	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("adding \"%s\" to %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		rc += addIndexEntry(dbi, (const char *)rpmvals, rec);
	    } else {
		int i, j;

		rpmMessage(RPMMESS_DEBUG, _("adding %d entries to %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

		for (i = 0; i < rpmcnt; i++) {
		    rpmMessage(RPMMESS_DEBUG, _("%6d %s\n"),
			i, rpmvals[i]);

		    rec->fileNumber = 0;

		    switch (dbi->dbi_rpmtag) {
		    case RPMTAG_BASENAMES:
			rec->fileNumber = i;
			break;
		    case RPMTAG_TRIGGERNAME:	/* don't add duplicates */
			if (i == 0)
			   break;
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				break;
			}
			if (j < i)
			    continue;
			break;
		    }

		    rc += addIndexEntry(dbi, rpmvals[i], rec);
		}
	    }

	    dbiSyncIndex(dbi);

	    switch (rpmtype) {
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		xfree(rpmvals);
		rpmvals = NULL;
		break;
	    }
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	if (rec) {
	    free(rec);
	    rec = NULL;
	}
    }

exit:
    unblockSignals();

    return rc;
}

/* XXX install.c */
int rpmdbUpdateRecord(rpmdb rpmdb, int offset, Header newHeader)
{
    int rc = 0;

    if (rpmdbRemove(rpmdb, offset, 1))
	return 1;

    if (rpmdbAdd(rpmdb, newHeader)) 
	return 1;

    return rc;
}

void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath)
{ 
    int i;
    char * filename;

    i = strlen(dbpath);
    if (dbpath[i - 1] != '/') {
	filename = alloca(i);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    
    filename = alloca(strlen(rootdir) + strlen(dbpath) + 40);

    {	dbiIndex dbi;
	int i;

	for (dbi = rpmdbi; dbi->dbi_basename != NULL; dbi++) {
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, dbi->dbi_basename);
	    unlink(filename);
	}
        for (i = 0; i < 16; i++) {
	    sprintf(filename, "%s/%s/__db.%03d", rootdir, dbpath, i);
	    unlink(filename);
	}
        for (i = 0; i < 4; i++) {
	    sprintf(filename, "%s/%s/packages.db%d", rootdir, dbpath, i);
	    unlink(filename);
	}
    }

    sprintf(filename, "%s/%s", rootdir, dbpath);
    rmdir(filename);

}

int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath)
{
    int i;
    char * ofilename, * nfilename;
    int rc = 0;
 
    i = strlen(olddbpath);
    if (olddbpath[i - 1] != '/') {
	ofilename = alloca(i + 2);
	strcpy(ofilename, olddbpath);
	ofilename[i] = '/';
	ofilename[i + 1] = '\0';
	olddbpath = ofilename;
    }
    
    i = strlen(newdbpath);
    if (newdbpath[i - 1] != '/') {
	nfilename = alloca(i + 2);
	strcpy(nfilename, newdbpath);
	nfilename[i] = '/';
	nfilename[i + 1] = '\0';
	newdbpath = nfilename;
    }
    
    ofilename = alloca(strlen(rootdir) + strlen(olddbpath) + 40);
    nfilename = alloca(strlen(rootdir) + strlen(newdbpath) + 40);

    switch(_useDbiMajor) {
    case 3:
      {	int i;
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, "packages.db3");
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, "packages.db3");
	    (void)rpmCleanPath(ofilename);
	    (void)rpmCleanPath(nfilename);
	    if (Rename(ofilename, nfilename)) rc = 1;
        for (i = 0; i < 16; i++) {
	    sprintf(ofilename, "%s/%s/__db.%03d", rootdir, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/__db.%03d", rootdir, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if (Rename(ofilename, nfilename)) rc = 1;
	}
        for (i = 0; i < 4; i++) {
	    sprintf(ofilename, "%s/%s/packages.db%d", rootdir, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/packages.db%d", rootdir, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if (Rename(ofilename, nfilename)) rc = 1;
	}
      }	break;
    case 2:
    case 1:
    case 0:
      {	dbiIndex dbi;
	int i;
	for (dbi = rpmdbi; dbi->dbi_basename != NULL; dbi++) {
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, dbi->dbi_basename);
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, dbi->dbi_basename);
	    (void)rpmCleanPath(ofilename);
	    (void)rpmCleanPath(nfilename);
	    if (Rename(ofilename, nfilename))
		rc = 1;
	}
        for (i = 0; i < 16; i++) {
	    sprintf(ofilename, "%s/%s/__db.%03d", rootdir, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (rpmfileexists(ofilename))
		unlink(ofilename);
	    sprintf(nfilename, "%s/%s/__db.%03d", rootdir, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if (rpmfileexists(nfilename))
		unlink(nfilename);
	}
      }	break;
    }

    return rc;
}

/* XXX transaction.c */
int rpmdbFindFpList(rpmdb rpmdb, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
    rpmdbMatchIterator mi;
    fingerPrintCache fpc;
    Header h;
    int i;

    mi = rpmdbInitIterator(rpmdb, RPMTAG_BASENAMES, NULL, 0);

    /* Gather all matches from the database */
    for (i = 0; i < numItems; i++) {
	rpmdbGrowIterator(mi, fpList[i].baseName, 0, i);
	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));
    }

    if ((i = rpmdbGetIteratorCount(mi)) == 0) {
	rpmdbFreeIterator(mi);
	return 0;
    }
    fpc = fpCacheCreate(i);

    rpmdbSortIterator(mi);
    /* iterator is now sorted by (recnum, filenum) */

    /* For each set of files matched in a package ... */
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char ** dirNames;
	const char ** baseNames;
	const char ** fullBaseNames;
	int_32 * dirIndexes;
	int_32 * fullDirIndexes;
	fingerPrint * fps;
	dbiIndexRecord im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->recOffset != mi->mi_set->recs[end].recOffset)
		break;
	}
	num = end - start;

	/* Compute fingerprints for this header's matches */
	headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
			    (void **) &dirNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, 
			    (void **) &fullBaseNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, 
			    (void **) &fullDirIndexes, NULL);

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].fileNumber];
	    dirIndexes[i] = fullDirIndexes[im[i].fileNumber];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < num; i++, im++) {
	    if (FP_EQUAL_DIFFERENT_CACHE(fps[i], fpList[im->fpNum]))
		dbiAppendIndexRecord(matchList[im->fpNum], im);
	}

	free(fps);
	free(dirNames);
	free(fullBaseNames);
	free(baseNames);
	free(dirIndexes);

	mi->mi_setx = end;
    }

    rpmdbFreeIterator(mi);

    fpCacheFree(fpc);

    return 0;

}

/* XXX transaction.c */
/* 0 found matches */
/* 1 no matches */
/* 2 error */
int findMatches(rpmdb rpmdb, const char * name, const char * version,
			const char * release, dbiIndexSet * matches)
{
    int dbix;
    int gotMatches;
    int rc;
    int i;

    dbix = dbiTagToDbix(RPMTAG_NAME);
    (void) dbiOpenIndex(rpmdb, dbix);
    rc = dbiSearchIndex(rpmdb->_dbi[dbix], name, 0, matches);
    if (rc != 0) {
	rc = ((rc == -1) ? 2 : 1);
	goto exit;
    }

    if (!version && !release) {
	rc = 0;
	goto exit;
    }

    gotMatches = 0;

    /* make sure the version and releases match */
    for (i = 0; i < dbiIndexSetCount(*matches); i++) {
	unsigned int recoff = dbiIndexRecordOffset(*matches, i);
	int goodRelease, goodVersion;
	const char * pkgVersion;
	const char * pkgRelease;
	Header h;

	if (recoff == 0)
	    continue;

	h = rpmdbGetRecord(rpmdb, recoff);
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT,_("cannot read header at %d for lookup"), 
		recoff);
	    rc = 2;
	    goto exit;
	}

	headerNVR(h, NULL, &pkgVersion, &pkgRelease);
	    
	goodRelease = goodVersion = 1;

	if (release && strcmp(release, pkgRelease)) goodRelease = 0;
	if (version && strcmp(version, pkgVersion)) goodVersion = 0;

	if (goodRelease && goodVersion) 
	    gotMatches = 1;
	else 
	    dbiIndexRecordOffsetSave(*matches, i, 0);

	headerFree(h);
    }

    if (!gotMatches) {
	rc = 1;
	goto exit;
    }
    rc = 0;

exit:
    if (rc && matches && *matches) {
	dbiFreeIndexSet(*matches);
	*matches = NULL;
    }
    return rc;
}

/* XXX query.c, rpminstall.c */
/* 0 found matches */
/* 1 no matches */
/* 2 error */
int rpmdbFindByLabel(rpmdb rpmdb, const char * arg, dbiIndexSet * matches)
{
    char * localarg, * chptr;
    char * release;
    int rc;
 
    if (!strlen(arg)) return 1;

    /* did they give us just a name? */
    rc = findMatches(rpmdb, arg, NULL, NULL, matches);
    if (rc != 1) return rc;

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    strcpy(localarg, arg);

    chptr = (localarg + strlen(localarg)) - 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    rc = findMatches(rpmdb, localarg, chptr + 1, NULL, matches);
    if (rc != 1) return rc;
    
    /* how about name-version-release? */

    release = chptr + 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    return findMatches(rpmdb, localarg, chptr + 1, release, matches);
}
