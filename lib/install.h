#ifndef H_INSTALL
#define H_INSTALL

/** \file lib/install.h
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
    FA_UNKNOWN = 0,	/*!< initial action (default action for source rpm) */
    FA_CREATE,		/*!< ... to be replaced. */
    FA_BACKUP,		/*!< ... renamed with ".rpmorig" extension. */
    FA_SAVE,		/*!< ... renamed with ".rpmsave" extension. */
    FA_SKIP, 		/*!< ... already replaced, don't remove. */
    FA_ALTNAME,		/*!< ... create with ".rpmnew" extension. */
    FA_REMOVE,		/*!< ... to be removed. */
    FA_SKIPNSTATE,	/*!< ... untouched, state "not installed". */
    FA_SKIPNETSHARED,	/*!< ... untouched, state "netshared". */
    FA_SKIPMULTILIB,	/*!< ... untouched. @todo state "multilib" ???. */
};

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPMULTILIB)

/**
 */
typedef	enum rollbackDir_e {
    ROLLBACK_SAVE	= 1,	/*!< Save files. */
    ROLLBACK_RESTORE	= 2,	/*!< Restore files. */
} rollbackDir;

/**
 * File types.
 * These are the types of files used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header. The values are arbitrary,
 * but are identical to the linux stat(2) file types.
 */
enum fileTypes {
    PIPE	= 1,	/*!< pipe/fifo */
    CDEV	= 2,	/*!< character device */
    XDIR	= 4,	/*!< directory */
    BDEV	= 6,	/*!< block device */
    REG		= 8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12,	/*!< socket */
};

/*@abstract@*/ typedef struct transactionFileInfo_s * TFI_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retrieve and run scriptlet from header.
 * @param ts		transaction set
 * @param h		header
 * @param scriptTag	scriptlet tag
 * @param progTag	scriptlet interpreter tag
 * @param arg		no. instances of package installed after scriptlet exec
 * @param norunScripts	should scriptlet be executed?
 * @return		0 on success
 */
int runInstScript(const rpmTransactionSet ts, Header h,
		int scriptTag, int progTag, int arg, int norunScripts);

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param ts		transaction set
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param h		header
 * @param countCorrection 0 if installing, -1 if removing, package
 * @return		0 on success, 1 on error
 */
int runTriggers(const rpmTransactionSet ts, int sense, Header h,
		int countCorrection);

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param ts		transaction set
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param h		header
 * @param countCorrection 0 if installing, -1 if removing, package
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
 * @param fi		transaction element file info
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int installBinaryPackage(const rpmTransactionSet ts, TFI_t fi);

/**
 * Erase binary package (from transaction set).
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param pkgKey	package private data
 * @return		0 on success
 */
int removeBinaryPackage(const rpmTransactionSet ts, TFI_t fi);

#ifdef __cplusplus
}
#endif

#endif	/* H_INSTALL */
