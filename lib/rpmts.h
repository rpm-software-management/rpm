#ifndef H_RPMTS
#define H_RPMTS

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmts.h
 * Structures and prototypes used for an rpmTransactionSet
 */

#include <rpmhash.h>	/* XXX hashTable */
#include "rpmal.h"	/* XXX availablePackage/relocateFileList ,*/

/*@unchecked@*/
/*@-exportlocal@*/
extern int _ts_debug;
/*@=exportlocal@*/

/*@unchecked@*/
/*@-exportlocal@*/
extern int _cacheDependsRC;
/*@=exportlocal@*/

/** \ingroup rpmdep
 * The set of packages to be installed/removed atomically.
 */
struct rpmTransactionSet_s {
    rpmtransFlags transFlags;	/*!< Bit(s) to control operation. */
/*@observer@*/ /*@null@*/
    rpmCallbackFunction notify;	/*!< Callback function. */
/*@observer@*/ /*@null@*/
    rpmCallbackData notifyData;	/*!< Callback private data. */
/*@refcounted@*/ /*@null@*/
    rpmProblemSet probs;	/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    int filesystemCount;	/*!< No. of mounted filesystems. */
/*@dependent@*/
    const char ** filesystems;	/*!< Mounted filesystem names. */
/*@only@*/ /*@null@*/
    struct diskspaceInfo * di;	/*!< Per filesystem disk/inode usage. */

    int dbmode;			/*!< Database open mode. */
/*@refcounted@*/ /*@null@*/
    rpmdb rpmdb;		/*!< Database handle. */
/*@only@*/
    hashTable ht;		/*!< Fingerprint hash table. */

/*@only@*/ /*@null@*/
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
    transactionElement * order;	/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */

    int chrootDone;		/*!< Has chroot(2) been been done? */
/*@only@*/ /*@null@*/
    const char * rootDir;	/*!< Path to top of install tree. */
/*@only@*/ /*@null@*/
    const char * currDir;	/*!< Current working directory. */
/*@null@*/
    FD_t scriptFd;		/*!< Scriptlet stdout/stderr. */
    int delta;			/*!< Delta for reallocation. */
    int_32 id;			/*!< Transaction id. */

    int verify_legacy;		/*!< Verify legacy signatures? */
    int nodigests;		/*!< Verify digests? */
    int nosignatures;		/*!< Verify signatures? */

/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * fn;		/*!< Current package fn. */
    int_32  sigtag;		/*!< Current package signature tag. */
    int_32  sigtype;		/*!< Current package signature data type. */
/*@null@*/ const void * sig;	/*!< Current package signature. */
    int_32 siglen;		/*!< Current package signature length. */
/*@null@*/
    struct pgpDig_s * dig;	/*!< Current signature/pubkey parametrs. */

/*@refs@*/ int nrefs;		/*!< Reference count. */

};
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return (malloc'd) header name-version-release string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
/*@only@*/ char * hGetNEVR(Header h, /*@out@*/ const char ** np )
	/*@modifies *np @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTS */
