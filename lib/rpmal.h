#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

typedef /*@abstract@*/ struct fileIndexEntry_s *	fileIndexEntry;
typedef /*@abstract@*/ struct dirInfo_s *		dirInfo;
typedef /*@abstract@*/ struct availableIndexEntry_s *	availableIndexEntry;
typedef /*@abstract@*/ struct availableIndex_s *	availableIndex;

typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo_s {
    union {
	int	count;
	/*@kept@*/ /*@null@*/ availablePackage suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/
    struct tsortInfo_s * tsi_next;
/*@kept@*/ /*@null@*/
    availablePackage tsi_pkg;
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
    struct tsortInfo_s tsi;		/*!< Dependency tsort data. */
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize available packckages, items, and directory list.
 * @param delta		no. of entries to add on each realloc
 * @return al		new available list
 */
/*@only@*/
availableList alCreate(int delta)
	/*@*/;

/**
 * Free available packages, items, and directory members.
 * @param al		available list
 * @return		NULL always
 */
/*@null@*/
availableList alFree(/*@only@*/ /*@null@*/ availableList al)
	/*@modifies al @*/;

/**
 * Delete package from available list.
 * @param al		available list
 * @param pkgNnum	package index
 */
/*@-exportlocal@*/
void alDelPackage(availableList al, int pkgNum)
	/*@modifies al @*/;
/*@=exportlocal@*/

/**
 * Add package to available list.
 * @param al		available list
 * @param pkgNnum	package index, < 0 to force an append
 * @param h		package header
 * @param key		package private data
 * @param fd		package file handle
 * @param relocs	package file relocations
 * @return		available package pointer
 */
/*@exposed@*/
availablePackage alAddPackage(availableList al, int pkgNum,
		Header h, /*@null@*/ /*@dependent@*/ const void * key,
		/*@null@*/ FD_t fd, /*@null@*/ rpmRelocation * relocs)
	/*@modifies al, h @*/;

/**
 * Generate index for available list.
 * @param al		available list
 */
void alMakeIndex(availableList al)
	/*@modifies al @*/;

/**
 * Check added package file lists for package(s) that provide a file.
 * @param al		available list
 * @param keyType	type of dependency
 * @param fileName	file name to search for
 * @return		available package pointer
 */
/*@-exportlocal@*/
/*@only@*/ /*@null@*/
availablePackage * alAllFileSatisfiesDepend(const availableList al,
		const char * keyType, const char * fileName)
	/*@*/;
/*@=exportlocal@*/

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		available package pointer
 */
/*@only@*/ /*@null@*/
availablePackage * alAllSatisfiesDepend(const availableList al,
		const char * keyType, const char * keyDepend,
		const char * keyName, const char * keyEVR, int keyFlags)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
