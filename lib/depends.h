#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>
#include <rpmhash.h>

typedef /*@abstract@*/ struct availablePackage_s *	availablePackage;
typedef /*@abstract@*/ struct availableIndexEntry_s *	availableIndexEntry;
typedef /*@abstract@*/ struct availableIndex_s *	availableIndex;
typedef /*@abstract@*/ struct fileIndexEntry_s *	fileIndexEntry;
typedef /*@abstract@*/ struct dirInfo_s *		dirInfo;
typedef /*@abstract@*/ struct availableList_s *		availableList;
typedef /*@abstract@*/ struct problemsSet_s *		problemsSet;
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;

/*@unchecked@*/
/*@-exportlocal@*/
extern int _ts_debug;
/*@=exportlocal@*/

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo {
    union {
	int	count;
	/*@kept@*/ /*@null@*/ availablePackage suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/ struct tsortInfo * tsi_next;
/*@kept@*/ /*@null@*/ availablePackage tsi_pkg;
    int		tsi_reqx;
    int		tsi_qcnt;
} ;
/*@=fielduse@*/

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
    Header h;				/*!< Package header. */
/*@dependent@*/ const char * name;	/*!< Header name. */
/*@dependent@*/ const char * version;	/*!< Header version. */
/*@dependent@*/ const char * release;	/*!< Header release. */
/*@owned@*/ const char ** provides;	/*!< Provides: name strings. */
/*@owned@*/ const char ** providesEVR;	/*!< Provides: [epoch:]version[-release] strings. */
/*@dependent@*/ int * provideFlags;	/*!< Provides: logical range qualifiers. */
/*@owned@*//*@null@*/ const char ** requires;	/*!< Requires: name strings. */
/*@owned@*//*@null@*/ const char ** requiresEVR;/*!< Requires: [epoch:]version[-release] strings. */
/*@dependent@*//*@null@*/ int * requireFlags;	/*!< Requires: logical range qualifiers. */
/*@owned@*//*@null@*/ const char ** baseNames;	/*!< Header file basenames. */
/*@dependent@*//*@null@*/ int_32 * epoch;	/*!< Header epoch (if any). */
    int providesCount;			/*!< No. of Provide:'s in header. */
    int requiresCount;			/*!< No. of Require:'s in header. */
    int filesCount;			/*!< No. of files in header. */
    int npreds;				/*!< No. of predecessors. */
    int depth;				/*!< Max. depth in dependency tree. */
    struct tsortInfo tsi;		/*!< Dependency tsort data. */
    uint_32 multiLib;	/* MULTILIB */
/*@kept@*//*@null@*/ const void * key;	/*!< Private data associated with a package (e.g. file name of package). */
/*@null@*/ rpmRelocation * relocs;
/*@null@*/ FD_t fd;
} ;

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
struct availableIndexEntry_s {
/*@dependent@*/ availablePackage package; /*!< Containing package. */
/*@dependent@*/ const char * entry;	/*!< Available item name. */
    size_t entryLen;			/*!< No. of bytes in name. */
    enum indexEntryType {
	IET_PROVIDES=1		/*!< A Provides: dependency. */
    } type;				/*!< Type of available item. */
} ;

/** \ingroup rpmdep
 * Index of all available items.
 */
struct availableIndex_s {
/*@null@*/ availableIndexEntry index;	/*!< Array of available items. */
    int size;				/*!< No. of available items. */
} ;

/** \ingroup rpmdep
 * A file to be installed/removed.
 */
struct fileIndexEntry_s {
    int pkgNum;				/*!< Containing package number. */
    int fileFlags;	/* MULTILIB */
/*@dependent@*/ /*@null@*/ const char * baseName;	/*!< File basename. */
} ;

/** \ingroup rpmdep
 * A directory to be installed/removed.
 */
struct dirInfo_s {
/*@owned@*/ const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;			/*!< No. bytes in directory path. */
/*@owned@*/ fileIndexEntry files;	/*!< Array of files in directory. */
    int numFiles;			/*!< No. files in directory. */
} ;

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct availableList_s {
/*@owned@*/ /*@null@*/ availablePackage list;	/*!< Set of packages. */
    struct availableIndex_s index;	/*!< Set of available items. */
    int delta;				/*!< Delta for pkg list reallocation. */
    int size;				/*!< No. of pkgs in list. */
    int alloced;			/*!< No. of pkgs allocated for list. */
    int numDirs;			/*!< No. of directories. */
/*@owned@*/ /*@null@*/ dirInfo dirs;	/*!< Set of directories. */
} ;

struct orderListIndex_s {
    int alIndex;
    int orIndex;
};


/** \ingroup rpmdep
 * A single package instance to be installed/removed atomically.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct transactionElement {
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
    struct availableList_s addedPackages;
				/*!< Set of packages being installed. */
    struct availableList_s availablePackages;
				/*!< Universe of possible packages. */
/*@only@*/ struct transactionElement * order;
				/*!< Packages sorted by dependencies. */
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
