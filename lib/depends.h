#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>
#include <rpmhash.h>

typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;
typedef /*@abstract@*/ struct transactionElement_s *	transactionElement;

typedef /*@abstract@*/ struct teIterator_s *		teIterator;

typedef /*@abstract@*/ struct availableList_s *		availableList;
typedef /*@abstract@*/ struct problemsSet_s *		problemsSet;

/*@unchecked@*/
/*@-exportlocal@*/
extern int _ts_debug;
/*@=exportlocal@*/

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct teIterator_s {
/*@refcounted@*/ rpmTransactionSet ts;	/*!< transaction set. */
    int reverse;			/*!< reversed traversal? */
    int ocsave;				/*!< last returned iterator index. */
    int oc;				/*!< iterator index. */
};

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo_s {
    union {
	int	count;
	/*@kept@*/ /*@null@*/ transactionElement suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/
    struct tsortInfo_s * tsi_next;
/*@null@*/
    transactionElement tsi_chain;
    int		tsi_reqx;
    int		tsi_qcnt;
};
/*@=fielduse@*/

/**
 */
struct orderListIndex_s {
    int alIndex;
    int orIndex;
};

/** \ingroup rpmdep
 * A single package instance to be installed/removed atomically.
 */
struct transactionElement_s {
/*@only@*/ /*@null@*/
    char * NEVR;
/*@owned@*/ /*@null@*/
    char * name;
/*@dependent@*/ /*@null@*/
    char * version;
/*@dependent@*/ /*@null@*/
    char * release;

    int npreds;				/*!< No. of predecessors. */
    int depth;				/*!< Max. depth in dependency tree. */
    struct tsortInfo_s tsi;
    enum rpmTransactionType {
	TR_ADDED,	/*!< Package will be installed. */
	TR_REMOVED	/*!< Package will be removed. */
    } type;		/*!< Package disposition (installed/removed). */

    uint_32 multiLib;	/* (TR_ADDED) MULTILIB */
    int_32 filesCount;	/* (TR_ADDED) No. files in package. */

/*@-fielduse@*/	/* LCL: confused by union? */
    union { 
/*@unused@*/ int addedIndex;
/*@unused@*/ struct {
	    int dboffset;
	    int dependsOnIndex;
	} removed;
    } u;
/*@=fielduse@*/

};

/**
 * A package dependency set.
 */
struct rpmDepSet_s {
    int i;			/*!< Element index. */

/*@observer@*/
    const char * Type;		/*!< Tag name. */
/*@only@*/ /*@null@*/
    const char * DNEVR;		/*!< Formatted dependency string. */

    rpmTag tagN;		/*!< Header tag. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for dependency set (or NULL) */
/*@only@*/
    const char ** N;		/*!< Name. */
/*@only@*/
    const char ** EVR;		/*!< Epoch-Version-Release. */
/*@only@*/
    const int_32 * Flags;	/*!< Flags identifying context/comparison. */
    rpmTagType Nt, EVRt, Ft;	/*!< Tag data types. */
    int Count;			/*!< No. of elements */
};

/** \ingroup rpmdep
 * The set of packages to be installed/removed atomically.
 */
struct rpmTransactionSet_s {
    rpmtransFlags transFlags;	/*!< Bit(s) to control operation. */
/*@observer@*/ /*@null@*/
    rpmCallbackFunction notify;	/*!< Callback function. */
/*@observer@*/ /*@null@*/
    rpmCallbackData notifyData;	/*!< Callback private data. */
/*@dependent@*/
    rpmProblemSet probs;	/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    int filesystemCount;	/*!< No. of mounted filesystems. */
/*@dependent@*/
    const char ** filesystems;	/*!< Mounted filesystem names. */
/*@only@*/
    struct diskspaceInfo * di;	/*!< Per filesystem disk/inode usage. */

    int dbmode;			/*!< Database open mode. */
/*@refcounted@*/ /*@null@*/
    rpmdb rpmdb;		/*!< Database handle. */
/*@only@*/ hashTable ht;	/*!< Fingerprint hash table. */

/*@only@*/
    int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;	/*!< No. removed package instances. */
    int allocedRemovedPackages;	/*!< Size of removed packages array. */

/*@only@*/
    availableList addedPackages;/*!< Set of packages being installed. */
    int numAddedPackages;	/*!< No. added package instances. */

/*@only@*/
    availableList availablePackages;
				/*!< Universe of available packages. */
    int numAvailablePackages;	/*!< No. available package instances. */

/*@owned@*/
    transactionElement order;	/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */

/*@only@*/ TFI_t flList;	/*!< Transaction element(s) file info. */

    int flEntries;		/*!< No. of transaction elements. */
    int chrootDone;		/*!< Has chroot(2) been been done? */
/*@only@*/ const char * rootDir;/*!< Path to top of install tree. */
/*@only@*/ const char * currDir;/*!< Current working directory. */
/*@null@*/ FD_t scriptFd;	/*!< Scriptlet stdout/stderr. */
    int delta;			/*!< Delta for reallocation. */
    int_32 id;			/*!< Transaction id. */

    int verify_legacy;		/*!< Verify legacy signatures? */

/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * fn;		/*!< Current package fn. */
    int_32  sigtag;		/*!< Current package signature tag. */
    int_32  sigtype;		/*!< Current package signature data type. */
/*@null@*/ const void * sig;	/*!< Current package signature. */
    int_32 siglen;		/*!< Current package signature length. */
/*@null@*/
    struct pgpDig_s * dig;	/*!< Current signature/pubkey parametrs. */

/*@refs@*/ int nrefs;		/*!< Reference count. */

} ;

/** \ingroup rpmdep
 * Problems encountered while checking dependencies.
 */
struct problemsSet_s {
    rpmDependencyConflict problems;	/*!< Problems encountered. */
    int num;			/*!< No. of problems found. */
    int alloced;		/*!< No. of problems allocated. */
} ;

#ifdef __cplusplus
extern "C" {
#endif

/*@access teIterator @*/
/*@access rpmTransactionSet @*/

/**
 * Return transaction element index.
 * @param tei		transaction element iterator
 * @return		element index
 */
/*@unused@*/ static inline
int teGetOc(teIterator tei)
	/*@*/
{
    return tei->ocsave;
}

/**
 * Destroy transaction element iterator.
 * @param tei		transaction element iterator
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
teIterator teFreeIterator(/*@only@*//*@null@*/ teIterator tei)
	/*@*/
{
    if (tei)
	tei->ts = rpmtsUnlink(tei->ts, "tsIterator");
    return _free(tei);
}

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @return		transaction element iterator
 */
/*@unused@*/ static inline /*@only@*/
teIterator teInitIterator(rpmTransactionSet ts)
	/*@modifies ts @*/
{
    teIterator tei = NULL;

    tei = xcalloc(1, sizeof(*tei));
    tei->ts = rpmtsLink(ts, "teIterator");
    tei->reverse = ((ts->transFlags & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    tei->oc = (tei->reverse ? (ts->orderCount - 1) : 0);
    tei->ocsave = tei->oc;
    return tei;
}

/**
 * Return next transaction element
 * @param tei		transaction element iterator
 * @return		transaction element, NULL on termination
 */
/*@unused@*/ static inline /*@dependent@*/
transactionElement teNextIterator(teIterator tei)
	/*@modifies tei @*/
{
    transactionElement te = NULL;
    int oc = -1;

    if (tei->reverse) {
	if (tei->oc >= 0)			oc = tei->oc--;
    } else {
    	if (tei->oc < tei->ts->orderCount)	oc = tei->oc++;
    }
    tei->ocsave = oc;
    if (oc != -1)
	te = tei->ts->order + oc;
    /*@-compdef -usereleased@*/ /* FIX: ts->order may be released */
    return te;
    /*@=compdef =usereleased@*/
}

/*@access rpmDepSet @*/

#if 0
#define	_DS_DEBUG	1
#endif

/**
 * Destroy a new dependency set.
 * @param ds		dependency set
 * @return		NULL always
 */
/*@unused@*/ static inline /*@only@*/ /*@null@*/
rpmDepSet dsFree(/*@only@*/ /*@null@*/ rpmDepSet ds)
	/*@modifies ds@*/
{
    HFD_t hfd = headerFreeData;
    rpmTag tagEVR, tagF;

#ifdef	_DS_DEBUG
fprintf(stderr, "*** ds %p --\n", ds);
#endif
    if (ds == NULL)
	return ds;

    if (ds->tagN == RPMTAG_PROVIDENAME) {
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (ds->tagN == RPMTAG_REQUIRENAME) {
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (ds->tagN == RPMTAG_CONFLICTNAME) {
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (ds->tagN == RPMTAG_OBSOLETENAME) {
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else {
	return NULL;
    }

    /*@-branchstate@*/
    if (ds->Count > 0) {
	ds->N = hfd(ds->N, ds->Nt);
	ds->EVR = hfd(ds->EVR, ds->EVRt);
	/*@-evalorder@*/
	ds->Flags = (ds->h != NULL ? hfd(ds->Flags, ds->Ft) : _free(ds->Flags));
	/*@=evalorder@*/
	ds->h = headerFree(ds->h, "dsFree");
    }

    ds->DNEVR = _free(ds->DNEVR);

    /*@=branchstate@*/
    memset(ds, 0, sizeof(*ds));		/* XXX trash and burn */
    ds = _free(ds);
    return NULL;
}

/**
 * Create and load a dependency set.
 * @param h		header
 * @param tagN		type of dependency
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new dependency set
 */
/*@unused@*/ static inline /*@only@*/ /*@null@*/
rpmDepSet dsNew(Header h, rpmTag tagN, int scareMem)
	/*@modifies h @*/
{
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpmTag tagEVR, tagF;
    rpmDepSet ds = NULL;
    const char * Type;

    if (tagN == RPMTAG_PROVIDENAME) {
	Type = "Provides";
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (tagN == RPMTAG_REQUIRENAME) {
	Type = "Requires";
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (tagN == RPMTAG_CONFLICTNAME) {
	Type = "Conflicts";
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (tagN == RPMTAG_OBSOLETENAME) {
	Type = "Obsoletes";
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else {
	goto exit;
    }

    ds = xcalloc(1, sizeof(*ds));
    ds->i = -1;

    ds->Type = Type;
    ds->DNEVR = NULL;

    ds->tagN = tagN;
    ds->h = (scareMem ? headerLink(h, "dsNew") : NULL);
    if (hge(h, tagN, &ds->Nt, (void **) &ds->N, &ds->Count)) {
	int xx;
	xx = hge(h, tagEVR, &ds->EVRt, (void **) &ds->EVR, NULL);
	xx = hge(h, tagF, &ds->Ft, (void **) &ds->Flags, NULL);
	if (!scareMem && ds->Flags != NULL)
	    ds->Flags = memcpy(xmalloc(ds->Count * sizeof(*ds->Flags)),
                                ds->Flags, ds->Count* sizeof(*ds->Flags));
    } else
	ds->h = headerFree(ds->h, "dsNew");

exit:
    /*@-nullret@*/ /* FIX: ds->Flags may be NULL. */
#ifdef	_DS_DEBUG
fprintf(stderr, "*** ds %p ++ %s[%d]\n", ds, ds->Type, ds->Count);
#endif
    return ds;
    /*@=nullret@*/
}

/**
 * Return formatted dependency string.
 * @param depend	type of dependency ("R" == Requires, "C" == Conflcts)
 * @param key		dependency
 * @return		formatted dependency (malloc'ed)
 */
/*@unused@*/ static inline /*@only@*/
char * dsDNEVR(const char * depend, const rpmDepSet key)
	/*@*/
{
    char * tbuf, * t;
    size_t nb;

    nb = 0;
    if (depend)	nb += strlen(depend) + 1;
    if (key->N[key->i])	nb += strlen(key->N[key->i]);
    if (key->Flags[key->i] & RPMSENSE_SENSEMASK) {
	if (nb)	nb++;
	if (key->Flags[key->i] & RPMSENSE_LESS)	nb++;
	if (key->Flags[key->i] & RPMSENSE_GREATER) nb++;
	if (key->Flags[key->i] & RPMSENSE_EQUAL)	nb++;
    }
    if (key->EVR[key->i] && *key->EVR[key->i]) {
	if (nb)	nb++;
	nb += strlen(key->EVR[key->i]);
    }

    t = tbuf = xmalloc(nb + 1);
    if (depend) {
	t = stpcpy(t, depend);
	*t++ = ' ';
    }
    if (key->N[key->i])
	t = stpcpy(t, key->N[key->i]);
    if (key->Flags[key->i] & RPMSENSE_SENSEMASK) {
	if (t != tbuf)	*t++ = ' ';
	if (key->Flags[key->i] & RPMSENSE_LESS)	*t++ = '<';
	if (key->Flags[key->i] & RPMSENSE_GREATER) *t++ = '>';
	if (key->Flags[key->i] & RPMSENSE_EQUAL)	*t++ = '=';
    }
    if (key->EVR[key->i] && *key->EVR[key->i]) {
	if (t != tbuf)	*t++ = ' ';
	t = stpcpy(t, key->EVR[key->i]);
    }
    *t = '\0';
    return tbuf;
}

/**
 * Return current dependency name.
 * @param ds		dependency set
 * @return		current dependency name, NULL on invalid
 */
/*@unused@*/ static inline /*@null@*/
const char * dsiGetN(/*@null@*/ rpmDepSet ds)
	/*@*/
{
    const char * N = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->N != NULL)
	    N = ds->N[ds->i];
    }
    return N;
}

/**
 * Return current dependency epoch-version-release.
 * @param ds		dependency set
 * @return		current dependency EVR, NULL on invalid
 */
/*@unused@*/ static inline /*@null@*/
const char * dsiGetEVR(/*@null@*/ rpmDepSet ds)
	/*@*/
{
    const char * EVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->EVR != NULL)
	    EVR = ds->EVR[ds->i];
    }
    return EVR;
}

/**
 * Return current dependency Flags.
 * @param ds		dependency set
 * @return		current dependency EVR, 0 on invalid
 */
/*@unused@*/ static inline
int_32 dsiGetFlags(/*@null@*/ rpmDepSet ds)
	/*@*/
{
    int_32 Flags = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Flags != NULL)
	    Flags = ds->Flags[ds->i];
    }
    return Flags;
}

/**
 * Notify of results of dependency match;
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
/*@unused@*/ static inline
void dsiNotify(/*@null@*/ rpmDepSet ds, /*@null@*/ const char * where, int rc)
	/*@*/
{
    if (!(ds != NULL && ds->i >= 0 && ds->i < ds->Count))
	return;
    if (ds->Type == NULL ||ds->DNEVR == NULL)
	return;

    rpmMessage(RPMMESS_DEBUG, "%9s: %-45s %-s %s\n", ds->Type,
		(!strcmp(ds->DNEVR, "cached") ? ds->DNEVR : ds->DNEVR+2),
		(rc ? _("NO ") : _("YES")),
		(where != NULL ? where : ""));
}

/**
 * Return next dependency set iterator index.
 * @param ds		dependency set
 * @return		dependency set iterator index, -1 on termination
 */
/*@unused@*/ static inline
int dsiNext(/*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/
{
    int i = -1;

    if (ds != NULL && ++ds->i >= 0) {
	if (ds->i < ds->Count) {
	    char t[2];
	    i = ds->i;
	    ds->DNEVR = _free(ds->DNEVR);
	    t[0] = ((ds->Type != NULL) ? ds->Type[0] : '\0');
	    t[1] = '\0';
	    /*@-nullstate@*/
	    ds->DNEVR = dsDNEVR(t, ds);
	    /*@=nullstate@*/
#ifdef	_DS_DEBUG
fprintf(stderr, "*** ds %p[%d] %s: %s\n", ds, i, ds->Type, ds->DNEVR);
#endif
	}
    }
    return i;
}

/**
 * Initialize dependency set iterator.
 * @param ds		dependency set
 * @return		dependency set
 */
/*@unused@*/ static inline /*@null@*/
rpmDepSet dsiInit(/*@returned@*/ /*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/
{
    if (ds != NULL)
	ds->i = -1;
    return ds;
}

/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
/*@unused@*/ static inline
void parseEVR(char * evr,
		/*@exposed@*/ /*@out@*/ const char ** ep,
		/*@exposed@*/ /*@out@*/ const char ** vp,
		/*@exposed@*/ /*@out@*/ const char ** rp)
	/*@modifies *ep, *vp, *rp @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && xisdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	/*@-branchstate@*/
	if (*epoch == '\0') epoch = "0";
	/*@=branchstate@*/
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
	*se++ = '\0';
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

/**
 * Compare two versioned dependency ranges, looking for overlap.
 * @param A		1st dependency
 * @param B		2nd dependency
 * @return		1 if dependencies overlap, 0 otherwise
 */
/*@unused@*/ static inline
int dsCompare(const rpmDepSet A, const rpmDepSet B)
	/*@*/
{
    const char *aDepend = (A->DNEVR != NULL ? xstrdup(A->DNEVR+2) : "");
    const char *bDepend = (B->DNEVR != NULL ? xstrdup(B->DNEVR+2) : "");
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

    /* Different names don't overlap. */
    if (strcmp(A->N[A->i], B->N[B->i])) {
	result = 0;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    if (!((A->Flags[A->i] & RPMSENSE_SENSEMASK) && (B->Flags[B->i] & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    /* If either EVR is non-existent or empty, always overlap. */
    if (!(A->EVR[A->i] && *A->EVR[A->i] && B->EVR[B->i] && *B->EVR[B->i])) {
	result = 1;
	goto exit;
    }

    /* Both AEVR and BEVR exist. */
    aEVR = xstrdup(A->EVR[A->i]);
    parseEVR(aEVR, &aE, &aV, &aR);
    bEVR = xstrdup(B->EVR[B->i]);
    parseEVR(bEVR, &bE, &bV, &bR);

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	/* XXX legacy epoch-less requires/conflicts compatibility */
	rpmMessage(RPMMESS_DEBUG, _("the \"B\" dependency needs an epoch (assuming same as \"A\")\n\tA %s\tB %s\n"),
		aDepend, bDepend);
	sense = 0;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR) {
	    sense = rpmvercmp(aR, bR);
	}
    }
    aEVR = _free(aEVR);
    bEVR = _free(bEVR);

    /* Detect overlap of {A,B} range. */
    result = 0;
    if (sense < 0 && ((A->Flags[A->i] & RPMSENSE_GREATER) || (B->Flags[B->i] & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((A->Flags[A->i] & RPMSENSE_LESS) || (B->Flags[B->i] & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((A->Flags[A->i] & RPMSENSE_EQUAL) && (B->Flags[B->i] & RPMSENSE_EQUAL)) ||
	 ((A->Flags[A->i] & RPMSENSE_LESS) && (B->Flags[B->i] & RPMSENSE_LESS)) ||
	 ((A->Flags[A->i] & RPMSENSE_GREATER) && (B->Flags[B->i] & RPMSENSE_GREATER)))) {
	result = 1;
    }

exit:
    rpmMessage(RPMMESS_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? _("YES") : _("NO ")), aDepend, bDepend);
    aDepend = _free(aDepend);
    bDepend = _free(bDepend);
    return result;
}

/**
 */
/*@unused@*/ static inline
int rangeMatchesDepFlags (Header h, const rpmDepSet req)
	/*@modifies h @*/
{
    int scareMem = 1;
    rpmDepSet provides = NULL;
    int result = 0;

    if (!(req->Flags[req->i] & RPMSENSE_SENSEMASK) || !req->EVR[req->i] || *req->EVR[req->i] == '\0')
	return 1;

    /* Get provides information from header */
    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides version info is available, match any requires.
     */
    provides = dsiInit(dsNew(h, RPMTAG_PROVIDENAME, scareMem));
    if (provides == NULL)
	goto exit;	/* XXX should never happen */

    if (provides->EVR == NULL) {
	result = 1;
	goto exit;
    }

    result = 0;
    if (provides != NULL)
    while (dsiNext(provides) >= 0) {

	/* Filter out provides that came along for the ride. */
	if (strcmp(provides->N[provides->i], req->N[req->i]))
	    continue;

	result = dsCompare(provides, req);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

exit:
    provides = dsFree(provides);

    return result;
}

/** \ingroup rpmdep
 * Compare package name-version-release from header with dependency, looking
 * for overlap.
 * @deprecated Remove from API when obsoletes is correctly implemented.
 * @param h		header
 * @param req		dependency
 * @return		1 if dependency overlaps, 0 otherwise
 */
/*@unused@*/ static inline
int headerMatchesDepFlags(Header h, const rpmDepSet req)
	/*@*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char *name, *version, *release;
    int_32 * epoch;
    const char * pkgEVR;
    char * p;
    int_32 pkgFlags = RPMSENSE_EQUAL;
    rpmDepSet pkg = memset(alloca(sizeof(*pkg)), 0, sizeof(*pkg));
    int rc;

    if (!((req->Flags[req->i] & RPMSENSE_SENSEMASK) && req->EVR[req->i] && *req->EVR[req->i]))
	return 1;

    /* Get package information from header */
    (void) headerNVR(h, &name, &version, &release);

    pkgEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*@-compmempass@*/ /* FIX: move pkg immediate variables from stack */
    pkg->i = -1;
    pkg->Type = "Provides";
    pkg->tagN = RPMTAG_PROVIDENAME;
    pkg->DNEVR = NULL;
    /*@-immediatetrans@*/
    pkg->N = &name;
    pkg->EVR = &pkgEVR;
    pkg->Flags = &pkgFlags;
    /*@=immediatetrans@*/
    pkg->Count = 1;
    (void) dsiNext(dsiInit(pkg));

    rc = dsCompare(pkg, req);

    pkg->DNEVR = _free(pkg->DNEVR);
    /*@=compmempass@*/

    return rc;
}

/**
 * Return (malloc'd) header name-version-release string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
/*@only@*/ char * hGetNVR(Header h, /*@out@*/ const char ** np )
	/*@modifies *np @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
