#include "system.h"

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include "rpmbuild.h"

#include <rpmurl.h>

/*@access FD_t@*/

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

void freeUrlinfo(urlinfo *u)
{
    if (u->ftpControl >= 0)
	close(u->ftpControl);
    FREE(u->url);
    FREE(u->service);
    FREE(u->user);
    FREE(u->password);
    FREE(u->host);
    FREE(u->portstr);
    FREE(u->path);
    FREE(u->proxyu);
    FREE(u->proxyh);

    FREE(u);
}

urlinfo *newUrlinfo(void)
{
    urlinfo *u;
    if ((u = xmalloc(sizeof(*u))) == NULL)
	return NULL;
    memset(u, 0, sizeof(*u));
    u->proxyp = -1;
    u->port = -1;
    u->ftpControl = -1;
    u->ftpGetFileDoneNeeded = 0;
    return u;
}

static int urlStrcmp(const char *str1, const char *str2)
{
    if (str1 && str2) {
	return (strcmp(str1, str2));
    } else
	if (str1 != str2)
	    return -1;
    return 0;
}

static void findUrlinfo(urlinfo **uret, int mustAsk)
{
    urlinfo *u;
    urlinfo **empty;
    /*@only@*/static urlinfo **uCache = NULL;
    static int uCount = 0;
    int i;

    if (uret == NULL)
	return;

    u = *uret;

    empty = NULL;
    for (i = 0; i < uCount; i++) {
	urlinfo *ou;
	if ((ou = uCache[i]) == NULL) {
	    if (empty == NULL)
		empty = &uCache[i];
	    continue;
	}
	/* Check for cache-miss condition. A cache miss is
	 *    a) both items are not NULL and don't compare.
	 *    b) either of the items is not NULL.
	 */
	if (urlStrcmp(u->service, ou->service))
	    continue;
	if (urlStrcmp(u->service, ou->service))
	    continue;
    	if (urlStrcmp(u->host, ou->host))
	    continue;
	if (urlStrcmp(u->user, ou->user))
	    continue;
	if (urlStrcmp(u->password, ou->password))
	    continue;
	if (urlStrcmp(u->portstr, ou->portstr))
	    continue;
	break;	/* Found item in cache */
    }

    if (i == uCount) {
	if (empty == NULL) {
	    uCount++;
	    if (uCache)
		uCache = xrealloc(uCache, sizeof(*uCache) * uCount);
	    else
		uCache = xmalloc(sizeof(*uCache));
	    empty = &uCache[i];
	}
	*empty = u;
    } else {
	/* Swap original url and path into the cached structure */
	const char *up = uCache[i]->path;
	uCache[i]->path = u->path;
	u->path = up;
	up = uCache[i]->url;
	uCache[i]->url = u->url;
	u->url = up;
	freeUrlinfo(u);
    }

    /* This URL is now cached. */
    *uret = u = uCache[i];

    /* Zap proxy host and port in case they have been reset */
    u->proxyp = -1;
    FREE(u->proxyh);

    /* Perform one-time FTP initialization */
    if (u->service && !strcmp(u->service, "ftp")) {

	if (mustAsk || (u->user != NULL && u->password == NULL)) {
	    char * prompt;
	    FREE(u->password);
	    prompt = alloca(strlen(u->host) + strlen(u->user) + 40);
	    sprintf(prompt, _("Password for %s@%s: "), u->user, u->host);
	    u->password = xstrdup(getpass(prompt));
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
    if (u->service && !strcmp(u->service, "http")) {

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

/*
 * Split URL into components. The URL can look like
 *	service://user:password@host:port/path
 */
int urlSplit(const char * url, urlinfo **uret)
{
    urlinfo *u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = newUrlinfo()) == NULL)
	return -1;

    if ((se = s = myurl = xstrdup(url)) == NULL) {
	freeUrlinfo(u);
	return -1;
    }

    u->url = xstrdup(url);

    do {
	/* Point to end of next item */
	while (*se && *se != '/') se++;
	if (*se == '\0') {
	    /* XXX can't find path */
	    if (myurl) free(myurl);
	    freeUrlinfo(u);
	    return -1;
	}
	/* Item was service. Save service and go for the rest ...*/
    	if ((se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->service = xstrdup(s);
	    se += 2;	/* skip over "//" */
	    s = se++;
	    continue;
	}
	
	/* Item was everything-but-path. Save path and continue parse on rest */
	u->path = xstrdup(se);
	*se = '\0';
	break;
    } while (1);

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
		freeUrlinfo(u);
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
	else if (!strcasecmp(u->service, "ftp"))
	    u->port = IPPORT_FTP;
	else if (!strcasecmp(u->service, "http"))
	    u->port = IPPORT_HTTP;
    }

    if (myurl) free(myurl);
    if (uret) {
	*uret = u;
	findUrlinfo(uret, 0);
    }
    return 0;
}

static int urlConnect(const char * url, /*@out@*/urlinfo ** uret)
{
    urlinfo *u;

    if (urlSplit(url, &u) < 0)
	return -1;

    if (!strcmp(u->service, "ftp") && u->ftpControl < 0) {

	u->ftpGetFileDoneNeeded = 0;	/* XXX PARANOIA */
	rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
		u->host,
		u->user ? u->user : "ftp",
		u->password ? u->password : "(username)");

	u->ftpControl = ftpOpen(u);

 	if (u->ftpControl < 0) {	/* XXX save ftpOpen error */
	    u->openError = u->ftpControl;
	    return u->ftpControl;
	}

    }

    if (uret != NULL)
	*uret = u;

    return 0;
}

urltype urlIsURL(const char * url)
{
    struct urlstring *us;

    for (us = urlstrings; us->leadin != NULL; us++) {
	if (strncmp(url, us->leadin, strlen(us->leadin)))
	    continue;
	return us->ret;
    }

    return URL_IS_UNKNOWN;
}

#ifdef	NOTYET
int urlAbort(FD_t fd)
{
    if (fd != NULL && fd->fd_url) {
	urlinfo *u = (urlinfo *)fd->fd_url;
	if (u->ftpControl >= 0)
	    ftpAbort(fd);
    }
}
#endif

int ufdClose(FD_t fd)
{
    if (fd != NULL && fd->fd_url) {
	urlinfo *u = (urlinfo *)fd->fd_url;
	/* Close the ftp control channel (not strictly necessary, but ... */
	if (u->ftpControl >= 0) {
	    ftpAbort(fd);
	    fd = NULL;	/* XXX ftpAbort does fdClose(fd) */
	    close(u->ftpControl);
	    u->ftpControl = -1;
	}
    }
    return fdClose(fd);
}

FD_t ufdOpen(const char *url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    urlinfo *u;

    switch (urlIsURL(url)) {
    case URL_IS_FTP:
	if (urlConnect(url, &u) < 0)
	    break;
	if ((fd = fdNew()) == NULL)
	    break;
	fd->fd_url = u;
	if ((u->openError = ftpGetFileDesc(fd)) < 0) {
	    u->ftpControl = -1;
	    fd = NULL;	/* XXX fd already closed */
	}
	break;
    case URL_IS_HTTP:
	if (urlSplit(url, &u))
	    break;
	if ((fd = fdNew()) == NULL)
	    break;
	fd->fd_url = u;
	fd->fd_fd = httpOpen(u);
	if (fd->fd_fd < 0)		/* XXX Save httpOpen error */
	    u->openError = fd->fd_fd;
	break;
    case URL_IS_PATH:
	if (urlSplit(url, &u))
	    break;
	fd = fdOpen(u->path, flags, mode);
	break;
    case URL_IS_DASH:
	fd = fdDup(STDIN_FILENO);
	break;
    case URL_IS_UNKNOWN:
    default:
	fd = fdOpen(url, flags, mode);
	break;
    }

    if (fd == NULL || fdFileno(fd) < 0) {
	ufdClose(fd);
	return NULL;
    }
    return fd;
}

int urlGetFile(const char * url, const char * dest) {
    int rc;
    FD_t sfd = NULL;
    FD_t tfd = NULL;

    sfd = ufdOpen(url, O_RDONLY, 0);
    if (sfd == NULL || fdFileno(sfd) < 0) {
	rpmMessage(RPMMESS_DEBUG, _("failed to open %s\n"), url);
	ufdClose(sfd);
	return FTPERR_UNKNOWN;
    }

    if (sfd->fd_url != NULL && dest == NULL) {
	const char *fileName = ((urlinfo *)sfd->fd_url)->path;
	if ((dest = strrchr(fileName, '/')) != NULL)
	    dest++;
	else
	    dest = fileName;
    }

    tfd = fdOpen(dest, O_CREAT|O_WRONLY|O_TRUNC, 0600);
    if (fdFileno(tfd) < 0) {
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s\n"), dest);
	fdClose(tfd);
	ufdClose(sfd);
	return FTPERR_UNKNOWN;
    }

    switch (urlIsURL(url)) {
    case URL_IS_FTP:
	if ((rc = ftpGetFile(sfd, tfd))) {
	    unlink(dest);
	    ufdClose(sfd);
	}
	/* XXX fdClose(sfd) done by copyData */
	break;
    case URL_IS_HTTP:
    case URL_IS_PATH:
    case URL_IS_DASH:
	if ((rc = httpGetFile(sfd, tfd))) {
	    unlink(dest);
	    ufdClose(sfd);
	}
	/* XXX fdClose(sfd) done by copyData */
	break;
    case URL_IS_UNKNOWN:
    default:
	rc = FTPERR_UNKNOWN;
	break;
    }

    fdClose(tfd);

    return rc;
}

const char *urlStrerror(const char *url)
{
    const char *retstr;
    urlinfo *u;
    switch (urlIsURL(url)) {
/* XXX This only works for httpOpen/ftpOpen/ftpGetFileDesc failures */
    case URL_IS_FTP:
    case URL_IS_HTTP:
	retstr = !urlSplit(url, &u)
		? ftpStrerror(u->openError) : "Malformed URL";
	break;
    default:
	retstr = strerror(errno);
	break;
    }
    return retstr;
}
