#ifndef H_DEPENDS
#define H_DEPENDS

/** \file lib/depends.h
 *
 */

#include <header.h>

/**
 * Dependncy ordering information.
 */
struct tsortInfo {
    union {
	int	count;
	/*@dependent@*/ struct availablePackage * suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ struct tsortInfo *tsi_next;
/*@dependent@*/ struct availablePackage * tsi_pkg;
    int		tsi_reqx;
};

/**
 * Info about a single package to be installed/removed.
 */
struct availablePackage {
    Header h;				/*!< Package header. */
/*@dependent@*/ const char * name;	/*!< Header name. */
/*@dependent@*/ const char * version;	/*!< Header version. */
/*@dependent@*/ const char * release;	/*!< Header release. */
/*@owned@*/ const char ** provides;	/*!< Provides: name strings. */
/*@owned@*/ const char ** providesEVR;	/*!< Provides: [epoch:]version[-release] strings. */
/*@dependent@*/ int * provideFlags;	/*!< Provides: logical range qualifiers. */
/*@owned@*/ const char ** requires;	/*!< Requires: name strings. */
/*@owned@*/ const char ** requiresEVR;	/*!< Requires: [epoch:]version[-release] strings. */
/*@dependent@*/ int * requireFlags;	/*!< Requires: logical range qualifiers. */
/*@owned@*/ const char ** baseNames;	/*!< Header file basenames. */
/*@dependent@*/ int_32 * epoch;		/*!< Header epoch (if any). */
    int providesCount;			/*!< No. of Provide:'s in header. */
    int requiresCount;			/*!< No. of Require:'s in header. */
    int filesCount;			/*!< No. of files in header. */
    struct tsortInfo tsi;		/*!< Dependency tsort data. */
    uint_32 multiLib;	/* MULTILIB */
/*@dependent@*/ const void * key;	/*!< Private data associated with a package (e.g. file name of package). */
    rpmRelocation * relocs;
/*@null@*/ FD_t fd;
} ;

/**
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

/**
 * Index of all available items.
 */
struct availableIndex {
/*@null@*/ struct availableIndexEntry * index; /*!< Array of available items. */
    int size;				/*!< No. of available items. */
} ;

/**
 * A file to be installed/removed.
 */
struct fileIndexEntry {
    int pkgNum;				/*!< Containing package number. */
    int fileFlags;	/* MULTILIB */
/*@dependent@*/ const char * baseName;	/*!< File basename. */
} ;

/**
 * A directory to be installed/removed.
 */
struct dirInfo {
/*@owned@*/ const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;			/*!< No. bytes in directory path. */
/*@owned@*/ struct fileIndexEntry * files; /*!< Array of files in directory. */
    int numFiles;			/*!< No. files in directory. */
} ;

/**
 * Set of available packages, items, and directories.
 */
struct availableList {
/*@owned@*/ /*@null@*/ struct availablePackage * list;	/*!< Set of packages. */
    struct availableIndex index;	/*!< Set of available items. */
    int size;
    int alloced;
    int numDirs;			/*! No. of directories. */
/*@owned@*/ struct dirInfo * dirs;	/*!< Set of directories. */
};

/**
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
};

/**
 * The set of packages to be installed/removed atomically.
 */
struct rpmTransactionSet_s {
/*@owned@*/ /*@null@*/ rpmdb rpmdb;	/*!< Database handle. */
/*@only@*/ int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;		/*!< No. removed rpmdb instances. */
    int allocedRemovedPackages;		/*!< Size of removed packages array. */
    struct availableList addedPackages;	/*!< Set of packages being installed. */
    struct availableList availablePackages; /*!< Universe of possible packages to install. */
/*@only@*/ struct transactionElement * order; /*!< Packages sorted by dependencies. */
    int orderCount;
    int orderAlloced;
/*@only@*/ const char * rootDir;	/*!< Path to top of install tree. */
/*@only@*/ const char * currDir;	/*!< Current working directory. */
/*@null@*/ FD_t scriptFd;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};

#ifdef __cplusplus
extern "C" {
#endif

/* XXX lib/uninstall.c */
int headerMatchesDepFlags(Header h, const char *reqName, const char * reqInfo, int reqFlags);

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
