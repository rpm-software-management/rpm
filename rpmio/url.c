#include "system.h"

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmbuild.h>
#include <rpmio.h>
#include <rpmurl.h>

#ifndef	IPPORT_FTP
#define	IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif

/*@access urlinfo@*/

#define	URL_IOBUF_SIZE	4096
int url_iobuf_size = URL_IOBUF_SIZE;

#define	RPMURL_DEBUG_IO		0x40000000
#define	RPMURL_DEBUG_REFS	0x20000000

int _url_debug = 0;
#define	DBG(_f, _m, _x)	if ((_url_debug | (_f)) & (_m)) fprintf _x

#define DBGIO(_f, _x)	DBG((_f), RPMURL_DEBUG_IO, _x)
#define DBGREFS(_f, _x)	DBG((_f), RPMURL_DEBUG_REFS, _x)

/*@only@*/ /*@null@*/ static urlinfo *uCache = NULL;
static int uCount = 0;

urlinfo XurlLink(urlinfo u, const char *msg, const char *file, unsigned line)
{
    URLSANE(u);
    u->nrefs++;
DBGREFS(0, (stderr, "--> url %p ++ %d %s at %s:%u\n", u, u->nrefs, msg, file, line));
    return u;
}

urlinfo XurlNew(const char *msg, const char *file, unsigned line)
{
    urlinfo u;
    if ((u = xmalloc(sizeof(*u))) == NULL)
	return NULL;
    memset(u, 0, sizeof(*u));
    u->proxyp = -1;
    u->port = -1;
    u->urltype = URL_IS_UNKNOWN;
    u->ctrl = NULL;
    u->data = NULL;
    u->bufAlloced = 0;
    u->buf = NULL;
    u->httpHasRange = 1;
    u->httpVersion = 0;
    u->nrefs = 0;
    u->magic = URLMAGIC;
    return XurlLink(u, msg, file, line);
}

urlinfo XurlFree(urlinfo u, const char *msg, const char *file, unsigned line)
{
    URLSANE(u);
DBGREFS(0, (stderr, "--> url %p -- %d %s at %s:%u\n", u, u->nrefs, msg, file, line));
    if (--u->nrefs > 0)
	return u;
    if (u->ctrl) {
#ifndef	NOTYET
	void * fp = fdGetFp(u->ctrl);
	if (fp) {
	    fdPush(u->ctrl, fpio, fp, -1);   /* Push fpio onto stack */
	    Fclose(u->ctrl);
	} else if (fdio->_fileno(u->ctrl) >= 0)
	    fdio->close(u->ctrl);
#else
	Fclose(u->ctrl);
#endif

	u->ctrl = fdio->_fdderef(u->ctrl, "persist ctrl (urlFree)", file, line);
	if (u->ctrl)
	    fprintf(stderr, _("warning: u %p ctrl %p nrefs != 0 (%s %s)\n"),
			u, u->ctrl, u->host, u->service);
    }
    if (u->data) {
#ifndef	NOTYET
	void * fp = fdGetFp(u->data);
	if (fp) {
	    fdPush(u->data, fpio, fp, -1);   /* Push fpio onto stack */
	    Fclose(u->data);
	} else if (fdio->_fileno(u->data) >= 0)
	    fdio->close(u->data);
#else
	Fclose(u->ctrl);
#endif

	u->data = fdio->_fdderef(u->data, "persist data (urlFree)", file, line);
	if (u->data)
	    fprintf(stderr, _("warning: u %p data %p nrefs != 0 (%s %s)\n"),
			u, u->data, u->host, u->service);
    }
    if (u->buf) {
	free(u->buf);
	u->buf = NULL;
    }
    FREE(u->url);
    FREE(u->service);
    FREE(u->user);
    FREE(u->password);
    FREE(u->host);
    FREE(u->portstr);
    FREE(u->proxyu);
    FREE(u->proxyh);

    /*@-refcounttrans@*/ FREE(u); /*@-refcounttrans@*/
    return NULL;
}

void urlFreeCache(void)
{
    int i;
    for (i = 0; i < uCount; i++) {
	if (uCache[i] == NULL) continue;
	uCache[i] = urlFree(uCache[i], "uCache");
	if (uCache[i])
	    fprintf(stderr, _("warning: uCache[%d] %p nrefs(%d) != 1 (%s %s)\n"),
		i, uCache[i], uCache[i]->nrefs,
		uCache[i]->host, uCache[i]->service);
    }
    if (uCache)
	free(uCache);
    uCache = NULL;
    uCount = 0;
}

static int urlStrcmp(const char *str1, const char *str2)
{
    if (str1 && str2)
	return (strcmp(str1, str2));
    if (str1 != str2)
	return -1;
    return 0;
}

static void urlFind(urlinfo *uret, int mustAsk)
{
    urlinfo u;
    int ucx;
    int i;

    if (uret == NULL)
	return;

    u = *uret;
    URLSANE(u);

    ucx = -1;
    for (i = 0; i < uCount; i++) {
	urlinfo ou;
	if ((ou = uCache[i]) == NULL) {
	    if (ucx < 0)
		ucx = i;
	    continue;
	}

	/* Check for cache-miss condition. A cache miss is
	 *    a) both items are not NULL and don't compare.
	 *    b) either of the items is not NULL.
	 */
	if (urlStrcmp(u->service, ou->service))
	    continue;
    	if (urlStrcmp(u->host, ou->host))
	    continue;
	if (urlStrcmp(u->user, ou->user))
	    continue;
	if (urlStrcmp(u->portstr, ou->portstr))
	    continue;
	break;	/* Found item in cache */
    }

    if (i == uCount) {
	if (ucx < 0) {
	    ucx = uCount++;
	    if (uCache)
		uCache = xrealloc(uCache, sizeof(*uCache) * uCount);
	    else
		uCache = xmalloc(sizeof(*uCache));
	}
	uCache[ucx] = urlLink(u, "uCache (miss)");
	u = urlFree(u, "urlSplit (urlFind miss)");
    } else {
	ucx = i;
	u = urlFree(u, "urlSplit (urlFind hit)");
    }

    /* This URL is now cached. */

    u = urlLink(uCache[ucx], "uCache");
    *uret = u;
    u = urlFree(u, "uCache (urlFind)");

    /* Zap proxy host and port in case they have been reset */
    u->proxyp = -1;
    FREE(u->proxyh);

    /* Perform one-time FTP initialization */
    if (u->urltype == URL_IS_FTP) {

	if (mustAsk || (u->user != NULL && u->password == NULL)) {
	    char * prompt;
	    prompt = alloca(strlen(u->host) + strlen(u->user) + 256);
	    sprintf(prompt, _("Password for %s@%s: "), u->user, u->host);
	    FREE(u->password);
	    u->password = xstrdup( /*@-unrecog@*/ getpass(prompt) /*@=unrecog@*/ );
	}

	if (u->proxyh == NULL) {
	    const char *proxy = rpmExpand("%{_ftpproxy}", NULL);
	    if (proxy && *proxy != '%') {
		const char *uu = (u->user ? u->user : "anonymous");
		char *nu = xmalloc(strlen(uu) + sizeof("@") + strlen(u->host));
		strcpy(nu, uu);
		strcat(nu, "@");
		strcat(nu, u->host);
		u->proxyu = nu;
		u->proxyh = xstrdup(proxy);
	    }
	    xfree(proxy);
	}

	if (u->proxyp < 0) {
	    const char *proxy = rpmExpand("%{_ftpport}", NULL);
	    if (proxy && *proxy != '%') {
		char *end;
		int port = strtol(proxy, &end, 0);
		if (!(end && *end == '\0')) {
		    fprintf(stderr, _("error: %sport must be a number\n"),
			u->service);
		    return;
		}
		u->proxyp = port;
	    }
	    xfree(proxy);
	}
    }

    /* Perform one-time HTTP initialization */
    if (u->urltype == URL_IS_HTTP) {

	if (u->proxyh == NULL) {
	    const char *proxy = rpmExpand("%{_httpproxy}", NULL);
	    if (proxy && *proxy != '%')
		u->proxyh = xstrdup(proxy);
	    xfree(proxy);
	}

	if (u->proxyp < 0) {
	    const char *proxy = rpmExpand("%{_httpport}", NULL);
	    if (proxy && *proxy != '%') {
		char *end;
		int port = strtol(proxy, &end, 0);
		if (!(end && *end == '\0')) {
		    fprintf(stderr, _("error: %sport must be a number\n"),
			u->service);
		    return;
		}
		u->proxyp = port;
	    }
	    xfree(proxy);
	}

    }

    return;
}

static struct urlstring {
    const char *leadin;
    urltype	ret;
} urlstrings[] = {
    { "file://",	URL_IS_PATH },
    { "ftp://",		URL_IS_FTP },
    { "http://",	URL_IS_HTTP },
    { "-",		URL_IS_DASH },
    { NULL,		URL_IS_UNKNOWN }
};

urltype urlIsURL(const char * url) {
    struct urlstring *us;

    if (url && *url) {
	for (us = urlstrings; us->leadin != NULL; us++) {
	    if (strncmp(url, us->leadin, strlen(us->leadin)))
		continue;
	    return us->ret;
	}
    }

    return URL_IS_UNKNOWN;
}

/* Return path portion of url (or pointer to NUL if url == NULL) */
int urlPath(const char * url, const char ** pathp)
{
    const char *path;
    int urltype;

    path = url;
    urltype = urlIsURL(url);
    switch (urltype) {
    case URL_IS_FTP:
	url += sizeof("ftp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTP:
    case URL_IS_PATH:
	url += sizeof("file://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_UNKNOWN:
	if (path == NULL) path = "";
	break;
    case URL_IS_DASH:
	path = "";
	break;
    }
    if (pathp)
	*pathp = path;
    return urltype;
}

/*
 * Split URL into components. The URL can look like
 *	service://user:password@host:port/path
 */
int urlSplit(const char * url, urlinfo *uret)
{
    urlinfo u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = urlNew("urlSplit")) == NULL)
	return -1;

    if ((se = s = myurl = xstrdup(url)) == NULL) {
	u = urlFree(u, "urlSplit (error #1)");
	return -1;
    }

    u->url = xstrdup(url);
    u->urltype = urlIsURL(url);

    while (1) {
	/* Point to end of next item */
	while (*se && *se != '/') se++;
	/* Item was service. Save service and go for the rest ...*/
    	if (*se && (se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->service = xstrdup(s);
	    se += 2;	/* skip over "//" */
	    s = se++;
	    continue;
	}
	
	/* Item was everything-but-path. Continue parse on rest */
	*se = '\0';
	break;
    }

    /* Look for ...@host... */
    fe = f = s;
    while (*fe && *fe != '@') fe++;
    if (*fe == '@') {
	s = fe + 1;
	*fe = '\0';
    	/* Look for user:password@host... */
	while (fe > f && *fe != ':') fe--;
	if (*fe == ':') {
	    *fe++ = '\0';
	    u->password = xstrdup(fe);
	}
	u->user = xstrdup(f);
    }

    /* Look for ...host:port */
    fe = f = s;
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = xstrdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (!(end && *end == '\0')) {
		rpmMessage(RPMMESS_ERROR, _("url port must be a number\n"));
		if (myurl) free(myurl);
		u = urlFree(u, "urlSplit (error #3)");
		return -1;
	    }
	}
    }
    u->host = xstrdup(f);

    if (u->port < 0 && u->service != NULL) {
	struct servent *serv;
	serv = /*@-unrecog@*/ getservbyname(u->service, "tcp") /*@=unrecog@*/;
	if (serv != NULL)
	    u->port = ntohs(serv->s_port);
	else if (u->urltype == URL_IS_FTP)
	    u->port = IPPORT_FTP;
	else if (u->urltype == URL_IS_HTTP)
	    u->port = IPPORT_HTTP;
    }

    if (myurl) free(myurl);
    if (uret) {
	*uret = u;
	urlFind(uret, 0);
    }
    return 0;
}

int urlGetFile(const char * url, const char * dest) {
    int rc;
    FD_t sfd = NULL;
    FD_t tfd = NULL;
    const char * sfuPath = NULL;
    int urlType = urlPath(url, &sfuPath);

    if (*sfuPath == '\0')
	return FTPERR_UNKNOWN;
	
    sfd = Fopen(url, "r.ufdio");
    if (sfd == NULL || Ferror(sfd)) {
	rpmMessage(RPMMESS_DEBUG, _("failed to open %s: %s\n"), url, Fstrerror(sfd));
	rc = FTPERR_UNKNOWN;
	goto exit;
    }

    if (dest == NULL) {
	if ((dest = strrchr(sfuPath, '/')) != NULL)
	    dest++;
	else
	    dest = sfuPath;
    }

    tfd = Fopen(dest, "w.ufdio");
if (_url_debug)
fprintf(stderr, "*** urlGetFile sfd %p %s tfd %p %s\n", sfd, url, tfd, dest);
    if (tfd == NULL || Ferror(tfd)) {
	/* XXX Fstrerror */
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s: %s\n"), dest, Fstrerror(tfd));
	rc = FTPERR_UNKNOWN;
	goto exit;
    }

    switch (urlType) {
    case URL_IS_FTP:
    case URL_IS_HTTP:
    case URL_IS_PATH:
    case URL_IS_DASH:
    case URL_IS_UNKNOWN:
	if ((rc = ufdGetFile(sfd, tfd))) {
	    Unlink(dest);
	    /* XXX FIXME: sfd possibly closed by copyData */
	    /*@-usereleased@*/ Fclose(sfd) /*@=usereleased@*/ ;
	}
	sfd = NULL;	/* XXX Fclose(sfd) done by ufdGetFile */
	break;
    default:
	rc = FTPERR_UNKNOWN;
	break;
    }

exit:
    if (tfd)
	Fclose(tfd);
    if (sfd)
	Fclose(sfd);

    return rc;
}
