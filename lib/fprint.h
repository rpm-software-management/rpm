#ifndef H_FINGERPRINT
#define H_FINGERPRINT

/** \ingroup rpmtrans
 * \file lib/fprint.h
 * Identify a file name path by a unique "finger print".
 */

#include "hash.h"
#include "header.h"

/**
 * Finger print cache entry.
 * This is really a directory and symlink cache. We don't differentiate between
 * the two. We can prepopulate it, which allows us to easily conduct "fake"
 * installs of a system w/o actually mounting filesystems.
 */
struct fprintCacheEntry_s {
    const char * dirName;		/*!< path to existing directory */
    dev_t dev;				/*!< stat(2) device number */
    ino_t ino;				/*!< stat(2) inode number */
    int isFake;				/*!< (currently unused) */
};

/**
 * Finger print cache.
 */
typedef /*@abstract@*/ struct fprintCache_s {
    hashTable ht;			/*!< hashed by dirName */
} * fingerPrintCache;

/**
 * Associates a trailing sub-directory and final base name with an existing
 * directory finger print.
 */
typedef struct fingerPrint_s {
/*! directory finger print entry (the directory path is stat(2)-able */
    const struct fprintCacheEntry_s * entry;
/*! trailing sub-directory path (directories that are not stat(2)-able */
/*@owned@*/ /*@null@*/ const char * subDir;
/*@dependent@*/ const char * baseName;	/*!< file base name */
} fingerPrint;

/* only if !scarceMemory */
/** */
#define fpFree(a) free((void *)(a).baseName)

/** */
#define	FP_ENTRY_EQUAL(a, b) (((a)->dev == (b)->dev) && ((a)->ino == (b)->ino))

/** */
#define FP_EQUAL(a, b) ( \
	FP_ENTRY_EQUAL((a).entry, (b).entry) && \
	!strcmp((a).baseName, (b).baseName) && ( \
	    ((a).subDir == (b).subDir) || \
	    ((a).subDir && (b).subDir && !strcmp((a).subDir, (b).subDir)) \
	) \
    )

#ifdef __cplusplus
extern "C" {
#endif

/* Be carefull with the memory... assert(*fullName == '/' || !scareMemory) */

/**
 * Create finger print cache.
 * @param sizeHint	number of elements expected
 * @return pointer to initialized fingerprint cache
 */
/*@only@*/ fingerPrintCache fpCacheCreate(int sizeHint)	/*@*/;

/**
 * Destroy finger print cache.
 * @param cache		pointer to fingerprint cache
 */
void		fpCacheFree(/*@only@*/ fingerPrintCache cache);

/**
 * Return finger print of a file path.
 * @param cache		pointer to fingerprint cache
 * @param dirName	leading directory name of file path
 * @param baseName	base name of file path
 * @param scareMemory
 * @return pointer to the finger print associated with a file path.
 */
fingerPrint	fpLookup(fingerPrintCache cache, const char * dirName, 
			const char * baseName, int scareMemory)	/*@*/;

/**
 * Return hash value for a finger print.
 * Hash based on dev and inode only!
 * @param key		pointer to finger print entry
 * @return hash value
 */
unsigned int fpHashFunction(const void * key)	/*@*/;

/**
 * Compare two finger print entries.
 * exactly equivalent to FP_EQUAL macro.
 * @param key1		finger print 1
 * @param key2		finger print 2
 * @return result of comparing key1 and key2
 */
int fpEqual(const void * key1, const void * key2)	/*@*/;

/**
 * Return finger prints of an array of file paths.
 * @warning: scareMemory is assumed!
 * @param cache		pointer to fingerprint cache
 * @param dirNames	directory names
 * @param baseNames	file base names
 * @param dirIndexes	index into dirNames for each baseNames
 * @param fileCount	number of file entries
 * @retval fpList	pointer to array of finger prints
 */
void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const int * dirIndexes, 
		  int fileCount, fingerPrint * fpList)
			/*@modifies cache, *fpList @*/;

/**
 * Return finger prints of all file names in header.
 * @warning: scareMemory is assumed!
 * @param cache		pointer to fingerprint cache
 * @param h		package header
 * @retval fpList	pointer to array of finger prints
 */
void fpLookupHeader(fingerPrintCache cache, Header h, fingerPrint * fpList)
	/*@modifies h, cache, *fpList @*/;

#ifdef __cplusplus
}
#endif

#endif
