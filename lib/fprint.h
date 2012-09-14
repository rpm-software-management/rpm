#ifndef H_FINGERPRINT
#define H_FINGERPRINT

/** \ingroup rpmtrans
 * \file lib/fprint.h
 * Identify a file name path by a unique "finger print".
 */

#include <rpm/rpmtypes.h>

/**
 */
typedef struct fprintCache_s * fingerPrintCache;

/**
 * @todo Convert to pointer and make abstract.
 */
typedef struct fingerPrint_s fingerPrint;

/**
 * Associates a trailing sub-directory and final base name with an existing
 * directory finger print.
 */
struct fingerPrint_s {
/*! directory finger print entry (the directory path is stat(2)-able */
    const struct fprintCacheEntry_s * entry;
/*! trailing sub-directory path (directories that are not stat(2)-able */
const char * subDir;
const char * baseName;	/*!< file base name */
};

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
};

struct rpmffi_s {
  rpmte p;
  int   fileno;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create finger print cache.
 * @param sizeHint	number of elements expected
 * @return pointer to initialized fingerprint cache
 */
RPM_GNUC_INTERNAL
fingerPrintCache fpCacheCreate(int sizeHint);

/**
 * Destroy finger print cache.
 * @param cache		pointer to fingerprint cache
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
fingerPrintCache fpCacheFree(fingerPrintCache cache);

RPM_GNUC_INTERNAL
int fpCacheGetByFp(fingerPrintCache cache, struct fingerPrint_s * fp,
		   struct rpmffi_s ** recs, int * numRecs);

RPM_GNUC_INTERNAL
void fpCachePopulate(fingerPrintCache cache, rpmts ts, int fileCount);

/* compare an existing fingerprint with a looked-up fingerprint for db/bn */
RPM_GNUC_INTERNAL
int fpLookupEquals(fingerPrintCache cache, fingerPrint * fp,
	           const char * dirName, const char * baseName);

/**
 * Return finger print of a file path.
 * @param cache		pointer to fingerprint cache
 * @param dirName	leading directory name of file path
 * @param baseName	base name of file path
 * @param scareMemory
 * @retval fp		pointer of fingerprint struct to fill out
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int fpLookup(fingerPrintCache cache,
	     const char * dirName, const char * baseName, int scareMemory,
	     fingerPrint *fp);

/**
 * Compare two finger print entries.
 * This routine is exactly equivalent to the FP_EQUAL macro.
 * @param key1		finger print 1
 * @param key2		finger print 2
 * @return result of comparing key1 and key2
 */
RPM_GNUC_INTERNAL
int fpEqual(const fingerPrint * key1, const fingerPrint * key2);

/**
 * Return finger prints of an array of file paths.
 * @warning: scareMemory is assumed!
 * @param cache		pointer to fingerprint cache
 * @param pool		pointer to file name pool
 * @param dirNames	directory names
 * @param baseNames	file base names
 * @param dirIndexes	index into dirNames for each baseNames
 * @param fileCount	number of file entries
 * @return		pointer to array of finger prints
 */
RPM_GNUC_INTERNAL
fingerPrint * fpLookupList(fingerPrintCache cache, rpmstrPool pool,
			   rpmsid * dirNames, rpmsid * baseNames,
			   const uint32_t * dirIndexes, int fileCount);

#ifdef __cplusplus
}
#endif

#endif
