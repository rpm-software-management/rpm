#ifndef H_RPMTS
#define H_RPMTS

/** \ingroup rpmts
 * \file lib/rpmts.h
 * Structures and prototypes used for an "rpmts" transaction set.
 */

#include "rpmps.h"

/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmts_debug;
/*@unchecked@*/
extern int _fps_debug;
/*@=exportlocal@*/

/**
 * Bit(s) to control digest and signature verification.
 */
typedef enum rpmVSFlags_e {
    RPMVSF_DEFAULT	= 0,
    RPMVSF_NOHDRCHK	= (1 <<  0),
    RPMVSF_NEEDPAYLOAD	= (1 <<  1),
    /* bit(s) 2-7 unused */
    RPMVSF_NOSHA1HEADER	= (1 <<  8),
    RPMVSF_NOMD5HEADER	= (1 <<  9),	/* unimplemented */
    RPMVSF_NODSAHEADER	= (1 << 10),
    RPMVSF_NORSAHEADER	= (1 << 11),	/* unimplemented */
    /* bit(s) 12-15 unused */
    RPMVSF_NOSHA1	= (1 << 16),	/* unimplemented */
    RPMVSF_NOMD5	= (1 << 17),
    RPMVSF_NODSA	= (1 << 18),
    RPMVSF_NORSA	= (1 << 19)
    /* bit(s) 16-31 unused */
} rpmVSFlags;

#define	_RPMVSF_NODIGESTS	\
  ( RPMVSF_NOSHA1HEADER |	\
    RPMVSF_NOMD5HEADER |	\
    RPMVSF_NOSHA1 |		\
    RPMVSF_NOMD5 )

#define	_RPMVSF_NOSIGNATURES	\
  ( RPMVSF_NODSAHEADER |	\
    RPMVSF_NORSAHEADER |	\
    RPMVSF_NODSA |		\
    RPMVSF_NORSA )

#define	_RPMVSF_NOHEADER	\
  ( RPMVSF_NOSHA1HEADER |	\
    RPMVSF_NOMD5HEADER |	\
    RPMVSF_NODSAHEADER |	\
    RPMVSF_NORSAHEADER )

#define	_RPMVSF_NOPAYLOAD	\
  ( RPMVSF_NOSHA1 |		\
    RPMVSF_NOMD5 |		\
    RPMVSF_NODSA |		\
    RPMVSF_NORSA )

#if defined(_RPMTS_INTERNAL)

#include "rpmhash.h"	/* XXX hashTable */
#include "rpmal.h"	/* XXX availablePackage/relocateFileList ,*/

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
    rpmtransFlags transFlags;	/*!< Bit(s) to control operation. */
    tsmStage goal;		/*!< Transaction goal (i.e. mode) */

/*@refcounted@*/ /*@null@*/
    rpmdb sdb;			/*!< Solve database handle. */
    int sdbmode;		/*!< Solve database open mode. */
/*@null@*/
    int (*solve) (rpmts ts, rpmds key, const void * data)
	/*@modifies ts @*/;	/*!< Search for NEVRA key. */
    const void * solveData;	/*!< Solve callback data */
    int nsuggests;		/*!< No. of depCheck suggestions. */
/*@only@*/ /*@null@*/
    const void ** suggests;	/*!< Possible depCheck suggestions. */

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

/*@refcounted@*/ /*@null@*/
    rpmdb rdb;			/*!< Install database handle. */
    int dbmode;			/*!< Install database open mode. */
/*@only@*/
    hashTable ht;		/*!< Fingerprint hash table. */

/*@only@*/ /*@null@*/
    int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;	/*!< No. removed package instances. */
    int allocedRemovedPackages;	/*!< Size of removed packages array. */

/*@only@*/
    rpmal addedPackages;	/*!< Set of packages being installed. */
    int numAddedPackages;	/*!< No. added package instances. */

#ifndef	DYING
/*@only@*/
    rpmal availablePackages;	/*!< Universe of available packages. */
    int numAvailablePackages;	/*!< No. available package instances. */
#endif

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

    rpmVSFlags vsflags;		/*!< Signature/digest verification flags. */

/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * fn;		/*!< Current package fn. */
    int_32  sigtag;		/*!< Current package signature tag. */
    int_32  sigtype;		/*!< Current package signature data type. */
/*@null@*/
    const void * sig;		/*!< Current package signature. */
    int_32 siglen;		/*!< Current package signature length. */

/*@only@*/ /*@null@*/
    const unsigned char * pkpkt;/*!< Current pubkey packet. */
    size_t pkpktlen;		/*!< Current pubkey packet length. */
    unsigned char pksignid[8];	/*!< Current pubkey fingerprint. */

/*@null@*/
    pgpDig dig;			/*!< Current signature/pubkey parameters. */

/*@refs@*/
    int nrefs;			/*!< Reference count. */

};
#endif	/* _RPMTS_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmts
 * Check that all dependencies can be resolved.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsCheck(rpmts ts)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Determine package order in a transaction set according to dependencies.
 *
 * Order packages, returning error if circular dependencies cannot be
 * eliminated by removing Requires's from the loop(s). Only dependencies from
 * added or removed packages are used to determine ordering using a
 * topological sort (Knuth vol. 1, p. 262). Use rpmtsCheck() to verify
 * that all dependencies can be resolved.
 *
 * The final order ends up as installed packages followed by removed packages,
 * with packages removed for upgrades immediately following the new package
 * to be installed.
 *
 * @param ts		transaction set
 * @return		no. of (added) packages that could not be ordered
 */
int rpmtsOrder(rpmts ts)
	/*@globals fileSystem, internalState@*/
	/*@modifies ts, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Process all package elements in a transaction set.
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
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Initialize the database used by the transaction.
 * @deprecated An explicit rpmdbInit() is almost never needed.
 * @param ts		transaction set
 * @param dbmode	O_RDONLY or O_RDWR
 * @return		0 on success
 */
int rpmtsInitDB(rpmts ts, int dbmode)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Rebuild the database used by the transaction.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsRebuildDB(rpmts ts)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Verify the database used by the transaction.
 * @param ts		transaction set
 * @return		0 on success
 */
int rpmtsVerifyDB(rpmts ts)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Return transaction database iterator.
 * @param ts		transaction set
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/
rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, rpmTag rpmtag,
			/*@null@*/ const void * keyp, size_t keylen)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Retrieve pubkey from rpm database.
 * @param ts		rpm transaction
 * @return		RPMSIG_OK on success, RPMSIG_NOKEY if not found
 */
rpmVerifySignatureReturn rpmtsFindPubkey(rpmts ts)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState */;

/** \ingroup rpmts
 * Close the database used by the transaction to solve dependencies.
 * @param ts		transaction set
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmtsCloseSDB(rpmts ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;
/*@=exportlocal@*/

/** \ingroup rpmts
 * Open the database used by the transaction to solve dependencies.
 * @param ts		transaction set
 * @param dbmode	O_RDONLY or O_RDWR
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmtsOpenSDB(rpmts ts, int dbmode)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Attempt to solve a needed dependency using the solve database.
 * @param ts		transaction set
 * @param ds		dependency set
 * @param data		opaque data associated with callback
 * @return		0 if resolved, 1 not found
 */
/*@-exportlocal@*/
int rpmtsSolve(rpmts ts, rpmds ds, const void * data)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Attempt to solve a needed dependency using memory resident tables.
 * @deprecated This function will move from rpmlib to the python bindings.
 * @param ts		transaction set
 * @param ds		dependency set
 * @return		0 if resolved (and added to ts), 1 not found
 */
/*@unused@*/
int rpmtsAvailable(rpmts ts, const rpmds ds)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/;

/**
 * Set dependency solver callback.
 * @param ts		transaction set
 * @param (*solve)	dependency solver callback
 * @param solveData	dependency solver callback data (opaque)
 * @return		0 on success
 */
int rpmtsSetSolveCallback(rpmts ts,
		int (*solve) (rpmts ts, rpmds ds, const void * data),
		const void * solveData)
	/*@modifies ts @*/;

/**
 * Return current transaction set problems.
 * @param ts		transaction set
 * @return		current problem set (or NULL)
 */
/*@null@*/
rpmps rpmtsProblems(rpmts ts)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Free signature verification data.
 * @param ts		transaction set
 */
void rpmtsCleanDig(rpmts ts)
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
 * Get verify signatures flag(s).
 * @param ts		transaction set
 * @return		verify signatures flags
 */
rpmVSFlags rpmtsVSFlags(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set verify signatures flag(s).
 * @param ts		transaction set
 * @param vsflags	new verify signatures flags
 * @return		previous value
 */
rpmVSFlags rpmtsSetVSFlags(rpmts ts, rpmVSFlags vsflags)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get transaction rootDir, i.e. path to chroot(2).
 * @param ts		transaction set
 * @return		transaction rootDir
 */
/*@observer@*/ /*@null@*/
extern const char * rpmtsRootDir(rpmts ts)
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
extern const char * rpmtsCurrDir(rpmts ts)
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
FD_t rpmtsScriptFd(rpmts ts)
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
int rpmtsChrootDone(rpmts ts)
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
 * Get signature tag.
 * @param ts		transaction set
 * @return		signature tag
 */
int_32 rpmtsSigtag(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get signature tag type.
 * @param ts		transaction set
 * @return		signature tag type
 */
int_32 rpmtsSigtype(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get signature tag data, i.e. from header.
 * @param ts		transaction set
 * @return		signature tag data
 */
/*@observer@*/ /*@null@*/
extern const void * rpmtsSig(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get signature tag data length, i.e. no. of bytes of data.
 * @param ts		transaction set
 * @return		signature tag data length
 */
int_32 rpmtsSiglen(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set signature tag info, i.e. from header.
 * @param ts		transaction set
 * @param sigtag	signature tag
 * @param sigtype	signature tag type
 * @param sig		signature tag data
 * @param siglen	signature tag data length
 * @return		0 always
 */
int rpmtsSetSig(rpmts ts,
		int_32 sigtag, int_32 sigtype,
		/*@kept@*/ /*@null@*/ const void * sig, int_32 siglen)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Get OpenPGP packet parameters, i.e. signature/pubkey constants.
 * @param ts		transaction set
 * @return		signature/pubkey constants.
 */
/*@exposed@*/ /*@null@*/
pgpDig rpmtsDig(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get OpenPGP signature constants.
 * @param ts		transaction set
 * @return		signature constants.
 */
/*@exposed@*/ /*@null@*/
pgpDigParams rpmtsSignature(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get OpenPGP pubkey constants.
 * @param ts		transaction set
 * @return		pubkey constants.
 */
/*@exposed@*/ /*@null@*/
pgpDigParams rpmtsPubkey(const rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get transaction set database handle.
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

/**
 * Perform transaction progress notify callback.
 * @param ts		transaction set
 * @param te		current transaction element
 * @param what		type of call back
 * @param amount	current value
 * @param total		final value
 * @return		callback dependent pointer
 */
/*@null@*/
void * rpmtsNotify(rpmts ts, rpmte te,
                rpmCallbackType what, unsigned long amount, unsigned long total)
	/*@*/;

/**
 * Return number of (ordered) transaction set elements.
 * @param ts		transaction set
 * @return		no. of transaction set elements
 */
int rpmtsNElements(rpmts ts)
	/*@*/;

/**
 * Return (ordered) transaction set element.
 * @param ts		transaction set
 * @param ix		transaction element index
 * @return		transaction element
 */
rpmte rpmtsElement(rpmts ts, int ix)
	/*@*/;

/** \ingroup rpmts
 * Get problem ignore bit mask, i.e. bits to filter encountered problems.
 * @param ts		transaction set
 * @return		ignore bit mask
 */
rpmprobFilterFlags rpmtsFilterFlags(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Get transaction flags, i.e. bits that control rpmtsRun().
 * @param ts		transaction set
 * @return		transaction flags
 */
rpmtransFlags rpmtsFlags(rpmts ts)
	/*@*/;

/** \ingroup rpmts
 * Set transaction flags, i.e. bits that control rpmtsRun().
 * @param ts		transaction set
 * @param transFlags	new transaction flags
 * @return		previous transaction flags
 */
rpmtransFlags rpmtsSetFlags(rpmts ts, rpmtransFlags transFlags)
	/*@modifies ts @*/;

/** \ingroup rpmts
 * Set transaction notify callback function and argument.
 *
 * @warning This call must be made before rpmtsRun() for
 *	install/upgrade/freshen to function correctly.
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
/*@newref@*/
rpmts rpmtsCreate(void)
	/*@*/;

/** \ingroup rpmts
 * Add package to be installed to transaction set.
 *
 * The transaction set is checked for duplicate package names.
 * If found, the package with the "newest" EVR will be replaced.
 *
 * @param ts		transaction set
 * @param h		header
 * @param key		package retrieval key (e.g. file name)
 * @param upgrade	is package being upgraded?
 * @param relocs	package file relocations
 * @return		0 on success, 1 on I/O error, 2 needs capabilities
 */
int rpmtsAddInstallElement(rpmts ts, Header h,
		/*@exposed@*/ /*@null@*/ const fnpyKey key, int upgrade,
		/*@null@*/ rpmRelocation * relocs)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmts
 * Add package to be erased to transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param dboffset	rpm database instance
 * @return		0 on success
 */
int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
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
/*@only@*/ char * hGetNEVR(Header h, /*@null@*/ /*@out@*/ const char ** np )
	/*@modifies *np @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTS */
