#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>
#include <rpmhash.h>

typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;
typedef /*@abstract@*/ struct transactionElement_s *	transactionElement;

typedef /*@abstract@*/ struct teIterator_s *		teIterator;

typedef /*@abstract@*/ struct availableList_s *		availableList;
typedef /*@abstract@*/ struct problemsSet_s *		problemsSet;

/*@unchecked@*/
/*@-exportlocal@*/
extern int _cacheDependsRC;
/*@=exportlocal@*/

/*@unchecked@*/
/*@-exportlocal@*/
extern int _te_debug;
/*@=exportlocal@*/

/*@unchecked@*/
/*@-exportlocal@*/
extern int _ts_debug;
/*@=exportlocal@*/

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
