#ifndef H_RPMTS
#define H_RPMTS

/** \ingroup rpmts
 * \file lib/rpmts.h
 * Structures and prototypes used for an "rpmts" transaction set.
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

/** \ingroup rpmts
 */
typedef	/*@abstract@*/ struct diskspaceInfo_s * rpmDiskSpaceInfo;

/** \ingroup rpmts
 */
struct diskspaceInfo_s {
    dev_t dev;			/*!< File system device number. */
    signed long bneeded;	/*!< No. of blocks needed. */
    signed long ineeded;	/*!< No. of inodes needed. */
    int bsize;			/*!< File system block size. */
    signed long bavail;		/*!< No. of blocks available. */
    signed long iavail;		/*!< No. of inodes available. */
};

/** \ingroup rpmts
 * Adjust for root only reserved space. On linux e2fs, this is 5%.
 */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

/** \ingroup rpmts
 */
typedef enum tsStage_e {
    TSM_UNKNOWN		=  0,
    TSM_INSTALL		=  7,
    TSM_ERASE		=  8,
} tsmStage;

/** \ingroup rpmts
 * The set of packages to be installed/removed atomically.
 */
struct rpmts_s {
    rpmtsFlags transFlags;	/*!< Bit(s) to control operation. */
    tsmStage goal;		/*!< Transaction goal (i.e. mode) */

/*@null@*/
    int (*solve) (rpmts ts, const rpmds key)
	/*@modifies ts @*/;	/*!< Search for NEVRA key. */
    int nsuggests;		/*!< No. of depCheck suggestions. */
/*@only@*/ /*@null@*/
    const void ** suggests;	/*!< Possible depCheck suggestions. */
/*@refcounted@*/ /*@null@*/
    rpmdb sdb;			/*!< Solve database handle. */

/*@observer@*/ /*@null@*/
    rpmCallbackFunction notify;	/*!< Callback function. */
/*@observer@*/ /*@null@*/
    rpmCallbackData notifyData;	/*!< Callback private data. */

/*@refcounted@*/ /*@null@*/
    rpmps probs;		/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    int filesystemCount;	/*!< No. of mounted filesystems. */
/*@dependent@*/ /*@null@*/
    const char ** filesystems;	/*!< Mounted filesystem names. */
/*@only@*/ /*@null@*/
    rpmDiskSpaceInfo dsi;	/*!< Per filesystem disk/inode usage. */

    int dbmode;			/*!< Database open mode. */
/*@refcounted@*/ /*@null@*/
    rpmdb rdb;			/*!< Install database handle. */
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
    rpmte * order;		/*!< Packages sorted by dependencies. */
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
    int_32 tid;			/*!< Transaction id. */

    int verify_legacy;		/*!< Verify legacy signatures? */
    int nodigests;		/*!< Verify digests? */
    int nosignatures;		/*!< Verify signatures? */
    int vsflags;		/*!< Signature verification flags. */
#define	_RPMTS_VSF_NODIGESTS		(1 << 0)
#define	_RPMTS_VSF_NOSIGNATURES		(1 << 1)
#define	_RPMTS_VSF_VERIFY_LEGACY	(1 << 2)

/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * fn;		/*!< Current package fn. */
    int_32  sigtag;		/*!< Current package signature tag. */
    int_32  sigtype;		/*!< Current package signature data type. */
/*@null@*/ const void * sig;	/*!< Current package signature. */
    int_32 siglen;		/*!< Current package signature length. */

/*@only@*/ /*@null@*/
    const unsigned char * pkpkt;/*!< Current pubkey packet. */
    size_t pkpktlen;		/*!< Current pubkey packet length. */
    unsigned char pksignid[8];	/*!< Current pubkey fingerprint. */

/*@null@*/
    struct pgpDig_s * dig;	/*!< Current signature/pubkey parametrs. */

/*@refs@*/ int nrefs;		/*!< Reference count. */

};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmts
 * Check that all dependencies can be resolved.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsCheck(rpmts ts)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Determine package order in a transaction set according to dependencies.
 *
 * Order packages, returning error if circular dependencies cannot be
 * eliminated by removing PreReq's from the loop(s). Only dependencies from
 * added or removed packages are used to determine ordering using a
 * topological sort (Knuth vol. 1, p. 262). Use rpmtsCheck() to verify
 * that all dependencies can be resolved.
 *
 * The final order ends up as installed packages followed by removed packages,
 * with packages removed for upgrades immediately following the new package
 * to be installed.
 *
 * The operation would be easier if we could sort the addedPackages array in the
 * transaction set, but we store indexes into the array in various places.
 *
 * @param ts		transaction set
 * @return		no. of (added) packages that could not be ordered
 */
int rpmtsOrder(rpmts ts)
	/*@globals fileSystem, internalState@*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Process all packages in a transaction set.
 *
 * @param ts		transaction set
 * @param okProbs	previously known problems (or NULL)
 * @param ignoreSet	bits to filter problem types
 * @return		0 on success, -1 on error, >0 with newProbs set
 */
int rpmtsRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState@*/
	/*@modifies ts, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/** \ingroup rpmts
 * Unreference a transaction instance.
 * @param ts		transaction set
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmts rpmtsUnlink (/*@killref@*/ /*@only@*/ rpmts ts,
		const char * msg)
	/*@modifies ts @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
rpmts XrpmtsUnlink (/*@killref@*/ /*@only@*/ rpmts ts,
		const char * msg, const char * fn, unsigned ln)
	/*@modifies ts @*/;
#define	rpmtsUnlink(_ts, _msg)	XrpmtsUnlink(_ts, _msg, __FILE__, __LINE__)

/** \ingroup rpmts
 * Reference a transaction set instance.
 * @param ts		transaction set
 * @param msg
 * @return		new transaction set reference
 */
/*@unused@*/
rpmts rpmtsLink (rpmts ts, const char * msg)
	/*@modifies ts @*/;

/** @todo Remove debugging entry from the ABI. */
rpmts XrpmtsLink (rpmts ts,
		const char * msg, const char * fn, unsigned ln)
        /*@modifies ts @*/;
#define	rpmtsLink(_ts, _msg)	XrpmtsLink(_ts, _msg, __FILE__, __LINE__)

/** \ingroup rpmts
 * Close the database used by the transaction.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsCloseDB(rpmts ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/** \ingroup rpmts
 * Open the database used by the transaction.
 * @param ts		transaction set
 * @param dbmode	O_RDONLY or O_RDWR
 * @return		0 on success
 */
int rpmtsOpenDB(rpmts ts, int dbmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Return transaction database iterator.
 * @param ts		transaction set
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/
rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, int rpmtag,
			/*@null@*/ const void * keyp, size_t keylen)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/**
 * Attempt to solve a needed dependency.
 * @param ts		transaction set
 * @param ds		dependency set
 * @return		0 if resolved (and added to ts), 1 not found
 */
/*@-exportlocal@*/
int rpmtsSolve(rpmts ts, rpmds ds)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Attempt to solve a needed dependency.
 * @param ts		transaction set
 * @param ds		dependency set
 * @return		0 if resolved (and added to ts), 1 not found
 */
/*@unused@*/
int rpmtsAvailable(rpmts ts, const rpmds ds)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/**
 * Return (and clear) current transaction set problems.
 * @param ts		transaction set
 * @return		current problem set (or NULL)
 */
/*@null@*/
rpmps rpmtsGetProblems(rpmts ts)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Re-create an empty transaction set.
 * @param ts		transaction set
 */
void rpmtsClean(rpmts ts)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Destroy transaction set, closing the database as well.
 * @param ts		transaction set
 * @return		NULL always
 */
/*@null@*/
rpmts rpmtsFree(/*@killref@*/ /*@only@*//*@null@*/ rpmts ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/** \ingroup rpmts
 * Set verify signatures flag(s).
 * @param ts		transaction set
 * @param vsflags	new verify signatures flags
 * @retrun		previous value
 */
int rpmtsSetVerifySigFlags(rpmts ts, int vsflags)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction rootDir, i.e. path to chroot(2).
 * @param ts		transaction set
 * @return		transaction rootDir
 */
/*@observer@*/ /*@null@*/
const char * rpmtsGetRootDir(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction rootDir, i.e. path to chroot(2).
 * @param ts		transaction set
 * @param rootDir	new transaction rootDir (or NULL)
 */
void rpmtsSetRootDir(rpmts ts, /*@null@*/ const char * rootDir)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction currDir, i.e. current directory before chroot(2).
 * @param ts		transaction set
 * @return		transaction currDir
 */
/*@observer@*/ /*@null@*/
const char * rpmtsGetCurrDir(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction currDir, i.e. current directory before chroot(2).
 * @param ts		transaction set
 * @param currDir	new transaction currDir (or NULL)
 */
void rpmtsSetCurrDir(rpmts ts, /*@null@*/ const char * currDir)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction script file handle, i.e. stdout/stderr on scriptlet execution
 * @param ts		transaction set
 * @return		transaction script file handle
 */
/*@null@*/
FD_t rpmtsGetScriptFd(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction script file handle, i.e. stdout/stderr on scriptlet execution
 * @param ts		transaction set
 * @param scriptFd	new script file handle (or NULL)
 */
void rpmtsSetScriptFd(rpmts ts, /*@null@*/ FD_t scriptFd)
	/*@modifies ts, scriptFd @*/;

/** \ingroup rpmts
 * Get chrootDone flag, i.e. has chroot(2) been performed?
 * @param ts		transaction set
 * @return		chrootDone flag
 */
int rpmtsGetChrootDone(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set chrootDone flag, i.e. has chroot(2) been performed?
 * @param ts		transaction set
 * @param chrootDone	new chrootDone flag
 * @return		previous chrootDone flag
 */
int rpmtsSetChrootDone(rpmts ts, int chrootDone)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction id, i.e. transaction time stamp.
 * @param ts		transaction set
 * @return		chrootDone flag
 */
int_32 rpmtsGetTid(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction id, i.e. transaction time stamp.
 * @param ts		transaction set
 * @param tid		new transaction id
 * @return		previous transaction id
 */
int_32 rpmtsSetTid(rpmts ts, int_32 tid)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction database handle.
 * @param ts		transaction set
 * @return		transaction database handle
 */
/*@null@*/
rpmdb rpmtsGetRdb(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Initialize disk space info for each and every mounted file systems.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsInitDSI(const rpmts ts)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Update disk space info for a file.
 * @param ts		transaction set
 * @param dev		mount point device
 * @param fileSize	file size
 * @param prevSize	previous file size (if upgrading)
 * @param fixupSize	size difference (if
 * @param action	file disposition
 */
void rpmtsUpdateDSI(const rpmts ts, dev_t dev,
		uint_32 fileSize, uint_32 prevSize, uint_32 fixupSize,
		fileAction action)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Check a transaction element for disk space problems.
 * @param ts		transaction set
 * @param te		current transaction element
 */
void rpmtsCheckDSIProblems(const rpmts ts, const rpmte te)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction flags, i.e. bits that control rpmtsRun().
 * @param ts		transaction set
 * @return		transaction flags
 */
rpmtsFlags rpmtsGetFlags(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction flags, i.e. bits that control rpmtsRun().
 * @param ts		transaction set
 * @param transFlags	new transaction flags
 * @return		previous transaction flags
 */
rpmtsFlags rpmtsSetFlags(rpmts ts, rpmtsFlags transFlags)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Set transaction notify callback function and argument.
 *
 * @warning This call must be made before rpmtsRun() for
 *	install/upgrade/freshen to "work".
 *
 * @param ts		transaction set
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @return		0 on success
 */
int rpmtsSetNotifyCallback(rpmts ts,
		/*@observer@*/ rpmCallbackFunction notify,
		/*@observer@*/ rpmCallbackData notifyData)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Create an empty transaction set.
 * @return		new transaction set
 */
/*@only@*/
rpmts rpmtsCreate(void)
	/*@*/;

/** \ingroup rpmts
 * Add package to be installed to transaction set.
 *
 * If fd is NULL, the callback set by rpmtsSetNotifyCallback() is used to
 * open and close the file descriptor. If Header is NULL, the fd is always
 * used, otherwise fd is only needed (and only opened) for actual package 
 * installation.
 *
 * @warning The fd argument has been eliminated, and is assumed always NULL.
 *
 * @param ts		transaction set
 * @param h		header
 * @param key		package retrieval key (e.g. file name)
 * @param upgrade	is package being upgraded?
 * @param relocs	package file relocations
 * @return		0 on success, 1 on I/O error, 2 needs capabilities
 */
int rpmtsAddPackage(rpmts ts, Header h,
		/*@exposed@*/ /*@null@*/ const fnpyKey key, int upgrade,
		/*@null@*/ rpmRelocation * relocs)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, h, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Add package to universe of possible packages to install in transaction set.
 * @warning The key parameter is non-functional.
 * @param ts		transaction set
 * @param h		header
 * @param key		package private data
 */
/*@unused@*/
void rpmtsAvailablePackage(rpmts ts, Header h,
		/*@exposed@*/ /*@null@*/ fnpyKey key)
	/*@modifies h, ts @*/;

/** \ingroup rpmts
 * Add package to be erased to transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param dboffset	rpm database instance
 * @return		0 on success
 */
int rpmtsRemovePackage(rpmts ts, Header h, int dboffset)
	/*@modifies ts, h @*/;

/** \ingroup rpmts
 * Retrieve keys from ordered transaction set.
 * @todo Removed packages have no keys, returned as interleaved NULL pointers.
 * @param ts		transaction set
 * @retval ep		address of returned element array pointer (or NULL)
 * @retval nep		address of no. of returned elements (or NULL)
 * @return		0 always
 */
/*@unused@*/
int rpmtsGetKeys(rpmts ts,
		/*@null@*/ /*@out@*/ fnpyKey ** ep,
		/*@null@*/ /*@out@*/ int * nep)
	/*@modifies ts, ep, nep @*/;

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
