#include "system.h"

#include <netinet/in.h>

#include "build/rpmbuild.h"

#include "url.h"
#include "ftp.h"

struct pwcacheEntry {
    char * machine;
    char * account;
    char * pw;
} ;

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

#if DYING
static char * getFtpPassword(char * machine, char * account, int mustAsk);
static int urlFtpLogin(const char * url, char ** fileNamePtr);
#endif

void freeUrlinfo(urlinfo *u)
{
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

static char * getFtpPassword(char * machine, char * account, int mustAsk)
{
    static /*@only@*/ struct pwcacheEntry * pwCache = NULL;
    static int pwCount = 0;
    int i;
    char * prompt;

    for (i = 0; i < pwCount; i++) {
	if (!strcmp(pwCache[i].machine, machine) &&
	    !strcmp(pwCache[i].account, account))
		break;
    }

    if (i < pwCount && !mustAsk) {
	return pwCache[i].pw;
    } else if (i == pwCount) {
	pwCount++;
	if (pwCache)
	    pwCache = realloc(pwCache, sizeof(*pwCache) * pwCount);
	else
	    pwCache = malloc(sizeof(*pwCache));

	pwCache[i].machine = strdup(machine);
	pwCache[i].account = strdup(account);
    } else
	free(pwCache[i].pw);

    prompt = alloca(strlen(machine) + strlen(account) + 50);
    sprintf(prompt, _("Password for %s@%s: "), account, machine);

    pwCache[i].pw = strdup(getpass(prompt));

    return pwCache[i].pw;
}

static int urlFtpLogin(const char * url, char ** fileNamePtr)
{
    urlinfo *u;
    char *proxy;
    char *proxyport;
    int ftpconn;
   
    rpmMessage(RPMMESS_DEBUG, _("getting %s via anonymous ftp\n"), url);

    if (urlSplit(url, &u))
	return -1;

    rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
		u->host,
		u->user ? u->user : "ftp", 
		u->password ? u->password : "(username)");

    if ((proxy = rpmGetVar(RPMVAR_FTPPROXY)) != NULL) {
	u->user = realloc(u->user, (strlen(u->user) + strlen(u->host) + 2) );
	strcat(u->user, "@");
	strcat(u->user, u->host);
	free(u->host);
	u->host = strdup(proxy);
    }

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

    ftpconn = ftpOpen(u);

    if (fileNamePtr && ftpconn >= 0)
	*fileNamePtr = strdup(u->path);

    freeUrlinfo(u);
    return ftpconn;
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

int urlSplit(const char * url, urlinfo **uret)
{
    urlinfo *u;
    char *myurl;
    char *s, *se, *f, *fe;

    if ((u = newUrlinfo()) == NULL)
	return -1;

    if ((se = s = myurl = strdup(url)) == NULL) {
	freeUrlinfo(u);
	return -1;
    }
    do {
	while (*se && *se != '/') se++;
	if (*se == '\0') {
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
		fprintf(stderr, _("error: url port must be a number\n"));
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
	    u->port = se->s_port;
	else if (!strcasecmp(u->service, "ftp"))
	    u->port = IPPORT_FTP;
	else if (!strcasecmp(u->service, "http"))
	    u->port = IPPORT_HTTP;

	/* XXX move elsewhere */
	if (!strcmp(u->service, "ftp") && u->user && u->password == NULL) {
	    u->password = getFtpPassword(u->host, u->user, 0);
	    if (u->password)
		u->password = strdup(u->password);
	}
    }

    if (myurl) free(myurl);
    if (uret)
	*uret = u;
    else
	freeUrlinfo(u);
    return 0;
}

int ufdClose(FD_t fd)
{
    if (fd != NULL && fd->fd_url) {
	int fdno = ((urlinfo *)fd->fd_url)->ftpControl;
	if (fdno >= 0)
	    ftpClose(fdno);
	free(fd->fd_url);
	fd->fd_url = NULL;
    }
    return fdClose(fd);
}

FD_t ufdOpen(const char *url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    urlinfo *u;

    switch (urlIsURL(url)) {
    case URL_IS_FTP:
	if ((fd = fdNew()) == NULL)
	    break;
	if ((u = newUrlinfo()) == NULL)
	    break;
    {	char * fileName;
	if ((u->ftpControl = urlFtpLogin(url, &fileName)) < 0)
	    break;
	fd->fd_fd = ftpGetFileDesc(u->ftpControl, fileName);
	free(fileName);
    }	break;
    case URL_IS_HTTP:
	if ((fd = fdNew()) == NULL)
	    break;
	if (urlSplit(url, &u))
	    break;
	fd->fd_url = u;
	fd->fd_fd = httpOpen(u);
	break;
    case URL_IS_PATH:
	if (urlSplit(url, &u))
	    break;
	fd = fdOpen(u->path, flags, mode);
	freeUrlinfo(u);
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

int urlGetFile(char * url, char * dest) {
    char * fileName;
    int ftpconn;
    int rc;
    FD_t fd;

    rpmMessage(RPMMESS_DEBUG, _("getting %s via anonymous ftp\n"), url);

    if ((ftpconn = urlFtpLogin(url, &fileName)) < 0) return ftpconn;

    fd = fdOpen(dest, O_CREAT|O_WRONLY|O_TRUNC, 0600);
    if (fdFileno(fd) < 0) {
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s\n"), dest);
	ftpClose(ftpconn);
	free(fileName);
	return FTPERR_UNKNOWN;
    }

    if ((rc = ftpGetFile(ftpconn, fileName, fd))) {
	free(fileName);
	unlink(dest);
	fdClose(fd);
	ftpClose(ftpconn);
	return rc;
    }    

    free(fileName);

    ftpClose(ftpconn);

    return rc;
}
