#ifndef H_RPMDB
#define H_RPMDB

#include "rpmlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "fprint.h"

/* for RPM's internal use only */

#define RPMDB_FLAG_JUSTCHECK	(1 << 0)
#define RPMDB_FLAG_MINIMAL	(1 << 1)

int openDatabase(char * prefix, char * dbpath, rpmdb *rpmdbp, int mode,
		 int perms, int flags);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);
void rpmdbRemoveDatabase(char * rootdir, char * dbpath);
int rpmdbMoveDatabase(char * rootdir, char * olddbpath, char * newdbpath);
/* matchList must be preallocated!!! */
int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
