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
/*@owned@*/ struct tsortInfo * tsi_next;
/*@dependent@*/ struct availablePackage * tsi_pkg;
    int		tsi_reqx;
    int		tsi_qcnt;
} ;

/**
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
    int delta;				/*!< Delta for pkg list reallocation. */
    int size;				/*!< No. of pkgs in list. */
    int alloced;			/*!< No. of pkgs allocated for list. */
    int numDirs;			/*!< No. of directories. */
/*@owned@*/ struct dirInfo * dirs;	/*!< Set of directories. */
} ;

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
} ;

/**
 */
typedef int (*HGE_t) (Header h, int_32 tag, /*@out@*/ int_32 * type,
			/*@out@*/ void ** p, /*@out@*/int_32 * c)
				/*@modifies *type, *p, *c @*/;

/**
 */
struct transactionFileInfo_s {
  /* for all packages */
    enum rpmTransactionType type;
    enum fileActions * actions;	/*!< file disposition */
/*@dependent@*/ struct fingerPrint_s * fps; /*!< file fingerprints */
    HGE_t hge;
    Header h;			/*!< Package header */
    const uint_32 * fflags;	/*!< File flags (from header) */
    const uint_32 * fsizes;	/*!< File sizes (from header) */
    const char ** bnl;		/*!< Base names (from header) */
    const char ** dnl;		/*!< Directory names (from header) */
    const int * dil;		/*!< Directory indices (from header) */
    const char ** obnl;		/*!< Original Base names (from header) */
    const char ** odnl;		/*!< Original Directory names (from header) */
    const int * odil;		/*!< Original Directory indices (from header) */
    const char ** fmd5s;	/*!< file MD5 sums (from header) */
    const char ** flinks;	/*!< file links (from header) */
/* XXX setuid/setgid bits are turned off if fsuer/fgroup doesn't map. */
    uint_16 * fmodes;		/*!< file modes (from header) */
    char * fstates;		/*!< file states (from header) */
    const char ** fuser;	/*!< file owner(s) */
    const char ** fgroup;	/*!< file group(s) */
    const char ** flangs;	/*!< file lang(s) */
    int fc;			/*!< No. of files. */
    int dc;			/*!< No. of directories. */
    int bnlmax;			/*!< Length (in bytes) of longest base name. */
    int dnlmax;			/*!< Length (in bytes) of longest dir name. */
    int mapflags;
    int striplen;
    const char ** apath;
    uid_t uid;
    gid_t gid;
    uid_t * fuids;
    gid_t * fgids;
    int * fmapflags;
    int magic;
#define	TFIMAGIC	0x09697923
  /* these are for TR_ADDED packages */
    struct availablePackage * ap;
    struct sharedFileInfo * replaced;
    uint_32 * replacedSizes;
  /* for TR_REMOVED packages */
    unsigned int record;
} ;

/**
 * The set of packages to be installed/removed atomically.
 */
struct rpmTransactionSet_s {
    rpmtransFlags transFlags;		/*!< Bit(s) to control operation. */
    rpmCallbackFunction notify;		/*!< Callback function. */
/*@observer@*/ rpmCallbackData notifyData;/*!< Callback private data. */
/*@dependent@*/ rpmProblemSet probs;	/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;	/*!< Bits to filter current problems. */
/*@owned@*/ /*@null@*/ rpmdb rpmdb;	/*!< Database handle. */
/*@only@*/ int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;		/*!< No. removed rpmdb instances. */
    int allocedRemovedPackages;		/*!< Size of removed packages array. */
    struct availableList addedPackages;/*!< Set of packages being installed. */
    struct availableList availablePackages;
				/*!< Universe of possible packages. */
/*@only@*/ struct transactionElement * order;
				/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */
    int chrootDone;		/*!< Has chroot(2) been been done? */
/*@only@*/ const char * rootDir;/*!< Path to top of install tree. */
/*@only@*/ const char * currDir;/*!< Current working directory. */
/*@null@*/ FD_t scriptFd;	/*!< Scriptlet stdout/stderr. */
    int delta;			/*!< Delta for reallocation. */
    int id;			/*!< Transaction id. */
} ;

/**
 * Problems encountered while checking dependencies.
 */
struct problemsSet {
    struct rpmDependencyConflict * problems;	/*!< Problems encountered. */
    int num;			/*!< No. of problems found. */
    int alloced;		/*!< No. of problems allocated. */
} ;

#ifdef __cplusplus
extern "C" {
#endif

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPMULTILIB)

/**
 */
void loadFi(Header h, struct transactionFileInfo_s * fi);

/**
 */
void freeFi(struct transactionFileInfo_s * fi);

/* XXX lib/scriptlet.c */
/**
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
	const char *reqName, const char * reqEVR, int reqFlags);

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
