#ifndef H_INSTALL
#define H_INSTALL

#include "header.h"
#include "rpmlib.h"

struct sharedFile {
    int mainFileNumber;
    int secRecOffset;
    int secFileNumber;
} ;

struct sharedFileInfo {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

enum fileActions { FA_UNKNOWN = 0, FA_CREATE, FA_BACKUP, FA_SAVE, FA_SKIP, 
		   FA_ALTNAME, FA_REMOVE, FA_SKIPNSTATE };
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

int removeBinaryPackage(char * root, rpmdb db, unsigned int offset, int flags,
			enum fileActions * actions, FD_t scriptFd);
int runInstScript(const char * prefix, Header h, int scriptTag, int progTag,
	          int arg, int norunScripts, FD_t err);
/* this looks for triggers in the database which h would set off */
int runTriggers(const char * root, rpmdb db, int sense, Header h,
		int countCorrection, FD_t scriptFd);
/* while this looks for triggers in h which are set off by things in the db
   database to calculate arguments to the trigger */
int runImmedTriggers(const char * root, rpmdb db, int sense, Header h,
		     int countCorrection, FD_t scriptFd);
int installBinaryPackage(const char * rootdir, rpmdb db, FD_t fd, Header h,
		         int flags, rpmCallbackFunction notify, 
			 void * notifyData, const void * pkgKey, 
			 enum fileActions * actions,
			 struct sharedFileInfo * sharedList, FD_t scriptFd);
const char * fileActionString(enum fileActions a);

#endif	/* H_INSTALL */
