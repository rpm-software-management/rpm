#ifndef H_DEPENDS
#define H_DEPENDS

#include <header.h>

struct availablePackage {
    Header h;
    const char ** provides;
    const char ** providesEVR;
    /*@dependent@*/ int * provideFlags;
    const char ** baseFileNames;
    /*@dependent@*/ const char * name;
    /*@dependent@*/ const char * version;
    /*@dependent@*/ const char * release;
    /*@dependent@*/ int_32 *epoch;
    int providesCount, filesCount;
    /*@dependent@*/ const void * key;
    rpmRelocation * relocs;
    /*@dependent@*/ FD_t fd;
} ;

enum indexEntryType { IET_NAME, IET_PROVIDES };

struct availableIndexEntry {
    /*@dependent@*/ struct availablePackage * package;
    /*@dependent@*/ const char * entry;
    enum indexEntryType type;
} ;

struct fileIndexEntry {
    struct availablePackage * package;
    const char * basename;
} ;

struct dirInfo {
    char * dirName;			/* strdup'd */
    int dirNum;
    struct fileIndexEntry * files;	/* malloc'd */
    int numFiles;
} ;

struct availableIndex {
    /*@null@*/ struct availableIndexEntry * index ;
    int size;
} ;

struct availableList {
    /*@owned@*/ /*@null@*/ struct availablePackage * list;
    struct availableIndex index;
    int size, alloced;
    int numDirs;
    struct dirInfo * dirs;		/* malloc'd */
};

struct transactionElement {
    enum rpmTransactionType { TR_ADDED, TR_REMOVED } type;
    union { 
	int addedIndex;
	struct {
	    int dboffset;
	    int dependsOnIndex;
	} removed;
    } u;
};

struct rpmTransactionSet_s {
    /*@owned@*/rpmdb db;			/* may be NULL */
    /*@only@*/int * removedPackages;
    int numRemovedPackages, allocedRemovedPackages;
    struct availableList addedPackages, availablePackages;
    /*@only@*/struct transactionElement * order;
    int orderCount, orderAlloced;
    /*@only@*/const char * root;
    FD_t scriptFd;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};

#ifdef __cplusplus
extern "C" {
#endif

int headerMatchesDepFlags(Header h, const char *reqName, const char * reqInfo, int reqFlags);

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
