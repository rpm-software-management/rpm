#ifndef H_INSTALL
#define H_INSTALL

/** \file lib/install.h
 *
 */

#include <rpmlib.h>

/**
 */
struct sharedFile {
    int mainFileNumber;
    int secRecOffset;
    int secFileNumber;
} ;

/**
 */
struct sharedFileInfo {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

/**
 * File disposition(s) during package install/erase.
 */
enum fileActions {
    FA_UNKNOWN = 0,
    FA_CREATE,
    FA_BACKUP,
    FA_SAVE,
    FA_SKIP, 
    FA_ALTNAME,
    FA_REMOVE,
    FA_SKIPNSTATE,
    FA_SKIPNETSHARED,
    FA_SKIPMULTILIB
};

/**
 * File types.
 * These are the types of files used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header.
 */
enum fileTypes {
    PIPE	= 1,	/*!< pipe/fifo */
    CDEV	= 2,	/*!< character device */
    XDIR	= 4,	/*!< directory */
    BDEV	= 6,	/*!< block device */
    REG		= 8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12	/*!< socket */
};

/*@abstract@*/ typedef struct transactionFileInfo_s * TFI_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @param ts		transaction set
 * @param h		header
 * @param scriptTag
 * @param progTag
 * @param arg
 * @param norunScripts
 */
int runInstScript(const rpmTransactionSet ts, Header h,
		int scriptTag, int progTag, int arg, int norunScripts);

/**
 * Run trigger scripts in the database that are fired by header.
 * @param ts		transaction set
 * @param sense		@todo Document.
 * @param h		header
 * @param countCorrection	@todo Document.
 * @return		0 on success, 1 on error
 */
int runTriggers(const rpmTransactionSet ts, int sense, Header h,
		int countCorrection);

/**
 * Run triggers from header that are fired by the database.
 * @param ts		transaction set
 * @param sense		@todo Document.
 * @param h		header
 * @param countCorrection	@todo Document.
 * @return		0 on success, 1 on error
 */
int runImmedTriggers(const rpmTransactionSet ts, int sense, Header h,
		int countCorrection);

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fileActionString(enum fileActions a);

/**
 * Install binary package (from transaction set).
 * @param ts		transaction set
 * @param h		package header
 * @param fi		transaction file info
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int installBinaryPackage(const rpmTransactionSet ts, Header h, TFI_t fi);

/**
 * Erase binary package (from transaction set).
 * @param ts		transaction set
 * @param offset	header instance in rpm database
 * @param h		package header
 * @param pkgKey	package private data
 * @param actions	array of file dispositions
 * @return
 */
int removeBinaryPackage(const rpmTransactionSet ts, unsigned int offset,
		Header h, const void * pkgKey, enum fileActions * actions);

#ifdef __cplusplus
}
#endif

#endif	/* H_INSTALL */
