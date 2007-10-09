/** \ingroup rpmdb dbi
 * \file rpmdb/rpmdb.c
 */

#include "system.h"

#define	_USE_COPY_LOAD	/* XXX don't use DB_DBT_MALLOC (yet) */

#include <sys/file.h>

#ifndef	DYING	/* XXX already in "system.h" */
#include <fnmatch.h>
#endif

#include <regex.h>

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmsq.h>

#include "rpmdb_internal.h"
#include "rpmdb.h"
#include "fprint.h"
#include "legacy.h"
#include "header_internal.h"	/* XXX for HEADERFLAG_ALLOCATED */
#include "debug.h"

int _rpmdb_debug = 0;

static int _rebuildinprogress = 0;
static int _db_filter_dups = 0;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

int * dbiTags = NULL;
int dbiTagsMax = 0;

/* We use this to comunicate back to the the rpm transaction
 *  what their install instance was on a rpmdbAdd().
 */ 
unsigned int myinstall_instance = 0;

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
 * Convert hex to binary nibble.
 * @param c		hex character
 * @return		binary nibble
 */
static inline unsigned char nibble(char c)
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

#ifdef	DYING
/**
 * Check key for printable characters.
 * @param ptr		key value pointer
 * @param len		key value length
 * @return		1 if only ASCII, 0 otherwise.
 */
static int printable(const void * ptr, size_t len)	
{
    const char * s = ptr;
    int i;
    for (i = 0; i < len; i++, s++)
	if (!(*s >= ' ' && *s <= '~')) return 0;
    return 1;
}
#endif

/**
 * Return dbi index used for rpm tag.
 * @param rpmtag	rpm header tag
 * @return		dbi index, -1 on error
 */
static int dbiTagToDbix(int rpmtag)
{
    int dbix;

    if (dbiTags != NULL)
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
    static const char * const _dbiTagStr_default =
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Sigmd5:Sha1header:Filemd5s:Depends:Pubkeys";
    char * dbiTagStr = NULL;
    char * o, * oe;
    int rpmtag;

    dbiTagStr = rpmExpand("%{?_dbi_tags}", NULL);
    if (!(dbiTagStr && *dbiTagStr)) {
	dbiTagStr = _free(dbiTagStr);
	dbiTagStr = xstrdup(_dbiTagStr_default);
    }

    /* Discard previous values. */
    dbiTags = _free(dbiTags);
    dbiTagsMax = 0;

    /* Always allocate package index */
    dbiTags = xcalloc(1, sizeof(*dbiTags));
    dbiTags[dbiTagsMax++] = RPMDBI_PACKAGES;

    for (o = dbiTagStr; o && *o; o = oe) {
	while (*o && xisspace(*o))
	    o++;
	if (*o == '\0')
	    break;
	for (oe = o; oe && *oe; oe++) {
	    if (xisspace(*oe))
		break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	rpmtag = rpmTagGetValue(o);
	if (rpmtag < 0) {
	    rpmlog(RPMMESS_WARNING,
		_("dbiTagsInit: unrecognized tag name: \"%s\" ignored\n"), o);
	    continue;
	}
	if (dbiTagToDbix(rpmtag) >= 0)
	    continue;

	dbiTags = xrealloc(dbiTags, (dbiTagsMax + 1) * sizeof(*dbiTags)); /* XXX memory leak */
	dbiTags[dbiTagsMax++] = rpmtag;
    }

    dbiTagStr = _free(dbiTagStr);
}

#define	DB1vec		NULL
#define	DB2vec		NULL

#ifdef HAVE_DB3_DB_H
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

static struct _dbiVec *mydbvecs[] = {
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
    if (dbix < 0 || dbix >= dbiTagsMax)
	return NULL;

    /* Is this index already open ? */
/* FIX: db->_dbi may be NULL */
    if ((dbi = db->_dbi[dbix]) != NULL)
	return dbi;

    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 4)
	_dbapi_rebuild = 4;
/*    _dbapi_wanted = (_rebuildinprogress ? -1 : db->db_api); */
    _dbapi_wanted = (_rebuildinprogress ? _dbapi_rebuild : db->db_api);

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 5 || mydbvecs[_dbapi] == NULL) {
            rpmlog(RPMMESS_DEBUG, "dbiOpen: _dbiapi failed\n");
	    return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	if (rc) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmlog(RPMERR_DBOPEN,
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
		rpmlog(RPMERR_DBOPEN, _("cannot open %s index\n"),
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
#ifdef HAVE_DB3_DB_H
      else
	dbi = db3Free(dbi);
#endif

/* FIX: db->_dbi may be NULL */
    return dbi;
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
    int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	return -1;

    if ((sdbir = data->data) == NULL) {
	*setp = NULL;
	return 0;
    }

    set = xmalloc(sizeof(*set));
    set->count = data->size / dbi->dbi_jlen;
    set->recs = xmalloc(set->count * sizeof(*(set->recs)));

    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(int_32):
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
	}
	break;
    case 1*sizeof(int_32):
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
    int i;

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
    case 2*sizeof(int_32):
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
    case 1*sizeof(int_32):
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
static int dbiAppendSet(dbiIndexSet set, const void * recs,
	int nrecs, size_t recsize, int sortset)
{
    const char * rptr = recs;
    size_t rlen = (recsize < sizeof(*(set->recs)))
		? recsize : sizeof(*(set->recs));

    if (set == NULL || recs == NULL || nrecs <= 0 || recsize == 0)
	return 1;

    set->recs = xrealloc(set->recs,
			(set->count + nrecs) * sizeof(*(set->recs)));

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
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;

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
    const char *	pattern;	/*!< pattern string */
    int			notmatch;	/*!< like "grep -v" */
    regex_t *		preg;		/*!< regex compiled pattern buffer */
    int			cflags;		/*!< regcomp(3) flags */
    int			eflags;		/*!< regexec(3) flags */
    int			fnflags;	/*!< fnmatch(3) flags */
} * miRE;

struct _rpmdbMatchIterator {
    rpmdbMatchIterator	mi_next;
    const void *	mi_keyp;
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
    rpmRC (*mi_hdrchk) (rpmts ts, const void * uh, size_t uc, const char ** msg);

};

static rpmdb rpmdbRock;

static rpmdbMatchIterator rpmmiRock;

int rpmdbCheckTerminate(int terminate)
{
    sigset_t newMask, oldMask;
    static int terminating = 0;

    if (terminating) return 1;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);

    if (sigismember(&rpmsqCaught, SIGINT)
     || sigismember(&rpmsqCaught, SIGQUIT)
     || sigismember(&rpmsqCaught, SIGHUP)
     || sigismember(&rpmsqCaught, SIGTERM)
     || sigismember(&rpmsqCaught, SIGPIPE)
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
/* sigset_t is abstract type */
	rpmlog(RPMMESS_DEBUG, "Exiting on signal(0x%lx) ...\n", *((unsigned long *)&rpmsqCaught));
	exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Block all signals, returning previous signal mask.
 */
static int blockSignals(rpmdb db, sigset_t * oldMask)
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
static int unblockSignals(rpmdb db, sigset_t * oldMask)
{
    (void) rpmdbCheckSignals();
    return sigprocmask(SIG_SETMASK, oldMask, NULL);
}

#define	_DB_ROOT	"/"
#define	_DB_HOME	"%{_dbpath}"
#define	_DB_FLAGS	0
#define _DB_MODE	0
#define _DB_PERMS	0644

#define _DB_MAJOR	-1
#define	_DB_ERRPFX	"rpmdb"

static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS, _DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_ERRPFX
};

static int isTemporaryDB(int rpmtag) 
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

    if (dbiTags != NULL)
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (db->_dbi[dbix] != NULL)
	    continue;
	/* Filter out temporary databases */
	if (isTemporaryDB(dbiTags[dbix])) 
	    continue;
	(void) dbiOpen(db, dbiTags[dbix], db->db_flags);
    }
    return rc;
}

int rpmdbCloseDBI(rpmdb db, int rpmtag)
{
    int dbix;
    int rc = 0;

    if (db == NULL || db->_dbi == NULL || dbiTags == NULL)
	return 0;

    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (dbiTags[dbix] != rpmtag)
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

    (void) rpmdbUnlink(db, "rpmdbClose");

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
	_db_filter_dups = rpmExpandNumeric("%{_filterdbdups}");
	_initialized = 1;
    }

    *db = dbTemplate;	/* structure assignment */

    db->_dbi = NULL;

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (mode >= 0)	db->db_mode = mode;
    if (perms >= 0)	db->db_perms = perms;
    if (flags >= 0)	db->db_flags = flags;

    /* HACK: no URL's for root prefixed dbpath yet. */
    if (root && *root) {
	const char * rootpath = NULL;
	urltype ut = urlPath(root, &rootpath);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    db->db_root = rpmGetPath(root, NULL);
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	case URL_IS_HKP:
	case URL_IS_DASH:
	default:
	    db->db_root = rpmGetPath(_DB_ROOT, NULL);
	    break;
	}
    } else
	db->db_root = rpmGetPath(_DB_ROOT, NULL);
    db->db_home = rpmGetPath( (home && *home ? home : _DB_HOME), NULL);
    if (!(db->db_home && db->db_home[0] != '%')) {
	rpmlog(RPMERR_DBOPEN, _("no dbpath has been set\n"));
	db->db_root = _free(db->db_root);
	db->db_home = _free(db->db_home);
	db = _free(db);
	return NULL;
    }
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);
    db->db_remove_env = 0;
    db->db_filter_dups = _db_filter_dups;
    db->db_ndbi = dbiTagsMax;
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));
    db->nrefs = 0;
    return rpmdbLink(db, "rpmdbCreate");
}

static int openDatabase(const char * prefix,
		const char * dbpath,
		int _dbapi, rpmdb *dbp,
		int mode, int perms, int flags)
{
    rpmdb db;
    int rc, xx;
    static int _tags_initialized = 0;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    if (!_tags_initialized || dbiTagsMax == 0) {
	dbiTagsInit();
	_tags_initialized++;
    }

    /* Insure that _dbapi has one of -1, 1, 2, or 3 */
    if (_dbapi < -1 || _dbapi > 4)
	_dbapi = -1;
    if (_dbapi == 0)
	_dbapi = 1;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
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
	if (dbiTags != NULL)
	for (dbix = 0; rc == 0 && dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    int rpmtag;

	    /* Filter out temporary databases */
	    if (isTemporaryDB((rpmtag = dbiTags[dbix])))
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

rpmdb XrpmdbUnlink(rpmdb db, const char * msg, const char * fn, unsigned ln)
{
if (_rpmdb_debug)
fprintf(stderr, "--> db %p -- %d %s at %s:%u\n", db, db->nrefs, msg, fn, ln);
    db->nrefs--;
    return NULL;
}

rpmdb XrpmdbLink(rpmdb db, const char * msg, const char * fn, unsigned ln)
{
    db->nrefs++;
if (_rpmdb_debug)
fprintf(stderr, "--> db %p ++ %d %s at %s:%u\n", db, db->nrefs, msg, fn, ln);
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
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char * dirName;
    const char * baseName;
    rpmTagType bnt, dnt;
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
    if (filespec == NULL) return -2;

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
    if (baseName == NULL)
	return -2;

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    dbi = dbiOpen(db, RPMTAG_BASENAMES, 0);
    if (dbi != NULL) {
	dbcursor = NULL;
	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

key->data = (void *) baseName;
key->size = strlen(baseName);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	if (rc > 0) {
	    rpmlog(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
	}

if (rc == 0)
(void) dbt2set(dbi, data, &allMatches);

	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
    } else
	rc = -2;

    if (rc) {
	allMatches = dbiFreeIndexSet(allMatches);
	fpc = fpCacheFree(fpc);
	return rc;
    }

    *matches = xcalloc(1, sizeof(**matches));
    rec = dbiIndexNewItem(0, 0);
    i = 0;
    if (allMatches != NULL)
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	uint_32 * dirIndexes;
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

	xx = hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, NULL);
	xx = hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);

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

	baseNames = hfd(baseNames, bnt);
	dirNames = hfd(dirNames, dnt);
	h = headerFree(h);
    }

    rec = _free(rec);
    allMatches = dbiFreeIndexSet(allMatches);

    fpc = fpCacheFree(fpc);

    if ((*matches)->count == 0) {
	*matches = dbiFreeIndexSet(*matches);
	return 1;
    }

    return 0;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb db, const char * name)
{
DBC * dbcursor = NULL;
DBT * key = alloca(sizeof(*key));
DBT * data = alloca(sizeof(*data));
    dbiIndex dbi;
    int rc;
    int xx;

    if (db == NULL)
	return 0;

memset(key, 0, sizeof(*key));
memset(data, 0, sizeof(*data));

    dbi = dbiOpen(db, RPMTAG_NAME, 0);
    if (dbi == NULL)
	return 0;

key->data = (void *) name;
key->size = strlen(name);

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
#ifndef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (rc == 0) {		/* success */
	dbiIndexSet matches;
	/* FIX: matches might be NULL */
	matches = NULL;
	(void) dbt2set(dbi, data, &matches);
	if (matches) {
	    rc = dbiIndexSetCount(matches);
	    matches = dbiFreeIndexSet(matches);
	}
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	rc = 0;
    } else {			/* error */
	rpmlog(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
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
    int gotMatches = 0;
    int rc;
    int i;

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
	rpmlog(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
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
    localarg = alloca(strlen(arg) + 1);
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
    if (s == localarg) return RPMRC_NOTFOUND;

    *s = '\0';
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, NULL, matches);
    if (rc != RPMRC_NOTFOUND) return rc;

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

    if (s == localarg) return RPMRC_NOTFOUND;

    *s = '\0';
   	/* FIX: *matches may be NULL. */
    return dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, release, matches);
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
	    const char * msg = NULL;
	    int lvl;

	    rpmrc = (*mi->mi_hdrchk) (mi->mi_ts, data->data, data->size, &msg);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMMESS_ERROR : RPMMESS_DEBUG);
	    rpmlog(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("miFreeHeader: skipping") : "write"),
			mi->mi_prevoffset, (msg ? msg : "\n"));
	    msg = _free(msg);
	}

	if (data->data != NULL && rpmrc != RPMRC_FAIL) {
	    (void) blockSignals(dbi->dbi_rpmdb, &signalMask);
	    rc = dbiPut(dbi, mi->mi_dbc, key, data, DB_KEYLAST);
	    if (rc) {
		rpmlog(RPMERR_DBPUTINDEX,
			_("error(%d) storing record #%d into %s\n"),
			rc, mi->mi_prevoffset, rpmTagGetName(dbi->dbi_rpmtag));
	    }
	    xx = dbiSync(dbi, 0);
	    (void) unblockSignals(dbi->dbi_rpmdb, &signalMask);
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
	    /* LCL: regfree has bogus only */
	    mire->preg = _free(mire->preg);
	}
    }
    mi->mi_re = _free(mi->mi_re);

    mi->mi_set = dbiFreeIndexSet(mi->mi_set);
    mi->mi_keyp = _free(mi->mi_keyp);
    mi->mi_db = rpmdbUnlink(mi->mi_db, "matchIterator");

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
	rc = strcmp(mire->pattern, val);
	if (rc) rc = 1;
	break;
    case RPMMIRE_DEFAULT:
    case RPMMIRE_REGEX:
	rc = regexec(mire->preg, val, 0, NULL, mire->eflags);
	if (rc && rc != REG_NOMATCH) {
	    char msg[256];
	    (void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmlog(RPMERR_REGEXEC, "%s: regexec failed: %s\n",
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
    const char * allpat = NULL;
    int notmatch = 0;
    regex_t * preg = NULL;
    int cflags = 0;
    int eflags = 0;
    int fnflags = 0;
    int rc = 0;

    if (defmode == (rpmMireMode)-1) {
	const char *t = rpmExpand("%{?_query_selector_match}", NULL);

	if (*t == '\0' || !strcmp(t, "default"))
	    defmode = RPMMIRE_DEFAULT;
	else if (!strcmp(t, "strcmp"))
	    defmode = RPMMIRE_STRCMP;
	else if (!strcmp(t, "regex"))
	    defmode = RPMMIRE_REGEX;
	else if (!strcmp(t, "glob"))
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
	    rpmlog(RPMERR_REGCOMP, "%s: regcomp failed: %s\n", allpat, msg);
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
	    /* LCL: regfree has bogus only */
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
    HGE_t hge = (HGE_t) headerGetEntryMinMemory;
    HFD_t hfd = (HFD_t) headerFreeData;
    union {
	void * ptr;
	const char ** argv;
	const char * str;
	int_32 * i32p;
	int_16 * i16p;
	int_8 * i8p;
    } u;
    char numbuf[32];
    rpmTagType t;
    int_32 c;
    miRE mire;
    static int_32 zero = 0;
    int ntags = 0;
    int nmatches = 0;
    int i, j;
    int rc;

    if (mi->mi_h == NULL)	/* XXX can't happen */
	return 0;

    /*
     * Apply tag tests, implicitly "||" for multiple patterns/values of a
     * single tag, implicitly "&&" between multiple tag patterns.
     */
    if ((mire = mi->mi_re) != NULL)
    for (i = 0; i < mi->mi_nre; i++, mire++) {
	int anymatch;

	if (!hge(mi->mi_h, mire->tag, &t, (void **)&u, &c)) {
	    if (mire->tag != RPMTAG_EPOCH) {
		ntags++;
		continue;
	    }
	    t = RPM_INT32_TYPE;
	    u.i32p = &zero;
	    c = 1;
	}

	anymatch = 0;		/* no matches yet */
	while (1) {
	    switch (t) {
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
		sprintf(numbuf, "%d", (int) *u.i8p);
		rc = miregexec(mire, numbuf);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		break;
	    case RPM_INT16_TYPE:
		sprintf(numbuf, "%d", (int) *u.i16p);
		rc = miregexec(mire, numbuf);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		break;
	    case RPM_INT32_TYPE:
		sprintf(numbuf, "%d", (int) *u.i32p);
		rc = miregexec(mire, numbuf);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		break;
	    case RPM_STRING_TYPE:
		rc = miregexec(mire, u.str);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		break;
	    case RPM_I18NSTRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
		for (j = 0; j < c; j++) {
		    rc = miregexec(mire, u.argv[j]);
		    if ((!rc && !mire->notmatch) || (rc && mire->notmatch)) {
			anymatch++;
			break;
		    }
		}
		break;
	    case RPM_BIN_TYPE:
		{
		const char * str = bin2hex((const char*) u.ptr, c);
		rc = miregexec(mire, str);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		_free(str);
		}
		break;
	    case RPM_NULL_TYPE:
	    default:
		break;
	    }
	    if ((i+1) < mi->mi_nre && mire[0].tag == mire[1].tag) {
		i++;
		mire++;
		continue;
	    }
	    break;
	}

	if ((rpmTagGetType(mire->tag) & RPM_MASK_RETURN_TYPE) == 
	    RPM_ARRAY_RETURN_TYPE) {
	    u.ptr = hfd(u.ptr, t);
	}

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
	rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
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
    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset)
	return mi->mi_h;

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
	    const char * msg = NULL;
	    int lvl;

	    rpmrc = (*mi->mi_hdrchk) (mi->mi_ts, uh, uhlen, &msg);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMMESS_ERROR : RPMMESS_DEBUG);
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
    if (mi->mi_h)
	mi->mi_h->flags |= HEADERFLAG_ALLOCATED;
#else
    mi->mi_h = headerCopyLoad(uh);
#endif
    if (mi->mi_h == NULL || !headerIsEntry(mi->mi_h, RPMTAG_NAME)) {
	rpmlog(RPMERR_BADHEADER,
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

    mi->mi_prevoffset = mi->mi_offset;
    mi->mi_modified = 0;

    return mi->mi_h;
}

static void rpmdbSortIterator(rpmdbMatchIterator mi)
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

/* LCL: segfault */
static int rpmdbGrowIterator(rpmdbMatchIterator mi, int fpNum)
{
    DBC * dbcursor;
    DBT * key;
    DBT * data;
    dbiIndex dbi = NULL;
    dbiIndexSet set;
    int rc;
    int xx;
    int i;

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
	    rpmlog(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
#ifdef	SQLITE_HACK
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
#endif
	return rc;
    }

    set = NULL;
    (void) dbt2set(dbi, data, &set);
    for (i = 0; i < set->count; i++)
	set->recs[i].fpNum = fpNum;

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (mi->mi_set == NULL) {
	mi->mi_set = set;
    } else {
#if 0
fprintf(stderr, "+++ %d = %d + %d\t\"%s\"\n", (mi->mi_set->count + set->count), mi->mi_set->count, set->count, ((char *)key->data));
#endif
	mi->mi_set->recs = xrealloc(mi->mi_set->recs,
		(mi->mi_set->count + set->count) * sizeof(*(mi->mi_set->recs)));
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
    const void * mi_keyp = NULL;
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
if (key->data && key->size == 0) key->size = strlen((char *)key->data);
if (key->data && key->size == 0) key->size++;	/* XXX "/" fixup. */

	    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	    if (rc > 0) {
		rpmlog(RPMERR_DBGETINDEX,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, (key->data ? key->data : "???"), rpmTagGetName(dbi->dbi_rpmtag));
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

assert(keylen == sizeof(k->ui));		/* xxx programmer error */
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

    mi->mi_db = rpmdbLink(db, "matchIterator");
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

/* XXX psm.c */
int rpmdbRemove(rpmdb db, int rid, unsigned int hdrNum,
		rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
{
DBC * dbcursor = NULL;
DBT * key = alloca(sizeof(*key));
DBT * data = alloca(sizeof(*data));
union _dbswap mi_offset;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    Header h;
    sigset_t signalMask;
    int ret = 0;
    int rc = 0;

    if (db == NULL)
	return 0;

memset(key, 0, sizeof(*key));
memset(data, 0, sizeof(*data));

    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
	h = rpmdbNextIterator(mi);
	if (h)
	    h = headerLink(h);
	mi = rpmdbFreeIterator(mi);
    }

    if (h == NULL) {
	rpmlog(RPMERR_DBCORRUPT, _("%s: cannot read header at 0x%x\n"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

#ifdef	DYING
    /* Add remove transaction id to header. */
    if (rid != 0 && rid != -1) {
	int_32 tid = rid;
	(void) headerAddEntry(h, RPMTAG_REMOVETID, RPM_INT32_TYPE, &tid, 1);
    }
#endif

    {	const char *n, *v, *r;
	(void) headerNVR(h, &n, &v, &r);
	rpmlog(RPMMESS_DEBUG, "  --- h#%8u %s-%s-%s\n", hdrNum, n, v, r);
    }

    (void) blockSignals(db, &signalMask);

	/* FIX: rpmvals heartburn */
    {	int dbix;
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	if (dbiTags != NULL)
	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    const char *av[1];
	    const char ** rpmvals = NULL;
	    rpmTagType rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int xx;
	    int i, j;

	    dbi = NULL;
	    rpmtag = dbiTags[dbix];

	    /* Filter out temporary databases */
	    if (isTemporaryDB(rpmtag)) 
		continue;

	    switch (rpmtag) {
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(db, rpmtag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
	      
mi_offset.ui = hdrNum;
if (dbiByteSwapped(dbi) == 1)
    _DBSWAP(mi_offset);
		key->data = &mi_offset;
		key->size = sizeof(mi_offset.ui);

		rc = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc) {
		    rpmlog(RPMERR_DBGETINDEX,
			_("error(%d) setting header #%d record for %s removal\n"),
			rc, hdrNum, rpmTagGetName(dbi->dbi_rpmtag));
		} else
		    rc = dbiDel(dbi, dbcursor, key, data, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		break;
	    }
	
	    if (!hge(h, rpmtag, &rpmtype, (void **) &rpmvals, &rpmcnt))
		continue;

	  dbi = dbiOpen(db, rpmtag, 0);
	  if (dbi != NULL) {
	    int printed;

	    if (rpmtype == RPM_STRING_TYPE) {
		/* XXX force uniform headerGetEntry return */
		av[0] = (const char *) rpmvals;
		rpmvals = av;
		rpmcnt = 1;
	    }

	    printed = 0;
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
	    for (i = 0; i < rpmcnt; i++) {
		dbiIndexSet set;
		int stringvalued;
		byte bin[32];

		switch (dbi->dbi_rpmtag) {
		case RPMTAG_FILEMD5S:
		    /* Filter out empty MD5 strings. */
		    if (!(rpmvals[i] && *rpmvals[i] != '\0'))
			continue;
		    break;
		default:
		    break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
		switch (rpmtype) {
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    key->size = sizeof(RPM_CHAR_TYPE);
		    key->data = rpmvals + i;
		    break;
		case RPM_INT16_TYPE:
		    key->size = sizeof(int_16);
		    key->data = rpmvals + i;
		    break;
		case RPM_INT32_TYPE:
		    key->size = sizeof(int_32);
		    key->data = rpmvals + i;
		    break;
		case RPM_BIN_TYPE:
		    key->size = rpmcnt;
		    key->data = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
		    if (dbi->dbi_rpmtag == RPMTAG_FILEMD5S) {
			const char * s;
			byte * t;

			s = rpmvals[i];
			t = bin;
			for (j = 0; j < 16; j++, t++, s += 2)
			    *t = (nibble(s[0]) << 4) | nibble(s[1]);
			key->data = bin;
			key->size = 16;
			break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			pgpDig dig = pgpNewDig();
			const byte * pkt;
			size_t pktlen;

			if (b64decode(rpmvals[i], (void **)&pkt, &pktlen))
			    continue;
			(void) pgpPrtPkts(pkt, pktlen, dig, 0);
			memcpy(bin, dig->pubkey.signid, 8);
			pkt = _free(pkt);
			dig = _free(dig);
			key->data = bin;
			key->size = 8;
			break;
		    }
		default:
		    key->data = (void *) rpmvals[i];
		    key->size = strlen(rpmvals[i]);
		    stringvalued = 1;
		    break;
		}

		if (!printed) {
		    if (rpmcnt == 1 && stringvalued) {
			rpmlog(RPMMESS_DEBUG,
				_("removing \"%s\" from %s index.\n"),
				(char *)key->data, rpmTagGetName(dbi->dbi_rpmtag));
		    } else {
			rpmlog(RPMMESS_DEBUG,
				_("removing %d entries from %s index.\n"),
				rpmcnt, rpmTagGetName(dbi->dbi_rpmtag));
		    }
		    printed++;
		}

		/* XXX
		 * This is almost right, but, if there are duplicate tag
		 * values, there will be duplicate attempts to remove
		 * the header instance. It's faster to just ignore errors
		 * than to do things correctly.
		 */

/* XXX with duplicates, an accurate data value and DB_GET_BOTH is needed. */

		set = NULL;

if (key->size == 0) key->size = strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */
 
		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc == 0) {			/* success */
		    (void) dbt2set(dbi, data, &set);
		} else if (rc == DB_NOTFOUND) {	/* not found */
		    continue;
		} else {			/* error */
		    rpmlog(RPMERR_DBGETINDEX,
			_("error(%d) setting \"%s\" records from %s index\n"),
			rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		    continue;
		}

		rc = dbiPruneSet(set, rec, 1, sizeof(*rec), 1);

		/* If nothing was pruned, then don't bother updating. */
		if (rc) {
		    set = dbiFreeIndexSet(set);
		    continue;
		}

		if (set->count > 0) {
		    (void) set2dbt(dbi, data, set);
		    rc = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
		    if (rc) {
			rpmlog(RPMERR_DBPUTINDEX,
				_("error(%d) storing record \"%s\" into %s\n"),
				rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		    data->data = _free(data->data);
		    data->size = 0;
		} else {
		    rc = dbiDel(dbi, dbcursor, key, data, 0);
		    if (rc) {
			rpmlog(RPMERR_DBPUTINDEX,
				_("error(%d) removing record \"%s\" from %s\n"),
				rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		}
		set = dbiFreeIndexSet(set);
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	    if (rpmtype != RPM_BIN_TYPE)	/* XXX WTFO? HACK ALERT */
		rpmvals = hfd(rpmvals, rpmtype);
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	rec = _free(rec);
    }

    (void) unblockSignals(db, &signalMask);

    h = headerFree(h);

    /* XXX return ret; */
    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb db, int iid, Header h,
		rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
{
DBC * dbcursor = NULL;
DBT * key = alloca(sizeof(*key));
DBT * data = alloca(sizeof(*data));
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    sigset_t signalMask;
    const char ** baseNames;
    rpmTagType bnt;
    int count = 0;
    dbiIndex dbi;
    int dbix;
    union _dbswap mi_offset;
    unsigned int hdrNum = 0;
    int ret = 0;
    int rc;
    int xx;

    /* Reinitialize to zero, so in the event the add fails
     * we won't have bogus information (i.e. the last succesful
     * add).
     */
    myinstall_instance = 0;

    if (db == NULL)
	return 0;

memset(key, 0, sizeof(*key));
memset(data, 0, sizeof(*data));

#ifdef	NOTYET	/* XXX headerRemoveEntry() broken on dribbles. */
    xx = headerRemoveEntry(h, RPMTAG_REMOVETID);
#endif
    if (iid != 0 && iid != -1) {
	int_32 tid = iid;
	if (!headerIsEntry(h, RPMTAG_INSTALLTID))
	   xx = headerAddEntry(h, RPMTAG_INSTALLTID, RPM_INT32_TYPE, &tid, 1);
    }

    /*
     * If old style filename tags is requested, the basenames need to be
     * retrieved early, and the header needs to be converted before
     * being written to the package header database.
     */

    xx = hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, &count);

    if (_noDirTokens)
	expandFilelist(h);

    (void) blockSignals(db, &signalMask);

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

	key->data = keyp;
	key->size = keylen;
	data->data = datap;
	data->size = datalen;
	ret = dbiGet(dbi, dbcursor, key, data, DB_SET);
	keyp = key->data;
	keylen = key->size;
	datap = data->data;
	datalen = data->size;

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

	key->data = keyp;
	key->size = keylen;
	data->data = datap;
	data->size = datalen;

	ret = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	dbcursor = NULL;
      }

    }

    if (ret) {
	rpmlog(RPMERR_DBCORRUPT,
		_("error(%d) allocating new package instance\n"), ret);
	goto exit;
    }

    /* Now update the indexes */

    if (hdrNum)
    {	
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	/* Save the header number for the current transaction. */
	myinstall_instance = hdrNum;
	
	if (dbiTags != NULL)
	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    const char *av[1];
	    const char **rpmvals = NULL;
	    rpmTagType rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int_32 * requireFlags;
	    rpmRC rpmrc;
	    int i, j;

	    rpmrc = RPMRC_NOTFOUND;
	    dbi = NULL;
	    requireFlags = NULL;
	    rpmtag = dbiTags[dbix];

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
key->data = (void *) &mi_offset;
key->size = sizeof(mi_offset.ui);
data->data = headerUnload(h);
data->size = headerSizeof(h, HEADER_MAGIC_NO);

		/* Check header digest/signature on blob export. */
		if (hdrchk && ts) {
		    const char * msg = NULL;
		    int lvl;

		    rpmrc = (*hdrchk) (ts, data->data, data->size, &msg);
		    lvl = (rpmrc == RPMRC_FAIL ? RPMMESS_ERROR : RPMMESS_DEBUG);
		    rpmlog(lvl, "%s h#%8u %s",
			(rpmrc == RPMRC_FAIL ? _("rpmdbAdd: skipping") : "  +++"),
				hdrNum, (msg ? msg : "\n"));
		    msg = _free(msg);
		}

		if (data->data != NULL && rpmrc != RPMRC_FAIL) {
		    xx = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
		    xx = dbiSync(dbi, 0);
		}
data->data = _free(data->data);
data->size = 0;
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		break;
	    case RPMTAG_BASENAMES:	/* XXX preserve legacy behavior */
		rpmtype = bnt;
		rpmvals = baseNames;
		rpmcnt = count;
		break;
	    case RPMTAG_REQUIRENAME:
		xx = hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		xx = hge(h, RPMTAG_REQUIREFLAGS, NULL, (void **)&requireFlags, NULL);
		break;
	    default:
		xx = hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		break;
	    }

	    if (rpmcnt <= 0) {
		if (rpmtag != RPMTAG_GROUP)
		    continue;

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }

	  dbi = dbiOpen(db, rpmtag, 0);
	  if (dbi != NULL) {
	    int printed;

	    if (rpmtype == RPM_STRING_TYPE) {
		/* XXX force uniform headerGetEntry return */
		av[0] = (const char *) rpmvals;
		rpmvals = av;
		rpmcnt = 1;
	    }

	    printed = 0;
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	    for (i = 0; i < rpmcnt; i++) {
		dbiIndexSet set;
		int stringvalued;
		byte bin[32];
		byte * t;

		/*
		 * Include the tagNum in all indices. rpm-3.0.4 and earlier
		 * included the tagNum only for files.
		 */
		rec->tagNum = i;
		switch (dbi->dbi_rpmtag) {
		case RPMTAG_PUBKEYS:
		    break;
		case RPMTAG_FILEMD5S:
		    /* Filter out empty MD5 strings. */
		    if (!(rpmvals[i] && *rpmvals[i] != '\0'))
			continue;
		    break;
		case RPMTAG_REQUIRENAME:
		    /* Filter out install prerequisites. */
		    if (requireFlags && isInstallPreReq(requireFlags[i]))
			continue;
		    break;
		case RPMTAG_TRIGGERNAME:
		    if (i) {	/* don't add duplicates */
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				break;
			}
			if (j < i)
			    continue;
		    }
		    break;
		default:
		    break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
		switch (rpmtype) {
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    key->size = sizeof(int_8);
		    key->data = rpmvals + i;
		    break;
		case RPM_INT16_TYPE:
		    key->size = sizeof(int_16);
		    key->data = rpmvals + i;
		    break;
		case RPM_INT32_TYPE:
		    key->size = sizeof(int_32);
		    key->data = rpmvals + i;
		    break;
		case RPM_BIN_TYPE:
		    key->size = rpmcnt;
		    key->data = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
		    if (dbi->dbi_rpmtag == RPMTAG_FILEMD5S) {
			const char * s;

			s = rpmvals[i];
			t = bin;
			for (j = 0; j < 16; j++, t++, s += 2)
			    *t = (nibble(s[0]) << 4) | nibble(s[1]);
			key->data = bin;
			key->size = 16;
			break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			pgpDig dig = pgpNewDig();
			const byte * pkt;
			size_t pktlen;

			if (b64decode(rpmvals[i], (void **)&pkt, &pktlen))
			    continue;
			(void) pgpPrtPkts(pkt, pktlen, dig, 0);
			memcpy(bin, dig->pubkey.signid, 8);
			pkt = _free(pkt);
			dig = _free(dig);
			key->data = bin;
			key->size = 8;
			break;
		    }
		default:
		    key->data = (void *) rpmvals[i];
		    key->size = strlen(rpmvals[i]);
		    stringvalued = 1;
		    break;
		}

		if (!printed) {
		    if (rpmcnt == 1 && stringvalued) {
			rpmlog(RPMMESS_DEBUG,
				_("adding \"%s\" to %s index.\n"),
				(char *)key->data, rpmTagGetName(dbi->dbi_rpmtag));
		    } else {
			rpmlog(RPMMESS_DEBUG,
				_("adding %d entries to %s index.\n"),
				rpmcnt, rpmTagGetName(dbi->dbi_rpmtag));
		    }
		    printed++;
		}

/* XXX with duplicates, an accurate data value and DB_GET_BOTH is needed. */

		set = NULL;

if (key->size == 0) key->size = strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc == 0) {			/* success */
		/* With duplicates, cursor is positioned, discard the record. */
		    if (!dbi->dbi_permit_dups)
			(void) dbt2set(dbi, data, &set);
		} else if (rc != DB_NOTFOUND) {	/* error */
		    rpmlog(RPMERR_DBGETINDEX,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		    continue;
		}

		if (set == NULL)		/* not found or duplicate */
		    set = xcalloc(1, sizeof(*set));

		(void) dbiAppendSet(set, rec, 1, sizeof(*rec), 0);

		(void) set2dbt(dbi, data, set);
		rc = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);

		if (rc) {
		    rpmlog(RPMERR_DBPUTINDEX,
				_("error(%d) storing record %s into %s\n"),
				rc, key->data, rpmTagGetName(dbi->dbi_rpmtag));
		    ret += 1;
		}
		data->data = _free(data->data);
		data->size = 0;
		set = dbiFreeIndexSet(set);
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	    if (rpmtype != RPM_BIN_TYPE)	/* XXX WTFO? HACK ALERT */
		rpmvals = hfd(rpmvals, rpmtype);
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	rec = _free(rec);
    }

exit:
    (void) unblockSignals(db, &signalMask);

    return ret;
}

#define _skip(_dn)	{ sizeof(_dn)-1, (_dn) }

static struct skipDir_s {
    int dnlen;
    const char * dn;
} skipDirs[] = {
    { 0, NULL }
};

static int skipDir(const char * dn)
{
    struct skipDir_s * sd = skipDirs;
    int dnlen;

    dnlen = strlen(dn);
    for (sd = skipDirs; sd->dn != NULL; sd++) {
	if (dnlen < sd->dnlen)
	    continue;
	if (strncmp(dn, sd->dn, sd->dnlen))
	    continue;
	return 1;
    }
    return 0;
}

/* XXX transaction.c */
int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
DBT * key;
DBT * data;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmdbMatchIterator mi;
    fingerPrintCache fpc;
    Header h;
    int i, xx;

    if (db == NULL) return 1;

    mi = rpmdbInitIterator(db, RPMTAG_BASENAMES, NULL, 0);
    if (mi == NULL)	/* XXX should  never happen */
	return 1;

key = &mi->mi_key;
data = &mi->mi_data;

    /* Gather all installed headers with matching basename's. */
    for (i = 0; i < numItems; i++) {

	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));

key->data = (void *) fpList[i].baseName;
key->size = strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	if (skipDir(fpList[i].entry->dirName))
	    continue;

	xx = rpmdbGrowIterator(mi, i);

    }

    if ((i = rpmdbGetIteratorCount(mi)) == 0) {
	mi = rpmdbFreeIterator(mi);
	return 0;
    }
    fpc = fpCacheCreate(i);

    rpmdbSortIterator(mi);
    /* iterator is now sorted by (recnum, filenum) */

    /* For all installed headers with matching basename's ... */
    if (mi != NULL)
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char ** dirNames;
	const char ** baseNames;
	const char ** fullBaseNames;
	rpmTagType bnt, dnt;
	uint_32 * dirIndexes;
	uint_32 * fullDirIndexes;
	fingerPrint * fps;
	dbiIndexItem im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched basename's in this package. */
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->hdrNum != mi->mi_set->recs[end].hdrNum)
		break;
	}
	num = end - start;

	/* Compute fingerprints for this installed header's matches */
	xx = hge(h, RPMTAG_BASENAMES, &bnt, (void **) &fullBaseNames, NULL);
	xx = hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &fullDirIndexes, NULL);

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
	    /* FIX: fpList[].subDir may be NULL */
	    if (!FP_EQUAL(fps[i], fpList[im->fpNum]))
		continue;
	    xx = dbiAppendSet(matchList[im->fpNum], im, 1, sizeof(*im), 0);
	}

	fps = _free(fps);
	dirNames = hfd(dirNames, dnt);
	fullBaseNames = hfd(fullBaseNames, bnt);
	baseNames = _free(baseNames);
	dirIndexes = _free(dirIndexes);

	mi->mi_setx = end;
    }

    mi = rpmdbFreeIterator(mi);

    fpc = fpCacheFree(fpc);

    return 0;

}

/**
 * Check if file esists using stat(2).
 * @param urlfn		file name (may be URL)
 * @return		1 if file exists, 0 if not
 */
static int rpmioFileExists(const char * urlfn)
{
    const char *fn;
    int urltype = urlPath(urlfn, &fn);
    struct stat buf;

    if (*fn == '\0') fn = "/";
    switch (urltype) {
    case URL_IS_HTTPS:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HKP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	if (Stat(fn, &buf)) {
	    switch(errno) {
	    case ENOENT:
	    case EINVAL:
		return 0;
	    }
	}
	break;
    case URL_IS_DASH:
    default:
	return 0;
	break;
    }

    return 1;
}

static int rpmdbRemoveDatabase(const char * prefix,
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
    
    filename = alloca(strlen(prefix) + strlen(dbpath) + 40);

    switch (_dbapi) {
    case 4:
    case 3:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base = rpmTagGetName(dbiTags[i]);
	    sprintf(filename, "%s/%s/%s", prefix, dbpath, base);
	    (void)rpmCleanPath(filename);
	    if (!rpmioFileExists(filename))
		continue;
	    xx = unlink(filename);
	}
	for (i = 0; i < 16; i++) {
	    sprintf(filename, "%s/%s/__db.%03d", prefix, dbpath, i);
	    (void)rpmCleanPath(filename);
	    if (!rpmioFileExists(filename))
		continue;
	    xx = unlink(filename);
	}
	break;
    case 2:
    case 1:
    case 0:
	break;
    }

    sprintf(filename, "%s/%s", prefix, dbpath);
    (void)rpmCleanPath(filename);
    xx = rmdir(filename);

    return 0;
}

static int rpmdbMoveDatabase(const char * prefix,
		const char * olddbpath, int _olddbapi,
		const char * newdbpath, int _newdbapi)
{
    int i;
    char * ofilename, * nfilename;
    struct stat * nst = alloca(sizeof(*nst));
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
    
    ofilename = alloca(strlen(prefix) + strlen(olddbpath) + 40);
    nfilename = alloca(strlen(prefix) + strlen(newdbpath) + 40);

    switch (_olddbapi) {
    case 4:
        /* Fall through */
    case 3:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base;
	    int rpmtag;

	    /* Filter out temporary databases */
	    if (isTemporaryDB((rpmtag = dbiTags[i])))
		continue;

	    base = rpmTagGetName(rpmtag);
	    sprintf(ofilename, "%s/%s/%s", prefix, olddbpath, base);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmioFileExists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/%s", prefix, newdbpath, base);
	    (void)rpmCleanPath(nfilename);

	    /*
	     * Get uid/gid/mode/mtime. If old doesn't exist, use new.
	     * XXX Yes, the variable names are backwards.
	     */
	    if (stat(nfilename, nst) < 0)
		if (stat(ofilename, nst) < 0)
		    continue;

	    if ((xx = rename(ofilename, nfilename)) != 0) {
		rc = 1;
		continue;
	    }
	    xx = chown(nfilename, nst->st_uid, nst->st_gid);
	    xx = chmod(nfilename, (nst->st_mode & 07777));
	    {	struct utimbuf stamp;
		stamp.actime = nst->st_atime;
		stamp.modtime = nst->st_mtime;
		xx = utime(nfilename, &stamp);
	    }
	}
	for (i = 0; i < 16; i++) {
	    sprintf(ofilename, "%s/%s/__db.%03d", prefix, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (rpmioFileExists(ofilename))
		xx = unlink(ofilename);
	    sprintf(nfilename, "%s/%s/__db.%03d", prefix, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if (rpmioFileExists(nfilename))
		xx = unlink(nfilename);
	}
	break;
    case 2:
    case 1:
    case 0:
	break;
    }
#ifdef	SQLITE_HACK_XXX
    if (rc || _olddbapi == _newdbapi)
	return rc;

    rc = rpmdbRemoveDatabase(prefix, newdbpath, _newdbapi);


    /* Remove /etc/rpm/macros.db1 configuration file if db3 rebuilt. */
    if (rc == 0 && _newdbapi == 1 && _olddbapi == 3) {
	const char * mdb1 = "/etc/rpm/macros.db1";
	struct stat st;
	if (!stat(mdb1, &st) && S_ISREG(st.st_mode) && !unlink(mdb1))
	    rpmlog(RPMMESS_DEBUG,
		_("removing %s after successful db3 rebuild.\n"), mdb1);
    }
#endif
    return rc;
}

int rpmdbRebuild(const char * prefix, rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
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
    int rc = 0, xx;
    int _dbapi;
    int _dbapi_rebuild;

    if (prefix == NULL) prefix = "/";

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    tfn = rpmGetPath("%{?_dbpath}", NULL);
    if (!(tfn && tfn[0] != '\0'))
    {
	rpmlog(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	dbpath += strlen(prefix) - 1;
    tfn = _free(tfn);

    tfn = rpmGetPath("%{?_dbpath_rebuild}", NULL);
    if (!(tfn && tfn[0] != '\0' && strcmp(tfn, dbpath)))
    {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
	tfn = _free(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	newdbpath += strlen(prefix) - 1;
    tfn = _free(tfn);

    rpmlog(RPMMESS_DEBUG, _("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmlog(RPMERR_MKDIR, _("temporary database %s already exists\n"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmlog(RPMMESS_DEBUG, _("creating directory %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmlog(RPMERR_MKDIR, _("creating directory %s: %s\n"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    _rebuildinprogress = 0;

    rpmlog(RPMMESS_DEBUG, _("opening old database with dbapi %d\n"),
		_dbapi);
    if (openDatabase(prefix, dbpath, _dbapi, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
    _dbapi = olddb->db_api;
    _rebuildinprogress = 1;
    rpmlog(RPMMESS_DEBUG, _("opening new database with dbapi %d\n"),
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
		rpmlog(RPMERR_INTERNAL,
			_("header #%u in the database is bad -- skipping.\n"),
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (_db_filter_dups || newdb->db_filter_dups) {
		const char * name, * version, * release;
		int skip = 0;

		(void) headerNVR(h, &name, &version, &release);

		{   rpmdbMatchIterator mi;
		    mi = rpmdbInitIterator(newdb, RPMTAG_NAME, name, 0);
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_VERSION,
				RPMMIRE_DEFAULT, version);
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_RELEASE,
				RPMMIRE_DEFAULT, release);
		    while (rpmdbNextIterator(mi)) {
			skip = 1;
			break;
		    }
		    mi = rpmdbFreeIterator(mi);
		}

		if (skip)
		    continue;
	    }

	    /* Deleted entries are eliminated in legacy headers by copy. */
	    {	Header nh = (headerIsEntry(h, RPMTAG_HEADERIMAGE)
				? headerCopy(h) : NULL);
		rc = rpmdbAdd(newdb, -1, (nh ? nh : h), ts, hdrchk);
		nh = headerFree(nh);
	    }

	    if (rc) {
		rpmlog(RPMERR_INTERNAL,
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
	rpmlog(RPMMESS_NORMAL, _("failed to rebuild database: original database "
		"remains in place\n"));

	xx = rpmdbRemoveDatabase(prefix, newdbpath, _dbapi_rebuild);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(prefix, newdbpath, _dbapi_rebuild, dbpath, _dbapi)) {
	    rpmlog(RPMMESS_ERROR, _("failed to replace old database with new "
			"database!\n"));
	    rpmlog(RPMMESS_ERROR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (removedir && !(rc == 0 && nocleanup)) {
	rpmlog(RPMMESS_DEBUG, _("removing directory %s\n"), newrootdbpath);
	if (Rmdir(newrootdbpath))
	    rpmlog(RPMMESS_ERROR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    newrootdbpath = _free(newrootdbpath);
    rootdbpath = _free(rootdbpath);

    return rc;
}
