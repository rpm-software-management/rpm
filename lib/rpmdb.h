#ifndef H_RPMDB
#define H_RPMDB

/** \file lib/rpmdb.h
 */

#include <rpmlib.h>

#include "fprint.h"

/* for RPM's internal use only */

#define RPMDB_FLAG_JUSTCHECK	(1 << 0)
#define RPMDB_FLAG_MINIMAL	(1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @param dbp		address of rpm database
 */
int openDatabase(const char * prefix, const char * dbpath, /*@out@*/rpmdb *dbp,
		int mode, int perms, int flags);

/**
 * @param db		rpm database
 */
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);

/**
 * @param db		rpm database
 */
int rpmdbAdd(rpmdb db, Header dbentry);

/**
 * @param db		rpm database
 */
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);

/**
 */
void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath);

/**
 */
int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath, const char * newdbpath);

/**
 * @param db		rpm database
 */
int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, /*@out@*/dbiIndexSet * matchList, 
		    int numItems);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
