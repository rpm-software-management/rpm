#ifndef H_RPMDB
#define H_RPMDB

#include <rpmlib.h>
#include "fprint.h"

/* for RPM's internal use only */

#define RPMDB_FLAG_JUSTCHECK	(1 << 0)
#define RPMDB_FLAG_MINIMAL	(1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

int openDatabase(const char * prefix, const char * dbpath, rpmdb *rpmdbp, int mode,
		 int perms, int flags);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);
void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath);
int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath);
/* matchList must be preallocated!!! */
int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, /*@out@*/dbiIndexSet * matchList, 
		    int numItems);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
