#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>
#include <rpmhash.h>

typedef /*@abstract@*/ struct availableList_s *		availableList;
typedef /*@abstract@*/ struct problemsSet_s *		problemsSet;
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;
typedef /*@abstract@*/ struct transactionElement_s *	transactionElement;

/*@unchecked@*/
/*@-exportlocal@*/
extern int _ts_debug;
/*@=exportlocal@*/

struct orderListIndex_s {
    int alIndex;
    int orIndex;
};


/** \ingroup rpmdep
 * A single package instance to be installed/removed atomically.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct transactionElement_s {
    enum rpmTransactionType {
	TR_ADDED,	/*!< Package will be installed. */
	TR_REMOVED	/*!< Package will be removed. */
    } type;		/*!< Package disposition (installed/removed). */
    union { 
/*@unused@*/ int addedIndex;
/*@unused@*/ struct {
	    int dboffset;
	    int dependsOnIndex;
	} removed;
    } u;
} ;
/*@=fielduse@*/

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
    int numRemovedPackages;	/*!< No. removed rpmdb instances. */
    int allocedRemovedPackages;	/*!< Size of removed packages array. */
/*@only@*/
    availableList addedPackages;/*!< Set of packages being installed. */
/*@only@*/
    availableList availablePackages;
				/*!< Universe of possible packages. */
/*@only@*/
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

/**
 * Return (malloc'd) header name-version-release string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
/*@only@*/ const char * hGetNVR(Header h, /*@out@*/ const char ** np )
	/*@modifies *np @*/;

/** \ingroup rpmdep
 * Compare package name-version-release from header with dependency, looking
 * for overlap.
 * @deprecated Remove from API when obsoletes is correctly implemented.
 * @param h		header
 * @param reqName	dependency name
 * @param reqEVR	dependency [epoch:]version[-release]
 * @param reqFlags	dependency logical range qualifiers
 * @return		1 if dependency overlaps, 0 otherwise
 */
int headerMatchesDepFlags(Header h,
	const char * reqName, const char * reqEVR, int reqFlags)
		/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
