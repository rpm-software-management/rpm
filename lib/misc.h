#ifndef H_MISC
#define H_MISC

/**
 * \file lib/misc.h
 *
 */

#include <string.h>
#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create directory if it does not exist, and make sure path is writable.
 * @note This will only create last component of directory path.
 * @param dpath		directory path
 * @param dname		directory use string
 * @return		rpmRC return code
 */
rpmRC rpmMkdirPath (const char * dpath, const char * dname);

/*
 * These may be called w/ a NULL argument to flush the cache -- they return
 * -1 if the user can't be found.
 */
int     unameToUid(const char * thisUname, uid_t * uid);
int     gnameToGid(const char * thisGname, gid_t * gid);

/*
 * Call w/ -1 to flush the cache, returns NULL if the user can't be found.
 */
const char * uidToUname(uid_t uid);
const char * gidToGname(gid_t gid);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
