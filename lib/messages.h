#ifndef H_MESSAGES
#define H_MESSAGES

#define MESS_DEBUG      1
#define MESS_VERBOSE    2
#define MESS_NORMAL     3
#define MESS_WARNING    4
#define MESS_ERROR      5
#define MESS_FATALERROR 6

#define MESS_QUIET (MESS_NORMAL + 1)

void increaseVerbosity(void);
void setVerbosity(int level);
void message(int level, char * format, ...);
int isVerbose(void);
int isDebug(void);

#endif
