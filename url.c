#include "system.h"

#include <netinet/in.h>

#include "build/rpmbuild.h"

#include "url.h"
#include "ftp.h"
#include "http.h"

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
    FREE(u->service);
    FREE(u->user);
    FREE(u->password);
    FREE(u->host);
    FREE(u->portstr);
    FREE(u->path);

    FREE(u);
}

urlinfo *newUrlinfo(void)
{
    urlinfo *u;
    if ((u = malloc(sizeof(*u))) == NULL)
	return NULL;
    memset(u, 0, sizeof(*u));
    u->port = -1;
    u->ftpControl = -1;
    return u;
}

static void findUrlinfo(urlinfo **uret, int mustAsk)
{
    urlinfo *u;
    urlinfo **empty;
    static urlinfo **uCache = NULL;
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
	if (u->service && ou->service && strcmp(ou->service, u->service))
	    continue;
	if (u->host && ou->host && strcmp(ou->host, u->host))
	    continue;
	if (u->user && ou->user && strcmp(ou->user, u->user))
	    continue;
	if (u->password && ou->password && strcmp(ou->password, u->password))
	    continue;
	if (u->portstr && ou->portstr && strcmp(ou->portstr, u->portstr))
	    continue;
	break;
    }

    if (i == uCount) {
	if (empty == NULL) {
	    uCount++;
	    if (uCache)
		uCache = realloc(uCache, sizeof(*uCache) * uCount);
	    else
		uCache = malloc(sizeof(*uCache));
	    empty = &uCache[i];
	}
	*empty = u;
    } else {
	const char *up = uCache[i]->path;
	uCache[i]->path = u->path;
	u->path = up;
	freeUrlinfo(u);
    }

    *uret = u = uCache[i];

    if (!strcmp(u->service, "ftp")) {
	if (mustAsk || (u->user != NULL && u->password == NULL)) {
	    char * prompt;
	    FREE(u->password);
	    prompt = alloca(strlen(u->host) + strlen(u->user) + 40);
	    sprintf(prompt, _("Password for %s@%s: "), u->user, u->host);
	    u->password = strdup(getpass(prompt));
	}
    }
    return;
}

int urlSplit(const char * url, urlinfo **uret)
{
    urlinfo *u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = newUrlinfo()) == NULL)
	return -1;

    if ((se = s = myurl = strdup(url)) == NULL) {
	freeUrlinfo(u);
	return -1;
    }
    do {
	while (*se && *se != '/') se++;
	if (*se == '\0') {
	    /* XXX can't find path */
	    if (myurl) free(myurl);
	    freeUrlinfo(u);
	    return -1;
	}
    	if ((se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->service = strdup(s);
	    se += 2;
	    s = se++;
	    continue;
	}
        u->path = strdup(se);
	*se = '\0';
	break;
    } while (1);

    fe = f = s;
    while (*fe && *fe != '@') fe++;
    if (*fe == '@') {
	s = fe + 1;
	*fe = '\0';
	while (fe > f && *fe != ':') fe--;
	if (*fe == ':') {
	    *fe++ = '\0';
	    u->password = strdup(fe);
	}
	u->user = strdup(f);
    }

    fe = f = s;
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = strdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (*end) {
		rpmMessage(RPMMESS_ERROR, _("url port must be a number\n"));
		if (myurl) free(myurl);
		freeUrlinfo(u);
		return -1;
	    }
	}
    }
    u->host = strdup(f);

    if (u->port < 0 && u->service != NULL) {
	struct servent *se;
	if ((se = getservbyname(u->service, "tcp")) != NULL)
	    u->port = ntohs(se->s_port);
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

static int urlConnect(const char * url, urlinfo ** uret)
{
    urlinfo *u;

    if (urlSplit(url, &u) < 0)
	return -1;

    if (!strcmp(u->service, "ftp") && u->ftpControl < 0) {
	char *proxy;
	char *proxyport;

	rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
		u->host,
		u->user ? u->user : "ftp",
		u->password ? u->password : "(username)");

	/* XXX FIXME: this doesn't work with urlinfo caching */
	if ((proxy = rpmGetVar(RPMVAR_FTPPROXY)) != NULL) {
	    char *nu = malloc(strlen(u->user) + strlen(u->host) + sizeof("@"));
	    strcpy(nu, u->user);
	    strcat(nu, "@");
	    strcat(nu, u->host);
	    free((void *)u->user);
	    u->user = nu;
	    free((void *)u->host);
	    u->host = strdup(proxy);
	}

	/* XXX FIXME: this doesn't work with urlinfo caching */
	if ((proxyport = rpmGetVar(RPMVAR_FTPPORT)) != NULL) {
	    int port;
	    char *end;
	    port = strtol(proxyport, &end, 0);
	    if (*end) {
		fprintf(stderr, _("error: ftpport must be a number\n"));
		return -1;
	    }
	    u->port = port;
	}

	u->ftpControl = ftpOpen(u);

 	if (u->ftpControl < 0)
	    return u->ftpControl;

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
	if (u->ftpControl >= 0)
	    ftpAbort(fd);
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
	if (ftpGetFileDesc(fd) < 0)
	    break;
	break;
    case URL_IS_HTTP:
	if (urlSplit(url, &u))
	    break;
	if ((fd = fdNew()) == NULL)
	    break;
        if (httpProxySetup(url,&u)) 
	    break;
	fd->fd_url = u;
	fd->fd_fd = httpOpen(u);
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
