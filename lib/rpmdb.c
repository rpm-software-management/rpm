#include "system.h"

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>
#include <assert.h>

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath/rpmGenPath */

#include "dbindex.h"
/*@access dbiIndexSet@*/
/*@access dbiIndexRecord@*/
/*@access rpmdbMatchIterator@*/

#include "falloc.h"
#include "fprint.h"
#include "misc.h"
#include "rpmdb.h"

extern int _noDirTokens;
extern int _useDbiMajor;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

struct _dbiIndex rpmdbi[] = {
    { "packages.rpm", 0,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "nameindex.rpm", RPMTAG_NAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "fileindex.rpm", RPMTAG_BASENAMES,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "groupindex.rpm", RPMTAG_GROUP,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "requiredby.rpm", RPMTAG_REQUIRENAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "providesindex.rpm", RPMTAG_PROVIDENAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "conflictsindex.rpm", RPMTAG_CONFLICTNAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { "triggerindex.rpm", RPMTAG_TRIGGERNAME,
	DBI_HASH, _DBI_FLAGS, _DBI_PERMS, _DBI_MAJOR, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL }
#define	RPMDBI_MIN		0
#define	RPMDBI_MAX		8
};

/**
 * Destroy element of index database set.
 * @param rec	element of index database set.
 */
static void inline dbiFreeIndexRecordInstance(dbiIndexRecord rec) {
    if (rec) free(rec);
}

/**
 * Create and initialize element of index database set.
 * @param recOffset	byte offset of header in db
 * @param fileNumber	file array index
 * @return	new element
 */
static dbiIndexRecord inline dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber) {
    dbiIndexRecord rec = xmalloc(sizeof(*rec));
    rec->recOffset = recOffset;
    rec->fileNumber = fileNumber;
    return rec;
}

/**
 * Flush pending operations to disk.
 * @param dbi	index database handle
 */
static inline int dbiSyncIndex(dbiIndex dbi) {
    int rc;

    rc = (*dbi->dbi_vec->sync) (dbi, 0);
    return rc;
}

/**
 * Return next index database key.
 * @param dbi	index database handle
 * @param keyp	address of first key
 * @param keylenp address of first key size
 * @return	0 success - fails if rec is not found
 */
static inline int dbiGetNextKey(dbiIndex dbi, void ** keyp, size_t * keylenp) {
    int rc;

    if (dbi == NULL)
	return 1;

    rc = (*dbi->dbi_vec->cget) (dbi, keyp, keylenp, NULL, NULL);

    return rc;
}

/**
 * Close cursor (if any);
 * @param dbi	index database handle
 * @return	0 success
 */
static inline int dbiFreeCursor(dbiIndex dbi) {
    int rc;

    if (dbi == NULL)
	return 1;

    rc = (*dbi->dbi_vec->cclose) (dbi);
    return rc;
}

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
static inline int dbiSearchIndex(dbiIndex dbi, const char * str, size_t len,
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
static inline int dbiUpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set) {
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

/* XXX the signal handling in here is not thread safe */

/* the requiredbyIndex isn't stricly necessary. In a perfect world, we could
   have each header keep a list of packages that need it. However, we
   can't reserve space in the header for extra information so all of the
   required packages would move in the database every time a package was
   added or removed. Instead, each package (or virtual package) name
   keeps a list of package offsets of packages that might depend on this
   one. Version numbers still need verification, but it gets us in the
   right area w/o a linear search through the database. */

struct rpmdb_s {
    const char *	db_root;
    const char *	db_home;
    int			db_flags;
    DBI_TYPE		db_type;
    int			db_mode;
    int			db_perms;
    int			db_major;
    int			db_mp_mmapsize;
    int			db_mp_size;
    int			db_cachesize;
    int			db_pagesize;
    unsigned int	db_h_ffactor;
    unsigned int	db_h_nelem;
    unsigned int	db_h_flags;
    dbiIndex		_dbi[RPMDBI_MAX];
#ifdef	DYING
#define	nameIndex		_dbi[RPMDBI_NAME]
#define	fileIndex		_dbi[RPMDBI_FILE]
#define	groupIndex		_dbi[RPMDBI_GROUP]
#define	requiredbyIndex		_dbi[RPMDBI_REQUIREDBY]
#define	providesIndex		_dbi[RPMDBI_PROVIDES]
#define	conflictsIndex		_dbi[RPMDBI_CONFLICTS]
#define	triggerIndex		_dbi[RPMDBI_TRIGGER]
#endif
};

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
#define	_DB_MP_MMAPSIZE	16 * 1024 * 1024
#define	_DB_MP_SIZE	2 * 1024 * 1024
#define	_DB_CACHESIZE	0
#define	_DB_PAGESIZE	0
#define	_DB_H_FFACTOR	0
#define	_DB_H_NELEM	0
#define	_DB_H_FLAGS	0

static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS,
    _DB_TYPE,	_DB_MODE, _DB_PERMS, _DB_MAJOR,
    _DB_MAJOR,	_DB_MP_MMAPSIZE, _DB_MP_SIZE, _DB_CACHESIZE, _DB_PAGESIZE,
		_DB_H_FFACTOR, _DB_H_NELEM, _DB_H_FLAGS
};

void rpmdbClose (rpmdb db)
{
    int dbix;

    for (dbix = RPMDBI_MAX; --dbix >= RPMDBI_MIN; ) {
	if (db->_dbi[dbix] == NULL)
	    continue;
    	dbiCloseIndex(db->_dbi[dbix]);
    	db->_dbi[dbix] = NULL;
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

    db->db_root = (root ? root : ""); 	/* accept NULL as a valid prefix */
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
    return db;

errxit:
    if (db)
	rpmdbClose(db);
    return db;
}

int openDatabase(const char * prefix, const char * dbpath, rpmdb *dbp,
		int mode, int perms, int flags)
{
    rpmdb db;
    int rc;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
	return 1;

    db = newRpmdb(prefix, dbpath, mode, perms, flags);

    {	int dbix;

	rc = 0;
	for (dbix = RPMDBI_MIN; rc == 0 && dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbiTemplate;
	    const char * filename;

	    dbiTemplate = rpmdbi + dbix;

	    filename = rpmGenPath(db->db_root, db->db_home,
		dbiTemplate->dbi_basename);

	    if (!justCheck || !rpmfileexists(filename)) {
		db->_dbi[dbix] = dbiOpenIndex(filename, db->db_mode, dbiTemplate);
	    }

	    if (db->_dbi[dbix] == NULL)
		continue;

	    switch (dbix) {
	    case 1:
		if (minimal)
		    goto exit;
		break;
	    case 2:
	    {	void * keyp = NULL;

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
		if (!justCheck && !dbiGetNextKey(db->_dbi[RPMDBI_FILE], &keyp, NULL)) {
		    const char * akey;
		    akey = keyp;
		    if (strchr(akey, '/')) {
			rpmError(RPMERR_OLDDB, _("old format database is present; "
				"use --rebuilddb to generate a new format database"));
			rc |= 1;
		    }
		    dbiFreeCursor(db->_dbi[RPMDBI_FILE]);
		}
	    }	break;
	    default:
		break;
	    }
	}
    }

exit:
    if (!(rc || justCheck || dbp == NULL))
	*dbp = db;
    else
	rpmdbClose(db);

    return rc;
}

#ifdef	DYING
static int doRpmdbOpen (const char * prefix, /*@out@*/ rpmdb * dbp,
			int mode, int perms, int flags)
{
    const char * dbpath = rpmGetPath("%{_dbpath}", NULL);
    int rc;

    if (!(dbpath && dbpath[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
    } else
    	rc = openDatabase(prefix, dbpath, dbp, mode, perms, flags);
    xfree(dbpath);
    return rc;
}
#endif

/* XXX called from python/upgrade.c */
int rpmdbOpenForTraversal(const char * prefix, rpmdb * dbp)
{
#ifdef	DYING
    return doRpmdbOpen(prefix, dbp, O_RDONLY, 0644, RPMDB_FLAG_MINIMAL);
#else
    return openDatabase(prefix, NULL, dbp, O_RDONLY, 0644, RPMDB_FLAG_MINIMAL);
#endif
}

/* XXX called from python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
#ifdef	DYING
    return doRpmdbOpen(prefix, dbp, mode, perms, 0);
#else
    return openDatabase(prefix, NULL, dbp, mode, perms, 0);
#endif
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb db;
#ifdef	DYING
    return doRpmdbOpen(prefix, &db, (O_CREAT | O_RDWR), perms, RPMDB_FLAG_JUSTCHECK);
#else
    int rc;
    rc = openDatabase(prefix, NULL, &db, (O_CREAT | O_RDWR), perms, RPMDB_FLAG_JUSTCHECK);
    if (db) {
	rpmdbClose(db);
	db = NULL;
    }
    return rc;
#endif
}

Header rpmdbGetRecord(rpmdb db, unsigned int offset)
{
    dbiIndex dbi = db->_dbi[RPMDBI_PACKAGES];
    void * uh;
    size_t uhlen;
    void * keyp = &offset;
    size_t keylen = sizeof(offset);
    int rc;

    rc = (*dbi->dbi_vec->get) (dbi, keyp, keylen, &uh, &uhlen);
    if (rc)
	return NULL;
    return headerLoad(uh);
}

static int rpmdbFindByFile(rpmdb db, const char * filespec,
			/*@out@*/ dbiIndexSet * matches)
{
    const char * dirName;
    const char * baseName;
    fingerPrintCache fpc;
    fingerPrint fp1;
    dbiIndexSet allMatches = NULL;
    dbiIndexRecord rec = NULL;
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

    rc = dbiSearchIndex(db->_dbi[RPMDBI_FILE], baseName, 0, &allMatches);
    if (rc) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
	fpCacheFree(fpc);
	return rc;
    }

    *matches = dbiCreateIndexSet();
    rec = dbiReturnIndexRecordInstance(0, 0);
    i = 0;
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	int_32 * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

	if ((h = rpmdbGetRecord(db, offset)) == NULL) {
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
	dbiFreeIndexRecordInstance(rec);
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

int rpmdbFindPackage(rpmdb db, const char * name, dbiIndexSet * matches) {
    return dbiSearchIndex(db->_dbi[RPMDBI_NAME], name, 0, matches);
}

int rpmdbCountPackages(rpmdb db, const char * name)
{
    dbiIndexSet matches = NULL;
    int rc;

    rc = dbiSearchIndex(db->_dbi[RPMDBI_NAME], name, 0, &matches);

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
    rpmdb		mi_db;
    dbiIndex		mi_dbi;
    int			mi_dbix;
    dbiIndexSet		mi_set;
    int			mi_setx;
    Header		mi_h;
    unsigned int	mi_offset;
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
	dbiIndex dbi = mi->mi_db->_dbi[RPMDBI_PACKAGES];
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
    void * uh;
    size_t uhlen;
    void * keyp;
    size_t keylen;
    int rc;

    if (mi == NULL)
	return NULL;

    dbi = mi->mi_db->_dbi[RPMDBI_PACKAGES];
    keyp = &mi->mi_offset;
    keylen = sizeof(mi->mi_offset);

top:
    /* XXX skip over instances with 0 join key */
    do {
	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	    mi->mi_setx++;
	} else {
	    rc = (*dbi->dbi_vec->cget) (dbi, &keyp, &keylen, NULL, NULL);
	    if (rc)
		return NULL;
	    memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));
	}
    } while (mi->mi_offset == 0);

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

    return mi->mi_h;
}

rpmdbMatchIterator rpmdbInitIterator(rpmdb db, int dbix, const void * key, size_t keylen)
{
    rpmdbMatchIterator mi;
    dbiIndex dbi = NULL;
    dbiIndexSet set = NULL;

    dbi = db->_dbi[dbix];

    if (key) {
	int rc;
	if (dbix == RPMDBI_FILE) {
	    rc = rpmdbFindByFile(db, key, &set);
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
    mi->mi_db = db;
    mi->mi_dbi = dbi;
    assert(dbi->dbi_dbcursor == NULL);
    mi->mi_dbix = dbix;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_h = NULL;
    mi->mi_offset = 0;
    mi->mi_version = NULL;
    mi->mi_release = NULL;
    return mi;
}

static void removeIndexEntry(dbiIndex dbi, const char * key, dbiIndexRecord rec,
		             int tolerant, const char * idxName)
{
    dbiIndexSet matches = NULL;
    int rc;
    
    rc = dbiSearchIndex(dbi, key, 0, &matches);
    switch (rc) {
      case 0:
	if (dbiRemoveIndexRecord(matches, rec)) {
	    if (!tolerant)
		rpmError(RPMERR_DBCORRUPT, _("package not found with key \"%s\" in %s"),
				key, idxName);
	} else {
	    dbiUpdateIndex(dbi, key, matches);
	       /* errors from above will be reported from dbindex.c */
	}
	break;
      case 1:
	if (!tolerant) 
	    rpmError(RPMERR_DBCORRUPT, _("key \"%s\" not found in %s"), 
			key, idxName);
	break;
      case 2:
	break;   /* error message already generated from dbindex.c */
    }
    if (matches) {
	dbiFreeIndexSet(matches);
	matches = NULL;
    }
}

int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant)
{
    Header h;

    h = rpmdbGetRecord(db, offset);
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("rpmdbRemove: cannot read header at 0x%x"),
	      offset);
	return 1;
    }

    blockSignals();

    {	int dbix;
	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < RPMDBI_MAX; dbix++) {
	    dbiIndex dbi;
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    dbi = db->_dbi[dbix];

	    if (dbi->dbi_rpmtag == 0) {
		(void) (*dbi->dbi_vec->del) (dbi, &offset, sizeof(offset));
		continue;
	    }
	
	    if (!headerGetEntry(h, dbi->dbi_rpmtag, &rpmtype,
		(void **) &rpmvals, &rpmcnt)) {
		rpmMessage(RPMMESS_DEBUG, _("removing 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
		continue;
	    }

	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("removing \"%s\" from %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		removeIndexEntry(dbi, (const char *)rpmvals,
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

		    removeIndexEntry(dbi, rpmvals[i],
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
	dbiFreeIndexRecordInstance(rec);
    }

    unblockSignals();

    headerFree(h);

    return 0;
}

static int addIndexEntry(dbiIndex dbi, const char *index, dbiIndexRecord rec)
{
    dbiIndexSet set = NULL;
    int rc;

    rc = dbiSearchIndex(dbi, index, 0, &set);

    switch (rc) {
    case -1:			/* error */
	if (set) {
	    dbiFreeIndexSet(set);
	    set = NULL;
	}
	return 1;
	/*@notreached@*/ break;
    case 1:			/* new item */
	set = dbiCreateIndexSet();
	break;
    default:
	break;
    }

    dbiAppendIndexRecord(set, rec);
    if (dbiUpdateIndex(dbi, index, set))
	exit(EXIT_FAILURE);	/* XXX W2DO? return 1; */

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return 0;
}

int rpmdbAdd(rpmdb db, Header h)
{
    const char ** baseNames;
    int count = 0;
    int type;
    dbiIndex dbi;
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

	dbi = db->_dbi[RPMDBI_PACKAGES];

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

    {	int dbix;
	dbiIndexRecord rec = dbiReturnIndexRecordInstance(offset, 0);

	for (dbix = RPMDBI_MIN; dbix < RPMDBI_MAX; dbix++) {
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;

	    dbi = db->_dbi[dbix];

	    if (dbi->dbi_rpmtag == 0) {
		size_t uhlen = headerSizeof(h, HEADER_MAGIC_NO);
		void * uh = headerUnload(h);
		(void) (*dbi->dbi_vec->put) (dbi, &offset, sizeof(offset), uh, uhlen);
		free(uh);
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
		    rpmMessage(RPMMESS_DEBUG, _("adding 0 %s entries.\n"),
			tagName(dbi->dbi_rpmtag));
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
		    rpmMessage(RPMMESS_DEBUG, _("\t%6d %s\n"),
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
	dbiFreeIndexRecordInstance(rec);
    }

exit:
    unblockSignals();

    return rc;
}

int rpmdbUpdateRecord(rpmdb db, int offset, Header newHeader)
{
    int rc = 0;

    if (rpmdbRemove(db, offset, 1))
	return 1;

    if (rpmdbAdd(db, newHeader)) 
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

#ifdef	DYING
typedef struct intMatch {
    unsigned int recOffset;
    unsigned int fileNumber;
    int fpNum;
} IM_t;
#else
typedef	struct _dbiIndexRecord IM_t;
#endif

static int intMatchCmp(const void * one, const void * two)
{
    const IM_t * a = one;
    const IM_t * b = two;

    if (a->recOffset < b->recOffset)
	return -1;
    else if (a->recOffset > b->recOffset)
	return 1;

    return 0;
}

int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
    int numIntMatches = 0;
    int intMatchesAlloced = numItems;
    IM_t * intMatches;
    int i, j;
    int start, end;
    int num;
    int_32 fc;
    const char ** dirNames, ** baseNames;
    const char ** fullBaseNames;
    int_32 * dirIndexes, * fullDirIndexes;
    fingerPrintCache fpc;
    dbiIndexRecord rec = NULL;

    /* this may be worth batching by baseName, but probably not as
       baseNames are quite unique as it is */

    intMatches = xcalloc(intMatchesAlloced, sizeof(*intMatches));

    /* Gather all matches from the database */
    for (i = 0; i < numItems; i++) {
	dbiIndexSet matches = NULL;
	int rc;
	rc = dbiSearchIndex(db->_dbi[RPMDBI_FILE], fpList[i].baseName, 0, &matches);
	switch (rc) {
	default:
	    break;
	case 2:
	    if (matches) {
		dbiFreeIndexSet(matches);
		matches = NULL;
	    }
	    free(intMatches);
	    return 1;
	    /*@notreached@*/ break;
	case 0:
	    if ((numIntMatches + matches->count) >= intMatchesAlloced) {
		intMatchesAlloced += matches->count;
		intMatchesAlloced += intMatchesAlloced / 5;
		intMatches = xrealloc(intMatches, 
				     sizeof(*intMatches) * intMatchesAlloced);
	    }

	    for (j = 0; j < matches->count; j++) {
		IM_t * im;
		
		im = intMatches + numIntMatches;
		im->recOffset = dbiIndexRecordOffset(matches, j);
		im->fileNumber = dbiIndexRecordFileNumber(matches, j);
		im->fpNum = i;
		numIntMatches++;
	    }

	    break;
	}
	if (matches) {
	    dbiFreeIndexSet(matches);
	    matches = NULL;
	}
    }

    qsort(intMatches, numIntMatches, sizeof(*intMatches), intMatchCmp);
    /* intMatches is now sorted by (recnum, filenum) */

    for (i = 0; i < numItems; i++)
	matchList[i] = dbiCreateIndexSet();

    fpc = fpCacheCreate(numIntMatches);

    rec = dbiReturnIndexRecordInstance(0, 0);

    /* For each set of files matched in a package ... */
    for (start = 0; start < numIntMatches; start = end) {
	IM_t * im;
	Header h;
	fingerPrint * fps;

	im = intMatches + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < numIntMatches; end++) {
	    if (im->recOffset != intMatches[end].recOffset)
		break;
	}
	num = end - start;

	/* Compute fingerprints for each file match in this package. */
	h = rpmdbGetRecord(db, im->recOffset);
	if (h == NULL) {
	    free(intMatches);
	    return 1;
	}

	headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
			    (void **) &dirNames, NULL);
	headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, 
			    (void **) &fullBaseNames, &fc);
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

	free(dirNames);
	free(fullBaseNames);
	free(baseNames);
	free(dirIndexes);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	
	for (i = 0; i < num; i++) {
	    j = im[i].fpNum;
	    if (FP_EQUAL_DIFFERENT_CACHE(fps[i], fpList[j])) {
		rec->recOffset = im[i].recOffset;
		rec->fileNumber = im[i].fileNumber;
		dbiAppendIndexRecord(matchList[j], rec);
	    }
	}

	headerFree(h);

	free(fps);
    }

    dbiFreeIndexRecordInstance(rec);
    fpCacheFree(fpc);
    free(intMatches);

    return 0;
}
