/* names.h -- user/group name/id cache plus hostname and buildtime */

#ifndef _H_NAMES_
#define _H_NAMES_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

char *getUname(uid_t uid);
char *getUnameS(char *uname);
char *getGname(gid_t gid);
char *getGnameS(char *gname);

char *buildHost(void);
time_t *getBuildTime(void);

#ifdef __cplusplus
}
#endif

#endif /* _H_NAMES_ */
