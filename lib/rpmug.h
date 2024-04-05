#ifndef _RPMUG_H
#define _RPMUG_H

#include <rpm/rpmutil.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
int rpmugUid(const char * name, uid_t * uid);

RPM_GNUC_INTERNAL
int rpmugGid(const char * name, gid_t * gid);

RPM_GNUC_INTERNAL
const char * rpmugUname(uid_t uid);

RPM_GNUC_INTERNAL
const char * rpmugGname(gid_t gid);

RPM_GNUC_INTERNAL
void rpmugFree(void);

#ifdef __cplusplus
}
#endif


#endif /* _RPMUG_H */
