/** \ingroup rpmio
 * \file rpmio/url.c
 */

#include "system.h"

#include <netinet/in.h>

#include <rpmmacro.h>
#include <rpmmessages.h>
#include <rpmio_internal.h>

#include "debug.h"

#ifndef	IPPORT_FTP
#define	IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif
#ifndef	IPPORT_HTTPS
#define	IPPORT_HTTPS	443
#endif
#ifndef	IPPORT_PGPKEYSERVER
#define	IPPORT_PGPKEYSERVER	11371
#endif

/**
 */
int _url_iobuf_size = RPMURL_IOBUF_SIZE;

/**
 */
int _url_debug = 0;

#define	URLDBG(_f, _m, _x)	if ((_url_debug | (_f)) & (_m)) fprintf _x

#define URLDBGIO(_f, _x)	URLDBG((_f), RPMURL_DEBUG_IO, _x)
#define URLDBGREFS(_f, _x)	URLDBG((_f), RPMURL_DEBUG_REFS, _x)

/**
 */
urlinfo *_url_cache = NULL;

/**
 */
int _url_count = 0;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
static inline void *
_free(const void * p)
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

urlinfo XurlLink(urlinfo u, const char *msg, const char *file, unsigned line)
{
    URLSANE(u);
    u->nrefs++;
URLDBGREFS(0, (stderr, "--> url %p ++ %d %s at %s:%u\n", u, u->nrefs, msg, file, line));
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
    u->nrefs = 0;
    u->magic = URLMAGIC;
    return XurlLink(u, msg, file, line);
}

urlinfo XurlFree(urlinfo u, const char *msg, const char *file, unsigned line)
{
    URLSANE(u);
URLDBGREFS(0, (stderr, "--> url %p -- %d %s at %s:%u\n", u, u->nrefs, msg, file, line));
    if (--u->nrefs > 0)
	return u;
    u->url = _free(u->url);
    u->scheme = _free((void *)u->scheme);
    u->user = _free((void *)u->user);
    u->password = _free((void *)u->password);
    u->host = _free((void *)u->host);
    u->portstr = _free((void *)u->portstr);
    u->proxyu = _free((void *)u->proxyu);
    u->proxyh = _free((void *)u->proxyh);

    u = _free(u);
    return NULL;
}

void urlFreeCache(void)
{
    if (_url_cache) {
	int i;
	for (i = 0; i < _url_count; i++) {
	    if (_url_cache[i] == NULL) continue;
	    _url_cache[i] = urlFree(_url_cache[i], "_url_cache");
	    if (_url_cache[i])
		fprintf(stderr,
			_("warning: _url_cache[%d] %p nrefs(%d) != 1 (%s %s)\n"),
			i, _url_cache[i], _url_cache[i]->nrefs,
			(_url_cache[i]->host ? _url_cache[i]->host : ""),
			(_url_cache[i]->scheme ? _url_cache[i]->scheme : ""));
	}
    }
    _url_cache = _free(_url_cache);
    _url_count = 0;
}

static int urlStrcmp(const char * str1, const char * str2)
{
    if (str1)
	if (str2)
	    return strcmp(str1, str2);
    if (str1 != str2)
	return -1;
    return 0;
}

static void urlFind(urlinfo * uret, int mustAsk)
{
    urlinfo u;
    int ucx;
    int i = 0;

    if (uret == NULL)
	return;

    u = *uret;
    URLSANE(u);

    ucx = -1;
    for (i = 0; i < _url_count; i++) {
	urlinfo ou = NULL;
	if (_url_cache == NULL || (ou = _url_cache[i]) == NULL) {
	    if (ucx < 0)
		ucx = i;
	    continue;
	}

	/* Check for cache-miss condition. A cache miss is
	 *    a) both items are not NULL and don't compare.
	 *    b) either of the items is not NULL.
	 */
	if (urlStrcmp(u->scheme, ou->scheme))
	    continue;
    	if (urlStrcmp(u->host, ou->host))
	    continue;
	if (urlStrcmp(u->user, ou->user))
	    continue;
	if (urlStrcmp(u->portstr, ou->portstr))
	    continue;
	break;	/* Found item in cache */
    }

    if (i == _url_count) {
	if (ucx < 0) {
	    ucx = _url_count++;
	    _url_cache = xrealloc(_url_cache, sizeof(*_url_cache) * _url_count);
	}
	if (_url_cache)		/* XXX always true */
	    _url_cache[ucx] = urlLink(u, "_url_cache (miss)");
	u = urlFree(u, "urlSplit (urlFind miss)");
    } else {
	ucx = i;
	u = urlFree(u, "urlSplit (urlFind hit)");
    }

    /* This URL is now cached. */

    if (_url_cache)		/* XXX always true */
	u = urlLink(_url_cache[ucx], "_url_cache");
    *uret = u;
    u = urlFree(u, "_url_cache (urlFind)");

    /* Zap proxy host and port in case they have been reset */
    u->proxyp = -1;
    u->proxyh = _free(u->proxyh);

    return;
}

/**
 */
static struct urlstring {
    const char * leadin;
    urltype	ret;
} urlstrings[] = {
    { "file://",	URL_IS_PATH },
    { "ftp://",		URL_IS_FTP },
    { "hkp://",		URL_IS_HKP },
    { "http://",	URL_IS_HTTP },
    { "https://",	URL_IS_HTTPS },
    { "-",		URL_IS_DASH },
    { NULL,		URL_IS_UNKNOWN }
};

urltype urlIsURL(const char * url)
{
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
urltype urlPath(const char * url, const char ** pathp)
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
    case URL_IS_PATH:
	url += sizeof("file://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HKP:
	url += sizeof("hkp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTP:
	url += sizeof("http://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTPS:
	url += sizeof("https://") - 1;
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
 *	scheme://user:password@host:port/path
  * or as in RFC2732 for IPv6 address
  *    service://user:password@[ip:v6:ad:dr:es:s]:port/path
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
	/* Item was scheme. Save scheme and go for the rest ...*/
    	if (*se && (se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->scheme = xstrdup(s);
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

    /* Look for ...host:port or [v6addr]:port*/
    fe = f = s;
    if (strchr(fe, '[') && strchr(fe, ']'))
    {
	    fe = strchr(f, ']');
	    *f++ = '\0';
	    *fe++ = '\0';
    }
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = xstrdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (!(end && *end == '\0')) {
		rpmMessage(RPMMESS_ERROR, _("url port must be a number\n"));
		myurl = _free(myurl);
		u = urlFree(u, "urlSplit (error #3)");
		return -1;
	    }
	}
    }
    u->host = xstrdup(f);

    if (u->port < 0 && u->scheme != NULL) {
	struct servent *serv;
	/* HACK hkp:// might lookup "pgpkeyserver" */
	serv = getservbyname(u->scheme, "tcp");
	if (serv != NULL)
	    u->port = ntohs(serv->s_port);
	else if (u->urltype == URL_IS_FTP)
	    u->port = IPPORT_FTP;
	else if (u->urltype == URL_IS_HKP)
	    u->port = IPPORT_PGPKEYSERVER;
	else if (u->urltype == URL_IS_HTTP)
	    u->port = IPPORT_HTTP;
	else if (u->urltype == URL_IS_HTTPS)
	    u->port = IPPORT_HTTPS;
    }

    myurl = _free(myurl);
    if (uret) {
	*uret = u;
/* FIX: rpmGlobalMacroContext not in <rpmlib.h> */
	urlFind(uret, 0);
    }
    return 0;
}

int urlGetFile(const char * url, const char * dest)
{
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

    if (dest == NULL)
	return FTPERR_UNKNOWN;

    tfd = Fopen(dest, "w.ufdio");
if (_url_debug)
fprintf(stderr, "*** urlGetFile sfd %p %s tfd %p %s\n", sfd, url, (tfd ? tfd : NULL), dest);
    if (tfd == NULL || Ferror(tfd)) {
	/* XXX Fstrerror */
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s: %s\n"), dest, Fstrerror(tfd));
	rc = FTPERR_UNKNOWN;
	goto exit;
    }

    switch (urlType) {
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_PATH:
    case URL_IS_DASH:
    case URL_IS_UNKNOWN:
	if ((rc = ufdGetFile(sfd, tfd))) {
	    (void) Unlink(dest);
	    /* XXX FIXME: sfd possibly closed by copyData */
	    (void) Fclose(sfd) ;
	}
	sfd = NULL;	/* XXX Fclose(sfd) done by ufdGetFile */
	break;
    default:
	rc = FTPERR_UNKNOWN;
	break;
    }

exit:
    if (tfd)
	(void) Fclose(tfd);
    if (sfd)
	(void) Fclose(sfd);

    return rc;
}
