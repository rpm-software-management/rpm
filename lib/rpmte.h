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
    TR_ADDED,			/*!< Package will be installed. */
    TR_REMOVED			/*!< Package will be removed. */
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

    int npreds;			/*!< No. of predecessors. */
    int depth;			/*!< Max. depth in dependency tree. */
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
/*@unused@*/ alKey addedKey;
/*@unused@*/ struct {
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
 * @return		new transaction element
 */
/*@only@*/ /*@null@*/
transactionElement teNew(void)
	/*@*/;

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
 * Retrieve tsort info for transaction element.
 * @param te		transaction element
 * @return		tsort info
 */
tsortInfo teGetTSI(transactionElement te)
	/*@*/;

/**
 * Retrieve pkgKey of TR_ADDED transaction element.
 * @param te		transaction element
 * @return		pkgKey
 */
/*@exposed@*/
alKey teGetAddedKey(transactionElement te)
	/*@*/;

/**
 * Retrieve dependent pkgKey of TR_REMOVED transaction element.
 * @param te		transaction element
 * @return		dependent pkgKey
 */
/*@exposed@*/
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
/*@null@*/
teIterator teFreeIterator(/*@only@*//*@null@*/ teIterator tei)
	/*@*/;

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @return		transaction element iterator
 */
/*@only@*/
teIterator teInitIterator(rpmTransactionSet ts)
	/*@modifies ts @*/;

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
