#ifndef H_URL
#define H_URL

typedef enum {
    URL_IS_UNKNOWN	= 0,
    URL_IS_DASH		= 1,
    URL_IS_PATH		= 2,
    URL_IS_FILE		= 3,
    URL_IS_FTP		= 4,
    URL_IS_HTTP		= 5,
} urltype;

typedef struct urlinfo {
    char *service;
    char *user;
    char *password;
    char *host;
    char *portstr;
    char *path;
    int	port;
    int ftpControl;
} urlinfo;

#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif

#ifdef __cplusplus
extern "C" {
#endif

urltype	urlIsURL(const char * url);
int 	rlSplit(const char *url, urlinfo **u);
urlinfo	*newUrlinfo(void);
void	freeUrlinfo(urlinfo *u);

FD_t	ufdOpen(const char * pathname, int flags, mode_t mode);
int	ufdClose(FD_t fd);

int urlGetFile(char * url, char * dest);

#ifdef __cplusplus
}
#endif

#endif
