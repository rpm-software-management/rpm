/** \ingroup rpmdb dbi
 * \file rpmdb/rpmdb.c
 */

#include "system.h"

#define	_USE_COPY_LOAD	/* XXX don't use DB_DBT_MALLOC (yet) */

/*@unchecked@*/
int _rpmdb_debug = 0;

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#ifndef	DYING	/* XXX already in "system.h" */
/*@-noparams@*/
#include <fnmatch.h>
/*@=noparams@*/
#if defined(__LCLINT__)
/*@-declundef -exportheader -redecl @*/ /* LCL: missing annotation */
extern int fnmatch (const char *pattern, const char *string, int flags)
	/*@*/;
/*@=declundef =exportheader =redecl @*/
#endif
#endif

#include <regex.h>
#if defined(__LCLINT__)
/*@-declundef -exportheader @*/ /* LCL: missing modifies (only is bogus) */
extern void regfree (/*@only@*/ regex_t *preg)
	/*@modifies *preg @*/;
/*@=declundef =exportheader @*/
#endif

#include <rpmio_internal.h>
#include <rpmmacro.h>

#include "rpmdb.h"
#include "fprint.h"
#include "legacy.h"
#include "header_internal.h"	/* XXX for HEADERFLAG_ALLOCATED */
#include "debug.h"

/*@access dbiIndexSet@*/
/*@access dbiIndexItem@*/
/*@access rpmts@*/		/* XXX compared with NULL */
/*@access Header@*/		/* XXX compared with NULL */
/*@access rpmdbMatchIterator@*/
/*@access pgpDig@*/

/*@unchecked@*/
static int _rebuildinprogress = 0;
/*@unchecked@*/
static int _db_filter_dups = 0;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

/*@unchecked@*/
/*@globstate@*/ /*@null@*/ int * dbiTags = NULL;
/*@unchecked@*/
int dbiTagsMax = 0;

/* Bit mask macros. */
/*@-exporttype@*/
typedef	unsigned int __pbm_bits;
/*@=exporttype@*/
#define	__PBM_NBITS		(8 * sizeof (__pbm_bits))
#define	__PBM_IX(d)		((d) / __PBM_NBITS)
#define __PBM_MASK(d)		((__pbm_bits) 1 << (((unsigned)(d)) % __PBM_NBITS))
/*@-exporttype@*/
typedef struct {
    __pbm_bits bits[1];
} pbm_set;
/*@=exporttype@*/
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
/*@unused@*/
static inline pbm_set * PBM_REALLOC(pbm_set ** sp, int * odp, int nd)
	/*@modifies *sp, *odp @*/
{
    int i, nb;

/*@-bounds -sizeoftype@*/
    if (nd > (*odp)) {
	nd *= 2;
	nb = __PBM_IX(nd) + 1;
/*@-unqualifiedtrans@*/
	*sp = xrealloc(*sp, nb * sizeof(__pbm_bits));
/*@=unqualifiedtrans@*/
	for (i = __PBM_IX(*odp) + 1; i < nb; i++)
	    __PBM_BITS(*sp)[i] = 0;
	*odp = nd;
    }
/*@=bounds =sizeoftype@*/
/*@-compdef -retalias -usereleased@*/
    return *sp;
/*@=compdef =retalias =usereleased@*/
}

/**
 * Convert hex to binary nibble.
 * @param c		hex character
 * @return		binary nibble
 */
static inline unsigned char nibble(char c)
	/*@*/
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
static int printable(const void * ptr, size_t len)	/*@*/
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
	/*@*/
{
    int dbix;

    if (dbiTags != NULL)
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
/*@-boundsread@*/
	if (rpmtag == dbiTags[dbix])
	    return dbix;
/*@=boundsread@*/
    }
    return -1;
}

/**
 * Initialize database (index, tag) tuple from configuration.
 */
static void dbiTagsInit(void)
	/*@globals rpmGlobalMacroContext, dbiTags, dbiTagsMax @*/
	/*@modifies rpmGlobalMacroContext, dbiTags, dbiTagsMax @*/
{
/*@observer@*/ static const char * const _dbiTagStr_default =
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
		/*@innerbreak@*/ break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		/*@innerbreak@*/ break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	rpmtag = tagValue(o);
	if (rpmtag < 0) {
	    rpmMessage(RPMMESS_WARNING,
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

/*@-redecl@*/
#define	DB1vec		NULL
#define	DB2vec		NULL

/*@-exportheadervar -declundef @*/
/*@unchecked@*/
extern struct _dbiVec db3vec;
/*@=exportheadervar =declundef @*/
#define	DB3vec		&db3vec
/*@=redecl@*/

/*@-nullassign@*/
/*@observer@*/ /*@unchecked@*/
static struct _dbiVec *mydbvecs[] = {
    DB1vec, DB1vec, DB2vec, DB3vec, NULL
};
/*@=nullassign@*/

dbiIndex dbiOpen(rpmdb db, rpmTag rpmtag, /*@unused@*/ unsigned int flags)
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
/*@-compdef@*/ /* FIX: db->_dbi may be NULL */
    if ((dbi = db->_dbi[dbix]) != NULL)
	return dbi;
/*@=compdef@*/

    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 3)
	_dbapi_rebuild = 3;
    _dbapi_wanted = (_rebuildinprogress ? -1 : db->db_api);

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 4 || mydbvecs[_dbapi] == NULL) {
	    return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	if (rc) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmError(RPMERR_DBOPEN,
			_("cannot open %s index using db%d - %s (%d)\n"),
			tagName(rpmtag), _dbapi,
			(rc > 0 ? strerror(rc) : ""), rc);
	    _dbapi = -1;
	}
	break;
    case -1:
	_dbapi = 4;
	while (_dbapi-- > 1) {
	    if (mydbvecs[_dbapi] == NULL)
		continue;
	    errno = 0;
	    dbi = NULL;
	    rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	    if (rc == 0 && dbi)
		/*@loopbreak@*/ break;
	}
	if (_dbapi <= 0) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmError(RPMERR_DBOPEN, _("cannot open %s index\n"),
			tagName(rpmtag));
	    rc = 1;
	    goto exit;
	}
	if (db->db_api == -1 && _dbapi > 0)
	    db->db_api = _dbapi;
    	break;
    }

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

exit:
    if (dbi != NULL && rc == 0) {
	db->_dbi[dbix] = dbi;
/*@-sizeoftype@*/
	if (rpmtag == RPMDBI_PACKAGES && db->db_bits == NULL) {
	    db->db_nbits = 1024;
	    if (!dbiStat(dbi, DB_FAST_STAT)) {
		DB_HASH_STAT * hash = (DB_HASH_STAT *)dbi->dbi_stats;
		if (hash)
		    db->db_nbits += hash->hash_nkeys;
	    }
	    db->db_bits = PBM_ALLOC(db->db_nbits);
	}
/*@=sizeoftype@*/
    } else
	dbi = db3Free(dbi);

/*@-compdef -nullstate@*/ /* FIX: db->_dbi may be NULL */
    return dbi;
/*@=compdef =nullstate@*/
}

/**
 * Create and initialize item for index database set.
 * @param hdrNum	header instance in db
 * @param tagNum	tag index in header
 * @return		new item
 */
static dbiIndexItem dbiIndexNewItem(unsigned int hdrNum, unsigned int tagNum)
	/*@*/
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
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

/**
 * Convert retrieved data to index set.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @retval setp		(malloc'ed) index set
 * @return		0 on success
 */
static int dbt2set(dbiIndex dbi, DBT * data, /*@out@*/ dbiIndexSet * setp)
	/*@modifies dbi, *setp @*/
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

/*@-bounds -sizeoftype @*/
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
/*@=bounds =sizeoftype @*/
/*@-compdef@*/
    return 0;
/*@=compdef@*/
}

/**
 * Convert index set to database representation.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @param set		index set
 * @return		0 on success
 */
static int set2dbt(dbiIndex dbi, DBT * data, dbiIndexSet set)
	/*@modifies dbi, *data @*/
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

/*@-bounds -sizeoftype@*/
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
/*@=bounds =sizeoftype@*/

/*@-compdef@*/
    return 0;
/*@=compdef@*/
}

/* XXX assumes hdrNum is first int in dbiIndexItem */
static int hdrNumCmp(const void * one, const void * two)
	/*@*/
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
	/*@modifies *set @*/
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
	/*@-mayaliasunique@*/
	memcpy(set->recs + set->count, rptr, rlen);
	/*@=mayaliasunique@*/
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
	/*@modifies set, recs @*/
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

/**
 */
/*@unchecked@*/
static sigset_t caught;

/* forward ref */
static void handler(int signum);

/**
 */
/*@unchecked@*/
/*@-fullinitblock@*/
static struct sigtbl_s {
    int signum;
    int active;
    void (*handler) (int signum);
    struct sigaction oact;
} satbl[] = {
    { SIGHUP,	0, handler },
    { SIGINT,	0, handler },
    { SIGTERM,	0, handler },
    { SIGQUIT,	0, handler },
    { SIGPIPE,	0, handler },
    { -1,	0, NULL },
};
/*@=fullinitblock@*/

/**
 */
/*@-incondefs@*/
static void handler(int signum)
	/*@globals caught, satbl @*/
	/*@modifies caught @*/
{
    struct sigtbl_s * tbl;

    for(tbl = satbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum != signum)
	    continue;
	if (!tbl->active)
	    continue;
	(void) sigaddset(&caught, signum);
	break;
    }
}
/*@=incondefs@*/

/**
 * Enable all signal handlers.
 */
static int enableSignals(void)
	/*@globals caught, satbl, fileSystem @*/
	/*@modifies caught, satbl, fileSystem @*/
{
    sigset_t newMask, oldMask;
    struct sigtbl_s * tbl;
    struct sigaction act;
    int rc;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    rc = 0;
    for(tbl = satbl; tbl->signum >= 0; tbl++) {
	if (tbl->active++ > 0)
	    continue;
	(void) sigdelset(&caught, tbl->signum);
	memset(&act, 0, sizeof(act));
	act.sa_handler = tbl->handler;
	rc = sigaction(tbl->signum, &act, &tbl->oact);
	if (rc) break;
    }
    return sigprocmask(SIG_SETMASK, &oldMask, NULL);
}

/*@unchecked@*/
static rpmdb rpmdbRock;

int rpmdbCheckSignals(void)
	/*@globals rpmdbRock, satbl @*/
	/*@modifies rpmdbRock @*/
{
    struct sigtbl_s * tbl;
    sigset_t newMask, oldMask;
    int terminate = 0;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    for(tbl = satbl; tbl->signum >= 0; tbl++) {
	if (tbl->active == 0)
	    continue;
	if (sigismember(&caught, tbl->signum))
	    terminate = 1;
    }

    if (terminate) {
	rpmdb db;
	rpmMessage(RPMMESS_DEBUG, "Exiting on signal ...\n");
/*@-newreftrans@*/
	while ((db = rpmdbRock) != NULL) {
/*@i@*/	    rpmdbRock = db->db_next;
	    db->db_next = NULL;
	    (void) rpmdbClose(db);
	}
/*@=newreftrans@*/
	exit(EXIT_FAILURE);
    }
    return sigprocmask(SIG_SETMASK, &oldMask, NULL);
}

/**
 * Disable all signal handlers.
 */
static int disableSignals(void)
	/*@globals satbl, fileSystem @*/
	/*@modifies satbl, fileSystem @*/
{
    struct sigtbl_s * tbl;
    sigset_t newMask, oldMask;
    int rc;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    rc = 0;
    for(tbl = satbl; tbl->signum >= 0; tbl++) {
	if (--tbl->active > 0)
	    continue;
	rc = sigaction(tbl->signum, &tbl->oact, NULL);
	if (rc) break;
    }
    return sigprocmask(SIG_SETMASK, &oldMask, NULL);
}

/**
 * Block all signals, returning previous signal mask.
 */
static int blockSignals(/*@unused@*/ rpmdb db, /*@out@*/ sigset_t * oldMask)
	/*@globals satbl, fileSystem @*/
	/*@modifies *oldMask, satbl, fileSystem @*/
{
    struct sigtbl_s * tbl;
    sigset_t newMask;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, oldMask);
    for(tbl = satbl; tbl->signum >= 0; tbl++) {
	if (tbl->active == 0)
	    continue;
	(void) sigdelset(&newMask, tbl->signum);
    }
    return sigprocmask(SIG_BLOCK, &newMask, NULL);
}

/**
 * Restore signal mask.
 */
/*@mayexit@*/
static int unblockSignals(/*@unused@*/ rpmdb db, sigset_t * oldMask)
	/*@globals rpmdbRock, fileSystem, internalState @*/
	/*@modifies rpmdbRock, fileSystem, internalState @*/
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

/*@-fullinitblock@*/
/*@observer@*/ /*@unchecked@*/
static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS, _DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_ERRPFX
};
/*@=fullinitblock@*/

int rpmdbOpenAll(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return -2;

    if (dbiTags != NULL)
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (db->_dbi[dbix] != NULL)
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
/*@-boundswrite@*/
	if (db->_dbi[dbix] != NULL) {
	    int xx;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}
/*@=boundswrite@*/
	break;
    }
    return rc;
}

/* XXX query.c, rpminstall.c, verify.c */
/*@-incondefs@*/
int rpmdbClose(rpmdb db)
	/*@globals rpmdbRock @*/
	/*@modifies rpmdbRock @*/
{
    rpmdb * prev, next;
    int dbix;
    int rc = 0;

    if (db == NULL)
	goto exit;

    (void) rpmdbUnlink(db, "rpmdbClose");

    /*@-usereleased@*/
    if (db->nrefs > 0)
	goto exit;

    if (db->_dbi)
    for (dbix = db->db_ndbi; --dbix >= 0; ) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
	/*@-unqualifiedtrans@*/		/* FIX: double indirection. */
    	xx = dbiClose(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    	db->_dbi[dbix] = NULL;
	/*@=unqualifiedtrans@*/
    }
    db->db_errpfx = _free(db->db_errpfx);
    db->db_root = _free(db->db_root);
    db->db_home = _free(db->db_home);
    db->db_bits = PBM_FREE(db->db_bits);
    db->_dbi = _free(db->_dbi);

/*@-newreftrans@*/
    prev = &rpmdbRock;
    while ((next = *prev) != NULL && next != db)
	prev = &next->db_next;
    if (next) {
/*@i@*/	*prev = next->db_next;
	next->db_next = NULL;
    }
/*@=newreftrans@*/

    /*@-refcounttrans@*/ db = _free(db); /*@=refcounttrans@*/
    /*@=usereleased@*/

exit:
    (void) disableSignals();
    return rc;
}
/*@=incondefs@*/

int rpmdbSync(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return 0;
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
    	xx = dbiSync(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    }
    return rc;
}

/*@-mods@*/	/* FIX: dbTemplate structure assignment */
static /*@only@*/ /*@null@*/
rpmdb newRpmdb(/*@kept@*/ /*@null@*/ const char * root,
		/*@kept@*/ /*@null@*/ const char * home,
		int mode, int perms, int flags)
	/*@globals _db_filter_dups, rpmGlobalMacroContext @*/
	/*@modifies _db_filter_dups, rpmGlobalMacroContext @*/
{
    rpmdb db = xcalloc(sizeof(*db), 1);
    const char * epfx = _DB_ERRPFX;
    static int _initialized = 0;

    if (!_initialized) {
	_db_filter_dups = rpmExpandNumeric("%{_filterdbdups}");
	_initialized = 1;
    }

/*@-boundswrite@*/
    /*@-assignexpose@*/
    *db = dbTemplate;	/* structure assignment */
    /*@=assignexpose@*/
/*@=boundswrite@*/

    db->_dbi = NULL;

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (mode >= 0)	db->db_mode = mode;
    if (perms >= 0)	db->db_perms = perms;
    if (flags >= 0)	db->db_flags = flags;

    /*@-nullpass@*/
    db->db_root = rpmGetPath( (root && *root ? root : _DB_ROOT), NULL);
    db->db_home = rpmGetPath( (home && *home ? home : _DB_HOME), NULL);
    /*@=nullpass@*/
    if (!(db->db_home && db->db_home[0] != '%')) {
	rpmError(RPMERR_DBOPEN, _("no dbpath has been set\n"));
	db->db_root = _free(db->db_root);
	db->db_home = _free(db->db_home);
	db = _free(db);
	/*@-globstate@*/ return NULL; /*@=globstate@*/
    }
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);
    db->db_remove_env = 0;
    db->db_filter_dups = _db_filter_dups;
    db->db_ndbi = dbiTagsMax;
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));
    db->nrefs = 0;
    /*@-globstate@*/
    return rpmdbLink(db, "rpmdbCreate");
    /*@=globstate@*/
}
/*@=mods@*/

static int openDatabase(/*@null@*/ const char * prefix,
		/*@null@*/ const char * dbpath,
		int _dbapi, /*@null@*/ /*@out@*/ rpmdb *dbp,
		int mode, int perms, int flags)
	/*@globals rpmdbRock, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies rpmdbRock, *dbp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(dbp) >= 0 @*/
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
    if (_dbapi < -1 || _dbapi > 3)
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

    (void) enableSignals();

    db->db_api = _dbapi;

    {	int dbix;

	rc = 0;
	if (dbiTags != NULL)
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
		/*@notreached@*/ /*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }

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
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMTAG_NAME:
		if (dbi == NULL) rc |= 1;
		if (minimal)
		    goto exit;
		/*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }
	}
    }

exit:
    if (rc || justCheck || dbp == NULL)
	xx = rpmdbClose(db);
    else {
/*@-assignexpose -newreftrans@*/
/*@i@*/	db->db_next = rpmdbRock;
	rpmdbRock = db;
/*@i@*/	*dbp = db;
/*@=assignexpose =newreftrans@*/
    }

    return rc;
}

rpmdb XrpmdbUnlink(rpmdb db, const char * msg, const char * fn, unsigned ln)
{
/*@-modfilesys@*/
if (_rpmdb_debug)
fprintf(stderr, "--> db %p -- %d %s at %s:%u\n", db, db->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    db->nrefs--;
    return NULL;
}

rpmdb XrpmdbLink(rpmdb db, const char * msg, const char * fn, unsigned ln)
{
    db->nrefs++;
/*@-modfilesys@*/
if (_rpmdb_debug)
fprintf(stderr, "--> db %p ++ %d %s at %s:%u\n", db, db->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    /*@-refcounttrans@*/ return db; /*@=refcounttrans@*/
}

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
/*@-boundswrite@*/
    return openDatabase(prefix, NULL, _dbapi, dbp, mode, perms, 0);
/*@=boundswrite@*/
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc;

/*@-boundswrite@*/
    rc = openDatabase(prefix, NULL, _dbapi, &db, (O_CREAT | O_RDWR),
		perms, RPMDB_FLAG_JUSTCHECK);
/*@=boundswrite@*/
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

/*@-boundswrite@*/
    rc = openDatabase(prefix, NULL, _dbapi, &db, O_RDONLY, 0644, 0);
/*@=boundswrite@*/

    if (db != NULL) {
	int dbix;
	int xx;
	rc = rpmdbOpenAll(db);

	for (dbix = db->db_ndbi; --dbix >= 0; ) {
	    if (db->_dbi[dbix] == NULL)
		continue;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiVerify(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}

	/*@-nullstate@*/	/* FIX: db->_dbi[] may be NULL. */
	xx = rpmdbClose(db);
	/*@=nullstate@*/
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
static int rpmdbFindByFile(rpmdb db, /*@null@*/ const char * filespec,
		DBT * key, DBT * data, /*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies db, *key, *data, *matches, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
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

    /*@-branchstate@*/
    if ((baseName = strrchr(filespec, '/')) != NULL) {
    	char * t;
	size_t len;

    	len = baseName - filespec + 1;
/*@-boundswrite@*/
	t = strncpy(alloca(len + 1), filespec, len);
	t[len] = '\0';
/*@=boundswrite@*/
	dirName = t;
	baseName++;
    } else {
	dirName = "";
	baseName = filespec;
    }
    /*@=branchstate@*/
    if (baseName == NULL)
	return -2;

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    dbi = dbiOpen(db, RPMTAG_BASENAMES, 0);
/*@-branchstate@*/
    if (dbi != NULL) {
	dbcursor = NULL;
	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

/*@-temptrans@*/
key->data = (void *) baseName;
/*@=temptrans@*/
key->size = strlen(baseName);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	if (rc > 0) {
	    rpmError(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, tagName(dbi->dbi_rpmtag));
	}

if (rc == 0)
(void) dbt2set(dbi, data, &allMatches);

	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
    } else
	rc = -2;
/*@=branchstate@*/

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
	int_32 * dirIndexes;
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
	    /*@-nullpass@*/
	    if (FP_EQUAL(fp1, fp2)) {
	    /*@=nullpass@*/
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

/*@-temptrans@*/
key->data = (void *) name;
/*@=temptrans@*/
key->size = strlen(name);

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    if (rc == 0) {		/* success */
	dbiIndexSet matches;
	/*@-nullpass@*/ /* FIX: matches might be NULL */
	matches = NULL;
	(void) dbt2set(dbi, data, &matches);
	if (matches) {
	    rc = dbiIndexSetCount(matches);
	    matches = dbiFreeIndexSet(matches);
	}
	/*@=nullpass@*/
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	rc = 0;
    } else {			/* error */
	rpmError(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, tagName(dbi->dbi_rpmtag));
	rc = -1;
    }

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
		/*@null@*/ const char * version,
		/*@null@*/ const char * release,
		/*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, *key, *data, *matches,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
{
    int gotMatches = 0;
    int rc;
    int i;

/*@-temptrans@*/
key->data = (void *) name;
/*@=temptrans@*/
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
	rpmError(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, tagName(dbi->dbi_rpmtag));
	return RPMRC_FAIL;
    }

    /* Make sure the version and release match. */
    /*@-branchstate@*/
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
/*@-boundswrite@*/
	if (h)
	    (*matches)->recs[gotMatches++] = (*matches)->recs[i];
	else
	    (*matches)->recs[i].hdrNum = 0;
/*@=boundswrite@*/
	mi = rpmdbFreeIterator(mi);
    }
    /*@=branchstate@*/

    if (gotMatches) {
	(*matches)->count = gotMatches;
	rc = RPMRC_OK;
    } else
	rc = RPMRC_NOTFOUND;

exit:
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
    if (rc && matches && *matches)
	*matches = dbiFreeIndexSet(*matches);
/*@=unqualifiedtrans@*/
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
		/*@null@*/ const char * arg, /*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, *key, *data, *matches,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
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

    /*@-unqualifiedtrans@*/ /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    s = stpcpy(localarg, arg);

    c = '\0';
    brackets = 0;
    for (s -= 1; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    /*@switchbreak@*/ break;
	case ']':
	    if (c != '[') brackets = 0;
	    /*@switchbreak@*/ break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

    /*@-nullstate@*/	/* FIX: *matches may be NULL. */
    if (s == localarg) return RPMRC_NOTFOUND;

/*@-boundswrite@*/
    *s = '\0';
/*@=boundswrite@*/
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, NULL, matches);
    /*@=nullstate@*/
    if (rc != RPMRC_NOTFOUND) return rc;

    /*@-unqualifiedtrans@*/ /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/
    
    /* how about name-version-release? */

    release = s + 1;

    c = '\0';
    brackets = 0;
    for (; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    /*@switchbreak@*/ break;
	case ']':
	    if (c != '[') brackets = 0;
	    /*@switchbreak@*/ break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

    if (s == localarg) return RPMRC_NOTFOUND;

/*@-boundswrite@*/
    *s = '\0';
/*@=boundswrite@*/
    /*@-nullstate@*/	/* FIX: *matches may be NULL. */
    return dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, release, matches);
    /*@=nullstate@*/
}

typedef struct miRE_s {
    rpmTag		tag;		/*!< header tag */
    rpmMireMode		mode;		/*!< pattern match mode */
/*@only@*/ const char *	pattern;	/*!< pattern string */
    int			notmatch;	/*!< like "grep -v" */
/*@only@*/ regex_t *	preg;		/*!< regex compiled pattern buffer */
    int			cflags;		/*!< regcomp(3) flags */
    int			eflags;		/*!< regexec(3) flags */
    int			fnflags;	/*!< fnmatch(3) flags */
} * miRE;

struct _rpmdbMatchIterator {
/*@only@*/
    const void *	mi_keyp;
    size_t		mi_keylen;
/*@refcounted@*/
    rpmdb		mi_db;
    rpmTag		mi_rpmtag;
    dbiIndexSet		mi_set;
    DBC *		mi_dbc;
    DBT			mi_key;
    DBT			mi_data;
    int			mi_setx;
/*@refcounted@*/ /*@null@*/
    Header		mi_h;
    int			mi_sorted;
    int			mi_cflags;
    int			mi_modified;
    unsigned int	mi_prevoffset;
    unsigned int	mi_offset;
    unsigned int	mi_filenum;
    int			mi_nre;
/*@only@*/ /*@null@*/
    miRE		mi_re;
/*@null@*/
    rpmts		mi_ts;
/*@null@*/
    rpmRC (*mi_hdrchk) (rpmts ts, const void * uh, size_t uc, const char ** msg)
	/*@modifies ts, *msg @*/;

};

/**
 * Rewrite a header into packages (if necessary) and free the header.
 *   Note: this is called from a markReplacedFiles iteration, and *must*
 *   preserve the "join key" (i.e. offset) for the header.
 * @param mi		database iterator
 * @param dbi		index database handle
 * @return 		0 on success
 */
static int miFreeHeader(rpmdbMatchIterator mi, dbiIndex dbi)
	/*@globals fileSystem, internalState @*/
	/*@modifies mi, fileSystem, internalState @*/
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

/*@i@*/	key->data = (void *) &mi->mi_prevoffset;
	key->size = sizeof(mi->mi_prevoffset);
	data->data = headerUnload(mi->mi_h);
	data->size = headerSizeof(mi->mi_h, HEADER_MAGIC_NO);

	/* Check header digest/signature on blob export (if requested). */
	if (mi->mi_hdrchk && mi->mi_ts) {
	    const char * msg = NULL;
	    int lvl;

	    rpmrc = (*mi->mi_hdrchk) (mi->mi_ts, data->data, data->size, &msg);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMMESS_ERROR : RPMMESS_DEBUG);
	    rpmMessage(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("miFreeHeader: skipping") : "write"),
			mi->mi_prevoffset, (msg ? msg : "\n"));
	    msg = _free(msg);
	}

	if (data->data != NULL && rpmrc != RPMRC_FAIL) {
	    (void) blockSignals(dbi->dbi_rpmdb, &signalMask);
	    rc = dbiPut(dbi, mi->mi_dbc, key, data, DB_KEYLAST);
	    if (rc) {
		rpmError(RPMERR_DBPUTINDEX,
			_("error(%d) storing record #%d into %s\n"),
			rc, mi->mi_prevoffset, tagName(dbi->dbi_rpmtag));
	    }
	    xx = dbiSync(dbi, 0);
	    (void) unblockSignals(dbi->dbi_rpmdb, &signalMask);
	}
	data->data = _free(data->data);
	data->size = 0;
    }

    mi->mi_h = headerFree(mi->mi_h);

/*@-nullstate@*/
    return rc;
/*@=nullstate@*/
}

rpmdbMatchIterator rpmdbFreeIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi;
    int xx;
    int i;

    if (mi == NULL)
	return NULL;

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
	    /*@+voidabstract -usereleased @*/ /* LCL: regfree has bogus only */
	    mire->preg = _free(mire->preg);
	    /*@=voidabstract =usereleased @*/
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
	/*@*/
{
    int rc = 0;

    switch (mire->mode) {
    case RPMMIRE_STRCMP:
	rc = strcmp(mire->pattern, val);
	if (rc) rc = 1;
	break;
    case RPMMIRE_DEFAULT:
    case RPMMIRE_REGEX:
/*@-boundswrite@*/
	rc = regexec(mire->preg, val, 0, NULL, mire->eflags);
/*@=boundswrite@*/
	if (rc && rc != REG_NOMATCH) {
	    char msg[256];
	    (void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmError(RPMERR_REGEXEC, "%s: regexec failed: %s\n",
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
static /*@only@*/ char * mireDup(rpmTag tag, rpmMireMode *modep,
			const char * pattern)
	/*@modifies *modep @*/
	/*@requires maxSet(modep) >= 0 @*/
{
    const char * s;
    char * pat;
    char * t;
    int brackets;
    size_t nb;
    int c;

/*@-boundswrite@*/
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
	/* periods are escaped, splats become '.*' */
	c = '\0';
	brackets = 0;
	for (s = pattern; *s != '\0'; s++) {
	    switch (*s) {
	    case '.':
	    case '*':
		if (!brackets) nb++;
		/*@switchbreak@*/ break;
	    case '\\':
		s++;
		/*@switchbreak@*/ break;
	    case '[':
		brackets = 1;
		/*@switchbreak@*/ break;
	    case ']':
		if (c != '[') brackets = 0;
		/*@switchbreak@*/ break;
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
		if (!brackets) *t++ = '\\';
		/*@switchbreak@*/ break;
	    case '*':
		if (!brackets) *t++ = '.';
		/*@switchbreak@*/ break;
	    case '\\':
		*t++ = *s++;
		/*@switchbreak@*/ break;
	    case '[':
		brackets = 1;
		/*@switchbreak@*/ break;
	    case ']':
		if (c != '[') brackets = 0;
		/*@switchbreak@*/ break;
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
/*@-boundswrite@*/

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

/*@-boundsread@*/
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
/*@=boundsread@*/

/*@-boundswrite@*/
    allpat = mireDup(tag, &mode, pattern);
/*@=boundswrite@*/

    if (mode == RPMMIRE_DEFAULT)
	mode = defmode;

    /*@-branchstate@*/
    switch (mode) {
    case RPMMIRE_DEFAULT:
    case RPMMIRE_STRCMP:
	break;
    case RPMMIRE_REGEX:
	/*@-type@*/
	preg = xcalloc(1, sizeof(*preg));
	/*@=type@*/
	cflags = (REG_EXTENDED | REG_NOSUB);
	rc = regcomp(preg, allpat, cflags);
	if (rc) {
	    char msg[256];
	    (void) regerror(rc, preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmError(RPMERR_REGCOMP, "%s: regcomp failed: %s\n", allpat, msg);
	}
	break;
    case RPMMIRE_GLOB:
	fnflags = FNM_PATHNAME | FNM_PERIOD;
	break;
    default:
	rc = -1;
	break;
    }
    /*@=branchstate@*/

    if (rc) {
	/*@=kepttrans@*/	/* FIX: mire has kept values */
	allpat = _free(allpat);
	if (preg) {
	    regfree(preg);
	    /*@+voidabstract -usereleased @*/ /* LCL: regfree has bogus only */
	    preg = _free(preg);
	    /*@=voidabstract =usereleased @*/
	}
	/*@=kepttrans@*/
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

/*@-boundsread@*/
    if (mi->mi_nre > 1)
	qsort(mi->mi_re, mi->mi_nre, sizeof(*mi->mi_re), mireCmp);
/*@=boundsread@*/

    return rc;
}

/**
 * Return iterator selector match.
 * @param mi		rpm database iterator
 * @return		1 if header should be skipped
 */
static int mireSkip (const rpmdbMatchIterator mi)
	/*@*/
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
/*@-boundsread@*/
    if ((mire = mi->mi_re) != NULL)
    for (i = 0; i < mi->mi_nre; i++, mire++) {
	int anymatch;

	if (!hge(mi->mi_h, mire->tag, &t, (void **)&u, &c)) {
	    if (mire->tag != RPMTAG_EPOCH)
		continue;
	    t = RPM_INT32_TYPE;
/*@-immediatetrans@*/
	    u.i32p = &zero;
/*@=immediatetrans@*/
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
		/*@switchbreak@*/ break;
	    case RPM_INT16_TYPE:
		sprintf(numbuf, "%d", (int) *u.i16p);
		rc = miregexec(mire, numbuf);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		/*@switchbreak@*/ break;
	    case RPM_INT32_TYPE:
		sprintf(numbuf, "%d", (int) *u.i32p);
		rc = miregexec(mire, numbuf);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		/*@switchbreak@*/ break;
	    case RPM_STRING_TYPE:
		rc = miregexec(mire, u.str);
		if ((!rc && !mire->notmatch) || (rc && mire->notmatch))
		    anymatch++;
		/*@switchbreak@*/ break;
	    case RPM_I18NSTRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
		for (j = 0; j < c; j++) {
		    rc = miregexec(mire, u.argv[j]);
		    if ((!rc && !mire->notmatch) || (rc && mire->notmatch)) {
			anymatch++;
			/*@innerbreak@*/ break;
		    }
		}
		/*@switchbreak@*/ break;
	    case RPM_NULL_TYPE:
	    case RPM_BIN_TYPE:
	    default:
		/*@switchbreak@*/ break;
	    }
	    if ((i+1) < mi->mi_nre && mire[0].tag == mire[1].tag) {
		i++;
		mire++;
		/*@innercontinue@*/ continue;
	    }
	    /*@innerbreak@*/ break;
	}
/*@=boundsread@*/

	u.ptr = hfd(u.ptr, t);

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
/*@-assignexpose -newreftrans @*/
/*@i@*/ mi->mi_ts = ts;
    mi->mi_hdrchk = hdrchk;
/*@=assignexpose =newreftrans @*/
    return rc;
}


/*@-nullstate@*/ /* FIX: mi->mi_key.data may be NULL */
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

/*@-boundswrite@*/
    key = &mi->mi_key;
    memset(key, 0, sizeof(*key));
    data = &mi->mi_data;
    memset(data, 0, sizeof(*data));
/*@=boundswrite@*/

top:
    uh = NULL;
    uhlen = 0;

    do {
  	/*@-branchstate -compmempass @*/
	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	    mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	    keyp = &mi->mi_offset;
	    keylen = sizeof(mi->mi_offset);
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
/*@-boundswrite@*/
	    if (keyp && mi->mi_setx && rc == 0)
		memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));
/*@=boundswrite@*/

	    /* Terminate on error or end of keys */
	    if (rc || (mi->mi_setx && mi->mi_offset == 0))
		return NULL;
	}
  	/*@=branchstate =compmempass @*/
	mi->mi_setx++;
    } while (mi->mi_offset == 0);

    /* If next header is identical, return it now. */
/*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset)
	return mi->mi_h;
/*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/

    /* Retrieve next header blob for index iterator. */
    /*@-branchstate -compmempass -immediatetrans @*/
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
    /*@=branchstate =compmempass =immediatetrans @*/

    /* Rewrite current header (if necessary) and unlink. */
    xx = miFreeHeader(mi, dbi);

    /* Is this the end of the iteration? */
    if (uh == NULL)
	return NULL;

    /* Check header digest/signature once (if requested). */
/*@-boundsread -branchstate -sizeoftype @*/
    if (mi->mi_hdrchk && mi->mi_ts) {
	rpmRC rpmrc = RPMRC_NOTFOUND;
	pbm_set * set = NULL;

	/* Don't bother re-checking a previously read header. */
	if (mi->mi_db->db_bits) {
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
	    rpmMessage(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("rpmdbNextIterator: skipping") : " read"),
			mi->mi_offset, (msg ? msg : "\n"));
	    msg = _free(msg);

	    /* Mark header checked. */
	    if (set && rpmrc == RPMRC_OK)
		PBM_SET(mi->mi_offset, set);

	    /* Skip damaged and inconsistent headers. */
	    if (rpmrc == RPMRC_FAIL)
		goto top;
	}
    }
/*@=boundsread =branchstate =sizeoftype @*/

    /* Did the header blob load correctly? */
#if !defined(_USE_COPY_LOAD)
/*@-onlytrans@*/
    mi->mi_h = headerLoad(uh);
/*@=onlytrans@*/
    if (mi->mi_h)
	mi->mi_h->flags |= HEADERFLAG_ALLOCATED;
#else
    mi->mi_h = headerCopyLoad(uh);
#endif
    if (mi->mi_h == NULL || !headerIsEntry(mi->mi_h, RPMTAG_NAME)) {
	rpmError(RPMERR_BADHEADER,
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

/*@-compdef -retalias -retexpose -usereleased @*/
    return mi->mi_h;
/*@=compdef =retalias =retexpose =usereleased @*/
}
/*@=nullstate@*/

static void rpmdbSortIterator(/*@null@*/ rpmdbMatchIterator mi)
	/*@modifies mi @*/
{
    if (mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0) {
    /*
     * mergesort is much (~10x with lots of identical basenames) faster
     * than pure quicksort, but glibc uses msort_with_tmp() on stack.
     */
#if defined(__GLIBC__)
/*@-boundsread@*/
	qsort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
/*@=boundsread@*/
#else
	mergesort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
#endif
	mi->mi_sorted = 1;
    }
}

/*@-bounds@*/ /* LCL: segfault */
static int rpmdbGrowIterator(/*@null@*/ rpmdbMatchIterator mi, int fpNum)
	/*@globals rpmGlobalMacroContext, fileSystem @*/
	/*@modifies mi, rpmGlobalMacroContext, fileSystem @*/
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
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    if (rc) {			/* error/not found */
	if (rc != DB_NOTFOUND)
	    rpmError(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, key->data, tagName(dbi->dbi_rpmtag));
	return rc;
    }

    set = NULL;
    (void) dbt2set(dbi, data, &set);
    for (i = 0; i < set->count; i++)
	set->recs[i].fpNum = fpNum;

/*@-branchstate@*/
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
/*@=branchstate@*/

    return rc;
}
/*@=bounds@*/

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

    mi = xcalloc(1, sizeof(*mi));
    key = &mi->mi_key;
    data = &mi->mi_data;

/*@-branchstate@*/
    if (rpmtag != RPMDBI_PACKAGES && keyp) {
	DBC * dbcursor = NULL;
	int rc;
	int xx;

	if (isLabel) {
	    /* XXX HACK to get rpmdbFindByLabel out of the API */
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
	    rc = dbiFindByLabel(dbi, dbcursor, key, data, keyp, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	} else if (rpmtag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(db, keyp, key, data, &set);
	} else {
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

/*@-temptrans@*/
key->data = (void *) keyp;
/*@=temptrans@*/
key->size = keylen;
if (key->data && key->size == 0) key->size = strlen((char *)key->data);
if (key->data && key->size == 0) key->size++;	/* XXX "/" fixup. */

/*@-nullstate@*/
	    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
/*@=nullstate@*/
	    if (rc > 0) {
		rpmError(RPMERR_DBGETINDEX,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, (key->data ? key->data : "???"), tagName(dbi->dbi_rpmtag));
	    }

if (rc == 0)
(void) dbt2set(dbi, data, &set);

	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	}
	if (rc)	{	/* error/not found */
	    set = dbiFreeIndexSet(set);
	    mi = _free(mi);
	    return NULL;
	}
    }
/*@=branchstate@*/

    if (keyp) {
	char * k;

	if (rpmtag != RPMDBI_PACKAGES && keylen == 0)
	    keylen = strlen(keyp);
	k = xmalloc(keylen + 1);
/*@-boundsread@*/
	memcpy(k, keyp, keylen);
/*@=boundsread@*/
	k[keylen] = '\0';	/* XXX for strings */
	mi_keyp = k;
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

    /*@-nullret@*/ /* FIX: mi->mi_key.data may be NULL */
    return mi;
    /*@=nullret@*/
}

/* XXX psm.c */
int rpmdbRemove(rpmdb db, /*@unused@*/ int rid, unsigned int hdrNum,
		/*@unused@*/ rpmts ts,
		/*@unused@*/ rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
{
DBC * dbcursor = NULL;
DBT * key = alloca(sizeof(*key));
DBT * data = alloca(sizeof(*data));
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
	rpmError(RPMERR_DBCORRUPT, _("%s: cannot read header at 0x%x\n"),
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
	rpmMessage(RPMMESS_DEBUG, "  --- h#%8u %s-%s-%s\n", hdrNum, n, v, r);
    }

    (void) blockSignals(db, &signalMask);

	/*@-nullpass -nullptrarith -nullderef @*/ /* FIX: rpmvals heartburn */
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
/*@-boundsread@*/
	    rpmtag = dbiTags[dbix];
/*@=boundsread@*/

	    /*@-branchstate@*/
	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(db, rpmtag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
	      
/*@-immediatetrans@*/
		key->data = &hdrNum;
/*@=immediatetrans@*/
		key->size = sizeof(hdrNum);

		rc = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc) {
		    rpmError(RPMERR_DBGETINDEX,
			_("error(%d) setting header #%d record for %s removal\n"),
			rc, hdrNum, tagName(dbi->dbi_rpmtag));
		} else
		    rc = dbiDel(dbi, dbcursor, key, data, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }
	    /*@=branchstate@*/
	
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
/*@-branchstate@*/
	    for (i = 0; i < rpmcnt; i++) {
		dbiIndexSet set;
		int stringvalued;
		byte bin[32];

		switch (dbi->dbi_rpmtag) {
		case RPMTAG_FILEMD5S:
		    /* Filter out empty MD5 strings. */
		    if (!(rpmvals[i] && *rpmvals[i] != '\0'))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		default:
		    /*@switchbreak@*/ break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
		switch (rpmtype) {
/*@-sizeoftype@*/
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    key->size = sizeof(RPM_CHAR_TYPE);
		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
		case RPM_INT16_TYPE:
		    key->size = sizeof(int_16);
		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
		case RPM_INT32_TYPE:
		    key->size = sizeof(int_32);
		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
/*@=sizeoftype@*/
		case RPM_BIN_TYPE:
		    key->size = rpmcnt;
		    key->data = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@switchbreak@*/ break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
/*@-boundsread@*/
		    if (dbi->dbi_rpmtag == RPMTAG_FILEMD5S) {
			const char * s;
			byte * t;

			s = rpmvals[i];
			t = bin;
			for (j = 0; j < 16; j++, t++, s += 2)
			    *t = (nibble(s[0]) << 4) | nibble(s[1]);
			key->data = bin;
			key->size = 16;
			/*@switchbreak@*/ break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			pgpDig dig = pgpNewDig();
			const byte * pkt;
			ssize_t pktlen;

			if (b64decode(rpmvals[i], (void **)&pkt, &pktlen))
			    /*@innercontinue@*/ continue;
			(void) pgpPrtPkts(pkt, pktlen, dig, 0);
			memcpy(bin, dig->pubkey.signid, 8);
			pkt = _free(pkt);
			dig = _free(dig);
			key->data = bin;
			key->size = 8;
			/*@switchbreak@*/ break;
		    }
/*@=boundsread@*/
		    /*@fallthrough@*/
		default:
/*@i@*/		    key->data = (void *) rpmvals[i];
		    key->size = strlen(rpmvals[i]);
		    stringvalued = 1;
		    /*@switchbreak@*/ break;
		}

		if (!printed) {
		    if (rpmcnt == 1 && stringvalued) {
			rpmMessage(RPMMESS_DEBUG,
				_("removing \"%s\" from %s index.\n"),
				(char *)key->data, tagName(dbi->dbi_rpmtag));
		    } else {
			rpmMessage(RPMMESS_DEBUG,
				_("removing %d entries from %s index.\n"),
				rpmcnt, tagName(dbi->dbi_rpmtag));
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
 
/*@-compmempass@*/
		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc == 0) {			/* success */
		    (void) dbt2set(dbi, data, &set);
		} else if (rc == DB_NOTFOUND) {	/* not found */
		    /*@innercontinue@*/ continue;
		} else {			/* error */
		    rpmError(RPMERR_DBGETINDEX,
			_("error(%d) setting \"%s\" records from %s index\n"),
			rc, key->data, tagName(dbi->dbi_rpmtag));
		    ret += 1;
		    /*@innercontinue@*/ continue;
		}
/*@=compmempass@*/

		rc = dbiPruneSet(set, rec, 1, sizeof(*rec), 1);

		/* If nothing was pruned, then don't bother updating. */
		if (rc) {
		    set = dbiFreeIndexSet(set);
		    /*@innercontinue@*/ continue;
		}

/*@-compmempass@*/
		if (set->count > 0) {
		    (void) set2dbt(dbi, data, set);
		    rc = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
		    if (rc) {
			rpmError(RPMERR_DBPUTINDEX,
				_("error(%d) storing record \"%s\" into %s\n"),
				rc, key->data, tagName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		    data->data = _free(data->data);
		    data->size = 0;
		} else {
		    rc = dbiDel(dbi, dbcursor, key, data, 0);
		    if (rc) {
			rpmError(RPMERR_DBPUTINDEX,
				_("error(%d) removing record \"%s\" from %s\n"),
				rc, key->data, tagName(dbi->dbi_rpmtag));
			ret += 1;
		    }
		}
/*@=compmempass@*/
		set = dbiFreeIndexSet(set);
	    }
/*@=branchstate@*/

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
    /*@=nullpass =nullptrarith =nullderef @*/

    (void) unblockSignals(db, &signalMask);

    h = headerFree(h);

    /* XXX return ret; */
    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb db, int iid, Header h,
		/*@unused@*/ rpmts ts,
		/*@unused@*/ rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
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
    unsigned int hdrNum = 0;
    int ret = 0;
    int rc;
    int xx;

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
      /*@-branchstate@*/
      if (dbi != NULL) {

	/* XXX db0: hack to pass sizeof header to fadAlloc */
	datap = h;
	datalen = headerSizeof(h, HEADER_MAGIC_NO);

	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	/* Retrieve join key for next header instance. */

/*@-compmempass@*/
	key->data = keyp;
	key->size = keylen;
/*@i@*/	data->data = datap;
	data->size = datalen;
	ret = dbiGet(dbi, dbcursor, key, data, DB_SET);
	keyp = key->data;
	keylen = key->size;
	datap = data->data;
	datalen = data->size;
/*@=compmempass@*/

/*@-bounds@*/
	hdrNum = 0;
	if (ret == 0 && datap)
	    memcpy(&hdrNum, datap, sizeof(hdrNum));
	++hdrNum;
	if (ret == 0 && datap) {
	    memcpy(datap, &hdrNum, sizeof(hdrNum));
	} else {
	    datap = &hdrNum;
	    datalen = sizeof(hdrNum);
	}
/*@=bounds@*/

	key->data = keyp;
	key->size = keylen;
/*@-kepttrans@*/
	data->data = datap;
/*@=kepttrans@*/
	data->size = datalen;

/*@-compmempass@*/
	ret = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
/*@=compmempass@*/
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	dbcursor = NULL;
      }
      /*@=branchstate@*/

    }

    if (ret) {
	rpmError(RPMERR_DBCORRUPT,
		_("error(%d) allocating new package instance\n"), ret);
	goto exit;
    }

    /* Now update the indexes */

    if (hdrNum)
    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

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
/*@-boundsread@*/
	    rpmtag = dbiTags[dbix];
/*@=boundsread@*/

	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
		dbi = dbiOpen(db, rpmtag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

key->data = (void *) &hdrNum;
key->size = sizeof(hdrNum);
data->data = headerUnload(h);
data->size = headerSizeof(h, HEADER_MAGIC_NO);

		/* Check header digest/signature on blob export. */
		if (hdrchk && ts) {
		    const char * msg = NULL;
		    int lvl;

		    rpmrc = (*hdrchk) (ts, data->data, data->size, &msg);
		    lvl = (rpmrc == RPMRC_FAIL ? RPMMESS_ERROR : RPMMESS_DEBUG);
		    rpmMessage(lvl, "%s h#%8u %s",
			(rpmrc == RPMRC_FAIL ? _("rpmdbAdd: skipping") : "  +++"),
				hdrNum, (msg ? msg : "\n"));
		    msg = _free(msg);
		}

		if (data->data != NULL && rpmrc != RPMRC_FAIL) {
/*@-compmempass@*/
		    xx = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
/*@=compmempass@*/
		    xx = dbiSync(dbi, 0);
		}
data->data = _free(data->data);
data->size = 0;
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMTAG_BASENAMES:	/* XXX preserve legacy behavior */
		rpmtype = bnt;
		rpmvals = baseNames;
		rpmcnt = count;
		/*@switchbreak@*/ break;
	    case RPMTAG_REQUIRENAME:
		xx = hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		xx = hge(h, RPMTAG_REQUIREFLAGS, NULL, (void **)&requireFlags, NULL);
		/*@switchbreak@*/ break;
	    default:
		xx = hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		/*@switchbreak@*/ break;
	    }

	    /*@-branchstate@*/
	    if (rpmcnt <= 0) {
		if (rpmtag != RPMTAG_GROUP)
		    continue;

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }
	    /*@=branchstate@*/

	  dbi = dbiOpen(db, rpmtag, 0);
	  if (dbi != NULL) {
	    int printed;

	    if (rpmtype == RPM_STRING_TYPE) {
		/* XXX force uniform headerGetEntry return */
		/*@-observertrans@*/
		av[0] = (const char *) rpmvals;
		/*@=observertrans@*/
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
		    /*@switchbreak@*/ break;
		case RPMTAG_FILEMD5S:
		    /* Filter out empty MD5 strings. */
		    if (!(rpmvals[i] && *rpmvals[i] != '\0'))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		case RPMTAG_REQUIRENAME:
		    /* Filter out install prerequisites. */
		    if (requireFlags && isInstallPreReq(requireFlags[i]))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		case RPMTAG_TRIGGERNAME:
		    if (i) {	/* don't add duplicates */
/*@-boundsread@*/
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				/*@innerbreak@*/ break;
			}
/*@=boundsread@*/
			if (j < i)
			    /*@innercontinue@*/ continue;
		    }
		    /*@switchbreak@*/ break;
		default:
		    /*@switchbreak@*/ break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
/*@-branchstate@*/
		switch (rpmtype) {
/*@-sizeoftype@*/
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    key->size = sizeof(int_8);
/*@i@*/		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
		case RPM_INT16_TYPE:
		    key->size = sizeof(int_16);
/*@i@*/		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
		case RPM_INT32_TYPE:
		    key->size = sizeof(int_32);
/*@i@*/		    key->data = rpmvals + i;
		    /*@switchbreak@*/ break;
/*@=sizeoftype@*/
		case RPM_BIN_TYPE:
		    key->size = rpmcnt;
/*@i@*/		    key->data = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@switchbreak@*/ break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
/*@-boundsread@*/
		    if (dbi->dbi_rpmtag == RPMTAG_FILEMD5S) {
			const char * s;

			s = rpmvals[i];
			t = bin;
			for (j = 0; j < 16; j++, t++, s += 2)
			    *t = (nibble(s[0]) << 4) | nibble(s[1]);
			key->data = bin;
			key->size = 16;
			/*@switchbreak@*/ break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			pgpDig dig = pgpNewDig();
			const byte * pkt;
			ssize_t pktlen;

			if (b64decode(rpmvals[i], (void **)&pkt, &pktlen))
			    /*@innercontinue@*/ continue;
			(void) pgpPrtPkts(pkt, pktlen, dig, 0);
			memcpy(bin, dig->pubkey.signid, 8);
			pkt = _free(pkt);
			dig = _free(dig);
			key->data = bin;
			key->size = 8;
			/*@switchbreak@*/ break;
		    }
/*@=boundsread@*/
		    /*@fallthrough@*/
		default:
/*@i@*/		    key->data = (void *) rpmvals[i];
		    key->size = strlen(rpmvals[i]);
		    stringvalued = 1;
		    /*@switchbreak@*/ break;
		}
/*@=branchstate@*/

		if (!printed) {
		    if (rpmcnt == 1 && stringvalued) {
			rpmMessage(RPMMESS_DEBUG,
				_("adding \"%s\" to %s index.\n"),
				(char *)key->data, tagName(dbi->dbi_rpmtag));
		    } else {
			rpmMessage(RPMMESS_DEBUG,
				_("adding %d entries to %s index.\n"),
				rpmcnt, tagName(dbi->dbi_rpmtag));
		    }
		    printed++;
		}

/* XXX with duplicates, an accurate data value and DB_GET_BOTH is needed. */

		set = NULL;

if (key->size == 0) key->size = strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

/*@-compmempass@*/
		rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
		if (rc == 0) {			/* success */
		/* With duplicates, cursor is positioned, discard the record. */
		    if (!dbi->dbi_permit_dups)
			(void) dbt2set(dbi, data, &set);
		} else if (rc != DB_NOTFOUND) {	/* error */
		    rpmError(RPMERR_DBGETINDEX,
			_("error(%d) getting \"%s\" records from %s index\n"),
			rc, key->data, tagName(dbi->dbi_rpmtag));
		    ret += 1;
		    /*@innercontinue@*/ continue;
		}
/*@=compmempass@*/

		if (set == NULL)		/* not found or duplicate */
		    set = xcalloc(1, sizeof(*set));

		(void) dbiAppendSet(set, rec, 1, sizeof(*rec), 0);

/*@-compmempass@*/
		(void) set2dbt(dbi, data, set);
		rc = dbiPut(dbi, dbcursor, key, data, DB_KEYLAST);
/*@=compmempass@*/

		if (rc) {
		    rpmError(RPMERR_DBPUTINDEX,
				_("error(%d) storing record %s into %s\n"),
				rc, key->data, tagName(dbi->dbi_rpmtag));
		    ret += 1;
		}
/*@-unqualifiedtrans@*/
		data->data = _free(data->data);
/*@=unqualifiedtrans@*/
		data->size = 0;
		set = dbiFreeIndexSet(set);
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	/*@-observertrans@*/
	    if (rpmtype != RPM_BIN_TYPE)	/* XXX WTFO? HACK ALERT */
		rpmvals = hfd(rpmvals, rpmtype);
	/*@=observertrans@*/
	    rpmtype = 0;
	    rpmcnt = 0;
	}
	/*@=nullpass =nullptrarith =nullderef @*/

	rec = _free(rec);
    }

exit:
    (void) unblockSignals(db, &signalMask);

    return ret;
}

#define _skip(_dn)	{ sizeof(_dn)-1, (_dn) }

/*@unchecked@*/ /*@observer@*/
static struct skipDir_s {
    int dnlen;
/*@observer@*/ /*@null@*/
    const char * dn;
} skipDirs[] = {
    _skip("/usr/share/zoneinfo"),
    _skip("/usr/share/locale"),
    _skip("/usr/share/i18n"),
    _skip("/usr/lib/locale"),
    { 0, NULL }
};

static int skipDir(const char * dn)
	/*@*/
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
/*@-compmempass@*/
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

    if (db == NULL) return 0;

    mi = rpmdbInitIterator(db, RPMTAG_BASENAMES, NULL, 0);
    if (mi == NULL)	/* XXX should  never happen */
	return 0;

key = &mi->mi_key;
data = &mi->mi_data;

    /* Gather all installed headers with matching basename's. */
    for (i = 0; i < numItems; i++) {

/*@-boundswrite@*/
	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));
/*@=boundswrite@*/

/*@-boundsread -dependenttrans@*/
key->data = (void *) fpList[i].baseName;
/*@=boundsread =dependenttrans@*/
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
	int_32 * dirIndexes;
	int_32 * fullDirIndexes;
	fingerPrint * fps;
	dbiIndexItem im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched basename's in this package. */
/*@-boundsread@*/
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->hdrNum != mi->mi_set->recs[end].hdrNum)
		/*@innerbreak@*/ break;
	}
/*@=boundsread@*/
	num = end - start;

	/* Compute fingerprints for this installed header's matches */
	xx = hge(h, RPMTAG_BASENAMES, &bnt, (void **) &fullBaseNames, NULL);
	xx = hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &fullDirIndexes, NULL);

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
/*@-bounds@*/
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].tagNum];
	    dirIndexes[i] = fullDirIndexes[im[i].tagNum];
	}
/*@=bounds@*/

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
/*@-boundsread@*/
	for (i = 0; i < num; i++, im++) {
	    /*@-nullpass@*/ /* FIX: fpList[].subDir may be NULL */
	    if (!FP_EQUAL(fps[i], fpList[im->fpNum]))
		/*@innercontinue@*/ continue;
	    /*@=nullpass@*/
	    xx = dbiAppendSet(matchList[im->fpNum], im, 1, sizeof(*im), 0);
	}
/*@=boundsread@*/

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
/*@=compmempass@*/

/**
 * Check if file esists using stat(2).
 * @param urlfn		file name (may be URL)
 * @return		1 if file exists, 0 if not
 */
static int rpmioFileExists(const char * urlfn)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/
{
    const char *fn;
    int urltype = urlPath(urlfn, &fn);
    struct stat buf;

    /*@-branchstate@*/
    if (*fn == '\0') fn = "/";
    /*@=branchstate@*/
    switch (urltype) {
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
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
	/*@notreached@*/ break;
    }

    return 1;
}

static int rpmdbRemoveDatabase(const char * prefix,
		const char * dbpath, int _dbapi)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{ 
    int i;
    char * filename;
    int xx;

    i = strlen(dbpath);
    /*@-bounds -branchstate@*/
    if (dbpath[i - 1] != '/') {
	filename = alloca(i);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    /*@=bounds =branchstate@*/
    
    filename = alloca(strlen(prefix) + strlen(dbpath) + 40);

    switch (_dbapi) {
    case 3:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
/*@-boundsread@*/
	    const char * base = tagName(dbiTags[i]);
/*@=boundsread@*/
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
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int i;
    char * ofilename, * nfilename;
    struct stat * nst = alloca(sizeof(*nst));
    int rc = 0;
    int xx;
 
    i = strlen(olddbpath);
    /*@-branchstate@*/
    if (olddbpath[i - 1] != '/') {
	ofilename = alloca(i + 2);
	strcpy(ofilename, olddbpath);
	ofilename[i] = '/';
	ofilename[i + 1] = '\0';
	olddbpath = ofilename;
    }
    /*@=branchstate@*/
    
    i = strlen(newdbpath);
    /*@-branchstate@*/
    if (newdbpath[i - 1] != '/') {
	nfilename = alloca(i + 2);
	strcpy(nfilename, newdbpath);
	nfilename[i] = '/';
	nfilename[i + 1] = '\0';
	newdbpath = nfilename;
    }
    /*@=branchstate@*/
    
    ofilename = alloca(strlen(prefix) + strlen(olddbpath) + 40);
    nfilename = alloca(strlen(prefix) + strlen(newdbpath) + 40);

    switch (_olddbapi) {
    case 3:
	if (dbiTags != NULL)
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
		/*@notreached@*/ /*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }

	    base = tagName(rpmtag);
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
	    if (!rpmioFileExists(ofilename))
		continue;
	    xx = unlink(ofilename);
	    sprintf(nfilename, "%s/%s/__db.%03d", prefix, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    xx = unlink(nfilename);
	}
	break;
    case 2:
    case 1:
    case 0:
	break;
    }
    if (rc || _olddbapi == _newdbapi)
	return rc;

    rc = rpmdbRemoveDatabase(prefix, newdbpath, _newdbapi);


    /* Remove /etc/rpm/macros.db1 configuration file if db3 rebuilt. */
    if (rc == 0 && _newdbapi == 1 && _olddbapi == 3) {
	const char * mdb1 = "/etc/rpm/macros.db1";
	struct stat st;
	if (!stat(mdb1, &st) && S_ISREG(st.st_mode) && !unlink(mdb1))
	    rpmMessage(RPMMESS_DEBUG,
		_("removing %s after successful db3 rebuild.\n"), mdb1);
    }
    return rc;
}

int rpmdbRebuild(const char * prefix, rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
	/*@globals _rebuildinprogress @*/
	/*@modifies _rebuildinprogress @*/
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

    /*@-branchstate@*/
    if (prefix == NULL) prefix = "/";
    /*@=branchstate@*/

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    /*@-nullpass@*/
    tfn = rpmGetPath("%{?_dbpath}", NULL);
    /*@=nullpass@*/
/*@-boundsread@*/
    if (!(tfn && tfn[0] != '\0'))
/*@=boundsread@*/
    {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	dbpath += strlen(prefix);
    tfn = _free(tfn);

    /*@-nullpass@*/
    tfn = rpmGetPath("%{?_dbpath_rebuild}", NULL);
    /*@=nullpass@*/
/*@-boundsread@*/
    if (!(tfn && tfn[0] != '\0' && strcmp(tfn, dbpath)))
/*@=boundsread@*/
    {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
/*@-boundswrite@*/
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
/*@=boundswrite@*/
	tfn = _free(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(prefix, tfn, NULL);
    if (!(prefix[0] == '/' && prefix[1] == '\0'))
	newdbpath += strlen(prefix);
    tfn = _free(tfn);

    rpmMessage(RPMMESS_DEBUG, _("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists\n"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("creating directory %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("creating directory %s: %s\n"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    rpmMessage(RPMMESS_DEBUG, _("opening old database with dbapi %d\n"),
		_dbapi);
    _rebuildinprogress = 1;
/*@-boundswrite@*/
    if (openDatabase(prefix, dbpath, _dbapi, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
/*@=boundswrite@*/
    _dbapi = olddb->db_api;
    _rebuildinprogress = 0;

    rpmMessage(RPMMESS_DEBUG, _("opening new database with dbapi %d\n"),
		_dbapi_rebuild);
    (void) rpmDefineMacro(NULL, "_rpmdb_rebuild %{nil}", -1);
/*@-boundswrite@*/
    if (openDatabase(prefix, newdbpath, _dbapi_rebuild, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }
/*@=boundswrite@*/
    _dbapi_rebuild = newdb->db_api;
    
    {	Header h = NULL;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	/* RPMDBI_PACKAGES */
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
		rpmError(RPMERR_INTERNAL,
			_("header #%u in the database is bad -- skipping.\n"),
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (_db_filter_dups || newdb->db_filter_dups) {
		const char * name, * version, * release;
		int skip = 0;

		(void) headerNVR(h, &name, &version, &release);

		/*@-shadow@*/
		{   rpmdbMatchIterator mi;
		    mi = rpmdbInitIterator(newdb, RPMTAG_NAME, name, 0);
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_VERSION,
				RPMMIRE_DEFAULT, version);
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_RELEASE,
				RPMMIRE_DEFAULT, release);
		    while (rpmdbNextIterator(mi)) {
			skip = 1;
			/*@innerbreak@*/ break;
		    }
		    mi = rpmdbFreeIterator(mi);
		}
		/*@=shadow@*/

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
		rpmError(RPMERR_INTERNAL,
			_("cannot add record originally at %u\n"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	mi = rpmdbFreeIterator(mi);

    }

    if (!nocleanup) {
	olddb->db_remove_env = 1;
	newdb->db_remove_env = 1;
    }
    xx = rpmdbClose(olddb);
    xx = rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, _("failed to rebuild database: original database "
		"remains in place\n"));

	xx = rpmdbRemoveDatabase(prefix, newdbpath, _dbapi_rebuild);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(prefix, newdbpath, _dbapi_rebuild, dbpath, _dbapi)) {
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
    newrootdbpath = _free(newrootdbpath);
    rootdbpath = _free(rootdbpath);

    return rc;
}
