#ifndef H_RPMURL
#define H_RPMURL

#ifndef IPPORT_FTP
#define IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif

#define FTPERR_BAD_SERVER_RESPONSE   -1
#define FTPERR_SERVER_IO_ERROR       -2
#define FTPERR_SERVER_TIMEOUT        -3
#define FTPERR_BAD_HOST_ADDR         -4
#define FTPERR_BAD_HOSTNAME          -5
#define FTPERR_FAILED_CONNECT        -6
#define FTPERR_FILE_IO_ERROR         -7
#define FTPERR_PASSIVE_ERROR         -8
#define FTPERR_FAILED_DATA_CONNECT   -9
#define FTPERR_FILE_NOT_FOUND        -10
#define FTPERR_NIC_ABORT_IN_PROGRESS -11
#define FTPERR_UNKNOWN               -100

typedef enum {
    URL_IS_UNKNOWN	= 0,
    URL_IS_DASH		= 1,
    URL_IS_PATH		= 2,
    URL_IS_FTP		= 3,
    URL_IS_HTTP		= 4
} urltype;

typedef struct urlinfo {
    const char *url;		/* copy of original url */
    const char *service;
    const char *user;
    const char *password;
    const char *host;
    const char *portstr;
    const char *path;
    const char *proxyu;		/* FTP: proxy user */
    const char *proxyh;		/* FTP/HTTP: proxy host */
    int proxyp;			/* FTP/HTTP: proxy port */
    int	port;
    int ftpControl;
    int ftpGetFileDoneNeeded;
    int openError;		/* Type of open failure */
} urlinfo;

#ifdef __cplusplus
extern "C" {
#endif

/*@only@*/ /*@observer@*/ const char * ftpStrerror(int ftpErrno);

void	urlSetCallback(rpmCallbackFunction notify, void *notifyData, int notifyCount);
int	httpOpen(urlinfo *u);
int	ftpOpen(urlinfo *u);

int	httpGetFile(/*@only@*/FD_t sfd, FD_t tfd);
int	ftpGetFile(/*@only@*/FD_t sfd, FD_t tfd);
int	ftpGetFileDesc(FD_t);
int	ftpAbort(/*@only@*/FD_t fd);
int	ftpClose(/*@only@*/FD_t fd);

urltype	urlIsURL(const char * url);
int 	urlSplit(const char *url, /*@out@*/urlinfo **u);
/*@only@*/urlinfo	*newUrlinfo(void);
void	freeUrlinfo(/*@only@*/urlinfo *u);

/*@only@*/ FD_t	ufdOpen(const char * pathname, int flags, mode_t mode);
int	ufdClose(/*@only@*/FD_t fd);
/*@observer@*/ const char *urlStrerror(const char *url);

int	urlGetFile(const char * url, const char * dest);
void    urlInvalidateCache(const char * url);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMURL */
