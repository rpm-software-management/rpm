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

typedef struct fprintCache_s {
    hashTable ht;		    /* hashed by dirName */
} * fingerPrintCache;

typedef struct fingerprint_s {
    const struct fprintCacheEntry_s * entry;
    const char * subdir;
    const char * basename;
} fingerPrint;

/* only if !scarceMemory */
#define fpFree(a) free((void *)(a).basename)

#define FP_EQUAL(a, b) ((&(a) == &(b)) || \
			       (((a).entry == (b).entry) && \
			        !strcmp((a).subdir, (b).subdir) && \
			        !strcmp((a).basename, (b).basename)))

#ifdef __cplusplus
extern "C" {
#endif

/* Be carefull with the memory... assert(*fullName == '/' || !scareMemory) */
fingerPrintCache fpCacheCreate(int sizeHint);
void fpCacheFree(fingerPrintCache cache);
fingerPrint fpLookup(fingerPrintCache cache, const char * fullName, 
		     int scareMemory);

/* Hash based on dev and inode only! */
unsigned int fpHashFunction(const void * key);
/* exactly equivalent to FP_EQUAL */
int fpEqual(const void * key1, const void * key2);

/* scareMemory is assumed for both of these! */
void fpLookupList(fingerPrintCache cache, char ** dirNames, char ** baseNames,
		  int * dirIndexes, int fileCount, fingerPrint * fpList);
void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList);

#ifdef __cplusplus
}
#endif

#endif
