#ifndef H_RPMURL
#define H_RPMURL

/** \ingroup rpmio
 * \file rpmio/rpmurl.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmurl
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

typedef struct urlinfo_s * urlinfo;

/** \ingroup rpmurl
 * URL control structure.
 */
struct urlinfo_s {
    char * url;			/*!< copy of original url */
    char * scheme;		/*!< URI scheme. */
    char * user;		/*!< URI user. */
    char * password;		/*!< URI password. */
    char * host;		/*!< URI host. */
    char * portstr;		/*!< URI port string. */
    char * proxyu;		/*!< FTP: proxy user */
    char * proxyh;		/*!< FTP/HTTP: proxy host */
    int proxyp;			/*!< FTP/HTTP: proxy port */
    int	port;			/*!< URI port. */
    int urltype;		/*!< URI type. */
    int openError;		/*!< Type of open failure */
    int magic;
};

extern int _url_debug;		/*!< URL debugging? */

/** \ingroup rpmurl
 * Create a URL info structure instance.
 * @return		new instance
 */
urlinfo	urlNew(void);

/** \ingroup rpmurl
 * Free a URL info structure instance.
 * @param u		URL control structure
 * @return		dereferenced instance (NULL if freed)
 */
urlinfo	urlFree(urlinfo u);

/** \ingroup rpmurl
 * Return type of URL.
 * @param url		url string
 * @return		type of url
 */
urltype	urlIsURL(const char * url);

/** \ingroup rpmurl
 * Return path component of URL.
 * @param url		url string
 * @retval pathp	pointer to path component of url
 * @return		type of url
 */
urltype	urlPath(const char * url, const char ** pathp);

/** \ingroup rpmurl
 * Parse URL string into a control structure.
 * @param url		url string
 * @retval uret		address of new control instance pointer
 * @return		0 on success, -1 on error
 */
int urlSplit(const char * url, urlinfo * uret);

/** \ingroup rpmurl
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
