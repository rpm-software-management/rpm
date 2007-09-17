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

extern int _url_debug;		/*!< URL debugging? */

/**
 * Create a URL info structure instance.
 * @return		new instance
 */
urlinfo	urlNew(void);

/**
 * Free a URL info structure instance.
 * @param u		URL control structure
 * @return		dereferenced instance (NULL if freed)
 */
urlinfo	urlFree(urlinfo u);

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
