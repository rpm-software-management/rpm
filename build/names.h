/* names.h -- user/group name/id cache plus hostname and buildtime */

#ifndef _NAMES_H_
#define _NAMES_H_

#include <sys/types.h>

char *getUname(uid_t uid);
char *getUnameS(char *uname);
char *getGname(gid_t gid);
char *getGnameS(char *gname);

char *buildHost(void);
time_t *getBuildTime(void);

#endif /* _NAMES_H_ */
