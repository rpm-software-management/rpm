#ifndef H_FINGERPRINT
#define H_FINGERPRINT

/** \ingroup rpmtrans
 * \file lib/fprint.h
 * Identify a file name path by a unique "finger print".
 */

#include <rpm/header.h>
#include <rpm/rpmte.h>
#include "lib/rpmdb_internal.h"

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

/* Create new hash table data type */
#define HASHTYPE rpmFpEntryHash
#define HTKEYTYPE const char *
#define HTDATATYPE const struct fprintCacheEntry_s *
#include "lib/rpmhash.H"

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

/**
 * Finger print cache.
 */
struct fprintCache_s {
    rpmFpEntryHash ht;			/*!< hashed by dirName */
};

/* Create new hash table data type */

struct rpmffi_s {
  rpmte p;
  int   fileno;
};

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE rpmFpHash
#define HTKEYTYPE const fingerPrint *
#define HTDATATYPE struct rpmffi_s
#include "lib/rpmhash.H"

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

/**
 * Return finger print of a file path.
 * @param cache		pointer to fingerprint cache
 * @param dirName	leading directory name of file path
 * @param baseName	base name of file path
 * @param scareMemory
 * @return pointer to the finger print associated with a file path.
 */
RPM_GNUC_INTERNAL
fingerPrint fpLookup(fingerPrintCache cache, const char * dirName, 
			const char * baseName, int scareMemory);

/**
 * Return hash value for a finger print.
 * Hash based on dev and inode only!
 * @param key		pointer to finger print entry
 * @return hash value
 */
RPM_GNUC_INTERNAL
unsigned int fpHashFunction(const fingerPrint * key);

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
 * @param dirNames	directory names
 * @param baseNames	file base names
 * @param dirIndexes	index into dirNames for each baseNames
 * @param fileCount	number of file entries
 * @retval fpList	pointer to array of finger prints
 */
RPM_GNUC_INTERNAL
void fpLookupList(fingerPrintCache cache, const char ** dirNames, 
		  const char ** baseNames, const uint32_t * dirIndexes, 
		  int fileCount, fingerPrint * fpList);

/**
 * Check file for to be installed symlinks in their path,
 *  correct their fingerprint and add it to newht.
 * @param ht            hash table containing all files fingerprints
 * @param newht         hash table to add the corrected fingerprints
 * @param fpc           fingerprint cache
 * @param fi            file iterator of the package
 * @param filenr        the number of the file we are dealing with
 */
void fpLookupSubdir(rpmFpHash symlinks, rpmFpHash fphash, fingerPrintCache fpc, rpmte p, int filenr);


#ifdef __cplusplus
}
#endif

#endif
