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

/* known arch? */
RPM_GNUC_INTERNAL
int rpmIsKnownArch(const char *name);

/*
 * These may be called w/ a NULL argument to flush the cache -- they return
 * -1 if the user can't be found.
 */
RPM_GNUC_INTERNAL
int     unameToUid(const char * thisUname, uid_t * uid);

RPM_GNUC_INTERNAL
int     gnameToGid(const char * thisGname, gid_t * gid);

/*
 * Call w/ -1 to flush the cache, returns NULL if the user can't be found.
 */
RPM_GNUC_INTERNAL
const char * uidToUname(uid_t uid);

RPM_GNUC_INTERNAL
const char * gidToGname(gid_t gid);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
