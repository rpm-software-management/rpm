#ifndef H_MISC
#define H_MISC

#include <unistd.h>

char ** splitString(char * str, int length, char sep);
void freeSplitString(char ** list);
void stripTrailingSlashes(char * str);

int exists(char * filespec);

int rpmvercmp(char * one, char * two);

/* these are like the normal functions, but they malloc() the space which
   is needed */
int dosetenv(const char *name, const char *value, int overwrite);
int doputenv(const char * str);

/* These may be called w/ a NULL argument to flush the cache -- they return
   -1 if the user can't be found */
uid_t unameToUid(char * thisUname);
uid_t gnameToGid(char * thisGname);

/* Call w/ -1 to flush the cache, returns NULL if the user can't be found */
char * uidToUname(uid_t uid);
char * gidToGname(gid_t gid);

#endif
