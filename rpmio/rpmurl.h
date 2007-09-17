#ifndef H_RPMURL
#define H_RPMURL

/** \ingroup rpmio
 * \file rpmio/rpmurl.h
 */

#include <assert.h>

/**
 * Supported URL types.
 */
typedef enum urltype_e {
    URL_IS_UNKNOWN	= 0,	/*!< unknown (aka a file) */
    URL_IS_DASH		= 1,	/*!< stdin/stdout */
    URL_IS_PATH		= 2,	/*!< file://... */
    URL_IS_FTP		= 3,	/*!< ftp://... */
    URL_IS_HTTP		= 4,	/*!< http://... */
    URL_IS_HTTPS	= 5,	/*!< https://... */
    URL_IS_HKP		= 6	/*!< hkp://... */
} urltype;

#define	URLMAGIC	0xd00b1ed0
#define	URLSANE(u)	assert(u && u->magic == URLMAGIC)

typedef struct urlinfo_s * urlinfo;

/**
 * URL control structure.
 */
struct urlinfo_s {
int nrefs;		/*!< no. of references */
    const char * url;		/*!< copy of original url */
    const char * scheme;	/*!< URI scheme. */
    const char * user;		/*!< URI user. */
    const char * password;	/*!< URI password. */
    const char * host;		/*!< URI host. */
    const char * portstr;	/*!< URI port string. */
    const char * proxyu;	/*!< FTP: proxy user */
    const char * proxyh;	/*!< FTP/HTTP: proxy host */
    int proxyp;			/*!< FTP/HTTP: proxy port */
    int	port;			/*!< URI port. */
    int urltype;		/*!< URI type. */
    int openError;		/*!< Type of open failure */
    int magic;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int _url_count;		/*!< No. of cached URL's. */

extern urlinfo * _url_cache;	/*!< URL cache. */

extern int _url_iobuf_size;	/*!< Initial size of URL I/O buffer. */
#define RPMURL_IOBUF_SIZE	4096

extern int _url_debug;		/*!< URL debugging? */
#define RPMURL_DEBUG_IO		0x40000000
#define RPMURL_DEBUG_REFS	0x20000000


/**
 * Create a URL control structure instance.
 * @param msg		debugging identifier (unused)
 * @return		new instance
 */
urlinfo	urlNew(const char * msg)	;

/** @todo Remove debugging entry from the ABI. */
urlinfo	XurlNew(const char * msg, const char * file, unsigned line)	;
#define	urlNew(_msg) XurlNew(_msg, __FILE__, __LINE__)

/**
 * Reference a URL control structure instance.
 * @param u		URL control structure
 * @param msg		debugging identifier (unused)
 * @return		referenced instance
 */
urlinfo	urlLink(urlinfo u, const char * msg);

/** @todo Remove debugging entry from the ABI. */
urlinfo	XurlLink(urlinfo u, const char * msg, const char * file, unsigned line);
#define	urlLink(_u, _msg) XurlLink(_u, _msg, __FILE__, __LINE__)

/**
 * Dereference a URL control structure instance.
 * @param u		URL control structure
 * @param msg		debugging identifier (unused)
 * @return		dereferenced instance (NULL if freed)
 */
urlinfo	urlFree( urlinfo u, const char * msg);

/** @todo Remove debugging entry from the ABI. */
urlinfo	XurlFree( urlinfo u, const char * msg,
		const char * file, unsigned line);
#define	urlFree(_u, _msg) XurlFree(_u, _msg, __FILE__, __LINE__)

/**
 * Free cached URL control structures.
 */
void urlFreeCache(void);

/**
 * Return type of URL.
 * @param url		url string
 * @return		type of url
 */
urltype	urlIsURL(const char * url);

/**
 * Return path component of URL.
 * @param url		url string
 * @retval pathp	pointer to path component of url
 * @return		type of url
 */
urltype	urlPath(const char * url, const char ** pathp);

/**
 * Parse URL string into a control structure.
 * @param url		url string
 * @retval uret		address of new control instance pointer
 * @return		0 on success, -1 on error
 */
int urlSplit(const char * url, urlinfo * uret);

/**
 * Copy data from URL to local file.
 * @param url		url string of source
 * @param dest		file name of destination
 * @return		0 on success, otherwise FTPERR_* code
 */
int urlGetFile(const char * url, const char * dest);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMURL */
