#ifndef H_DEPENDS
#define H_DEPENDS

#include <header.h>

struct availablePackage {
    Header h;
    const char ** provides;
    const char ** providesEVR;	/* unused */
    int * providesFlags;		/* unused */
    const char ** files;
    const char * name, * version, * release;
    int_32 *epoch;
    int providesCount, filesCount;
    /*@owned@*/ const void * key;
    rpmRelocation * relocs;
    /*@owned@*/ FD_t fd;
} ;

enum indexEntryType { IET_NAME, IET_PROVIDES, IET_FILE };

struct availableIndexEntry {
    struct availablePackage * package;
    const char * entry;
    enum indexEntryType type;
} ;

struct availableIndex {
    struct availableIndexEntry * index ;
    int size;
} ;

struct availableList {
    struct availablePackage * list;
    struct availableIndex index;
    int size, alloced;
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
