#ifndef H_FINGERPRINT
#define H_FINGERPRINT

#include "hash.h"
#include "header.h"

/* This is really a directory and symlink cache. We don't differentiate between
   the two. We can prepopulate it, which allows us to easily conduct "fake"
   installs of a system w/o actually mounting filesystems. */
struct fprintCacheEntry_s {
    char * dirName;
    dev_t dev;
    ino_t ino;
    int isFake;
};

typedef /*@abstract@*/ struct fprintCache_s {
    hashTable ht;		    /* hashed by dirName */
} * fingerPrintCache;

typedef struct fingerprint_s {
    const struct fprintCacheEntry_s * entry;
    /*@owned@*/ /*@null@*/ const char * subDir;
    /*@dependent@*/ const char * baseName;
} fingerPrint;

/* only if !scarceMemory */
#define fpFree(a) free((void *)(a).baseName)

#define FP_EQUAL(a, b) ( \
	(&(a) == &(b)) || ( \
		((a).entry == (b).entry) && \
		!strcmp((a).baseName, (b).baseName) && \
		(((a).subDir && (b).subDir && !strcmp((a).subDir, (b).subDir)) \
			|| ((a).subDir == (b).subDir)) \
		) \
	)

#define	FP_ENTRY_EQUAL(a, b) ( \
	((a)->dev == (b)->dev) && \
	((a)->ino == (b)->ino) && \
	!strcmp((a)->dirName, (b)->dirName))

#define FP_EQUAL_DIFFERENT_CACHE(a, b) ( \
	FP_ENTRY_EQUAL((a).entry, (b).entry) && \
	!strcmp((a).baseName, (b).baseName) && \
		(((a).subDir && (b).subDir && !strcmp((a).subDir, (b).subDir)) \
			|| ((a).subDir == (b).subDir)) \
	)

#ifdef __cplusplus
extern "C" {
#endif

/* Be carefull with the memory... assert(*fullName == '/' || !scareMemory) */
 /*@only@*/ fingerPrintCache fpCacheCreate(int sizeHint);
void		fpCacheFree(/*@only@*/ fingerPrintCache cache);
fingerPrint	fpLookup(fingerPrintCache cache, const char * dirName, 
			const char * baseName, int scareMemory);

/* Hash based on dev and inode only! */
unsigned int fpHashFunction(const void * key);
/* exactly equivalent to FP_EQUAL */
int fpEqual(const void * key1, const void * key2);

/* scareMemory is assumed for both of these! */
void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const int * dirIndexes, 
		  int fileCount, fingerPrint * fpList);
void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList);

#ifdef __cplusplus
}
#endif

#endif
