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

typedef /*@owned@*/ urlinfo * urlinfop;
/*@only@*/ /*@null@*/ static urlinfop *uCache = NULL;
static int uCount = 0;

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

void freeUrlinfoCache(void)
{
    int i;
    for (i = 0; i < uCount; i++) {
	if (uCache[i])
	    freeUrlinfo(uCache[i]);
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

static void findUrlinfo(urlinfo **uret, int mustAsk)
{
    urlinfo *u;
    urlinfo **empty;
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

    while (1) {
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

int urlGetFile(const char * url, const char * dest) {
    int rc;
    FD_t sfd = NULL;
    FD_t tfd = NULL;
    urlinfo * sfu;

    sfd = ufdOpen(url, O_RDONLY, 0);
    if (sfd == NULL || Ferror(sfd)) {
	/* XXX Fstrerror */
	rpmMessage(RPMMESS_DEBUG, _("failed to open %s\n"), url);
	Fclose(sfd);
	return FTPERR_UNKNOWN;
    }

    sfu = ufdGetUrlinfo(sfd);

    if (sfu != NULL && dest == NULL) {
	const char *fileName = sfu->path;
	if ((dest = strrchr(fileName, '/')) != NULL)
	    dest++;
	else
	    dest = fileName;
    }

    tfd = fdOpen(dest, O_CREAT|O_WRONLY|O_TRUNC, 0600);
    if (Ferror(tfd)) {
	/* XXX Fstrerror */
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s\n"), dest);
	Fclose(tfd);
	Fclose(sfd);
	return FTPERR_UNKNOWN;
    }

    switch (urlIsURL(url)) {
    case URL_IS_FTP:
	if ((rc = ftpGetFile(sfd, tfd))) {
	    unlink(dest);
	    /*@-usereleased@*/ Fclose(sfd) /*@=usereleased@*/ ;
	}
	/* XXX Fclose(sfd) done by copyData */
	break;
    case URL_IS_HTTP:
    case URL_IS_PATH:
    case URL_IS_DASH:
	if ((rc = httpGetFile(sfd, tfd))) {
	    unlink(dest);
	    /*@-usereleased@*/ Fclose(sfd) /*@=usereleased@*/ ;
	}
	/* XXX Fclose(sfd) done by copyData */
	break;
    case URL_IS_UNKNOWN:
    default:
	rc = FTPERR_UNKNOWN;
	break;
    }

    Fclose(tfd);

    return rc;
}
