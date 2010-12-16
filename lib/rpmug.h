#ifndef _RPMUG_H
#define _RPMUG_H

#include <sys/types.h>

const char * rpmugStashStr(const char *str);

int rpmugUid(const char * name, uid_t * uid);

int rpmugGid(const char * name, gid_t * gid);

const char * rpmugUname(uid_t uid);

const char * rpmugGname(gid_t gid);

void rpmugFree(void);

#endif /* _RPMUG_H */
