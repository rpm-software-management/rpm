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
 */
typedef /*@abstract@*/ struct transactionFileInfo_s * TFI_t;

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
