#ifndef H_RPMTE
#define H_RPMTE

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmte.h
 * Structures used for a transactionElement.
 */

typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;

typedef /*@abstract@*/ struct teIterator_s *		teIterator;

/*@unchecked@*/
/*@-exportlocal@*/
extern int _te_debug;
/*@=exportlocal@*/

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo_s {
    union {
	int	count;
	/*@exposed@*/ /*@dependent@*/ /*@null@*/
	transactionElement suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/
    struct tsortInfo_s * tsi_next;
/*@exposed@*/ /*@dependent@*/ /*@null@*/
    transactionElement tsi_chain;
    int		tsi_reqx;
    int		tsi_qcnt;
};
/*@=fielduse@*/

/** \ingroup rpmdep
 */
typedef enum rpmTransactionType_e {
    TR_ADDED		= (1 << 0),	/*!< Package will be installed. */
    TR_REMOVED		= (1 << 1)	/*!< Package will be removed. */
} rpmTransactionType;

/** \ingroup rpmdep
 * A single package instance to be installed/removed atomically.
 */
struct transactionElement_s {
    rpmTransactionType type;	/*!< Package disposition (installed/removed). */

/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Package header. */
/*@only@*/
    const char * NEVR;		/*!< Package name-version-release. */
/*@owned@*/
    const char * name;		/*!< Name: */
/*@only@*/ /*@null@*/
    char * epoch;
/*@dependent@*/ /*@null@*/
    char * version;		/*!< Version: */
/*@dependent@*/ /*@null@*/
    char * release;		/*!< Release: */
/*@only@*/ /*@null@*/
    const char * arch;		/*!< Architecture hint. */
/*@only@*/ /*@null@*/
    const char * os;		/*!< Operating system hint. */

    transactionElement parent;	/*!< Parent transaction element. */
    int degree;			/*!< No. of immediate children. */
    int depth;			/*!< Max. depth in dependency tree. */
    int npreds;			/*!< No. of predecessors. */
    int tree;			/*!< Tree index. */
/*@owned@*/
    tsortInfo tsi;		/*!< Dependency ordering chains. */

/*@refcounted@*/ /*@null@*/
    rpmDepSet this;		/*!< This package's provided NEVR. */
/*@refcounted@*/ /*@null@*/
    rpmDepSet provides;		/*!< Provides: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmDepSet requires;		/*!< Requires: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmDepSet conflicts;	/*!< Conflicts: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmDepSet obsoletes;	/*!< Obsoletes: dependencies. */
/*@refcounted@*/ /*@null@*/
    TFI_t fi;			/*!< File information. */

    uint_32 multiLib;		/*!< (TR_ADDED) MULTILIB */

/*@exposed@*/ /*@dependent@*/ /*@null@*/
    fnpyKey key;		/*!< (TR_ADDED) Retrieval key. */
/*@owned@*/ /*@null@*/
    rpmRelocation * relocs;	/*!< (TR_ADDED) Payload file relocations. */
/*@refcounted@*/ /*@null@*/	
    FD_t fd;			/*!< (TR_ADDED) Payload file descriptor. */

/*@-fielduse@*/	/* LCL: confused by union? */
    union { 
/*@exposed@*/ /*@dependent@*/ /*@null@*/
	alKey addedKey;
	struct {
/*@exposed@*/ /*@dependent@*/ /*@null@*/
	    alKey dependsOnKey;
	    int dboffset;
	} removed;
    } u;
/*@=fielduse@*/

};

#if defined(_NEED_TEITERATOR)
/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct teIterator_s {
/*@refcounted@*/
    rpmTransactionSet ts;	/*!< transaction set. */
    int reverse;		/*!< reversed traversal? */
    int ocsave;			/*!< last returned iterator index. */
    int oc;			/*!< iterator index. */
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a transaction element.
 * @param te		transaction element
 * @return		NULL always
 */
/*@null@*/
transactionElement teFree(/*@only@*/ /*@null@*/ transactionElement te)
	/*@modifies te@*/;
/**
 * Create a transaction element.
 * @param ts		transaction set
 * @param h		header
 * @param type		TR_ADDED/TR_REMOVED
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 * @param dboffset	(TR_REMOVED) rpmdb instance
 * @param pkgKey	associated added package (if any)
 * @return		new transaction element
 */
/*@only@*/ /*@null@*/
transactionElement teNew(const rpmTransactionSet ts, Header h,
		rpmTransactionType type,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmRelocation * relocs,
		int dboffset,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey pkgKey)
	/*@modifies ts, h @*/;

/**
 * Retrieve type of transaction element.
 * @param te		transaction element
 * @return		type
 */
rpmTransactionType teGetType(transactionElement te)
	/*@*/;

/**
 * Retrieve name string of transaction element.
 * @param te		transaction element
 * @return		name string
 */
/*@observer@*/
const char * teGetN(transactionElement te)
	/*@*/;

/**
 * Retrieve epoch string of transaction element.
 * @param te		transaction element
 * @return		epoch string
 */
/*@observer@*/ /*@null@*/
const char * teGetE(transactionElement te)
	/*@*/;

/**
 * Retrieve version string of transaction element.
 * @param te		transaction element
 * @return		version string
 */
/*@observer@*/ /*@null@*/
const char * teGetV(transactionElement te)
	/*@*/;

/**
 * Retrieve release string of transaction element.
 * @param te		transaction element
 * @return		release string
 */
/*@observer@*/ /*@null@*/
const char * teGetR(transactionElement te)
	/*@*/;

/**
 * Retrieve arch string of transaction element.
 * @param te		transaction element
 * @return		arch string
 */
/*@observer@*/ /*@null@*/
const char * teGetA(transactionElement te)
	/*@*/;

/**
 * Retrieve os string of transaction element.
 * @param te		transaction element
 * @return		os string
 */
/*@observer@*/ /*@null@*/
const char * teGetO(transactionElement te)
	/*@*/;

/**
 * Retrieve multlib flags of transaction element.
 * @param te		transaction element
 * @return		multilib flags
 */
int teGetMultiLib(transactionElement te)
	/*@*/;

/**
 * Set multlib flags of transaction element.
 * @param te		transaction element
 * @param nmultiLib	new multilib flags
 * @return		previous multilib flags
 */
int teSetMultiLib(transactionElement te, int nmultiLib)
	/*@modifies te @*/;

/**
 * Retrieve tsort tree depth of transaction element.
 * @param te		transaction element
 * @return		depth
 */
int teGetDepth(transactionElement te)
	/*@*/;

/**
 * Set tsort tree depth of transaction element.
 * @param te		transaction element
 * @param ndepth	new depth
 * @return		previous depth
 */
int teSetDepth(transactionElement te, int ndepth)
	/*@modifies te @*/;

/**
 * Retrieve tsort no. of predecessors of transaction element.
 * @param te		transaction element
 * @return		no. of predecessors
 */
int teGetNpreds(transactionElement te)
	/*@*/;

/**
 * Set tsort no. of predecessors of transaction element.
 * @param te		transaction element
 * @param npreds	new no. of predecessors
 * @return		previous no. of predecessors
 */
int teSetNpreds(transactionElement te, int npreds)
	/*@modifies te @*/;

/**
 * Retrieve tree index of transaction element.
 * @param te		transaction element
 * @return		tree index
 */
int teGetTree(transactionElement te)
	/*@*/;

/**
 * Set tree index of transaction element.
 * @param te		transaction element
 * @param ntree		new tree index
 * @return		previous tree index
 */
int teSetTree(transactionElement te, int ntree)
	/*@modifies te @*/;

/**
 * Retrieve parent transaction element.
 * @param te		transaction element
 * @return		parent transaction element
 */
transactionElement teGetParent(transactionElement te)
	/*@*/;

/**
 * Set parent transaction element.
 * @param te		transaction element
 * @param pte		new parent transaction element
 * @return		previous parent transaction element
 */
transactionElement teSetParent(transactionElement te, transactionElement pte)
	/*@modifies te @*/;

/**
 * Retrieve number of children of transaction element.
 * @param te		transaction element
 * @return		tree index
 */
int teGetDegree(transactionElement te)
	/*@*/;

/**
 * Set number of children of transaction element.
 * @param te		transaction element
 * @param ntree		new number of children
 * @return		previous number of children
 */
int teSetDegree(transactionElement te, int ntree)
	/*@modifies te @*/;

/**
 * Retrieve tsort info for transaction element.
 * @param te		transaction element
 * @return		tsort info
 */
tsortInfo teGetTSI(transactionElement te)
	/*@*/;

/**
 * Destroy tsort info of transaction element.
 * @param te		transaction element
 */
void teFreeTSI(transactionElement te)
	/*@modifies te @*/;

/**
 * Initialize tsort info of transaction element.
 * @param te		transaction element
 */
void teNewTSI(transactionElement te)
	/*@modifies te @*/;

/**
 * Destroy dependency set info of transaction element.
 * @param te		transaction element
 */
void teCleanDS(transactionElement te)
	/*@modifies te @*/;

/**
 * Retrieve pkgKey of TR_ADDED transaction element.
 * @param te		transaction element
 * @return		pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey teGetAddedKey(transactionElement te)
	/*@*/;

/**
 * Set pkgKey of TR_ADDED transaction element.
 * @param te		transaction element
 * @param npkgKey	new pkgKey
 * @return		previous pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey teSetAddedKey(transactionElement te,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey npkgKey)
	/*@modifies te @*/;

/**
 * Retrieve dependent pkgKey of TR_REMOVED transaction element.
 * @param te		transaction element
 * @return		dependent pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey teGetDependsOnKey(transactionElement te)
	/*@*/;

/**
 * Retrieve rpmdb instance of TR_REMOVED transaction element.
 * @param te		transaction element
 * @return		rpmdb instance
 */
int teGetDBOffset(transactionElement te)
	/*@*/;

/**
 * Retrieve name-version-release string from transaction element.
 * @param te		transaction element
 * @return		name-version-release string
 */
/*@observer@*/
const char * teGetNEVR(transactionElement te)
	/*@*/;

/**
 * Retrieve file handle from transaction element.
 * @param te		transaction element
 * @return		file handle
 */
FD_t teGetFd(transactionElement te)
	/*@*/;

/**
 * Retrieve key from transaction element.
 * @param te		transaction element
 * @return		key
 */
/*@exposed@*/
fnpyKey teGetKey(transactionElement te)
	/*@*/;

/**
 * Retrieve dependency tag set from transaction element.
 * @param te		transaction element
 * @param tag		dependency tag
 * @return		dependency tag set
 */
rpmDepSet teGetDS(transactionElement te, rpmTag tag)
	/*@*/;

/**
 * Retrieve file info tag set from transaction element.
 * @param te		transaction element
 * @param tag		file info tag
 * @return		file info tag set
 */
TFI_t teGetFI(transactionElement te, rpmTag tag)
	/*@*/;

#if defined(_NEED_TEITERATOR)
/**
 * Return transaction element index.
 * @param tei		transaction element iterator
 * @return		transaction element index
 */
int teiGetOc(teIterator tei)
	/*@*/;

/**
 * Destroy transaction element iterator.
 * @param tei		transaction element iterator
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
teIterator teFreeIterator(/*@only@*//*@null@*/ teIterator tei)
	/*@*/;

/**
 * Destroy transaction element iterator.
 * @param tei		transaction element iterator
 * @return		NULL always
 */
/*@null@*/
teIterator XteFreeIterator(/*@only@*//*@null@*/ teIterator tei,
		const char * fn, unsigned int ln)
	/*@*/;
#define	teFreeIterator(_tei)	XteFreeIterator(_tei, __FILE__, __LINE__)

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @return		transaction element iterator
 */
/*@unused@*/ /*@only@*/
teIterator teInitIterator(rpmTransactionSet ts)
	/*@modifies ts @*/;

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @return		transaction element iterator
 */
/*@unused@*/ /*@only@*/
teIterator XteInitIterator(rpmTransactionSet ts,
		const char * fn, unsigned int ln)
	/*@modifies ts @*/;
#define	teInitIterator(_ts)	XteInitIterator(_ts, __FILE__, __LINE__)

/**
 * Return next transaction element
 * @param tei		transaction element iterator
 * @return		transaction element, NULL on termination
 */
/*@dependent@*/ /*@null@*/
transactionElement teNextIterator(teIterator tei)
	/*@modifies tei @*/;

/**
 * Return next transaction element of type.
 * @param tei		transaction element iterator
 * @param type		transaction element type selector
 * @return		next transaction element of type, NULL on termination
 */
/*@dependent@*/ /*@null@*/
transactionElement teNext(teIterator tei, rpmTransactionType type)
        /*@modifies tei @*/;

#endif	/* defined(_NEED_TEITERATOR) */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTE */
