#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
struct tsortInfo {
    union {
	int	count;
	/*@kept@*/ /*@null@*/ struct availablePackage * suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/ struct tsortInfo * tsi_next;
/*@kept@*/ /*@null@*/ struct availablePackage * tsi_pkg;
    int		tsi_reqx;
    int		tsi_qcnt;
} ;

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage {
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
struct availableIndexEntry {
/*@dependent@*/ struct availablePackage * package; /*!< Containing package. */
/*@dependent@*/ const char * entry;	/*!< Available item name. */
    size_t entryLen;			/*!< No. of bytes in name. */
    enum indexEntryType {
	IET_PROVIDES=1		/*!< A Provides: dependency. */
    } type;				/*!< Type of available item. */
} ;

/** \ingroup rpmdep
 * Index of all available items.
 */
struct availableIndex {
/*@null@*/ struct availableIndexEntry * index; /*!< Array of available items. */
    int size;				/*!< No. of available items. */
} ;

/** \ingroup rpmdep
 * A file to be installed/removed.
 */
struct fileIndexEntry {
    int pkgNum;				/*!< Containing package number. */
    int fileFlags;	/* MULTILIB */
/*@dependent@*/ /*@null@*/ const char * baseName;	/*!< File basename. */
} ;

/** \ingroup rpmdep
 * A directory to be installed/removed.
 */
typedef struct dirInfo_s {
/*@owned@*/ const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;			/*!< No. bytes in directory path. */
/*@owned@*/ struct fileIndexEntry * files; /*!< Array of files in directory. */
    int numFiles;			/*!< No. files in directory. */
} * dirInfo ;

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
typedef /*@abstract@*/ struct availableList_s {
/*@owned@*/ /*@null@*/ struct availablePackage * list;	/*!< Set of packages. */
    struct availableIndex index;	/*!< Set of available items. */
    int delta;				/*!< Delta for pkg list reallocation. */
    int size;				/*!< No. of pkgs in list. */
    int alloced;			/*!< No. of pkgs allocated for list. */
    int numDirs;			/*!< No. of directories. */
/*@owned@*/ /*@null@*/ dirInfo dirs;	/*!< Set of directories. */
} * availableList;

/** \ingroup rpmdep
 * A single package instance to be installed/removed atomically.
 */
struct transactionElement {
    enum rpmTransactionType {
	TR_ADDED,	/*!< Package will be installed. */
	TR_REMOVED	/*!< Package will be removed. */
    } type;		/*!< Package disposition (installed/removed). */
    union { 
	int addedIndex;
	struct {
	    int dboffset;
	    int dependsOnIndex;
	} removed;
    } u;
} ;

/** \ingroup rpmdep
 * The set of packages to be installed/removed atomically.
 */
struct rpmTransactionSet_s {
    rpmtransFlags transFlags;		/*!< Bit(s) to control operation. */
/*@null@*/ rpmCallbackFunction notify;	/*!< Callback function. */
/*@observer@*/ /*@null@*/ rpmCallbackData notifyData;
					/*!< Callback private data. */
/*@dependent@*/ rpmProblemSet probs;	/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;	/*!< Bits to filter current problems. */
    int filesystemCount;		/*!< No. of mounted filesystems. */
/*@dependent@*/ const char ** filesystems; /*!< Mounted filesystem names. */
/*@only@*/ struct diskspaceInfo * di;	/*!< Per filesystem disk/inode usage. */
/*@kept@*/ /*@null@*/ rpmdb rpmdb;	/*!< Database handle. */
/*@only@*/ int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;		/*!< No. removed rpmdb instances. */
    int allocedRemovedPackages;		/*!< Size of removed packages array. */
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
    int id;			/*!< Transaction id. */
} ;

/** \ingroup rpmdep
 * Problems encountered while checking dependencies.
 */
typedef /*@abstract@*/ struct problemsSet_s {
    rpmDependencyConflict problems;	/*!< Problems encountered. */
    int num;			/*!< No. of problems found. */
    int alloced;		/*!< No. of problems allocated. */
} * problemsSet;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmdep
 * Compare package name-version-release from header with dependency, looking
 * for overlap.
 * @deprecated Remove from API when obsoletes is correctly eliminated.
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
