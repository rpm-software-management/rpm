#ifndef H_URL
#define H_URL

#include "ftp.h"

/* right now, only ftp type URL's are supported */

typedef enum {
	URL_IS_UNKNOWN	= 0,
	URL_IS_DASH	= 1,
	URL_IS_PATH	= 2,
	URL_IS_FILE	= 3,
	URL_IS_FTP	= 4,
	URL_IS_HTTP	= 5,
} urltype;

urltype urlIsURL(const char * url);

int urlGetFile(char * url, char * dest);

FD_t	ufdOpen(const char * pathname, int flags, mode_t mode);
int	ufdClose(FD_t fd);

#endif
