#ifndef H_INSTALL
#define H_INSTALL

#include "header.h"
#include "rpmlib.h"

struct sharedFile {
    int mainFileNumber;
    int secRecOffset;
    int secFileNumber;
} ;

enum fileActions { UNKNOWN, CREATE, BACKUP, SAVE, SKIP, ALTNAME, REMOVE };
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

int removeBinaryPackage(char * root, rpmdb db, unsigned int offset, int flags,
			enum fileActions * actions);
int runInstScript(char * prefix, Header h, int scriptTag, int progTag,
	          int arg, int norunScripts, FD_t err);
/* this looks for triggers in the database which h would set off */
int runTriggers(char * root, rpmdb db, int sense, Header h,
		int countCorrection);
/* while this looks for triggers in h which are set off by things in the db
   database to calculate arguments to the trigger */
int runImmedTriggers(char * root, rpmdb db, int sense, Header h,
		     int countCorrection);
int installBinaryPackage(char * rootdir, rpmdb db, FD_t fd, Header h,
		         int flags, rpmNotifyFunction notify, 
			 void * notifyData, enum fileActions * actions);
const char * fileActionString(enum fileActions a);

#endif	/* H_INSTALL */
