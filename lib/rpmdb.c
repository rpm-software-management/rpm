#include "system.h"

static int _debug = 0;
#define	INLINE

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include <rpmlib.h>
#include <rpmurl.h>	/* XXX for assert.h */
#include <rpmmacro.h>	/* XXX for rpmGetPath/rpmGenPath */

#include "rpmdb.h"
/*@access dbiIndexSet@*/
/*@access dbiIndexItem@*/
/*@access rpmdbMatchIterator@*/

#include "fprint.h"
#include "misc.h"

extern int _noDirTokens;

int _filterDbDups = 0;	/* Filter duplicate entries ? (bug in pre rpm-3.0.4) */

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

static int dbiTagsMax = 0;
static int *dbiTags = NULL;

/**
 * Return dbi index used for rpm tag.
 * @param rpmtag	rpm header tag
 * @return dbi index, -1 on error
 */
static int dbiTagToDbix(int rpmtag)
{
    int dbix;

    if (!(dbiTagsMax > 0 && dbiTags))
	return -1;
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (rpmtag == dbiTags[dbix])
	    return dbix;
    }
    return -1;
}

/**
 * Initialize database (index, tag) tuple from configuration.
 */
static void dbiTagsInit(void)
{
    static const char * _dbiTagStr_default =
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername";
    char * dbiTagStr;
    char * o, * oe;
    int rpmtag;

    dbiTagStr = rpmExpand("%{_dbi_tags}", NULL);
    if (!(dbiTagStr && *dbiTagStr && *dbiTagStr != '%')) {
	xfree(dbiTagStr);
	dbiTagStr = xstrdup(_dbiTagStr_default);
    }

    if (dbiTagsMax || dbiTags) {
	free(dbiTags);
	dbiTags = NULL;
	dbiTagsMax = 0;
    }

    /* Always allocate package index */
    dbiTagsMax = 1;
    dbiTags = xcalloc(1, dbiTagsMax * sizeof(*dbiTags));

    for (o = dbiTagStr; o && *o; o = oe) {
	while (*o && isspace(*o))
	    o++;
	if (*o == '\0')
	    break;
	for (oe = o; oe && *oe; oe++) {
	    if (isspace(*oe))
		break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	rpmtag = tagValue(o);
	if (rpmtag < 0) {

	    fprintf(stderr, _("dbiTagsInit: unrecognized tag name: \"%s\" ignored\n"), o);
	    continue;
	}
	if (dbiTagToDbix(rpmtag) >= 0)
	    continue;

	dbiTags = xrealloc(dbiTags, (dbiTagsMax + 1) * sizeof(*dbiTags));
	dbiTags[dbiTagsMax++] = rpmtag;
    }

    free(dbiTagStr);
}

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

static struct _dbiVec *mydbvecs[] = {
    DB1vec, DB1vec, DB2vec, DB3vec, NULL
};

INLINE int dbiSync(dbiIndex dbi, unsigned int flags) {
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    Sync %s\n", tagName(dbi->dbi_rpmtag));
    return (*dbi->dbi_vec->sync) (dbi, flags);
}

INLINE int dbiByteSwapped(dbiIndex dbi) {
    return (*dbi->dbi_vec->byteswapped) (dbi);
}

INLINE int XdbiCopen(dbiIndex dbi, DBC ** dbcp, unsigned int flags,
	const char * f, unsigned int l)
{
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "+++ RMW %s (%s:%u)\n", tagName(dbi->dbi_rpmtag), f, l);
    return (*dbi->dbi_vec->copen) (dbi, dbcp, flags);
}

INLINE int XdbiCclose(dbiIndex dbi, DBC * dbcursor, unsigned int flags,
	const char * f, unsigned int l)
{
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "--- RMW %s (%s:%u)\n", tagName(dbi->dbi_rpmtag), f, l);
    return (*dbi->dbi_vec->cclose) (dbi, dbcursor, flags);
}

INLINE int dbiDel(dbiIndex dbi, DBC * dbcursor, const void * keyp, size_t keylen, unsigned int flags)
{
    int NULkey;
    int rc;

    /* XXX make sure that keylen is correct for "" lookup */
    NULkey = (keyp && *((char *)keyp) == '\0' && keylen == 0);
    if (NULkey) keylen++;
    rc = (*dbi->dbi_vec->cdel) (dbi, dbcursor, keyp, keylen, flags);
    if (NULkey) keylen--;

if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    Del %s key (%p,%ld) %s rc %d\n", tagName(dbi->dbi_rpmtag), keyp, (long)keylen, (dbi->dbi_rpmtag != RPMDBI_PACKAGES ? (char *)keyp : ""), rc);

    return rc;
}

INLINE int dbiGet(dbiIndex dbi, DBC * dbcursor, void ** keypp, size_t * keylenp,
	void ** datapp, size_t * datalenp, unsigned int flags)
{
    int NULkey;
    int rc;

    /* XXX make sure that keylen is correct for "" lookup */
    NULkey = (keypp && *keypp && *((char *)(*keypp)) == '\0' && keylenp && *keylenp == 0);
    if (NULkey) (*keylenp)++;
    rc = (*dbi->dbi_vec->cget) (dbi, dbcursor, keypp, keylenp, datapp, datalenp, flags);
    if (NULkey) (*keylenp)--;

if (_debug < 0 || dbi->dbi_debug) {
char keyval[32];
int dataval = 0xdeadbeef;
if (dbi->dbi_rpmtag == RPMDBI_PACKAGES && keypp && *keypp && keylenp && *keylenp >= sizeof(keyval)) {
    int keyint;
    memcpy(&keyint, *keypp, sizeof(keyint));
    sprintf(keyval, "%d", keyint);
} else keyval[0] = '\0';
if (rc == 0 && datapp && *datapp && datalenp && *datalenp >= sizeof(dataval))
    memcpy(&dataval, *datapp, sizeof(dataval));
fprintf(stderr, "    Get %s key (%p,%ld) data (%p,%ld) \"%s\" %x rc %d\n",
    tagName(dbi->dbi_rpmtag), *keypp, (long)*keylenp, *datapp, (long)*datalenp,
    (dbi->dbi_rpmtag != RPMDBI_PACKAGES ? (char *)*keypp : keyval), dataval, rc);
}
    return rc;
}

INLINE int dbiPut(dbiIndex dbi, DBC * dbcursor, const void * keyp, size_t keylen,
	const void * datap, size_t datalen, unsigned int flags)
{
    int NULkey;
    int rc;

    /* XXX make sure that keylen is correct for "" lookup */
    NULkey = (keyp && *((char *)keyp) == '\0' && keylen == 0);
    if (NULkey) keylen++;
    rc = (*dbi->dbi_vec->cput) (dbi, dbcursor, keyp, keylen, datap, datalen, flags);
    if (NULkey) keylen--;

if (_debug < 0 || dbi->dbi_debug) {
int dataval = 0xdeadbeef;
if (datap) memcpy(&dataval, datap, sizeof(dataval));
fprintf(stderr, "    Put %s key (%p,%ld) data (%p,%ld) \"%s\" %x rc %d\n", tagName(dbi->dbi_rpmtag), keyp, (long)keylen, datap, (long)datalen, (dbi->dbi_rpmtag != RPMDBI_PACKAGES ? (char *)keyp : ""), dataval, rc);
}

    return rc;
}

INLINE int dbiClose(dbiIndex dbi, unsigned int flags) {
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    %s Close\n", tagName(dbi->dbi_rpmtag));
    return (*dbi->dbi_vec->close) (dbi, flags);
}

dbiIndex dbiOpen(rpmdb rpmdb, int rpmtag, unsigned int flags)
{
    int dbix;
    dbiIndex dbi = NULL;
    int _dbapi, _dbapi_rebuild, _dbapi_wanted;
    int rc = 0;

    dbix = dbiTagToDbix(rpmtag);
    if (dbix < 0 || dbix >= dbiTagsMax)
	return NULL;

    /* Is this index already open ? */
    if ((dbi = rpmdb->_dbi[dbix]) != NULL)
	return dbi;

    _dbapi_wanted = rpmdb->db_api;
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 3)
	_dbapi_rebuild = 3;

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 4 || mydbvecs[_dbapi] == NULL) {
	    static int _printed = 0;
	    if (!_printed++)
		fprintf(stderr, _("\n\
--> This version of rpm was not compiled with support for \"%%_dbapi %d\".\n\
    Please verify the setting of the macro %%_dbapi using \"rpm --showrc\"\n\
    and configure \"%%_dbapi -1\" (e.g. create and/or edit /etc/rpm/macros).\n\
\n\
"), _dbapi_wanted);
	   return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (rpmdb, rpmtag, &dbi);
	if (rc == 0 && dbi)
	    break;
	/*@fallthrough@*/
    case -1:
	_dbapi = 4;
	while (_dbapi-- > 1) {
	    if (mydbvecs[_dbapi] == NULL)
		continue;
	    errno = 0;
	    dbi = NULL;
	    rc = (*mydbvecs[_dbapi]->open) (rpmdb, rpmtag, &dbi);
	    if (rc == 0 && dbi)
		break;
	}
	if (rpmdb->db_api == -1 && _dbapi > 0)
	    rpmdb->db_api = _dbapi;
    	break;
    }

    if (_dbapi <= 0) {		/* failed to open with any dbapi */
	static int _printed[32];
	if (!_printed[dbix & 0x1f]++)
	    rpmError(RPMERR_DBOPEN, _("dbiOpen: cannot open %s index"),
		tagName(rpmtag));
	rc = 1;
    } else			/* suggest possible conversion */
    if (_dbapi_wanted < 0 && _dbapi != _dbapi_rebuild) {
	static int _printed = 0;
	rc = (dbi->dbi_rpmtag != RPMDBI_PACKAGES);
	if (rc && !_printed++)
	    fprintf(stderr, _("\n\
--> The database is in db%d format, not the suggested db%d format.\n\
    Please verify the setting of the macros %%_dbapi and %%_dbapi_rebuild\n\
    using \"rpm --showrc\", and either run \"rpm --rebuilddb\" as root\n\
    to convert your database from db%d to db%d format, or configure\n\
    \"%%_dbapi_rebuild %d\" (e.g. create and/or edit /etc/rpm/macros).\n\
\n\
"), _dbapi, (_dbapi_rebuild > 0 ? _dbapi_rebuild : 3),
    _dbapi, (_dbapi_rebuild > 0 ? _dbapi_rebuild : 3), _dbapi);
    } else			/* suggest possible configuration */
    if (_dbapi_wanted >= 0 && _dbapi != _dbapi_wanted) {
	static int _printed = 0;
	if (!_printed++)
	    fprintf(stderr, _("\n\
--> The configured %%_dbapi was db%d, but the database is db%d format.\n\
    Please verify the setting of the macro %%_dbapi using \"rpm --showrc\"\n\
    and configure \"%%_dbapi %d\" (e.g. create and/or edit /etc/rpm/macros).\n\
\n\
"), _dbapi_wanted, _dbapi, _dbapi);
	rc = 1;
    }

    if (rc == 0 && dbi) {
	rpmdb->_dbi[dbix] = dbi;
    } else if (dbi) {
	/* XXX FIXME: dbNopen handles failures already. */
	db3Free(dbi);
	dbi = NULL;
    }

    return dbi;
}

/**
 * Create and initialize item for index database set.
 * @param hdrNum	header instance in db
 * @param tagNum	tag index in header
 * @return		new item
 */
static INLINE dbiIndexItem dbiIndexNewItem(unsigned int hdrNum, unsigned int tagNum) {
    dbiIndexItem rec = xcalloc(1, sizeof(*rec));
    rec->hdrNum = hdrNum;
    rec->tagNum = tagNum;
    return rec;
}

union _dbswap {
    unsigned int ui;
    unsigned char uc[4];
};

#define	_DBSWAP(_a) \
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

/**
 * Return items that match criteria.
 * @param dbi		index database handle
 * @param keyp		search key
 * @param keylen	search key length (0 will use strlen(key))
 * @param setp		address of items retrieved from index database
 * @return		-1 error, 0 success, 1 not found
 */
static int dbiSearch(dbiIndex dbi, DBC * dbcursor, const char * keyp, size_t keylen,
		dbiIndexSet * setp)
{
    void * datap;
    size_t datalen;
    int rc;

    if (setp) *setp = NULL;
    if (keylen == 0) keylen = strlen(keyp);

    rc = dbiGet(dbi, dbcursor, (void **)&keyp, &keylen, &datap, &datalen, 0);

    if (rc > 0) {
	rpmError(RPMERR_DBGETINDEX, _("error(%d) getting \"%s\" records from %s index"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
    } else
    if (rc == 0 && setp) {
	int _dbbyteswapped = dbiByteSwapped(dbi);
	const char * sdbir = datap;
	dbiIndexSet set;
	int i;

	set = xmalloc(sizeof(*set));

	/* Convert to database internal format */
	switch (dbi->dbi_jlen) {
	default:
	case 2*sizeof(int_32):
	    set->count = datalen / (2*sizeof(int_32));
	    set->recs = xmalloc(set->count * sizeof(*(set->recs)));
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum, tagNum;

		memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
		sdbir += sizeof(hdrNum.ui);
		memcpy(&tagNum.ui, sdbir, sizeof(tagNum.ui));
		sdbir += sizeof(tagNum.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		    _DBSWAP(tagNum);
		}
		set->recs[i].hdrNum = hdrNum.ui;
		set->recs[i].tagNum = tagNum.ui;
		set->recs[i].fpNum = 0;
		set->recs[i].dbNum = 0;
	    }
	    break;
	case 1*sizeof(int_32):
	    set->count = datalen / (1*sizeof(int_32));
	    set->recs = xmalloc(set->count * sizeof(*(set->recs)));
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum;

		memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
		sdbir += sizeof(hdrNum.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		}
		set->recs[i].hdrNum = hdrNum.ui;
		set->recs[i].tagNum = 0;
		set->recs[i].fpNum = 0;
		set->recs[i].dbNum = 0;
	    }
	    break;
	}
	*setp = set;
    }
    return rc;
}

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param keyp	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
/*@-compmempass@*/
static int dbiUpdateIndex(dbiIndex dbi, DBC * dbcursor, const char * keyp, dbiIndexSet set)
{
    size_t keylen = strlen(keyp);
    void * datap;
    size_t datalen;
    int rc;

    if (set->count) {
	char * tdbir;
	int i;
	int _dbbyteswapped = dbiByteSwapped(dbi);

	/* Convert to database internal format */

	switch (dbi->dbi_jlen) {
	default:
	case 2*sizeof(int_32):
	    datalen = set->count * (2 * sizeof(int_32));
	    datap = tdbir = alloca(datalen);
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum, tagNum;

		hdrNum.ui = set->recs[i].hdrNum;
		tagNum.ui = set->recs[i].tagNum;
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		    _DBSWAP(tagNum);
		}
		memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
		tdbir += sizeof(hdrNum.ui);
		memcpy(tdbir, &tagNum.ui, sizeof(tagNum.ui));
		tdbir += sizeof(tagNum.ui);
	    }
	    break;
	case 1*sizeof(int_32):
	    datalen = set->count * (1 * sizeof(int_32));
	    datap = tdbir = alloca(datalen);
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum;

		hdrNum.ui = set->recs[i].hdrNum;
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		}
		memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
		tdbir += sizeof(hdrNum.ui);
	    }
	    break;
	}

	rc = dbiPut(dbi, dbcursor, keyp, keylen, datap, datalen, 0);

	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error(%d) storing record %s into %s"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
	}

    } else {

	rc = dbiDel(dbi, dbcursor, keyp, keylen, 0);

	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX, _("error(%d) removing record %s from %s"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
	}

    }

    return rc;
}
/*@=compmempass@*/

/* XXX assumes hdrNum is first int in dbiIndexItem */
static int hdrNumCmp(const void * one, const void * two) {
    const int * a = one, * b = two;
    return (*a - *b);
}

/**
 * Append element(s) to set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to append to set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sortset	should resulting set be sorted?
 * @return		0 success, 1 failure (bad args)
 */
static INLINE int dbiAppendSet(dbiIndexSet set, const void * recs,
	int nrecs, size_t recsize, int sortset)
{
    const char * rptr = recs;
    size_t rlen = (recsize < sizeof(*(set->recs)))
		? recsize : sizeof(*(set->recs));

    if (set == NULL || recs == NULL || nrecs <= 0 || recsize <= 0)
	return 1;

    set->recs = (set->count == 0)
	? xmalloc(nrecs * sizeof(*(set->recs)))
	: xrealloc(set->recs, (set->count + nrecs) * sizeof(*(set->recs)));

    memset(set->recs + set->count, 0, nrecs * sizeof(*(set->recs)));

    while (nrecs-- > 0) {
	memcpy(set->recs + set->count, rptr, rlen);
	rptr += recsize;
	set->count++;
    }

    if (set->count > 1 && sortset)
	qsort(set->recs, set->count, sizeof(*(set->recs)), hdrNumCmp);

    return 0;
}

/**
 * Remove element(s) from set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to remove from set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sorted	array is already sorted?
 * @return		0 success, 1 failure (no items found)
 */
static INLINE int dbiPruneSet(dbiIndexSet set, void * recs, int nrecs,
	size_t recsize, int sorted)
{
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;

    if (nrecs > 1 && !sorted)
	qsort(recs, nrecs, recsize, hdrNumCmp);

    for (from = 0; from < num; from++) {
	if (bsearch(&set->recs[from], recs, nrecs, recsize, hdrNumCmp)) {
	    set->count--;
	    continue;
	}
	if (from != to)
	    set->recs[to] = set->recs[from]; /* structure assignment */
	to++;
	numCopied++;
    }

    return (numCopied == num);
}

/* XXX transaction.c */
unsigned int dbiIndexSetCount(dbiIndexSet set) {
    return set->count;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set->recs[recno].hdrNum;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set->recs[recno].tagNum;
}

/* XXX transaction.c */
void dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	if (set->recs) free(set->recs);
	free(set);
    }
}

/**
 * Disable all signals, returning previous signal mask.
 */
static void blockSignals(rpmdb rpmdb, sigset_t * oldMask)
{
    sigset_t newMask;

    /* XXX HACK (disabled) permit ^C aborts for now ... */
    if (!(rpmdb && rpmdb->db_api == 4)) {
	sigfillset(&newMask);		/* block all signals */
	sigprocmask(SIG_BLOCK, &newMask, oldMask);
    }
}

/**
 * Restore signal mask.
 */
static void unblockSignals(rpmdb rpmdb, sigset_t * oldMask)
{
    /* XXX HACK (disabled) permit ^C aborts for now ... */
    if (!(rpmdb && rpmdb->db_api == 4)) {
	sigprocmask(SIG_SETMASK, oldMask, NULL);
    }
}

#define	_DB_ROOT	"/"
#define	_DB_HOME	"%{_dbpath}"
#define	_DB_FLAGS	0
#define _DB_MODE	0
#define _DB_PERMS	0644

#define _DB_MAJOR	-1
#define	_DB_REMOVE_ENV	0
#define	_DB_FILTER_DUPS	0
#define	_DB_ERRPFX	"rpmdb"

static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS, _DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_REMOVE_ENV, _DB_FILTER_DUPS, _DB_ERRPFX
};

/* XXX query.c, rpminstall.c, verify.c */
int rpmdbClose (rpmdb rpmdb)
{
    int dbix;

    for (dbix = rpmdb->db_ndbi; --dbix >= 0; ) {
	if (rpmdb->_dbi[dbix] == NULL)
	    continue;
    	dbiClose(rpmdb->_dbi[dbix], 0);
    	rpmdb->_dbi[dbix] = NULL;
    }
    if (rpmdb->db_errpfx) {
	xfree(rpmdb->db_errpfx);
	rpmdb->db_errpfx = NULL;
    }
    if (rpmdb->db_root) {
	xfree(rpmdb->db_root);
	rpmdb->db_root = NULL;
    }
    if (rpmdb->db_home) {
	xfree(rpmdb->db_home);
	rpmdb->db_home = NULL;
    }
    if (rpmdb->_dbi) {
	xfree(rpmdb->_dbi);
	rpmdb->_dbi = NULL;
    }
    free(rpmdb);
    return 0;
}

int rpmdbSync(rpmdb rpmdb)
{
    int dbix;

    for (dbix = 0; dbix < rpmdb->db_ndbi; dbix++) {
	int xx;
	if (rpmdb->_dbi[dbix] == NULL)
	    continue;
    	xx = dbiSync(rpmdb->_dbi[dbix], 0);
    }
    return 0;
}

static /*@only@*/ rpmdb newRpmdb(const char * root, const char * home,
		int mode, int perms, int flags)
{
    rpmdb rpmdb = xcalloc(sizeof(*rpmdb), 1);
    static int _initialized = 0;

    if (!_initialized) {
	_filterDbDups = rpmExpandNumeric("%{_filterdbdups}");
	_initialized = 1;
    }

    *rpmdb = dbTemplate;	/* structure assignment */

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (root)
	rpmdb->db_root = (*root ? root : _DB_ROOT);
    if (home)
	rpmdb->db_home = (*home ? home : _DB_HOME);
    if (mode >= 0)	rpmdb->db_mode = mode;
    if (perms >= 0)	rpmdb->db_perms = perms;
    if (flags >= 0)	rpmdb->db_flags = flags;

    if (rpmdb->db_root)
	rpmdb->db_root = rpmGetPath(rpmdb->db_root, NULL);
    if (rpmdb->db_home) {
	rpmdb->db_home = rpmGetPath(rpmdb->db_home, NULL);
	if (!(rpmdb->db_home && rpmdb->db_home[0] != '%')) {
	    rpmError(RPMERR_DBOPEN, _("no dbpath has been set"));
	   goto errxit;
	}
    }
    if (rpmdb->db_errpfx)
	rpmdb->db_errpfx = xstrdup(rpmdb->db_errpfx);
    rpmdb->db_remove_env = 0;
    rpmdb->db_filter_dups = _filterDbDups;
    rpmdb->db_ndbi = dbiTagsMax;
    rpmdb->_dbi = xcalloc(rpmdb->db_ndbi, sizeof(*rpmdb->_dbi));
    return rpmdb;

errxit:
    if (rpmdb)
	rpmdbClose(rpmdb);
    return NULL;
}

static int openDatabase(const char * prefix, const char * dbpath, int _dbapi,
	rpmdb *dbp, int mode, int perms, int flags)
{
    rpmdb rpmdb;
    int rc;
    static int _initialized = 0;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    if (!_initialized || dbiTagsMax == 0) {
	dbiTagsInit();
	_initialized++;
    }

    /* Insure that _dbapi has one of -1, 1, 2, or 3 */
    if (_dbapi < -1 || _dbapi > 3)
	_dbapi = -1;
    if (_dbapi == 0)
	_dbapi = 1;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
	return 1;

    rpmdb = newRpmdb(prefix, dbpath, mode, perms, flags);
    rpmdb->db_api = _dbapi;

    {	int dbix;

	rc = 0;
	for (dbix = 0; rc == 0 && dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[dbix])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    dbi = dbiOpen(rpmdb, rpmtag, 0);

	    switch (rpmtag) {
	    case RPMDBI_PACKAGES:
		if (dbi == NULL) rc |= 1;
#if 0
		if (rpmdb->db_api == 3)
#endif
		    goto exit;
		break;
	    case RPMTAG_NAME:
		if (dbi == NULL) rc |= 1;
		if (minimal)
		    goto exit;
		break;
	    case RPMTAG_BASENAMES:
	    {	void * keyp = NULL;
		DBC * dbcursor;
		int xx;

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
		if (justCheck)
		    break;
		dbcursor = NULL;
		xx = dbiCopen(dbi, &dbcursor, 0);
		xx = dbiGet(dbi, dbcursor, &keyp, NULL, NULL, NULL, 0);
		if (xx == 0) {
		    const char * akey = keyp;
		    if (strchr(akey, '/')) {
			rpmError(RPMERR_OLDDB, _("old format database is present; "
				"use --rebuilddb to generate a new format database"));
			rc |= 1;
		    }
		}
		xx = dbiCclose(dbi, dbcursor, 0);
		dbcursor = NULL;
	    }	break;
	    default:
		break;
	    }
	}
    }

exit:
    if (rc || justCheck || dbp == NULL)
	rpmdbClose(rpmdb);
    else
	*dbp = rpmdb;

    return rc;
}

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    return openDatabase(prefix, NULL, _dbapi, dbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb rpmdb = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc;

    rc = openDatabase(prefix, NULL, _dbapi, &rpmdb, (O_CREAT | O_RDWR), perms, RPMDB_FLAG_JUSTCHECK);
    if (rpmdb) {
	rpmdbClose(rpmdb);
	rpmdb = NULL;
    }
    return rc;
}

#ifndef	DYING_NOTYET
/* XXX install.c, query.c, transaction.c, uninstall.c */
static Header rpmdbGetRecord(rpmdb rpmdb, unsigned int offset)
{
    dbiIndex dbi;
    DBC * dbcursor = NULL;
    void * uh;
    size_t uhlen;
    void * keyp = &offset;
    size_t keylen = sizeof(offset);
    int rc;
    int xx;

    dbi = dbiOpen(rpmdb, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)
	return NULL;
    xx = dbiCopen(dbi, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, &keyp, &keylen, &uh, &uhlen, 0);
    xx = dbiCclose(dbi, dbcursor, 0);
    if (rc)
	return NULL;
    return headerLoad(uh);
}
#endif

static int rpmdbFindByFile(rpmdb rpmdb, const char * filespec,
			/*@out@*/ dbiIndexSet * matches)
{
    const char * dirName;
    const char * baseName;
    fingerPrintCache fpc;
    fingerPrint fp1;
    dbiIndex dbi = NULL;
    DBC * dbcursor;
    dbiIndexSet allMatches = NULL;
    dbiIndexItem rec = NULL;
    int i;
    int rc;
    int xx;

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

    dbi = dbiOpen(rpmdb, RPMTAG_BASENAMES, 0);
    dbcursor = NULL;
    xx = dbiCopen(dbi, &dbcursor, 0);
    rc = dbiSearch(dbi, dbcursor, baseName, 0, &allMatches);
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
    if (rc) {
	dbiFreeIndexSet(allMatches);
	allMatches = NULL;
	fpCacheFree(fpc);
	return rc;
    }

    *matches = xcalloc(1, sizeof(**matches));
    rec = dbiIndexNewItem(0, 0);
    i = 0;
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	int_32 * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

#ifndef	DYING_NOTYET
	h = rpmdbGetRecord(rpmdb, offset);
#else
	{   rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, &offset, sizeof(offset));
	    h = rpmdbNextIterator(mi);
	    if (h)
		h = headerLink(h);
	    rpmdbFreeIterator(mi);
	}
#endif

	if (h == NULL) {
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
		rec->hdrNum = dbiIndexRecordOffset(allMatches, i);
		rec->tagNum = dbiIndexRecordFileNumber(allMatches, i);
		dbiAppendSet(*matches, rec, 1, sizeof(*rec), 0);
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
    dbiIndex dbi;
    dbiIndexSet matches = NULL;
    int rc = -1;
    int xx;

    dbi = dbiOpen(rpmdb, RPMTAG_NAME, 0);
    if (dbi) {
	DBC * dbcursor = NULL;
	xx = dbiCopen(dbi, &dbcursor, 0);
	rc = dbiSearch(dbi, dbcursor, name, 0, &matches);
	xx = dbiCclose(dbi, dbcursor, 0);
    }

    if (rc == 0)	/* success */
	rc = dbiIndexSetCount(matches);
    else if (rc > 0)	/* error */
	rpmError(RPMERR_DBCORRUPT, _("error(%d) counting packages"), rc);
    else		/* not found */
	rc = 0;

    if (matches)
	dbiFreeIndexSet(matches);

    return rc;
}

/* XXX transaction.c */
/* 0 found matches */
/* 1 no matches */
/* 2 error */
static int dbiFindMatches(dbiIndex dbi, DBC * dbcursor, const char * name, const char * version,
			const char * release, dbiIndexSet * matches)
{
    int gotMatches;
    int rc;
    int i;

    rc = dbiSearch(dbi, dbcursor, name, 0, matches);

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

#ifndef	DYING_NOTYET
	h = rpmdbGetRecord(dbi->dbi_rpmdb, recoff);
#else
    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, &recoff, sizeof(recoff));
	h = rpmdbNextIterator(mi);
	if (h)
	    h = headerLink(h);
	rpmdbFreeIterator(mi);
    }
#endif
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT, _("%s: cannot read header at 0x%x"),
		"findMatches", recoff);
	    rc = 2;
	    goto exit;
	}

	headerNVR(h, NULL, &pkgVersion, &pkgRelease);
	    
	goodRelease = goodVersion = 1;

	if (release && strcmp(release, pkgRelease)) goodRelease = 0;
	if (version && strcmp(version, pkgVersion)) goodVersion = 0;

	if (goodRelease && goodVersion) {
	    /* structure assignment */
	    (*matches)->recs[gotMatches++] = (*matches)->recs[i];
	} else 
	    (*matches)->recs[i].hdrNum = 0;

	headerFree(h);
    }

    if (gotMatches) {
	(*matches)->count = gotMatches;
	rc = 0;
    } else {
	rc = 1;
    }

exit:
    if (rc && matches && *matches) {
	dbiFreeIndexSet(*matches);
	*matches = NULL;
    }
    return rc;
}

/* 0 found matches */
/* 1 no matches */
/* 2 error */
static int dbiFindByLabel(dbiIndex dbi, DBC * dbcursor, const char * arg, dbiIndexSet * matches)
{
    char * localarg, * chptr;
    char * release;
    int rc;
 
    if (!strlen(arg)) return 1;

    /* did they give us just a name? */
    rc = dbiFindMatches(dbi, dbcursor, arg, NULL, NULL, matches);
    if (rc != 1) return rc;
    if (*matches) dbiFreeIndexSet(*matches);

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    strcpy(localarg, arg);

    chptr = (localarg + strlen(localarg)) - 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    rc = dbiFindMatches(dbi, dbcursor, localarg, chptr + 1, NULL, matches);
    if (rc != 1) return rc;
    if (*matches) dbiFreeIndexSet(*matches);
    
    /* how about name-version-release? */

    release = chptr + 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    return dbiFindMatches(dbi, dbcursor, localarg, chptr + 1, release, matches);
}

/**
 * Rewrite a header in the database.
 *   Note: this is called from a markReplacedFiles iteration, and *must*
 *   preserve the "join key" (i.e. offset) for the header.
 * @param dbi		index database handle (always RPMDBI_PACKAGES)
 * @param offset	join key
 * @param h		rpm header
 * @return 		0 on success
 */
static int dbiUpdateRecord(dbiIndex dbi, DBC * dbcursor, int offset, Header h)
{
    sigset_t signalMask;
    void * uh;
    size_t uhlen;
    int rc;
    int xx;

    if (_noDirTokens)
	expandFilelist(h);

    uhlen = headerSizeof(h, HEADER_MAGIC_NO);
    uh = headerUnload(h);
    blockSignals(dbi->dbi_rpmdb, &signalMask);
    rc = dbiPut(dbi, dbcursor, &offset, sizeof(offset), uh, uhlen, 0);
    xx = dbiSync(dbi, 0);
    unblockSignals(dbi->dbi_rpmdb, &signalMask);
    free(uh);

    return rc;
}

struct _rpmdbMatchIterator {
    const void *	mi_keyp;
    size_t		mi_keylen;
    rpmdb		mi_rpmdb;
    int			mi_rpmtag;
    dbiIndexSet		mi_set;
    int			mi_setx;
    Header		mi_h;
    int			mi_sorted;
    int			mi_modified;
    unsigned int	mi_prevoffset;
    unsigned int	mi_offset;
    unsigned int	mi_filenum;
    unsigned int	mi_fpnum;
    unsigned int	mi_dbnum;
    const char *	mi_version;
    const char *	mi_release;
};

void rpmdbFreeIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi = NULL;
    int xx;

    if (mi == NULL)
	return;

    dbi = dbiOpen(mi->mi_rpmdb, RPMDBI_PACKAGES, 0);
    if (mi->mi_h) {
	if (mi->mi_modified && mi->mi_prevoffset) {
	    DBC * dbcursor = NULL;
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    dbiUpdateRecord(dbi, dbcursor, mi->mi_prevoffset, mi->mi_h);
	    xx = dbiCclose(dbi, dbcursor, 0);
	}
	headerFree(mi->mi_h);
	mi->mi_h = NULL;
    }
    if (dbi->dbi_rmw) {
	xx = dbiCclose(dbi, dbi->dbi_rmw, 0);
	dbi->dbi_rmw = NULL;
    }

    if (mi->mi_release) {
	xfree(mi->mi_release);
	mi->mi_release = NULL;
    }
    if (mi->mi_version) {
	xfree(mi->mi_version);
	mi->mi_version = NULL;
    }
    if (mi->mi_set) {
	dbiFreeIndexSet(mi->mi_set);
	mi->mi_set = NULL;
    }
    if (mi->mi_keyp) {
	xfree(mi->mi_keyp);
	mi->mi_keyp = NULL;
    }
    free(mi);
}

rpmdb rpmdbGetIteratorRpmDB(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return 0;
    return mi->mi_rpmdb;
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

int rpmdbSetIteratorModified(rpmdbMatchIterator mi, int modified) {
    int rc;
    if (mi == NULL)
	return 0;
    rc = mi->mi_modified;
    mi->mi_modified = modified;
    return rc;
}

Header XrpmdbNextIterator(rpmdbMatchIterator mi, const char * f, unsigned l)
{
    dbiIndex dbi;
    DBC * dbcursor;
    void * uh = NULL;
    size_t uhlen = 0;
    void * keyp;
    size_t keylen;
    int rc;
    int xx;

    if (mi == NULL)
	return NULL;

    dbi = dbiOpen(mi->mi_rpmdb, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)
	return NULL;
    xx = XdbiCopen(dbi, &dbcursor, 0, f, l);

top:
    /* XXX skip over instances with 0 join key */
    do {
	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	    mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	    keyp = &mi->mi_offset;
	    keylen = sizeof(mi->mi_offset);
	} else {
	    keyp = (void *)mi->mi_keyp;		/* XXX FIXME const */
	    keylen = mi->mi_keylen;

	    rc = dbiGet(dbi, dbcursor, &keyp, &keylen, &uh, &uhlen, 0);

	    if (rc == 0 && keyp && mi->mi_setx)
		memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));

	    /* Terminate on error or end of keys */
	    if (rc || (mi->mi_setx && mi->mi_offset == 0))
		return NULL;
	}
	mi->mi_setx++;
    } while (mi->mi_offset == 0);

    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset)
	return mi->mi_h;

    /* Retrieve next header */
    if (uh == NULL) {
	rc = dbiGet(dbi, dbcursor, &keyp, &keylen, &uh, &uhlen, 0);
	if (rc)
	    return NULL;
    }

    /* Free current header */
    if (mi->mi_h) {
	if (mi->mi_modified && mi->mi_prevoffset)
	    dbiUpdateRecord(dbi, dbcursor, mi->mi_prevoffset, mi->mi_h);
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
    mi->mi_modified = 0;
    return mi->mi_h;
}

static void rpmdbSortIterator(rpmdbMatchIterator mi) {
    if (mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0) {
	qsort(mi->mi_set->recs, mi->mi_set->count, sizeof(*mi->mi_set->recs),
		hdrNumCmp);
	mi->mi_sorted = 1;
    }
}

static int rpmdbGrowIterator(rpmdbMatchIterator mi,
	const void * keyp, size_t keylen, int fpNum)
{
    dbiIndex dbi = NULL;
    DBC * dbcursor;
    dbiIndexSet set = NULL;
    int rc;
    int xx;

    if (!(mi && keyp))
	return 1;

    dbi = dbiOpen(mi->mi_rpmdb, mi->mi_rpmtag, 0);
    if (dbi == NULL)
	return 1;

    if (keylen == 0)
	keylen = strlen(keyp);

    dbcursor = NULL;
    xx = dbiCopen(dbi, &dbcursor, 0);
    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);
    xx = dbiCclose(dbi, dbcursor, 0);

    if (rc == 0) {	/* success */
	int i;
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
    }

    if (set)
	dbiFreeIndexSet(set);
    return rc;
}

int rpmdbPruneIterator(rpmdbMatchIterator mi, int * hdrNums,
	int nHdrNums, int sorted)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set)
	dbiPruneSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), sorted);
    return 0;
}

int rpmdbAppendIterator(rpmdbMatchIterator mi, int * hdrNums, int nHdrNums)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set == NULL)
	mi->mi_set = xcalloc(1, sizeof(*mi->mi_set));
    dbiAppendSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), 0);
    return 0;
}

rpmdbMatchIterator rpmdbInitIterator(rpmdb rpmdb, int rpmtag,
	const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi = NULL;
    dbiIndexSet set = NULL;
    dbiIndex dbi;
    int isLabel = 0;

    /* XXX HACK to remove rpmdbFindByLabel/findMatches from the API */
    switch (rpmtag) {
    case RPMDBI_LABEL:
	rpmtag = RPMTAG_NAME;
	isLabel = 1;
	break;
    }

    dbi = dbiOpen(rpmdb, rpmtag, 0);
    if (dbi == NULL)
	return NULL;

#if 0
    assert(dbi->dbi_rmw == NULL);	/* db3: avoid "lost" cursors */
#else
if (dbi->dbi_rmw)
fprintf(stderr, "*** RMW %s %p\n", tagName(rpmtag), dbi->dbi_rmw);
#endif
    assert(dbi->dbi_lastoffset == 0);	/* db0: avoid "lost" cursors */

    dbi->dbi_lastoffset = 0;		/* db0: rewind to beginning */

    if (rpmtag != RPMDBI_PACKAGES && keyp) {
	DBC * dbcursor;
	int rc;
	int xx;

	if (isLabel) {
	    /* XXX HACK to get rpmdbFindByLabel out of the API */
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    rc = dbiFindByLabel(dbi, dbcursor, keyp, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	} else if (rpmtag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(rpmdb, keyp, &set);
	} else {
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	}
	if (rc)	{	/* error/not found */
	    if (set)
		dbiFreeIndexSet(set);
	    return NULL;
	}
    }

    if (keyp) {
	char * k;

	if (rpmtag != RPMDBI_PACKAGES && keylen == 0)
	    keylen = strlen(keyp);
	k = xmalloc(keylen + 1);
	memcpy(k, keyp, keylen);
	k[keylen] = '\0';	/* XXX for strings */
	keyp = k;
    }

    mi = xcalloc(sizeof(*mi), 1);
    mi->mi_keyp = keyp;
    mi->mi_keylen = keylen;

    mi->mi_rpmdb = rpmdb;
    mi->mi_rpmtag = rpmtag;

    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_h = NULL;
    mi->mi_sorted = 0;
    mi->mi_modified = 0;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_filenum = 0;
    mi->mi_fpnum = 0;
    mi->mi_dbnum = 0;
    mi->mi_version = NULL;
    mi->mi_release = NULL;
    return mi;
}

static INLINE int removeIndexEntry(dbiIndex dbi, DBC * dbcursor, const char * keyp,
		dbiIndexItem rec)
{
    dbiIndexSet set = NULL;
    int rc;
    
    rc = dbiSearch(dbi, dbcursor, keyp, 0, &set);

    if (rc < 0)			/* not found */
	rc = 0;
    else if (rc > 0)		/* error */
	rc = 1;		/* error message already generated from dbindex.c */
    else {			/* success */
	if (!dbiPruneSet(set, rec, 1, sizeof(*rec), 1) &&
	    dbiUpdateIndex(dbi, dbcursor, keyp, set))
	    rc = 1;
    }

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return rc;
}

/* XXX install.c uninstall.c */
int rpmdbRemove(rpmdb rpmdb, unsigned int hdrNum)
{
    Header h;
    sigset_t signalMask;

#ifndef	DYING_NOTYET
    h = rpmdbGetRecord(rpmdb, hdrNum);
#else
  { rpmdbMatchIterator mi;
    mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
    h = rpmdbNextIterator(mi);
    if (h)
	h = headerLink(h);
    rpmdbFreeIterator(mi);
  }
#endif
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("%s: cannot read header at 0x%x"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

    {	const char *n, *v, *r;
	headerNVR(h, &n, &v, &r);
	rpmMessage(RPMMESS_VERBOSE, "  --- %10d %s-%s-%s\n", hdrNum, n, v, r);
    }

    blockSignals(rpmdb, &signalMask);

    {	int dbix;
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    DBC * dbcursor;
	    const char *av[1];
	    const char ** rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int xx;
	    int i;

	    dbi = NULL;
	    rpmtag = dbiTags[dbix];

	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(rpmdb, rpmtag, 0);
		xx = dbiCopen(dbi, &dbcursor, 0);
		xx = dbiDel(dbi, dbcursor, &hdrNum, sizeof(hdrNum), 0);
		xx = dbiCclose(dbi, dbcursor, 0);
		/* XXX HACK sync is on the bt with multiple db access */
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		/*@notreached@*/ break;
	    }
	
	    if (!headerGetEntry(h, rpmtag, &rpmtype,
		(void **) &rpmvals, &rpmcnt)) {
#if 0
		rpmMessage(RPMMESS_DEBUG, _("removing 0 %s entries.\n"),
			tagName(rpmtag));
#endif
		continue;
	    }

	    dbi = dbiOpen(rpmdb, rpmtag, 0);
	    xx = dbiCopen(dbi, &dbcursor, 0);

	    if (rpmtype == RPM_STRING_TYPE) {

		rpmMessage(RPMMESS_DEBUG, _("removing \"%s\" from %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		/* XXX force uniform headerGetEntry return */
		av[0] = (const char *) rpmvals;
		rpmvals = av;
		rpmcnt = 1;
	    } else {

		rpmMessage(RPMMESS_DEBUG, _("removing %d entries from %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

	    }

	    for (i = 0; i < rpmcnt; i++) {
#if 0
		rpmMessage(RPMMESS_DEBUG, _("%6d %s\n"), i, rpmvals[i]);
#endif
		/*
		 * This is almost right, but, if there are duplicate tag
		 * values, there will be duplicate attempts to remove
		 * the header instance. It's easier to just ignore errors
		 * than to do things correctly.
		 */
		xx = removeIndexEntry(dbi, dbcursor, rpmvals[i], rec);
	    }

	    xx = dbiCclose(dbi, dbcursor, 0);

	    /* XXX HACK sync is on the bt with multiple db access */
	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);

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

    unblockSignals(rpmdb, &signalMask);

    headerFree(h);

    return 0;
}

static INLINE int addIndexEntry(dbiIndex dbi, DBC * dbcursor, const char *index, dbiIndexItem rec)
{
    dbiIndexSet set = NULL;
    int rc;

    rc = dbiSearch(dbi, dbcursor, index, 0, &set);

    if (rc > 0) {
	rc = 1;			/* error */
    } else {
	if (rc < 0) {		/* not found */
	    rc = 0;
	    set = xcalloc(1, sizeof(*set));
	}
	dbiAppendSet(set, rec, 1, sizeof(*rec), 0);
	if (dbiUpdateIndex(dbi, dbcursor, index, set))
	    rc = 1;
    }

    if (set) {
	dbiFreeIndexSet(set);
	set = NULL;
    }

    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb rpmdb, Header h)
{
    sigset_t signalMask;
    const char ** baseNames;
    int count = 0;
    int type;
    dbiIndex dbi;
    int dbix;
    unsigned int hdrNum;
    int rc = 0;
    int xx;

    /*
     * If old style filename tags is requested, the basenames need to be
     * retrieved early, and the header needs to be converted before
     * being written to the package header database.
     */

    headerGetEntry(h, RPMTAG_BASENAMES, &type, (void **) &baseNames, &count);

    if (_noDirTokens)
	expandFilelist(h);

    blockSignals(rpmdb, &signalMask);

    {
	unsigned int firstkey = 0;
	DBC * dbcursor;
	void * keyp = &firstkey;
	size_t keylen = sizeof(firstkey);
	void * datap = NULL;
	size_t datalen = 0;

	dbi = dbiOpen(rpmdb, RPMDBI_PACKAGES, 0);

	/* XXX db0: hack to pass sizeof header to fadAlloc */
	datap = h;
	datalen = headerSizeof(h, HEADER_MAGIC_NO);

	xx = dbiCopen(dbi, &dbcursor, 0);

	/* Retrieve join key for next header instance. */

	rc = dbiGet(dbi, dbcursor, &keyp, &keylen, &datap, &datalen, 0);

	hdrNum = 0;
	if (rc == 0 && datap)
	    memcpy(&hdrNum, datap, sizeof(hdrNum));
	++hdrNum;
	if (rc == 0 && datap) {
	    memcpy(datap, &hdrNum, sizeof(hdrNum));
	} else {
	    datap = &hdrNum;
	    datalen = sizeof(hdrNum);
	}

	rc = dbiPut(dbi, dbcursor, keyp, keylen, datap, datalen, 0);
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, 0);

    }

    if (rc) {
	rpmError(RPMERR_DBCORRUPT, _("error(%d) allocating new package instance"), rc);
	goto exit;
    }

    /* Now update the indexes */

    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    DBC * dbcursor;
	    const char *av[1];
	    const char **rpmvals = NULL;
	    int rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int i, j;

	    dbi = NULL;
	    rpmtag = dbiTags[dbix];

	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(rpmdb, rpmtag, 0);
		xx = dbiCopen(dbi, &dbcursor, 0);
		xx = dbiUpdateRecord(dbi, dbcursor, hdrNum, h);
		xx = dbiCclose(dbi, dbcursor, 0);
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		{   const char *n, *v, *r;
		    headerNVR(h, &n, &v, &r);
		    rpmMessage(RPMMESS_VERBOSE, "  +++ %10d %s-%s-%s\n", hdrNum, n, v, r);
		}
		continue;
		/*@notreached@*/ break;
	    /* XXX preserve legacy behavior */
	    case RPMTAG_BASENAMES:
		rpmtype = type;
		rpmvals = baseNames;
		rpmcnt = count;
		break;
	    default:
		headerGetEntry(h, rpmtag, &rpmtype,
			(void **) &rpmvals, &rpmcnt);
		break;
	    }

	    if (rpmcnt <= 0) {
		if (rpmtag != RPMTAG_GROUP) {
#if 0
		    rpmMessage(RPMMESS_DEBUG, _("adding 0 %s entries.\n"),
			tagName(rpmtag));
#endif
		    continue;
		}

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }

	    dbi = dbiOpen(rpmdb, rpmtag, 0);

	    xx = dbiCopen(dbi, &dbcursor, 0);
	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("adding \"%s\" to %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		/* XXX force uniform headerGetEntry return */
		av[0] = (const char *) rpmvals;
		rpmvals = av;
		rpmcnt = 1;
	    } else {

		rpmMessage(RPMMESS_DEBUG, _("adding %d entries to %s index:\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

	    }

	    for (i = 0; i < rpmcnt; i++) {
#if 0
		rpmMessage(RPMMESS_DEBUG, _("%6d %s\n"), i, rpmvals[i]);
#endif

		/*
		 * Include the tagNum in all indices. rpm-3.0.4 and earlier
		 * included the tagNum only for files.
		 */
		switch (dbi->dbi_rpmtag) {
		case RPMTAG_TRIGGERNAME:
		    if (i) {	/* don't add duplicates */
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				break;
			}
			if (j < i)
			    continue;
		    }
		    /*@fallthrough@*/
		default:
		    rec->tagNum = i;
		    break;
		}

		rc += addIndexEntry(dbi, dbcursor, rpmvals[i], rec);
	    }
	    xx = dbiCclose(dbi, dbcursor, 0);

	    /* XXX HACK sync is on the bt with multiple db access */
	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);

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
    unblockSignals(rpmdb, &signalMask);

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
	dbiIndexItem im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->hdrNum != mi->mi_set->recs[end].hdrNum)
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
	    baseNames[i] = fullBaseNames[im[i].tagNum];
	    dirIndexes[i] = fullDirIndexes[im[i].tagNum];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < num; i++, im++) {
	    if (FP_EQUAL(fps[i], fpList[im->fpNum]))
		dbiAppendSet(matchList[im->fpNum], im, 1, sizeof(*im), 0);
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

static int rpmdbRemoveDatabase(const char * rootdir,
	const char * dbpath, int _dbapi)
{ 
    int i;
    char * filename;
    int xx;

    i = strlen(dbpath);
    if (dbpath[i - 1] != '/') {
	filename = alloca(i);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    
    filename = alloca(strlen(rootdir) + strlen(dbpath) + 40);

    switch (_dbapi) {
    case 3:
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base = tagName(dbiTags[i]);
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, base);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	}
	for (i = 0; i < 16; i++) {
	    sprintf(filename, "%s/%s/__db.%03d", rootdir, dbpath, i);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	}
	break;
    case 2:
    case 1:
    case 0:
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base = db1basename(dbiTags[i]);
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, base);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	    xfree(base);
	}
	break;
    }

    sprintf(filename, "%s/%s", rootdir, dbpath);
    (void)rpmCleanPath(filename);
    xx = rmdir(filename);

    return 0;
}

static int rpmdbMoveDatabase(const char * rootdir,
	const char * olddbpath, int _olddbapi,
	const char * newdbpath, int _newdbapi)
{
    int i;
    char * ofilename, * nfilename;
    int rc = 0;
    int xx;
 
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

    switch (_olddbapi) {
    case 3:
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[i])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    base = tagName(rpmtag);
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, base);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, base);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	}
	for (i = 0; i < 16; i++) {
	    sprintf(ofilename, "%s/%s/__db.%03d", rootdir, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/__db.%03d", rootdir, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	}
	break;
    case 2:
    case 1:
    case 0:
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[i])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    base = db1basename(rpmtag);
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, base);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, base);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	    xfree(base);
	}
	break;
    }
    if (rc || _olddbapi == _newdbapi)
	return rc;

    rc = rpmdbRemoveDatabase(rootdir, newdbpath, _newdbapi);
    return rc;
}

int rpmdbRebuild(const char * rootdir)
{
    rpmdb olddb;
    const char * dbpath = NULL;
    const char * rootdbpath = NULL;
    rpmdb newdb;
    const char * newdbpath = NULL;
    const char * newrootdbpath = NULL;
    const char * tfn;
    int nocleanup = 1;
    int failed = 0;
    int removedir = 0;
    int rc = 0;
    int _dbapi;
    int _dbapi_rebuild;

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    tfn = rpmGetPath("%{_dbpath}", NULL);
    if (!(tfn && tfn[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	dbpath += strlen(rootdir);
    xfree(tfn);

    tfn = rpmGetPath("%{_dbpath_rebuild}", NULL);
    if (!(tfn && tfn[0] != '%' && strcmp(tfn, dbpath))) {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
	if (tfn) xfree(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	newdbpath += strlen(rootdir);
    xfree(tfn);

    rpmMessage(RPMMESS_DEBUG, _("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("creating directory %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("error creating directory %s: %s"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    rpmMessage(RPMMESS_DEBUG, _("opening old database with dbapi %d\n"),
		_dbapi);
    if (openDatabase(rootdir, dbpath, _dbapi, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
    _dbapi = olddb->db_api;

    rpmMessage(RPMMESS_DEBUG, _("opening new database with dbapi %d\n"),
		_dbapi_rebuild);
    if (openDatabase(rootdir, newdbpath, _dbapi_rebuild, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }
    _dbapi_rebuild = newdb->db_api;
    
    {	Header h = NULL;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(olddb, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {

	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (!(headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
	    {
		rpmError(RPMERR_INTERNAL,
			_("record number %d in database is bad -- skipping."), 
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (newdb->db_filter_dups) {
		const char * name, * version, * release;
		int skip = 0;

		headerNVR(h, &name, &version, &release);

		{   rpmdbMatchIterator mi;
		    mi = rpmdbInitIterator(newdb, RPMTAG_NAME, name, 0);
		    rpmdbSetIteratorVersion(mi, version);
		    rpmdbSetIteratorRelease(mi, release);
		    while (rpmdbNextIterator(mi)) {
			skip = 1;
			break;
		    }
		    rpmdbFreeIterator(mi);
		}

		if (skip)
		    continue;
	    }

	    /* Retrofit "Provide: name = EVR" for binary packages. */
	    providePackageNVR(h);

	    if (rpmdbAdd(newdb, h)) {
		rpmError(RPMERR_INTERNAL,
			_("cannot add record originally at %d"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	rpmdbFreeIterator(mi);

    }

    if (!nocleanup) {
	olddb->db_remove_env = 1;
	newdb->db_remove_env = 1;
    }
    rpmdbClose(olddb);
    rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, _("failed to rebuild database: original database "
		"remains in place\n"));

	rpmdbRemoveDatabase(rootdir, newdbpath, _dbapi_rebuild);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(rootdir, newdbpath, _dbapi_rebuild, dbpath, _dbapi)) {
	    rpmMessage(RPMMESS_ERROR, _("failed to replace old database with new "
			"database!\n"));
	    rpmMessage(RPMMESS_ERROR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (removedir && !(rc == 0 && nocleanup)) {
	rpmMessage(RPMMESS_DEBUG, _("removing directory %s\n"), newrootdbpath);
	if (Rmdir(newrootdbpath))
	    rpmMessage(RPMMESS_ERROR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    if (newrootdbpath)	xfree(newrootdbpath);
    if (rootdbpath)	xfree(rootdbpath);

    return rc;
}
