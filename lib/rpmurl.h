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

typedef /*@abstract@*/ /*@refcounted@*/ struct urlinfo {
/*@refs@*/ int nrefs;
    const char * url;		/* copy of original url */
    const char * service;
    const char * user;
    const char * password;
    const char * host;
    const char * portstr;
    const char * path;
    const char * proxyu;	/* FTP: proxy user */
    const char * proxyh;	/* FTP/HTTP: proxy host */
    int proxyp;			/* FTP/HTTP: proxy port */
    int	port;
    FD_t ftpControl;
    int ftpFileDoneNeeded;
    int openError;		/* Type of open failure */
    int httpVersion;
    int httpHasRange;
    int httpContentLength;
    int httpPersist;
} *urlinfo;

#ifdef __cplusplus
extern "C" {
#endif

int	ftpCheckResponse(urlinfo u, /*@out@*/ char ** str);
int	httpOpen(urlinfo u, const char * httpcmd);
int	ftpOpen(urlinfo u);
int	ftpFileDone(urlinfo u);
int	ftpFileDesc(urlinfo u, const char * cmd, FD_t fd);

urlinfo	urlLink(urlinfo u, const char * msg);
urlinfo	XurlLink(urlinfo u, const char * msg, const char * file, unsigned line);
#define	urlLink(_u, _msg) XurlLink(_u, _msg, __FILE__, __LINE__)

urlinfo	urlNew(const char * msg);
urlinfo	XurlNew(const char * msg, const char * file, unsigned line);
#define	urlNew(_msg) XurlNew(_msg, __FILE__, __LINE__)

void	urlFree( /*@killref@*/ urlinfo u, const char * msg);
void	XurlFree( /*@killref@*/ urlinfo u, const char * msg, const char * file, unsigned line);
#define	urlFree(_u, _msg) XurlFree(_u, _msg, __FILE__, __LINE__)

void	urlFreeCache(void);

urltype	urlIsURL(const char * url);
int 	urlSplit(const char * url, /*@out@*/ urlinfo * u);

int	urlGetFile(const char * url, const char * dest);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMURL */
