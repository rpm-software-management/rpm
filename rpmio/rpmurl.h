#ifndef H_RPMURL
#define H_RPMURL

#include <assert.h>

typedef enum {
    URL_IS_UNKNOWN	= 0,
    URL_IS_DASH		= 1,
    URL_IS_PATH		= 2,
    URL_IS_FTP		= 3,
    URL_IS_HTTP		= 4
} urltype;

#define	URLMAGIC	0xd00b1ed0
#define	URLSANE(u)	assert(u && u->magic == URLMAGIC)

typedef /*@abstract@*/ /*@refcounted@*/ struct urlinfo {
/*@refs@*/ int nrefs;
    const char * url;		/* copy of original url */
    const char * service;
    const char * user;
    const char * password;
    const char * host;
    const char * portstr;
    const char * proxyu;	/* FTP: proxy user */
    const char * proxyh;	/* FTP/HTTP: proxy host */
    int proxyp;			/* FTP/HTTP: proxy port */
    int	port;
    int urltype;
    FD_t ctrl;			/* control channel */
    FD_t data;			/* per-xfer data channel */
    int bufAlloced;		/* sizeof I/O buffer */
    char *buf;			/* I/O buffer */
    int openError;		/* Type of open failure */
    int httpVersion;
    int httpHasRange;
    int magic;
} *urlinfo;

#ifdef __cplusplus
extern "C" {
#endif

extern int url_iobuf_size;

urlinfo	urlLink(urlinfo u, const char * msg);
urlinfo	XurlLink(urlinfo u, const char * msg, const char * file, unsigned line);
#define	urlLink(_u, _msg) XurlLink(_u, _msg, __FILE__, __LINE__)

urlinfo	urlNew(const char * msg);
urlinfo	XurlNew(const char * msg, const char * file, unsigned line);
#define	urlNew(_msg) XurlNew(_msg, __FILE__, __LINE__)

urlinfo	urlFree( /*@killref@*/ urlinfo u, const char * msg);
urlinfo	XurlFree( /*@killref@*/ urlinfo u, const char * msg, const char * file, unsigned line);
#define	urlFree(_u, _msg) XurlFree(_u, _msg, __FILE__, __LINE__)

void	urlFreeCache(void);

urltype	urlIsURL(const char * url);
int	urlPath(const char * url, /*@out@*/ const char ** pathp);
int 	urlSplit(const char * url, /*@out@*/ urlinfo * u);

int	urlGetFile(const char * url, const char * dest);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMURL */
