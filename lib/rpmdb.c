/** \ingroup rpmdb dbi
 * \file lib/rpmdb.c
 */

#include "system.h"

#define	_USE_COPY_LOAD	/* XXX don't use DB_DBT_MALLOC (yet) */

#include <sys/file.h>

#ifndef	DYING	/* XXX already in "system.h" */
#include <fnmatch.h>
#endif

#include <regex.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmsq.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmds.h>			/* XXX isInstallPreReq macro only */
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/argv.h>

#include "lib/rpmdb_internal.h"
#include "lib/fprint.h"
#include "lib/header_internal.h"	/* XXX for headerSetInstance() */
#include "debug.h"

int _rpmdb_debug = 0;

static int _rebuildinprogress = 0;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

struct dbiTags_s {
    rpmTag * tags;
    rpmTag max;
    int nlink;
};

/* XXX should dbitags be per-db instead? */
static struct dbiTags_s dbiTags = { NULL, 0, 0 };

/* Bit mask macros. */
typedef	unsigned int __pbm_bits;
#define	__PBM_NBITS		(8 * sizeof (__pbm_bits))
#define	__PBM_IX(d)		((d) / __PBM_NBITS)
#define __PBM_MASK(d)		((__pbm_bits) 1 << (((unsigned)(d)) % __PBM_NBITS))
typedef struct {
    __pbm_bits bits[1];
} pbm_set;
#define	__PBM_BITS(set)	((set)->bits)

#define	PBM_FREE(s)	_free(s);
#define PBM_SET(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] |= __PBM_MASK (d))
#define PBM_CLR(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] &= ~__PBM_MASK (d))
#define PBM_ISSET(d, s) ((__PBM_BITS (s)[__PBM_IX (d)] & __PBM_MASK (d)) != 0)

#define	PBM_ALLOC(d)	xcalloc(__PBM_IX (d) + 1, sizeof(__pbm_bits))

/**
 * Reallocate a bit map.
 * @retval sp		address of bit map pointer
 * @retval odp		no. of bits in map
 * @param nd		desired no. of bits
 */
static inline pbm_set * PBM_REALLOC(pbm_set ** sp, int * odp, int nd)
{
    int i, nb;

    if (nd > (*odp)) {
	nd *= 2;
	nb = __PBM_IX(nd) + 1;
	*sp = xrealloc(*sp, nb * sizeof(__pbm_bits));
	for (i = __PBM_IX(*odp) + 1; i < nb; i++)
	    __PBM_BITS(*sp)[i] = 0;
	*odp = nd;
    }
    return *sp;
}

/**
 * Return dbi index used for rpm tag.
 * @param rpmtag	rpm header tag
 * @return		dbi index, -1 on error
 */
static int dbiTagToDbix(rpmTag rpmtag)
{
    int dbix;

    if (dbiTags.tags != NULL)
    for (dbix = 0; dbix < dbiTags.max; dbix++) {
	if (rpmtag == dbiTags.tags[dbix])
	    return dbix;
    }
    return -1;
}

/**
 * Initialize database (index, tag) tuple from configuration.
 */
static void dbiTagsInit(void)
{
    static const char * const _dbiTagStr_default =
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:ObsoleteName:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Sigmd5:Sha1header:Filedigests:Depends:Pubkeys";
    char * dbiTagStr = NULL;
    char * o, * oe;
    rpmTag rpmtag;

    dbiTags.nlink++;
    if (dbiTags.tags != NULL && dbiTags.max > 0) {
	return;
    }

    dbiTagStr = rpmExpand("%{?_dbi_tags}", NULL);
    if (!(dbiTagStr && *dbiTagStr)) {
	dbiTagStr = _free(dbiTagStr);
	dbiTagStr = xstrdup(_dbiTagStr_default);
    }

    /* Discard previous values. */
    dbiTags.tags = _free(dbiTags.tags);
    dbiTags.max = 0;

    /* Always allocate package index */
    dbiTags.tags = xcalloc(1, sizeof(*dbiTags.tags));
    dbiTags.tags[dbiTags.max++] = RPMDBI_PACKAGES;

    for (o = dbiTagStr; o && *o; o = oe) {
	while (*o && risspace(*o))
	    o++;
	if (*o == '\0')
	    break;
	for (oe = o; oe && *oe; oe++) {
	    if (risspace(*oe))
		break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	rpmtag = rpmTagGetValue(o);
	if (rpmtag == RPMTAG_NOT_FOUND) {
	    rpmlog(RPMLOG_WARNING,
		_("dbiTagsInit: unrecognized tag name: \"%s\" ignored\n"), o);
	    continue;
	}
	if (dbiTagToDbix(rpmtag) >= 0)
	    continue;

	dbiTags.tags = xrealloc(dbiTags.tags, (dbiTags.max + 1) * sizeof(*dbiTags.tags)); /* XXX memory leak */
	dbiTags.tags[dbiTags.max++] = rpmtag;
    }

    dbiTagStr = _free(dbiTagStr);
}

static void dbiTagsFree(void)
{
    if (--dbiTags.nlink > 0) {
	return;
    }
    dbiTags.tags = _free(dbiTags.tags);
    dbiTags.max = 0;
}

#define	DB1vec		NULL
#define	DB2vec		NULL

#ifdef HAVE_DB_H
extern struct _dbiVec db3vec;
#define	DB3vec		&db3vec
#else
#define DB3vec		NULL
#endif

#ifdef HAVE_SQLITE3_H
extern struct _dbiVec sqlitevec;
#define	SQLITEvec	&sqlitevec
#else
#define SQLITEvec	NULL
#endif

static struct _dbiVec * const mydbvecs[] = {
    DB1vec, DB1vec, DB2vec, DB3vec, SQLITEvec, NULL
};

dbiIndex dbiOpen(rpmdb db, rpmTag rpmtag, unsigned int flags)
{
    int dbix;
    dbiIndex dbi = NULL;
    int _dbapi, _dbapi_rebuild, _dbapi_wanted;
    int rc = 0;

    if (db == NULL)
	return NULL;

    dbix = dbiTagToDbix(rpmtag);
    if (dbix < 0 || dbix >= dbiTags.max)
	return NULL;

    /* Is this index already open ? */
    /* FIX: db->_dbi may be NULL */
    if ((dbi = db->_dbi[dbix]) != NULL)
	return dbi;

    /* Try to ensure db home exists, error out if we cant even create */
    if (!db->db_mkdirDone) {
	const char *dbhome = rpmdbHome(db);
	db->db_mkdirDone = (rpmioMkpath(dbhome, 0755, getuid(), getgid()) == 0);
	if (!db->db_mkdirDone) return NULL;
    }

    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 4)
	_dbapi_rebuild = 4;
/*    _dbapi_wanted = (_rebuildinprogress ? -1 : db->db_api); */
    _dbapi_wanted = (_rebuildinprogress ? _dbapi_rebuild : db->db_api);

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 5 || mydbvecs[_dbapi] == NULL) {
            rpmlog(RPMLOG_ERR, _("dbiOpen: dbapi %d not available\n"), _dbapi);
	    return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	if (rc) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmlog(RPMLOG_ERR,
			_("cannot open %s index using db%d - %s (%d)\n"),
			rpmTagGetName(rpmtag), _dbapi,
			(rc > 0 ? strerror(rc) : ""), rc);
	    _dbapi = -1;
	}
	break;
    case -1:
	_dbapi = 5;
	while (_dbapi-- > 1) {
	    if (mydbvecs[_dbapi] == NULL)
		continue;
	    errno = 0;
	    dbi = NULL;
	    rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	    if (rc == 0 && dbi)
		break;
	}
	if (_dbapi <= 0) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmlog(RPMLOG_ERR, _("cannot open %s index\n"),
			rpmTagGetName(rpmtag));
	    rc = 1;
	    goto exit;
	}
	if (db->db_api == -1 && _dbapi > 0)
	    db->db_api = _dbapi;
    	break;
    }

/* We don't ever _REQUIRE_ conversion... */
#define	SQLITE_HACK
#ifdef	SQLITE_HACK_XXX
    /* Require conversion. */
    if (rc && _dbapi_wanted >= 0 && _dbapi != _dbapi_wanted && _dbapi_wanted == _dbapi_rebuild) {
	rc = (_rebuildinprogress ? 0 : 1);
	goto exit;
    }

    /* Suggest possible configuration */
    if (_dbapi_wanted >= 0 && _dbapi != _dbapi_wanted) {
	rc = 1;
	goto exit;
    }

    /* Suggest possible configuration */
    if (_dbapi_wanted < 0 && _dbapi != _dbapi_rebuild) {
	rc = (_rebuildinprogress ? 0 : 1);
	goto exit;
    }
#endif

exit:
    if (dbi != NULL && rc == 0) {
	db->_dbi[dbix] = dbi;
	if (rpmtag == RPMDBI_PACKAGES && db->db_bits == NULL) {
	    db->db_nbits = 1024;
	    if (!dbiStat(dbi, DB_FAST_STAT)) {
		DB_HASH_STAT * hash = (DB_HASH_STAT *)dbi->dbi_stats;
		if (hash)
		    db->db_nbits += hash->hash_nkeys;
	    }
	    db->db_bits = PBM_ALLOC(db->db_nbits);
	}
    }
#ifdef HAVE_DB_H
      else
	dbi = db3Free(dbi);
#endif

/* FIX: db->_dbi may be NULL */
    return dbi;
}

/* Retrieve (key,data) pair from index database. */
int dbiGet(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
{
    int rc;
    assert((flags == DB_NEXT) || (key->data != NULL && key->size > 0));
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_getops, 0);
    rc = (dbi->dbi_vec->cget) (dbi, dbcursor, key, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_getops, data->size);
    return rc;
}

/* Store (key,data) pair in index database. */
int dbiPut(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
{
    int rc;
    assert(key->data != NULL && key->size > 0 && data->data != NULL && data->size > 0);
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_putops, (ssize_t) 0);
    rc = (dbi->dbi_vec->cput) (dbi, dbcursor, key, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_putops, (ssize_t) data->size);
    return rc;
}

/**
 * Create and initialize item for index database set.
 * @param hdrNum	header instance in db
 * @param tagNum	tag index in header
 * @return		new item
 */
static dbiIndexItem dbiIndexNewItem(unsigned int hdrNum, unsigned int tagNum)
{
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
\
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
\
  }

/* 
 * Ensure sufficient memory for nrecs of new records in dbiIndexSet.
 * Allocate in power of two sizes to avoid memory fragmentation, so
 * realloc is not always needed.
 */
static inline void dbiGrowSet(dbiIndexSet set, unsigned int nrecs)
{
    size_t need = (set->count + nrecs) * sizeof(*(set->recs));
    size_t alloced = set->alloced ? set->alloced : 1 << 4;

    while (alloced < need)
	alloced <<= 1;

    if (alloced != set->alloced) {
	set->recs = xrealloc(set->recs, alloced);
	set->alloced = alloced;
    }
}

/**
 * Convert retrieved data to index set.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @retval setp		(malloc'ed) index set
 * @return		0 on success
 */
static int dbt2set(dbiIndex dbi, DBT * data, dbiIndexSet * setp)
{
    int _dbbyteswapped = dbiByteSwapped(dbi);
    const char * sdbir;
    dbiIndexSet set;
    unsigned int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	return -1;

    if ((sdbir = data->data) == NULL) {
	*setp = NULL;
	return 0;
    }

    set = xcalloc(1, sizeof(*set));
    dbiGrowSet(set, data->size / dbi->dbi_jlen);
    set->count = data->size / dbi->dbi_jlen;

    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(int32_t):
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
	}
	break;
    case 1*sizeof(int32_t):
	for (i = 0; i < set->count; i++) {
	    union _dbswap hdrNum;

	    memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
	    sdbir += sizeof(hdrNum.ui);
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
	    }
	    set->recs[i].hdrNum = hdrNum.ui;
	    set->recs[i].tagNum = 0;
	}
	break;
    }
    *setp = set;
    return 0;
}

/**
 * Convert index set to database representation.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @param set		index set
 * @return		0 on success
 */
static int set2dbt(dbiIndex dbi, DBT * data, dbiIndexSet set)
{
    int _dbbyteswapped = dbiByteSwapped(dbi);
    char * tdbir;
    unsigned int i;

    if (dbi == NULL || data == NULL || set == NULL)
	return -1;

    data->size = set->count * (dbi->dbi_jlen);
    if (data->size == 0) {
	data->data = NULL;
	return 0;
    }
    tdbir = data->data = xmalloc(data->size);

    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(int32_t):
	for (i = 0; i < set->count; i++) {
	    union _dbswap hdrNum, tagNum;

	    memset(&hdrNum, 0, sizeof(hdrNum));
	    memset(&tagNum, 0, sizeof(tagNum));
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
    case 1*sizeof(int32_t):
	for (i = 0; i < set->count; i++) {
	    union _dbswap hdrNum;

	    memset(&hdrNum, 0, sizeof(hdrNum));
	    hdrNum.ui = set->recs[i].hdrNum;
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
	    }
	    memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
	    tdbir += sizeof(hdrNum.ui);
	}
	break;
    }

    return 0;
}

/* XXX assumes hdrNum is first int in dbiIndexItem */
static int hdrNumCmp(const void * one, const void * two)
{
    const unsigned int * a = one, * b = two;
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
static int dbiAppendSet(dbiIndexSet set, const void * recs,
	int nrecs, size_t recsize, int sortset)
{
    const char * rptr = recs;
    size_t rlen = (recsize < sizeof(*(set->recs)))
		? recsize : sizeof(*(set->recs));

    if (set == NULL || recs == NULL || nrecs <= 0 || recsize == 0)
	return 1;

    dbiGrowSet(set, nrecs);
    memset(set->recs + set->count, 0, nrecs * sizeof(*(set->recs)));

    while (nrecs-- > 0) {
	memcpy(set->recs + set->count, rptr, rlen);
	rptr += recsize;
	set->count++;
    }

    if (sortset && set->count > 1)
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
static int dbiPruneSet(dbiIndexSet set, void * recs, int nrecs,
		size_t recsize, int sorted)
{
    unsigned int from;
    unsigned int to = 0;
    unsigned int num = set->count;
    unsigned int numCopied = 0;

    assert(set->count > 0);
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
dbiIndexSet dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	set->recs = _free(set->recs);
	set = _free(set);
    }
    return set;
}

typedef struct miRE_s {
    rpmTag		tag;		/*!< header tag */
    rpmMireMode		mode;		/*!< pattern match mode */
    char *		pattern;	/*!< pattern string */
    int			notmatch;	/*!< like "grep -v" */
    regex_t *		preg;		/*!< regex compiled pattern buffer */
    int			cflags;		/*!< regcomp(3) flags */
    int			eflags;		/*!< regexec(3) flags */
    int			fnflags;	/*!< fnmatch(3) flags */
} * miRE;

struct rpmdbMatchIterator_s {
    rpmdbMatchIterator	mi_next;
    void *		mi_keyp;
    size_t		mi_keylen;
    rpmdb		mi_db;
    rpmTag		mi_rpmtag;
    dbiIndexSet		mi_set;
    DBC *		mi_dbc;
    DBT			mi_key;
    DBT			mi_data;
    int			mi_setx;
    Header		mi_h;
    int			mi_sorted;
    int			mi_cflags;
    int			mi_modified;
    unsigned int	mi_prevoffset;	/* header instance (native endian) */
    unsigned int	mi_offset;	/* header instance (native endian) */
    unsigned int	mi_filenum;	/* tag element (native endian) */
    int			mi_nre;
    miRE		mi_re;
    rpmts		mi_ts;
    rpmRC (*mi_hdrchk) (rpmts ts, const void * uh, size_t uc, char ** msg);

};

static rpmdb rpmdbRock;

static rpmdbMatchIterator rpmmiRock;

int rpmdbCheckTerminate(int terminate)
{
    sigset_t newMask, oldMask;
    static int terminating = 0;

    if (terminating) return 0;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);

    if (rpmsqIsCaught(SIGINT) > 0
     || rpmsqIsCaught(SIGQUIT) > 0
     || rpmsqIsCaught(SIGHUP) > 0
     || rpmsqIsCaught(SIGTERM) > 0
     || rpmsqIsCaught(SIGPIPE) > 0
     || terminate)
	terminating = 1;

    if (terminating) {
	rpmdb db;
	rpmdbMatchIterator mi;

	while ((mi = rpmmiRock) != NULL) {
	    rpmmiRock = mi->mi_next;
	    mi->mi_next = NULL;
	    mi = rpmdbFreeIterator(mi);
	}

	while ((db = rpmdbRock) != NULL) {
	    rpmdbRock = db->db_next;
	    db->db_next = NULL;
	    (void) rpmdbClose(db);
	}
    }
    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return terminating;
}

int rpmdbCheckSignals(void)
{
    if (rpmdbCheckTerminate(0)) {
	rpmlog(RPMLOG_DEBUG, "Exiting on signal...\n");
	exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Block all signals, returning previous signal mask.
 */
static int blockSignals(sigset_t * oldMask)
{
    sigset_t newMask;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, oldMask);
    (void) sigdelset(&newMask, SIGINT);
    (void) sigdelset(&newMask, SIGQUIT);
    (void) sigdelset(&newMask, SIGHUP);
    (void) sigdelset(&newMask, SIGTERM);
    (void) sigdelset(&newMask, SIGPIPE);
    return sigprocmask(SIG_BLOCK, &newMask, NULL);
}

/**
 * Restore signal mask.
 */
static int unblockSignals(sigset_t * oldMask)
{
    (void) rpmdbCheckSignals();
    return sigprocmask(SIG_SETMASK, oldMask, NULL);
}

#define	_DB_ROOT	"/"
#define	_DB_HOME	"%{_dbpath}"
#define _DB_FULLPATH	NULL
#define	_DB_FLAGS	0
#define _DB_MODE	0
#define _DB_PERMS	0644

#define _DB_MAJOR	-1
#define	_DB_ERRPFX	"rpmdb"

static struct rpmdb_s const dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FULLPATH, _DB_FLAGS, _DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_ERRPFX
};

static int isTemporaryDB(rpmTag rpmtag) 
{
    int rc = 0;
    switch (rpmtag) {
    case RPMDBI_AVAILABLE:
    case RPMDBI_ADDED:
    case RPMDBI_REMOVED:
    case RPMDBI_DEPENDS:
	rc = 1;
	break;
    default:
	break;
    }
    return rc;
}

rpmop rpmdbOp(rpmdb rpmdb, rpmdbOpX opx)
{
    rpmop op = NULL;
    switch (opx) {
    case RPMDB_OP_DBGET:
	op = &rpmdb->db_getops;
	break;
    case RPMDB_OP_DBPUT:
	op = &rpmdb->db_putops;
	break;
    case RPMDB_OP_DBDEL:
	op = &rpmdb->db_delops;
	break;
    default:
	break;
    }
    return op;
}

const char *rpmdbHome(rpmdb db)
{
    const char *dbdir = NULL;
    if (db) {
	dbdir = db->db_chrootDone ? db->db_home : db->db_fullpath;
    }
    return dbdir;
}

int rpmdbSetChrootDone(rpmdb db, int chrootDone)
{
    int ochrootDone = 0;
    if (db != NULL) {
	ochrootDone = db->db_chrootDone;
	db->db_chrootDone = chrootDone;
    }
    return ochrootDone;
}

int rpmdbOpenAll(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return -2;

    if (dbiTags.tags != NULL)
    for (dbix = 0; dbix < dbiTags.max; dbix++) {
	if (db->_dbi[dbix] != NULL)
	    continue;
	/* Filter out temporary databases */
	if (isTemporaryDB(dbiTags.tags[dbix])) 
	    continue;
	(void) dbiOpen(db, dbiTags.tags[dbix], db->db_flags);
    }
    return rc;
}

int rpmdbCloseDBI(rpmdb db, rpmTag rpmtag)
{
    int dbix;
    int rc = 0;

    if (db == NULL || db->_dbi == NULL || dbiTags.tags == NULL)
	return 0;

    for (dbix = 0; dbix < dbiTags.max; dbix++) {
	if (dbiTags.tags[dbix] != rpmtag)
	    continue;
	if (db->_dbi[dbix] != NULL) {
	    int xx;
	   		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	}
	break;
    }
    return rc;
}

/* XXX query.c, rpminstall.c, verify.c */
int rpmdbClose(rpmdb db)
{
    rpmdb * prev, next;
    int dbix;
    int rc = 0;

    if (db == NULL)
	goto exit;

    (void) rpmdbUnlink(db, RPMDBG_M("rpmdbClose"));

    if (db->nrefs > 0)
	goto exit;

    if (db->_dbi)
    for (dbix = db->db_ndbi; --dbix >= 0; ) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
    	xx = dbiClose(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    	db->_dbi[dbix] = NULL;
    }
    db->db_errpfx = _free(db->db_errpfx);
    db->db_root = _free(db->db_root);
    db->db_home = _free(db->db_home);
    db->db_fullpath = _free(db->db_fullpath);
    db->db_bits = PBM_FREE(db->db_bits);
    db->_dbi = _free(db->_dbi);

    prev = &rpmdbRock;
    while ((next = *prev) != NULL && next != db)
	prev = &next->db_next;
    if (next) {
        *prev = next->db_next;
	next->db_next = NULL;
    }

    db = _free(db);

    dbiTagsFree();

exit:
    (void) rpmsqEnable(-SIGHUP,	NULL);
    (void) rpmsqEnable(-SIGINT,	NULL);
    (void) rpmsqEnable(-SIGTERM,NULL);
    (void) rpmsqEnable(-SIGQUIT,NULL);
    (void) rpmsqEnable(-SIGPIPE,NULL);
    return rc;
}

int rpmdbSync(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return 0;
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
	if (db->_dbi[dbix]->dbi_no_dbsync)
	    continue;
    	xx = dbiSync(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    }
    return rc;
}

/* FIX: dbTemplate structure assignment */
static
rpmdb newRpmdb(const char * root,
		const char * home,
		int mode, int perms, int flags)
{
    rpmdb db = xcalloc(sizeof(*db), 1);
    const char * epfx = _DB_ERRPFX;
    static int _initialized = 0;

    if (!_initialized) {
	_initialized = 1;
    }

    *db = dbTemplate;	/* structure assignment */

    db->_dbi = NULL;

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (mode >= 0)	db->db_mode = mode;
    if (perms >= 0)	db->db_perms = perms;
    if (flags >= 0)	db->db_flags = flags;

    db->db_home = rpmGetPath( (home && *home ? home : _DB_HOME), NULL);
    if (!(db->db_home && db->db_home[0] != '%')) {
	rpmlog(RPMLOG_ERR, _("no dbpath has been set\n"));
	db->db_home = _free(db->db_home);
	db = _free(db);
	return NULL;
    }
    db->db_root = rpmGetPath((root && *root) ? root : _DB_ROOT, NULL);
    db->db_fullpath = rpmGenPath(db->db_root, db->db_home, NULL);
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);
    /* XXX remove environment after chrooted operations, for now... */
    db->db_remove_env = (!rstreq(db->db_root, "/") ? 1 : 0);
    db->db_ndbi = dbiTags.max;
    db->db_malloc = rmalloc;
    db->db_realloc = rrealloc;
    db->db_free = NULL; /* XXX rfree() prototype differs from free() */
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));
    db->nrefs = 0;
    return rpmdbLink(db, RPMDBG_M("rpmdbCreate"));
}

static int openDatabase(const char * prefix,
		const char * dbpath,
		int _dbapi, rpmdb *dbp,
		int mode, int perms, int flags)
{
    rpmdb db;
    int rc, xx;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    dbiTagsInit();

    /* Insure that _dbapi has one of -1, 1, 2, or 3 */
    if (_dbapi < -1 || _dbapi > 4)
	_dbapi = -1;
    if (_dbapi == 0)
	_dbapi = 1;

    if (dbp)
	*dbp = NULL;
    if ((mode & O_ACCMODE) == O_WRONLY) 
	return 1;

    db = newRpmdb(prefix, dbpath, mode, perms, flags);
    if (db == NULL)
	return 1;

    (void) rpmsqEnable(SIGHUP,	NULL);
    (void) rpmsqEnable(SIGINT,	NULL);
    (void) rpmsqEnable(SIGTERM,NULL);
    (void) rpmsqEnable(SIGQUIT,NULL);
    (void) rpmsqEnable(SIGPIPE,NULL);

    db->db_api = _dbapi;

    {	int dbix;

	rc = 0;
	if (dbiTags.tags != NULL)
	for (dbix = 0; rc == 0 && dbix < dbiTags.max; dbix++) {
	    dbiIndex dbi;
	    rpmTag rpmtag;

	    /* Filter out temporary databases */
	    if (isTemporaryDB((rpmtag = dbiTags.tags[dbix])))
		continue;

	    dbi = dbiOpen(db, rpmtag, 0);
	    if (dbi == NULL) {
		rc = -2;
		break;
	    }

	    switch (rpmtag) {
	    case RPMDBI_PACKAGES:
		if (dbi == NULL) rc |= 1;
#if 0
		/* XXX open only Packages, indices created on the fly. */
		if (db->db_api == 3)
#endif
		    goto exit;
		break;
	    case RPMTAG_NAME:
		if (dbi == NULL) rc |= 1;
		if (minimal)
		    goto exit;
		break;
	    default:
		break;
	    }
	}
    }

exit:
    if (rc || justCheck || dbp == NULL)
	xx = rpmdbClose(db);
    else {
	db->db_next = rpmdbRock;
	rpmdbRock = db;
        *dbp = db;
    }

    return rc;
}

rpmdb rpmdbUnlink(rpmdb db, const char * msg)
{
if (_rpmdb_debug)
fprintf(stderr, "--> db %p -- %d %s\n", db, db->nrefs, msg);
    db->nrefs--;
    return NULL;
}

rpmdb rpmdbLink(rpmdb db, const char * msg)
{
    db->nrefs++;
if (_rpmdb_debug)
fprintf(stderr, "--> db %p ++ %d %s\n", db, db->nrefs, msg);
    return db;
}

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    return openDatabase(prefix, NULL, _dbapi, dbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc;

    rc = openDatabase(prefix, NULL, _dbapi, &db, (O_CREAT | O_RDWR),
		perms, RPMDB_FLAG_JUSTCHECK);
    if (db != NULL) {
	int xx;
	xx = rpmdbOpenAll(db);
	if (xx && rc == 0) rc = xx;
	xx = rpmdbClose(db);
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
    return rc;
}

int rpmdbVerify(const char * prefix)
{
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc = 0;

    rc = openDatabase(prefix, NULL, _dbapi, &db, O_RDONLY, 0644, 0);

    if (db != NULL) {
	int dbix;
	int xx;
	rc = rpmdbOpenAll(db);

	for (dbix = db->db_ndbi; --dbix >= 0; ) {
	    if (db->_dbi[dbix] == NULL)
		continue;
	   		/* FIX: double indirection. */
	    xx = dbiVerify(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	}

	/* FIX: db->_dbi[] may be NULL. */
	xx = rpmdbClose(db);
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
    return rc;
}

/**
 * Find file matches in database.
 * @param db		rpm database
 * @param filespec
 * @param key
 * @param data
 * @param matches
 * @return		0 on success, 1 on not found, -2 on error
 */
static int rpmdbFindByFile(rpmdb db, const char * filespec,
		DBT * key, DBT * data, dbiIndexSet * matches)
{
    char * dirName = NULL;
    const char * baseName;
    fingerPrintCache fpc = NULL;
    fingerPrint fp1;
    dbiIndex dbi = NULL;
    DBC * dbcursor;
    dbiIndexSet allMatches = NULL;
    dbiIndexItem rec = NULL;
    unsigned int i;
    int rc = -2; /* assume error */
    int xx;

    *matches = NULL;
    if (filespec == NULL) return rc; /* nothing alloced yet */

    if ((baseName = strrchr(filespec, '/')) != NULL) {
	size_t len = baseName - filespec + 1;
	dirName = strncpy(xmalloc(len + 1), filespec, len);
	dirName[len] = '\0';
	baseName++;
    } else {
	dirName = xstrdup("");
	baseName = filespec;
    }
    if (baseName == NULL)
	goto exit;

    dbi = dbiOpen(db, RPMTAG_BASENAMES, 0);
    if (dbi != NULL) {
	dbcursor = NULL;
	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

	key->data = (void *) baseName;
	key->size = strlen(baseName);
	if (key->size == 0) 
	    key->size++;	/* XXX "/" fixup. */

	rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	if (rc > 0) {
	    rpmlog(RPMLOG_ERR,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, (char*)key->data, rpmTagGetName(dbi->dbi_rpmtag));
	}

	if (rc == 0)
	    (void) dbt2set(dbi, data, &allMatches);

	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
    } else
	rc = -2;

    if (rc || allMatches == NULL) goto exit;

    *matches = xcalloc(1, sizeof(**matches));
    rec = dbiIndexNewItem(0, 0);
    fpc = fpCacheCreate(allMatches->count);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    i = 0;
    while (i < allMatches->count) {
	struct rpmtd_s bn, dn, di;
	const char ** baseNames, ** dirNames;
	uint32_t * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

	{   rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, &offset, sizeof(offset));
	    h = rpmdbNextIterator(mi);
	    if (h)
		h = headerLink(h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (h == NULL) {
	    i++;
	    continue;
	}

	headerGet(h, RPMTAG_BASENAMES, &bn, HEADERGET_MINMEM);
	headerGet(h, RPMTAG_DIRNAMES, &dn, HEADERGET_MINMEM);
	headerGet(h, RPMTAG_DIRINDEXES, &di, HEADERGET_MINMEM);
	baseNames = bn.data;
	dirNames = dn.data;
	dirIndexes = di.data;

	do {
	    fingerPrint fp2;
	    int num = dbiIndexRecordFileNumber(allMatches, i);

	    fp2 = fpLookup(fpc, dirNames[dirIndexes[num]], baseNames[num], 1);
	    if (FP_EQUAL(fp1, fp2)) {
		rec->hdrNum = dbiIndexRecordOffset(allMatches, i);
		rec->tagNum = dbiIndexRecordFileNumber(allMatches, i);
		xx = dbiAppendSet(*matches, rec, 1, sizeof(*rec), 0);
	    }

	    prevoff = offset;
	    i++;
	    if (i < allMatches->count)
		offset = dbiIndexRecordOffset(allMatches, i);
	} while (i < allMatches->count && offset == prevoff);

	rpmtdFreeData(&bn);
	rpmtdFreeData(&dn);
	rpmtdFreeData(&di);
	h = headerFree(h);
    }

    rec = _free(rec);
    fpCacheFree(fpc);

    if ((*matches)->count == 0) {
	*matches = dbiFreeIndexSet(*matches);
	rc = 1;
    } else {
	rc = 0;
    }

exit:
    dbiFreeIndexSet(allMatches);
    free(dirName);
    return rc;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb db, const char * name)
{
    DBC * dbcursor = NULL;
    DBT key; 
    DBT data; 
    dbiIndex dbi;
    int rc;
    int xx;

    if (db == NULL)
	return 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    dbi = dbiOpen(db, RPMTAG_NAME, 0);
    if (dbi == NULL)
	return 0;

    key.data = (void *) name;
    key.size = strlen(name);

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
#ifndef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (rc == 0) {		/* success */
	dbiIndexSet matches;
	/* FIX: matches might be NULL */
	matches = NULL;
	(void) dbt2set(dbi, &data, &matches);
	if (matches) {
	    rc = dbiIndexSetCount(matches);
	    matches = dbiFreeIndexSet(matches);
	}
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	rc = 0;
    } else {			/* error */
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
	rc = -1;
    }

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    return rc;
}

/**
 * Attempt partial matches on name[-version[-release]] strings.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param key		search key/length/flags
 * @param data		search data/length/flags
 * @param name		package name
 * @param version	package version (can be a pattern)
 * @param release	package release (can be a pattern)
 * @retval matches	set of header instances that match
 * @return 		RPMRC_OK on match, RPMRC_NOMATCH or RPMRC_FAIL
 */
static rpmRC dbiFindMatches(dbiIndex dbi, DBC * dbcursor,
		DBT * key, DBT * data,
		const char * name,
		const char * version,
		const char * release,
		dbiIndexSet * matches)
{
    unsigned int gotMatches = 0;
    int rc;
    unsigned int i;

    key->data = (void *) name;
    key->size = strlen(name);

    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);

    if (rc == 0) {		/* success */
	(void) dbt2set(dbi, data, matches);
	if (version == NULL && release == NULL)
	    return RPMRC_OK;
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	return RPMRC_NOTFOUND;
    } else {			/* error */
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, (char*)key->data, rpmTagGetName(dbi->dbi_rpmtag));
	return RPMRC_FAIL;
    }

    /* Make sure the version and release match. */
    for (i = 0; i < dbiIndexSetCount(*matches); i++) {
	unsigned int recoff = dbiIndexRecordOffset(*matches, i);
	rpmdbMatchIterator mi;
	Header h;

	if (recoff == 0)
	    continue;

	mi = rpmdbInitIterator(dbi->dbi_rpmdb,
			RPMDBI_PACKAGES, &recoff, sizeof(recoff));

	/* Set iterator selectors for version/release if available. */
	if (version &&
	    rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT, version))
	{
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	if (release &&
	    rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT, release))
	{
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	h = rpmdbNextIterator(mi);
	if (h)
	    (*matches)->recs[gotMatches++] = (*matches)->recs[i];
	else
	    (*matches)->recs[i].hdrNum = 0;
	mi = rpmdbFreeIterator(mi);
    }

    if (gotMatches) {
	(*matches)->count = gotMatches;
	rc = RPMRC_OK;
    } else
	rc = RPMRC_NOTFOUND;

exit:
/* FIX: double indirection */
    if (rc && matches && *matches)
	*matches = dbiFreeIndexSet(*matches);
    return rc;
}

/**
 * Lookup by name, name-version, and finally by name-version-release.
 * Both version and release can be patterns.
 * @todo Name must be an exact match, as name is a db key.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param key		search key/length/flags
 * @param data		search data/length/flags
 * @param arg		name[-version[-release]] string
 * @retval matches	set of header instances that match
 * @return 		RPMRC_OK on match, RPMRC_NOMATCH or RPMRC_FAIL
 */
static rpmRC dbiFindByLabel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		const char * arg, dbiIndexSet * matches)
{
    const char * release;
    char * localarg;
    char * s;
    char c;
    int brackets;
    rpmRC rc;
 
    if (arg == NULL || strlen(arg) == 0) return RPMRC_NOTFOUND;

    /* did they give us just a name? */
    rc = dbiFindMatches(dbi, dbcursor, key, data, arg, NULL, NULL, matches);
    if (rc != RPMRC_NOTFOUND) return rc;

    /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);

    /* maybe a name and a release */
    localarg = xmalloc(strlen(arg) + 1);
    s = stpcpy(localarg, arg);

    c = '\0';
    brackets = 0;
    for (s -= 1; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    break;
	case ']':
	    if (c != '[') brackets = 0;
	    break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

   	/* FIX: *matches may be NULL. */
    if (s == localarg) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    *s = '\0';
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, NULL, matches);
    if (rc != RPMRC_NOTFOUND) goto exit;

    /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);
    
    /* how about name-version-release? */

    release = s + 1;

    c = '\0';
    brackets = 0;
    for (; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    break;
	case ']':
	    if (c != '[') brackets = 0;
	    break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

    if (s == localarg) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    *s = '\0';
   	/* FIX: *matches may be NULL. */
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, release, matches);
exit:
    free(localarg);
    return rc;
}

/**
 * Rewrite a header into packages (if necessary) and free the header.
 *   Note: this is called from a markReplacedFiles iteration, and *must*
 *   preserve the "join key" (i.e. offset) for the header.
 * @param mi		database iterator
 * @param dbi		index database handle
 * @return 		0 on success
 */
static int miFreeHeader(rpmdbMatchIterator mi, dbiIndex dbi)
{
    int rc = 0;

    if (mi == NULL || mi->mi_h == NULL)
	return 0;

    if (dbi && mi->mi_dbc && mi->mi_modified && mi->mi_prevoffset) {
	DBT * key = &mi->mi_key;
	DBT * data = &mi->mi_data;
	sigset_t signalMask;
	rpmRC rpmrc = RPMRC_NOTFOUND;
	int xx;

	key->data = (void *) &mi->mi_prevoffset;
	key->size = sizeof(mi->mi_prevoffset);
	data->data = headerUnload(mi->mi_h);
	data->size = headerSizeof(mi->mi_h, HEADER_MAGIC_NO);

	/* Check header digest/signature on blob export (if requested). */
	if (mi->mi_hdrchk && mi->mi_ts) {
	    char * msg = NULL;
	    int lvl;

	    rpmrc = (*mi->mi_hdrchk) (mi->mi_ts, data->data, data->size, &msg);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
	    rpmlog(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("miFreeHeader: skipping") : "write"),
			mi->mi_prevoffset, (msg ? msg : "\n"));
	    msg = _free(msg);
	}

	if (data->data != NULL && rpmrc != RPMRC_FAIL) {
	    (void) blockSignals(&signalMask);
	    rc = dbiPut(dbi, mi->mi_dbc, key, data, DB_KEYLAST);
	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("error(%d) storing record #%d into %s\n"),
			rc, mi->mi_prevoffset, rpmTagGetName(dbi->dbi_rpmtag));
	    }
	    xx = dbiSync(dbi, 0);
	    (void) unblockSignals(&signalMask);
	}
	data->data = _free(data->data);
	data->size = 0;
    }

    mi->mi_h = headerFree(mi->mi_h);

    return rc;
}

rpmdbMatchIterator rpmdbFreeIterator(rpmdbMatchIterator mi)
{
    rpmdbMatchIterator * prev, next;
    dbiIndex dbi;
    int xx;
    int i;

    if (mi == NULL)
	return NULL;

    prev = &rpmmiRock;
    while ((next = *prev) != NULL && next != mi)
	prev = &next->mi_next;
    if (next) {
	*prev = next->mi_next;
	next->mi_next = NULL;
    }

    dbi = dbiOpen(mi->mi_db, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)	/* XXX can't happen */
	return NULL;

    xx = miFreeHeader(mi, dbi);

    if (mi->mi_dbc)
	xx = dbiCclose(dbi, mi->mi_dbc, 0);
    mi->mi_dbc = NULL;

    if (mi->mi_re != NULL)
    for (i = 0; i < mi->mi_nre; i++) {
	miRE mire = mi->mi_re + i;
	mire->pattern = _free(mire->pattern);
	if (mire->preg != NULL) {
	    regfree(mire->preg);
	    mire->preg = _free(mire->preg);
	}
    }
    mi->mi_re = _free(mi->mi_re);

    mi->mi_set = dbiFreeIndexSet(mi->mi_set);
    mi->mi_keyp = _free(mi->mi_keyp);
    mi->mi_db = rpmdbUnlink(mi->mi_db, RPMDBG_M("matchIterator"));

    mi = _free(mi);

    (void) rpmdbCheckSignals();

    return mi;
}

unsigned int rpmdbGetIteratorOffset(rpmdbMatchIterator mi) {
    return (mi ? mi->mi_offset : 0);
}

unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi) {
    return (mi ? mi->mi_filenum : 0);
}

int rpmdbGetIteratorCount(rpmdbMatchIterator mi) {
    return (mi && mi->mi_set ?  mi->mi_set->count : 0);
}

/**
 * Return pattern match.
 * @param mire		match iterator regex
 * @param val		value to match
 * @return		0 if pattern matches, >0 on nomatch, <0 on error
 */
static int miregexec(miRE mire, const char * val)
{
    int rc = 0;

    switch (mire->mode) {
    case RPMMIRE_STRCMP:
	rc = (!rstreq(mire->pattern, val));
	break;
    case RPMMIRE_DEFAULT:
    case RPMMIRE_REGEX:
	rc = regexec(mire->preg, val, 0, NULL, mire->eflags);
	if (rc && rc != REG_NOMATCH) {
	    char msg[256];
	    (void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmlog(RPMLOG_ERR, _("%s: regexec failed: %s\n"),
			mire->pattern, msg);
	    rc = -1;
	}
	break;
    case RPMMIRE_GLOB:
	rc = fnmatch(mire->pattern, val, mire->fnflags);
	if (rc && rc != FNM_NOMATCH)
	    rc = -1;
	break;
    default:
	rc = -1;
	break;
    }

    return rc;
}

/**
 * Compare iterator selectors by rpm tag (qsort/bsearch).
 * @param a		1st iterator selector
 * @param b		2nd iterator selector
 * @return		result of comparison
 */
static int mireCmp(const void * a, const void * b)
{
    const miRE mireA = (const miRE) a;
    const miRE mireB = (const miRE) b;
    return (mireA->tag - mireB->tag);
}

/**
 * Copy pattern, escaping for appropriate mode.
 * @param tag		rpm tag
 * @retval modep	type of pattern match
 * @param pattern	pattern to duplicate
 * @return		duplicated pattern
 */
static char * mireDup(rpmTag tag, rpmMireMode *modep,
			const char * pattern)
{
    const char * s;
    char * pat;
    char * t;
    int brackets;
    size_t nb;
    int c;

    switch (*modep) {
    default:
    case RPMMIRE_DEFAULT:
	if (tag == RPMTAG_DIRNAMES || tag == RPMTAG_BASENAMES) {
	    *modep = RPMMIRE_GLOB;
	    pat = xstrdup(pattern);
	    break;
	}

	nb = strlen(pattern) + sizeof("^$");

	/* Find no. of bytes needed for pattern. */
	/* periods and plusses are escaped, splats become '.*' */
	c = '\0';
	brackets = 0;
	for (s = pattern; *s != '\0'; s++) {
	    switch (*s) {
	    case '.':
	    case '+':
	    case '*':
		if (!brackets) nb++;
		break;
	    case '\\':
		s++;
		break;
	    case '[':
		brackets = 1;
		break;
	    case ']':
		if (c != '[') brackets = 0;
		break;
	    }
	    c = *s;
	}

	pat = t = xmalloc(nb);

	if (pattern[0] != '^') *t++ = '^';

	/* Copy pattern, escaping periods, prefixing splats with period. */
	c = '\0';
	brackets = 0;
	for (s = pattern; *s != '\0'; s++, t++) {
	    switch (*s) {
	    case '.':
	    case '+':
		if (!brackets) *t++ = '\\';
		break;
	    case '*':
		if (!brackets) *t++ = '.';
		break;
	    case '\\':
		*t++ = *s++;
		break;
	    case '[':
		brackets = 1;
		break;
	    case ']':
		if (c != '[') brackets = 0;
		break;
	    }
	    c = *t = *s;
	}

	if (s > pattern && s[-1] != '$') *t++ = '$';
	*t = '\0';
	*modep = RPMMIRE_REGEX;
	break;
    case RPMMIRE_STRCMP:
    case RPMMIRE_REGEX:
    case RPMMIRE_GLOB:
	pat = xstrdup(pattern);
	break;
    }

    return pat;
}

int rpmdbSetIteratorRE(rpmdbMatchIterator mi, rpmTag tag,
		rpmMireMode mode, const char * pattern)
{
    static rpmMireMode defmode = (rpmMireMode)-1;
    miRE mire = NULL;
    char * allpat = NULL;
    int notmatch = 0;
    regex_t * preg = NULL;
    int cflags = 0;
    int eflags = 0;
    int fnflags = 0;
    int rc = 0;

    if (defmode == (rpmMireMode)-1) {
	char *t = rpmExpand("%{?_query_selector_match}", NULL);

	if (*t == '\0' || rstreq(t, "default"))
	    defmode = RPMMIRE_DEFAULT;
	else if (rstreq(t, "strcmp"))
	    defmode = RPMMIRE_STRCMP;
	else if (rstreq(t, "regex"))
	    defmode = RPMMIRE_REGEX;
	else if (rstreq(t, "glob"))
	    defmode = RPMMIRE_GLOB;
	else
	    defmode = RPMMIRE_DEFAULT;
	t = _free(t);
     }

    if (mi == NULL || pattern == NULL)
	return rc;

    /* Leading '!' inverts pattern match sense, like "grep -v". */
    if (*pattern == '!') {
	notmatch = 1;
	pattern++;
    }

    allpat = mireDup(tag, &mode, pattern);

    if (mode == RPMMIRE_DEFAULT)
	mode = defmode;

    switch (mode) {
    case RPMMIRE_DEFAULT:
    case RPMMIRE_STRCMP:
	break;
    case RPMMIRE_REGEX:
	preg = xcalloc(1, sizeof(*preg));
	cflags = (REG_EXTENDED | REG_NOSUB);
	rc = regcomp(preg, allpat, cflags);
	if (rc) {
	    char msg[256];
	    (void) regerror(rc, preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmlog(RPMLOG_ERR, _("%s: regcomp failed: %s\n"), allpat, msg);
	}
	break;
    case RPMMIRE_GLOB:
	fnflags = FNM_PATHNAME | FNM_PERIOD;
	break;
    default:
	rc = -1;
	break;
    }

    if (rc) {
	/* FIX: mire has kept values */
	allpat = _free(allpat);
	if (preg) {
	    regfree(preg);
	    preg = _free(preg);
	}
	return rc;
    }

    mi->mi_re = xrealloc(mi->mi_re, (mi->mi_nre + 1) * sizeof(*mi->mi_re));
    mire = mi->mi_re + mi->mi_nre;
    mi->mi_nre++;
    
    mire->tag = tag;
    mire->mode = mode;
    mire->pattern = allpat;
    mire->notmatch = notmatch;
    mire->preg = preg;
    mire->cflags = cflags;
    mire->eflags = eflags;
    mire->fnflags = fnflags;

    if (mi->mi_nre > 1)
	qsort(mi->mi_re, mi->mi_nre, sizeof(*mi->mi_re), mireCmp);

    return rc;
}

/**
 * Return iterator selector match.
 * @param mi		rpm database iterator
 * @return		1 if header should be skipped
 */
static int mireSkip (const rpmdbMatchIterator mi)
{
    miRE mire;
    uint32_t zero = 0;
    int ntags = 0;
    int nmatches = 0;
    int rc;

    if (mi->mi_h == NULL)	/* XXX can't happen */
	return 0;

    /*
     * Apply tag tests, implicitly "||" for multiple patterns/values of a
     * single tag, implicitly "&&" between multiple tag patterns.
     */
    if ((mire = mi->mi_re) != NULL)
    for (int i = 0; i < mi->mi_nre; i++, mire++) {
	int anymatch;
	struct rpmtd_s td;

	if (!headerGet(mi->mi_h, mire->tag, &td, HEADERGET_MINMEM)) {
	    if (mire->tag != RPMTAG_EPOCH) {
		ntags++;
		continue;
	    }
	    /* "is package already installed" checks rely on this behavior */
	    td.count = 1;
	    td.type = RPM_INT32_TYPE;
	    td.data = &zero;
	}

	anymatch = 0;		/* no matches yet */
	while (1) {
	    rpmtdInit(&td);
	    while (rpmtdNext(&td) >= 0) {
	    	char *str = rpmtdFormat(&td, RPMTD_FORMAT_STRING, NULL);
		if (str) {
		    rc = miregexec(mire, str);
		    if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
			anymatch++;
		    free(str);
		}
	    }
	    if ((i+1) < mi->mi_nre && mire[0].tag == mire[1].tag) {
		i++;
		mire++;
		continue;
	    }
	    break;
	}
	rpmtdFreeData(&td);

	ntags++;
	if (anymatch)
	    nmatches++;
    }

    return (ntags == nmatches ? 0 : 1);
}

int rpmdbSetIteratorRewrite(rpmdbMatchIterator mi, int rewrite)
{
    int rc;
    if (mi == NULL)
	return 0;
    rc = (mi->mi_cflags & DB_WRITECURSOR) ? 1 : 0;
    if (rewrite)
	mi->mi_cflags |= DB_WRITECURSOR;
    else
	mi->mi_cflags &= ~DB_WRITECURSOR;
    return rc;
}

int rpmdbSetIteratorModified(rpmdbMatchIterator mi, int modified)
{
    int rc;
    if (mi == NULL)
	return 0;
    rc = mi->mi_modified;
    mi->mi_modified = modified;
    return rc;
}

int rpmdbSetHdrChk(rpmdbMatchIterator mi, rpmts ts,
	rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, char ** msg))
{
    int rc = 0;
    if (mi == NULL)
	return 0;
    /* XXX forward linkage prevents rpmtsLink */
    mi->mi_ts = ts;
    mi->mi_hdrchk = hdrchk;
    return rc;
}


/* FIX: mi->mi_key.data may be NULL */
Header rpmdbNextIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi;
    void * uh;
    size_t uhlen;
    DBT * key;
    DBT * data;
    void * keyp;
    size_t keylen;
    int rc;
    int xx;

    if (mi == NULL)
	return NULL;

    dbi = dbiOpen(mi->mi_db, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)
	return NULL;

    /*
     * Cursors are per-iterator, not per-dbi, so get a cursor for the
     * iterator on 1st call. If the iteration is to rewrite headers, and the
     * CDB model is used for the database, then the cursor needs to
     * marked with DB_WRITECURSOR as well.
     */
    if (mi->mi_dbc == NULL)
	xx = dbiCopen(dbi, dbi->dbi_txnid, &mi->mi_dbc, mi->mi_cflags);

    key = &mi->mi_key;
    memset(key, 0, sizeof(*key));
    data = &mi->mi_data;
    memset(data, 0, sizeof(*data));

top:
    uh = NULL;
    uhlen = 0;

    do {
	union _dbswap mi_offset;

	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	    mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	    mi_offset.ui = mi->mi_offset;
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    keyp = &mi_offset;
	    keylen = sizeof(mi_offset.ui);
	} else {

	    key->data = keyp = (void *)mi->mi_keyp;
	    key->size = keylen = mi->mi_keylen;
	    data->data = uh;
	    data->size = uhlen;
#if !defined(_USE_COPY_LOAD)
	    data->flags |= DB_DBT_MALLOC;
#endif
	    rc = dbiGet(dbi, mi->mi_dbc, key, data,
			(key->data == NULL ? DB_NEXT : DB_SET));
	    data->flags = 0;
	    keyp = key->data;
	    keylen = key->size;
	    uh = data->data;
	    uhlen = data->size;

	    /*
	     * If we got the next key, save the header instance number.
	     *
	     * For db3 Packages, instance 0 (i.e. mi->mi_setx == 0) is the
	     * largest header instance in the database, and should be
	     * skipped.
	     */
	    if (keyp && mi->mi_setx && rc == 0) {
		memcpy(&mi_offset, keyp, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
    		_DBSWAP(mi_offset);
		mi->mi_offset = mi_offset.ui;
	    }

	    /* Terminate on error or end of keys */
	    if (rc || (mi->mi_setx && mi->mi_offset == 0))
		return NULL;
	}
	mi->mi_setx++;
    } while (mi->mi_offset == 0);

    /* If next header is identical, return it now. */
    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset) {
	/* ...but rpmdb record numbers are unique, avoid endless loop */
	return (mi->mi_rpmtag == RPMDBI_PACKAGES) ? NULL : mi->mi_h;
    }

    /* Retrieve next header blob for index iterator. */
    if (uh == NULL) {
	key->data = keyp;
	key->size = keylen;
#if !defined(_USE_COPY_LOAD)
	data->flags |= DB_DBT_MALLOC;
#endif
	rc = dbiGet(dbi, mi->mi_dbc, key, data, DB_SET);
	data->flags = 0;
	keyp = key->data;
	keylen = key->size;
	uh = data->data;
	uhlen = data->size;
	if (rc)
	    return NULL;
    }

    /* Rewrite current header (if necessary) and unlink. */
    xx = miFreeHeader(mi, dbi);

    /* Is this the end of the iteration? */
    if (uh == NULL)
	return NULL;

    /* Check header digest/signature once (if requested). */
    if (mi->mi_hdrchk && mi->mi_ts) {
	rpmRC rpmrc = RPMRC_NOTFOUND;

	/* Don't bother re-checking a previously read header. */
	if (mi->mi_db->db_bits) {
	    pbm_set * set;

	    set = PBM_REALLOC((pbm_set **)&mi->mi_db->db_bits,
			&mi->mi_db->db_nbits, mi->mi_offset);
	    if (PBM_ISSET(mi->mi_offset, set))
		rpmrc = RPMRC_OK;
	}

	/* If blob is unchecked, check blob import consistency now. */
	if (rpmrc != RPMRC_OK) {
	    char * msg = NULL;
	    int lvl;

	    rpmrc = (*mi->mi_hdrchk) (mi->mi_ts, uh, uhlen, &msg);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
	    rpmlog(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("rpmdbNextIterator: skipping") : " read"),
			mi->mi_offset, (msg ? msg : "\n"));
	    msg = _free(msg);

	    /* Mark header checked. */
	    if (mi->mi_db && mi->mi_db->db_bits && rpmrc == RPMRC_OK) {
		pbm_set * set;

		set = PBM_REALLOC((pbm_set **)&mi->mi_db->db_bits,
			&mi->mi_db->db_nbits, mi->mi_offset);
		PBM_SET(mi->mi_offset, set);
	    }

	    /* Skip damaged and inconsistent headers. */
	    if (rpmrc == RPMRC_FAIL)
		goto top;
	}
    }

    /* Did the header blob load correctly? */
#if !defined(_USE_COPY_LOAD)
    mi->mi_h = headerLoad(uh);
#else
    mi->mi_h = headerCopyLoad(uh);
#endif
    if (mi->mi_h == NULL || !headerIsEntry(mi->mi_h, RPMTAG_NAME)) {
	rpmlog(RPMLOG_ERR,
		_("rpmdb: damaged header #%u retrieved -- skipping.\n"),
		mi->mi_offset);
	goto top;
    }

    /*
     * Skip this header if iterator selector (if any) doesn't match.
     */
    if (mireSkip(mi)) {
	/* XXX hack, can't restart with Packages locked on single instance. */
	if (mi->mi_set || mi->mi_keyp == NULL)
	    goto top;
	return NULL;
    }
    headerSetInstance(mi->mi_h, mi->mi_offset);

    mi->mi_prevoffset = mi->mi_offset;
    mi->mi_modified = 0;

    return mi->mi_h;
}

/** \ingroup rpmdb
 * sort the iterator by (recnum, filenum)
 * Return database iterator.
 * @param mi		rpm database iterator
 */
void rpmdbSortIterator(rpmdbMatchIterator mi)
{
    if (mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0) {
    /*
     * mergesort is much (~10x with lots of identical basenames) faster
     * than pure quicksort, but glibc uses msort_with_tmp() on stack.
     */
#if defined(__GLIBC__)
	qsort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
#else
	mergesort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
#endif
	mi->mi_sorted = 1;
    }
}

static int rpmdbGrowIterator(rpmdbMatchIterator mi)
{
    DBC * dbcursor;
    DBT * key;
    DBT * data;
    dbiIndex dbi = NULL;
    dbiIndexSet set;
    int rc;
    int xx;

    if (mi == NULL)
	return 1;

    dbcursor = mi->mi_dbc;
    key = &mi->mi_key;
    data = &mi->mi_data;
    if (key->data == NULL)
	return 1;

    dbi = dbiOpen(mi->mi_db, mi->mi_rpmtag, 0);
    if (dbi == NULL)
	return 1;

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
#ifndef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (rc) {			/* error/not found */
	if (rc != DB_NOTFOUND)
	    rpmlog(RPMLOG_ERR,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, (char*)key->data, rpmTagGetName(dbi->dbi_rpmtag));
#ifdef	SQLITE_HACK
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
#endif
	return rc;
    }

    set = NULL;
    (void) dbt2set(dbi, data, &set);

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (mi->mi_set == NULL) {
	mi->mi_set = set;
    } else {
	dbiGrowSet(mi->mi_set, set->count);
	memcpy(mi->mi_set->recs + mi->mi_set->count, set->recs,
		set->count * sizeof(*(mi->mi_set->recs)));
	mi->mi_set->count += set->count;
	set = dbiFreeIndexSet(set);
    }

    return rc;
}

int rpmdbPruneIterator(rpmdbMatchIterator mi, int * hdrNums,
	int nHdrNums, int sorted)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set)
	(void) dbiPruneSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), sorted);
    return 0;
}

int rpmdbAppendIterator(rpmdbMatchIterator mi, const int * hdrNums, int nHdrNums)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set == NULL)
	mi->mi_set = xcalloc(1, sizeof(*mi->mi_set));
    (void) dbiAppendSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), 0);
    return 0;
}

rpmdbMatchIterator rpmdbInitIterator(rpmdb db, rpmTag rpmtag,
		const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi;
    DBT * key;
    DBT * data;
    dbiIndexSet set = NULL;
    dbiIndex dbi;
    void * mi_keyp = NULL;
    int isLabel = 0;

    if (db == NULL)
	return NULL;

    (void) rpmdbCheckSignals();

    /* XXX HACK to remove rpmdbFindByLabel/findMatches from the API */
    if (rpmtag == RPMDBI_LABEL) {
	rpmtag = RPMTAG_NAME;
	isLabel = 1;
    }

    dbi = dbiOpen(db, rpmtag, 0);
    if (dbi == NULL)
	return NULL;

    /* Chain cursors for teardown on abnormal exit. */
    mi = xcalloc(1, sizeof(*mi));
    mi->mi_next = rpmmiRock;
    rpmmiRock = mi;

    key = &mi->mi_key;
    data = &mi->mi_data;

    /*
     * Handle label and file name special cases.
     * Otherwise, retrieve join keys for secondary lookup.
     */
    if (rpmtag != RPMDBI_PACKAGES && keyp) {
	DBC * dbcursor = NULL;
	int rc;
	int xx;

	if (isLabel) {
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
	    rc = dbiFindByLabel(dbi, dbcursor, key, data, keyp, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	} else if (rpmtag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(db, keyp, key, data, &set);
	} else {
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

	    key->data = (void *) keyp;
	    key->size = keylen;
	    if (key->data && key->size == 0)
	        key->size = strlen((char *)key->data);
	    if (key->data && key->size == 0)
	        key->size++;	/* XXX "/" fixup. */

	    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	    if (rc > 0) {
		rpmlog(RPMLOG_ERR,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, (key->data ? (char *)key->data : "???"), 
			rpmTagGetName(dbi->dbi_rpmtag));
	    }

	    /* Join keys need to be native endian internally. */
	    if (rc == 0)
		(void) dbt2set(dbi, data, &set);

	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	}
	if (rc)	{	/* error/not found */
	    set = dbiFreeIndexSet(set);
	    rpmmiRock = mi->mi_next;
	    mi->mi_next = NULL;
	    mi = _free(mi);
	    return NULL;
	}
    }

    /* Copy the retrieval key, byte swapping header instance if necessary. */
    if (keyp) {
	switch (rpmtag) {
	case RPMDBI_PACKAGES:
	  { union _dbswap *k;

	    assert(keylen == sizeof(k->ui));	/* xxx programmer error */
	    k = xmalloc(sizeof(*k));
	    memcpy(k, keyp, keylen);
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(*k);
	    mi_keyp = k;
	  } break;
	default:
	  { char * k;
	    if (keylen == 0)
		keylen = strlen(keyp);
	    k = xmalloc(keylen + 1);
	    memcpy(k, keyp, keylen);
	    k[keylen] = '\0';	/* XXX assumes strings */
	    mi_keyp = k;
	  } break;
	}
    }

    mi->mi_keyp = mi_keyp;
    mi->mi_keylen = keylen;

    mi->mi_db = rpmdbLink(db, RPMDBG_M("matchIterator"));
    mi->mi_rpmtag = rpmtag;

    mi->mi_dbc = NULL;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_h = NULL;
    mi->mi_sorted = 0;
    mi->mi_cflags = 0;
    mi->mi_modified = 0;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_filenum = 0;
    mi->mi_nre = 0;
    mi->mi_re = NULL;

    mi->mi_ts = NULL;
    mi->mi_hdrchk = NULL;

    return mi;
}

/** \ingroup rpmdb
 * Return database iterator.
 * @param mi		rpm database iterator
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		0 on success
 */
int rpmdbExtendIterator(rpmdbMatchIterator mi,
			const void * keyp, size_t keylen)
{
    mi->mi_key.data = (void *) keyp;
    mi->mi_key.size = keylen ? keylen : strlen(keyp);
    return rpmdbGrowIterator(mi);
}

/*
 * Convert current tag data to db key
 * @param tagdata	Tag data container
 * @retval key		DB key struct
 * @retval freedata	Should key.data be freed afterwards
 * Return 0 to signal this item should be discarded (ie continue)
 */
static int td2key(rpmtd tagdata, DBT *key, int *freedata) 
{
    const char *str = NULL;
    uint8_t *bin = NULL;

    *freedata = 0; 
    switch (rpmtdType(tagdata)) {
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	key->size = sizeof(uint8_t);
	key->data = rpmtdGetChar(tagdata);
	break;
    case RPM_INT16_TYPE:
	key->size = sizeof(uint16_t);
	key->data = rpmtdGetUint16(tagdata);
	break;
    case RPM_INT32_TYPE:
	key->size = sizeof(uint32_t);
	key->data = rpmtdGetUint32(tagdata);
	break;
    case RPM_INT64_TYPE:
	key->size = sizeof(uint64_t);
	key->data = rpmtdGetUint64(tagdata);
	break;
    case RPM_BIN_TYPE:
	key->size = tagdata->count;
	key->data = tagdata->data;
	break;
    case RPM_STRING_TYPE:
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	str = rpmtdGetString(tagdata);
	if (rpmtdTag(tagdata) == RPMTAG_FILEDIGESTS) {
	    size_t binlen;
	    uint8_t *t;
	
	    /* Filter out empty MD5 strings. */
	    if (!(str && *str != '\0'))
		return 0;

	    binlen = strlen(str) / 2;
	    bin = xmalloc(binlen);
	    /* Convert from hex to binary. */
	    t = bin;
	    for (int j = 0; j < binlen; j++, t++, str += 2) 
		*t = (rnibble(str[0]) << 4) | rnibble(str[1]);
	    key->data = bin;
	    key->size = binlen;
	    *freedata = 1;
	    break;
	} else if (rpmtdTag(tagdata) == RPMTAG_PUBKEYS) {
	    /* Extract the pubkey id from the base64 blob. */
	    bin = xmalloc(sizeof(pgpKeyID_t));
	    int nbin = pgpExtractPubkeyFingerprint(str, bin);
	    if (nbin <= 0) {
		free(bin);
		return 0;
	    }
	    key->data = bin;
	    key->size = nbin;
	    *freedata = 1;
	    break;
	} else {
	    /* fallthrough */;
	}
    default:
	str = rpmtdGetString(tagdata);
	key->data = (char *) str; /* XXX discards const */
	key->size = strlen(str);
	break;
    }

    if (key->size == 0) 
	key->size = strlen((char *)key->data);
    if (key->size == 0) 
	key->size++;	/* XXX "/" fixup. */

    return 1;
}

static void logAddRemove(int removing, rpmtd tagdata)
{
    rpm_count_t c = rpmtdCount(tagdata);
    if (c == 1 && rpmtdType(tagdata) == RPM_STRING_TYPE) {
	rpmlog(RPMLOG_DEBUG, "%s \"%s\" %s %s index.\n",
		removing ? "removing" : "adding", rpmtdGetString(tagdata), 
		removing ? "from" : "to", 
		rpmTagGetName(rpmtdTag(tagdata)));
    } else if (c > 0) {
	rpmlog(RPMLOG_DEBUG, "%s %d entries %s %s index.\n",
		removing ? "removing" : "adding", c, 
		removing ? "from" : "to", 
		rpmTagGetName(rpmtdTag(tagdata)));
    }
}

/* XXX psm.c */
int rpmdbRemove(rpmdb db, int rid, unsigned int hdrNum,
		rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, char ** msg))
{
    DBC * dbcursor = NULL;
    DBT key;
    DBT data;
    union _dbswap mi_offset;
    Header h;
    sigset_t signalMask;
    int ret = 0;
    int rc = 0;

    if (db == NULL)
	return 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
	h = rpmdbNextIterator(mi);
	if (h)
	    h = headerLink(h);
	mi = rpmdbFreeIterator(mi);
    }

    if (h == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: cannot read header at 0x%x\n"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

    {	
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	rpmlog(RPMLOG_DEBUG, "  --- h#%8u %s\n", hdrNum, nevra);
	free(nevra);
    }

    (void) blockSignals(&signalMask);

	/* FIX: rpmvals heartburn */
    {	int dbix;
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	if (dbiTags.tags != NULL)
	for (dbix = 0; dbix < dbiTags.max; dbix++) {
	    dbiIndex dbi;
	    rpmTag rpmtag;
	    int xx = 0;
	    struct rpmtd_s tagdata;

	    dbi = NULL;
	    rpmtag = dbiTags.tags[dbix];

	    /* Filter out temporary databases */
	    if (isTemporaryDB(rpmtag)) 
		continue;

	    if (rpmtag == RPMDBI_PACKAGES) {
		dbi = dbiOpen(db, rpmtag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
	      
		mi_offset.ui = hdrNum;
		if (dbiByteSwapped(dbi) == 1)
    		    _DBSWAP(mi_offset);
		key.data = &mi_offset;
		key.size = sizeof(mi_offset.ui);

		rc = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
		rc = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
		if (rc) {
		    rpmlog(RPMLOG_ERR,
			_("error(%d) setting header #%d record for %s removal\n"),
			rc, hdrNum, rpmTagGetName(dbi->dbi_rpmtag));
		} else
		    rc = dbiDel(dbi, dbcursor, &key, &data, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
	    }
	
	    if (!headerGet(h, rpmtag, &tagdata, HEADERGET_MINMEM))
		continue;

	    if (!(dbi = dbiOpen(db, rpmtag, 0))) {
		rpmtdFreeData(&tagdata);
		continue;
	    }
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	    logAddRemove(1, &tagdata);
	    while (rpmtdNext(&tagdata) >= 0) {
		dbiIndexSet set;
		int freedata = 0;

		if (!td2key(&tagdata, &key, &freedata)) {
		    continue;
		}

		/* XXX
		 * This is almost right, but, if there are duplicate tag
		 * values, there will be duplicate attempts to remove
		 * the header instance. It's faster to just ignore errors
		 * than to do things correctly.
		 */

		/* 
 		 * XXX with duplicates, an accurate data value and 
 		 * DB_GET_BOTH is needed. 
 		 * */
		set = NULL;

		rc = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
		if (rc == 0) {			/* success */
		    (void) dbt2set(dbi, &data, &set);
		} else if (rc == DB_NOTFOUND) {	/* not found */
		    goto cont;
		} else {			/* error */
		    rpmlog(RPMLOG_ERR,
			_("error(%d) setting \"%s\" records from %s index\n"),
			rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		    goto cont;
		}

		rc = dbiPruneSet(set, rec, 1, sizeof(*rec), 1);

		/* If nothing was pruned, then don't bother updating. */
		if (rc) {
		    set = dbiFreeIndexSet(set);
		    goto cont;
		}

		if (set->count > 0) {
		    (void) set2dbt(dbi, &data, set);
		    rc = dbiPut(dbi, dbcursor, &key, &data, DB_KEYLAST);
		    if (rc) {
			rpmlog(RPMLOG_ERR,
				_("error(%d) storing record \"%s\" into %s\n"),
				rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		    data.data = _free(data.data);
		    data.size = 0;
		} else {
		    rc = dbiDel(dbi, dbcursor, &key, &data, 0);
		    if (rc) {
			rpmlog(RPMLOG_ERR,
				_("error(%d) removing record \"%s\" from %s\n"),
				rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		}
		set = dbiFreeIndexSet(set);
cont:
		if (freedata) {
		   free(key.data); 
		}
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);

	    rpmtdFreeData(&tagdata);
	}

	rec = _free(rec);
    }

    (void) unblockSignals(&signalMask);

    h = headerFree(h);

    /* XXX return ret; */
    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb db, int iid, Header h,
	     rpmts ts,
	     rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, char ** msg))
{
    DBC * dbcursor = NULL;
    DBT key;
    DBT data;
    sigset_t signalMask;
    dbiIndex dbi;
    int dbix;
    union _dbswap mi_offset;
    unsigned int hdrNum = 0;
    int ret = 0;
    int rc;
    int xx;

    if (db == NULL)
	return 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

#ifdef	NOTYET	/* XXX headerDel() broken on dribbles. */
    xx = headerDel(h, RPMTAG_REMOVETID);
#endif
    if (iid != 0 && iid != -1) {
	rpm_tid_t tid = iid;
	if (!headerIsEntry(h, RPMTAG_INSTALLTID)) 
	    headerPutUint32(h, RPMTAG_INSTALLTID, &tid, 1);
    }

    (void) blockSignals(&signalMask);

    {
	unsigned int firstkey = 0;
	void * keyp = &firstkey;
	size_t keylen = sizeof(firstkey);
	void * datap = NULL;
	size_t datalen = 0;

      dbi = dbiOpen(db, RPMDBI_PACKAGES, 0);
      if (dbi != NULL) {

	/* XXX db0: hack to pass sizeof header to fadAlloc */
	datap = h;
	datalen = headerSizeof(h, HEADER_MAGIC_NO);

	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	/* Retrieve join key for next header instance. */

	key.data = keyp;
	key.size = keylen;
	data.data = datap;
	data.size = datalen;
	ret = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
	keyp = key.data;
	keylen = key.size;
	datap = data.data;
	datalen = data.size;

	hdrNum = 0;
	if (ret == 0 && datap) {
	    memcpy(&mi_offset, datap, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    hdrNum = mi_offset.ui;
	}
	++hdrNum;
	mi_offset.ui = hdrNum;
	if (dbiByteSwapped(dbi) == 1)
	    _DBSWAP(mi_offset);
	if (ret == 0 && datap) {
	    memcpy(datap, &mi_offset, sizeof(mi_offset.ui));
	} else {
	    datap = &mi_offset;
	    datalen = sizeof(mi_offset.ui);
	}

	key.data = keyp;
	key.size = keylen;
	data.data = datap;
	data.size = datalen;

	ret = dbiPut(dbi, dbcursor, &key, &data, DB_KEYLAST);
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	dbcursor = NULL;
      }

    }

    if (ret) {
	rpmlog(RPMLOG_ERR,
		_("error(%d) allocating new package instance\n"), ret);
	goto exit;
    }

    /* Now update the indexes */

    if (hdrNum)
    {	
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	if (dbiTags.tags != NULL)
	for (dbix = 0; dbix < dbiTags.max; dbix++) {
	    rpmTag rpmtag;
	    rpmRC rpmrc;
	    int j;
	    struct rpmtd_s tagdata, reqflags;

	    rpmrc = RPMRC_NOTFOUND;
	    dbi = NULL;
	    rpmtag = dbiTags.tags[dbix];

	    /* Filter out temporary databases */
	    if (isTemporaryDB(rpmtag)) 
		continue;

	    switch (rpmtag) {
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(db, rpmtag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

		mi_offset.ui = hdrNum;
		if (dbiByteSwapped(dbi) == 1)
    		    _DBSWAP(mi_offset);
		key.data = (void *) &mi_offset;
		key.size = sizeof(mi_offset.ui);
		data.data = headerUnload(h);
		data.size = headerSizeof(h, HEADER_MAGIC_NO);

		/* Check header digest/signature on blob export. */
		if (hdrchk && ts) {
		    char * msg = NULL;
		    int lvl;

		    rpmrc = (*hdrchk) (ts, data.data, data.size, &msg);
		    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
		    rpmlog(lvl, "%s h#%8u %s",
			(rpmrc == RPMRC_FAIL ? _("rpmdbAdd: skipping") : "  +++"),
				hdrNum, (msg ? msg : "\n"));
		    msg = _free(msg);
		}

		if (data.data != NULL && rpmrc != RPMRC_FAIL) {
		    xx = dbiPut(dbi, dbcursor, &key, &data, DB_KEYLAST);
		    xx = dbiSync(dbi, 0);
		}
		data.data = _free(data.data);
		data.size = 0;
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		break;
	    case RPMTAG_REQUIRENAME:
		headerGet(h, rpmtag, &tagdata, HEADERGET_MINMEM);
		headerGet(h, RPMTAG_REQUIREFLAGS, &reqflags, HEADERGET_MINMEM);
		break;
	    default:
		headerGet(h, rpmtag, &tagdata, HEADERGET_MINMEM);
		break;
	    }

	    if (rpmtdCount(&tagdata) == 0) {
		if (rpmtag != RPMTAG_GROUP)
		    continue;

		/* XXX preserve legacy behavior */
		tagdata.type = RPM_STRING_TYPE;
		tagdata.data = (const char **) "Unknown";
		tagdata.count = 1;
	    }

	    if (!(dbi = dbiOpen(db, rpmtag, 0))) {
		rpmtdFreeData(&tagdata);
		continue;
	    }
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	    logAddRemove(0, &tagdata);
	    while (rpmtdNext(&tagdata) >= 0) {
		dbiIndexSet set;
		int i, freedata = 0;

		/*
		 * Include the tagNum in all indices. rpm-3.0.4 and earlier
		 * included the tagNum only for files.
		 */
		i = rec->tagNum = rpmtdGetIndex(&tagdata);
		switch (rpmtag) {
		case RPMTAG_REQUIRENAME: {
		    /* Filter out install prerequisites. */
		    rpm_flag_t *rflag = rpmtdNextUint32(&reqflags);
		    if (rflag && isInstallPreReq(*rflag) &&
				 !isErasePreReq(*rflag))
			continue;
		    break;
		    }
		case RPMTAG_TRIGGERNAME:
		    if (i > 0) {	/* don't add duplicates */
			const char **tnames = tagdata.data;
			const char *str = rpmtdGetString(&tagdata);
			for (j = 0; j < i; j++) {
			    if (rstreq(str, tnames[j]))
				break;
			}
			if (j < i)
			    continue;
		    }
		    break;
		default:
		    break;
		}

		if (!td2key(&tagdata, &key, &freedata)) {
		    continue;
		}

		/* 
 		 * XXX with duplicates, an accurate data value and 
 		 * DB_GET_BOTH is needed. 
 		 */

		set = NULL;

		rc = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
		if (rc == 0) {			/* success */
		/* With duplicates, cursor is positioned, discard the record. */
		    if (!dbi->dbi_permit_dups)
			(void) dbt2set(dbi, &data, &set);
		} else if (rc != DB_NOTFOUND) {	/* error */
		    rpmlog(RPMLOG_ERR,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		    goto cont;
		}

		if (set == NULL)		/* not found or duplicate */
		    set = xcalloc(1, sizeof(*set));

		(void) dbiAppendSet(set, rec, 1, sizeof(*rec), 0);

		(void) set2dbt(dbi, &data, set);
		rc = dbiPut(dbi, dbcursor, &key, &data, DB_KEYLAST);

		if (rc) {
		    rpmlog(RPMLOG_ERR,
				_("error(%d) storing record %s into %s\n"),
				rc, (char*)key.data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		}
		data.data = _free(data.data);
		data.size = 0;
		set = dbiFreeIndexSet(set);
cont:
		if (freedata) {
		    free(key.data);
		}
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);

	    rpmtdFreeData(&tagdata);
	}

	rec = _free(rec);
	/* If everthing ok, mark header as installed now */
	if (ret == 0) {
	    headerSetInstance(h, hdrNum);
	}
    }

exit:
    (void) unblockSignals(&signalMask);

    return ret;
}

/*
 * Remove DB4 environment (and lock), ie the equivalent of 
 * rm -f <prefix>/<dbpath>/__db.???
 * Environment files not existing is not an error, failure to unlink is,
 * return zero on success.
 * Only useful for BDB, dbapi 3 and 4.  
 * TODO/FIX: push this down to db3.c where it belongs
 */
static int cleanDbenv(const char *prefix, const char *dbpath)
{
    ARGV_t paths = NULL, p;
    int rc = 0; 
    char *pattern = rpmGetPath(prefix, "/", dbpath, "/__db.???", NULL);

    if (rpmGlob(pattern, NULL, &paths) == 0) {
	for (p = paths; *p; p++) {
	    rc += unlink(*p);
	}
	argvFree(paths);
    }
    free(pattern);
    return rc;
}

static int rpmdbRemoveDatabase(const char * prefix,
		const char * dbpath, int _dbapi)
{ 
    int i;
    char *path;
    int xx;

    dbiTagsInit();
    switch (_dbapi) {
    case 4:
    case 3:
	if (dbiTags.tags != NULL)
	for (i = 0; i < dbiTags.max; i++) {
	    const char * base = rpmTagGetName(dbiTags.tags[i]);
	    path = rpmGetPath(prefix, "/", dbpath, "/", base, NULL);
	    if (access(path, F_OK) == 0)
	    	xx = unlink(path);
	    free(path);
	}
	cleanDbenv(prefix, dbpath);
	break;
    case 2:
    case 1:
    case 0:
	break;
    }
    dbiTagsFree();

    path = rpmGetPath(prefix, "/", dbpath, NULL);
    xx = rmdir(path);
    free(path);

    return 0;
}

static int rpmdbMoveDatabase(const char * prefix,
		const char * olddbpath, int _olddbapi,
		const char * newdbpath, int _newdbapi)
{
    int i;
    struct stat st;
    int rc = 0;
    int xx;
    int selinux = is_selinux_enabled() && (matchpathcon_init(NULL) != -1);
    sigset_t sigMask;

    dbiTagsInit();
    blockSignals(&sigMask);
    switch (_olddbapi) {
    case 4:
        /* Fall through */
    case 3:
	if (dbiTags.tags != NULL)
	for (i = 0; i < dbiTags.max; i++) {
	    const char * base;
	    char *src, *dest;
	    rpmTag rpmtag;

	    /* Filter out temporary databases */
	    if (isTemporaryDB((rpmtag = dbiTags.tags[i])))
		continue;

	    base = rpmTagGetName(rpmtag);
	    src = rpmGetPath(prefix, "/", olddbpath, "/", base, NULL);
	    dest = rpmGetPath(prefix, "/", newdbpath, "/", base, NULL);

	    if (access(src, F_OK) != 0)
		goto cont;

	    /*
	     * Restore uid/gid/mode/mtime/security context if possible.
	     */
	    if (stat(dest, &st) < 0)
		if (stat(src, &st) < 0)
		    goto cont;

	    if ((xx = rename(src, dest)) != 0) {
		rc = 1;
		goto cont;
	    }
	    xx = chown(dest, st.st_uid, st.st_gid);
	    xx = chmod(dest, (st.st_mode & 07777));
	    {	struct utimbuf stamp;
		stamp.actime = st.st_atime;
		stamp.modtime = st.st_mtime;
		xx = utime(dest, &stamp);
	    }

	    if (selinux) {
		security_context_t scon = NULL;
		if (matchpathcon(dest, st.st_mode, &scon) != -1) {
		    (void) setfilecon(dest, scon);
		    freecon(scon);
		}
	    }
		
cont:
	    free(src);
	    free(dest);
	}

	cleanDbenv(prefix, olddbpath);
	cleanDbenv(prefix, newdbpath);
	break;
    case 2:
    case 1:
    case 0:
	break;
    }
    unblockSignals(&sigMask);

#ifdef	SQLITE_HACK_XXX
    if (rc || _olddbapi == _newdbapi)
	return rc;

    rc = rpmdbRemoveDatabase(prefix, newdbpath, _newdbapi);

#endif
    if (selinux) {
	(void) matchpathcon_fini();
    }
    dbiTagsFree();
    return rc;
}

int rpmdbRebuild(const char * prefix, rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, char ** msg))
{
    rpmdb olddb;
    char * dbpath = NULL;
    char * rootdbpath = NULL;
    rpmdb newdb;
    char * newdbpath = NULL;
    char * newrootdbpath = NULL;
    char * tfn;
    int nocleanup = 1;
    int failed = 0;
    int removedir = 0;
    int rc = 0, xx;
    int _dbapi;
    int _dbapi_rebuild;

    if (prefix == NULL) prefix = "/";

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    tfn = rpmGetPath("%{?_dbpath}", NULL);
    if (!(tfn && tfn[0] != '\0'))
    {
	rpmlog(RPMLOG_ERR, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	dbpath += strlen(prefix) - 1;
    tfn = _free(tfn);

    tfn = rpmGetPath("%{?_dbpath_rebuild}", NULL);
    if (!(tfn && tfn[0] != '\0' && !rstreq(tfn, dbpath)))
    {
	tfn = _free(tfn);
	rasprintf(&tfn, "%srebuilddb.%d", dbpath, (int) getpid());
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	newdbpath += strlen(prefix) - 1;
    tfn = _free(tfn);

    rpmlog(RPMLOG_DEBUG, "rebuilding database %s into %s\n",
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmlog(RPMLOG_ERR, _("temporary database %s already exists\n"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmlog(RPMLOG_DEBUG, "creating directory %s\n", newrootdbpath);
    if (mkdir(newrootdbpath, 0755)) {
	rpmlog(RPMLOG_ERR, _("failed to create directory %s: %s\n"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    _rebuildinprogress = 0;

    rpmlog(RPMLOG_DEBUG, "opening old database with dbapi %d\n",
		_dbapi);
    if (openDatabase(prefix, dbpath, _dbapi, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
    _dbapi = olddb->db_api;
    _rebuildinprogress = 1;
    rpmlog(RPMLOG_DEBUG, "opening new database with dbapi %d\n",
		_dbapi_rebuild);
    (void) rpmDefineMacro(NULL, "_rpmdb_rebuild %{nil}", -1);
    if (openDatabase(prefix, newdbpath, _dbapi_rebuild, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }

    _rebuildinprogress = 0;

    _dbapi_rebuild = newdb->db_api;
    
    {	Header h = NULL;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	mi = rpmdbInitIterator(olddb, RPMDBI_PACKAGES, NULL, 0);
	if (ts && hdrchk)
	    (void) rpmdbSetHdrChk(mi, ts, hdrchk);

	while ((h = rpmdbNextIterator(mi)) != NULL) {

	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (!(headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
	    {
		rpmlog(RPMLOG_ERR,
			_("header #%u in the database is bad -- skipping.\n"),
			_RECNUM);
		continue;
	    }

	    /* Deleted entries are eliminated in legacy headers by copy. */
	    {	Header nh = (headerIsEntry(h, RPMTAG_HEADERIMAGE)
				? headerCopy(h) : NULL);
		rc = rpmdbAdd(newdb, -1, (nh ? nh : h), ts, hdrchk);
		nh = headerFree(nh);
	    }

	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("cannot add record originally at %u\n"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	mi = rpmdbFreeIterator(mi);

    }

    xx = rpmdbClose(olddb);
    xx = rpmdbClose(newdb);

    if (failed) {
	rpmlog(RPMLOG_WARNING, 
		_("failed to rebuild database: original database "
		"remains in place\n"));

	xx = rpmdbRemoveDatabase(prefix, newdbpath, _dbapi_rebuild);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(prefix, newdbpath, _dbapi_rebuild, dbpath, _dbapi)) {
	    rpmlog(RPMLOG_ERR, _("failed to replace old database with new "
			"database!\n"));
	    rpmlog(RPMLOG_ERR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (removedir && !(rc == 0 && nocleanup)) {
	rpmlog(RPMLOG_DEBUG, "removing directory %s\n", newrootdbpath);
	if (rmdir(newrootdbpath))
	    rpmlog(RPMLOG_ERR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    newrootdbpath = _free(newrootdbpath);
    rootdbpath = _free(rootdbpath);

    return rc;
}
