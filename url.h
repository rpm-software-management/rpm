#ifndef H_URL
#define H_URL

#include "ftp.h"

/* right now, only ftp type URL's are supported */

struct urlContext {
    int ftpControl;
};

int urlIsURL(char * url);
int urlGetFile(char * url, char * dest);
int urlGetFd(char * url, struct urlContext * context);
int urlFinishedFd(struct urlContext * context);

#endif
