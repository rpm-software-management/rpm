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
 */
enum fileActions { FA_UNKNOWN = 0, FA_CREATE, FA_BACKUP, FA_SAVE, FA_SKIP, 
		   FA_ALTNAME, FA_REMOVE, FA_SKIPNSTATE, FA_SKIPNETSHARED,
		   FA_SKIPMULTILIB };

/**
 */
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @param prefix	path to top of install tree
 * @param h		header
 * @param scriptTag
 * @param progTag
 * @param arg
 * @param norunScripts
 * @param err		stderr file handle
 */
int runInstScript(const char * prefix, Header h, int scriptTag, int progTag,
		int arg, int norunScripts, FD_t err);

/**
 * Run trigger scripts in the database that are fired by header.
 * @param root		path to top of install tree
 * @param rpmdb		rpm database
 * @param sense
 * @param h		header
 * @param countCorrection
 * @param scriptFd
 */
int runTriggers(const char * root, rpmdb rpmdb, int sense, Header h,
		int countCorrection, FD_t scriptFd);

/* while this looks for triggers in h which are set off by things in the db
   database to calculate arguments to the trigger */
/**
 * @param root		path to top of install tree
 * @param rpmdb		rpm database
 * @param sense
 * @param h		header
 * @param countCorrection
 * @param scriptFd
 */
int runImmedTriggers(const char * root, rpmdb rpmdb, int sense, Header h,
		int countCorrection, FD_t scriptFd);

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fileActionString(enum fileActions a);

/**
 * Install binary package (from transaction set).
 * @param rootdir	path to top of install tree
 * @param rpmdb		rpm database
 * @param fd		package file handle
 * @param h		package header
 * @param flags		transaction flags
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @param pkgKey	package private data
 * @param actions	array of file dispositions
 * @param sharedList	header instances of packages that share files
 * @param scriptFd
 * @return		0 on success, 1 on bad magic, 2 on error
 */
int installBinaryPackage(const char * rootdir, rpmdb rpmdb, FD_t fd, Header h,
		int flags, rpmCallbackFunction notify, 
		void * notifyData, const void * pkgKey, 
		enum fileActions * actions,
		struct sharedFileInfo * sharedList, FD_t scriptFd);

/**
 * Erase binary package (from transaction set).
 * @param rootdir	path to top of install tree
 * @param rpmdb		rpm database
 * @param fd		package file handle
 * @param offset	header instance in rpm database
 * @param h		package header
 * @param flags		transaction flags
 * @param notify	progress callback
 * @param notifyData	progress callback private data
 * @param pkgKey	package private data
 * @param actions	array of file dispositions
 * @param scriptFd
 * @return
 */
int removeBinaryPackage(const char * rootdir, rpmdb rpmdb, unsigned int offset,
		Header h,
		int flags, rpmCallbackFunction notify, 
		void * notifyData, const void * pkgKey, 
		enum fileActions * actions,
		FD_t scriptFd);

#ifdef __cplusplus
}
#endif

#endif	/* H_INSTALL */
