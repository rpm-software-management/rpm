#ifndef H_RPMDB
#define H_RPMDB

#include "rpmlib.h"

/* for RPM's internal use only */

int openDatabase(char * prefix, char * dbpath, rpmdb *rpmdbp, int mode, 
		 int perms, int justcheck);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);
void rpmdbRemoveDatabase(char * rootdir, char * dbpath);
int rpmdbMoveDatabase(char * rootdir, char * olddbpath, char * newdbpath);

#endif
