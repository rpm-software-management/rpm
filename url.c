#include "system.h"

#include "build/rpmbuild.h"

#include "ftp.h"

typedef struct {
	int	ftpControl;
} urlContext;

#include "url.h"

struct pwcacheEntry {
    char * machine;
    char * account;
    char * pw;
} ;

static char * getFtpPassword(char * machine, char * account, int mustAsk);
static int urlFtpLogin(const char * url, char ** fileNamePtr);
static int urlFtpSplit(char * url, char ** user, char ** pw, char ** host, 
		char ** path);

static char * getFtpPassword(char * machine, char * account, int mustAsk) {
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

static int urlFtpSplit(char * url, char ** user, char ** pw, char ** host, 
		char ** path) {
    char * chptr, * machineName, * fileName;
    char * userName = NULL;
    char * password = NULL;

    url += 6;			/* skip ftp:// */

    chptr = url;
    while (*chptr && (*chptr != '/')) chptr++;
    if (!*chptr) return -1;

    machineName = url;		/* could still have user:pass@ though */
    fileName = chptr;

    *path = strdup(chptr);
 
    *chptr = '\0';

    chptr = fileName;
    while (chptr > url && *chptr != '@') chptr--;
    if (chptr > url) {		/* we have a username */
	*chptr = '\0';
	userName = machineName;
	machineName = chptr + 1;
	
	chptr = userName;
	while (*chptr && *chptr != ':') chptr++;
	if (*chptr) {		/* we have a password */
	    *chptr = '\0';
	    password = chptr + 1;
	}
    }
	
    if (userName && !password) {
	password = getFtpPassword(machineName, userName, 0);
    } 

    if (userName)
	*user = strdup(userName);

    if (password)
	*pw = strdup(password);

    *host = strdup(machineName);

    return 0;
}

static int urlFtpLogin(const char * url, char ** fileNamePtr) {
    char * buf;
    char * machineName, * fileName;
    char * userName = NULL;
    char * password = NULL;
    char * proxy;
    char * portStr, * endPtr;
    int port;
    int ftpconn;
   
    rpmMessage(RPMMESS_DEBUG, _("getting %s via anonymous ftp\n"), url);

    buf = alloca(strlen(url) + 1);
    strcpy(buf, url);

    urlFtpSplit(buf, &userName, &password, &machineName, &fileName);

    rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"), machineName,
		userName ? userName : "ftp", 
		password ? password : "(username)");

    proxy = rpmGetVar(RPMVAR_FTPPROXY);
    portStr = rpmGetVar(RPMVAR_FTPPORT);
    if (!portStr) {
	port = -1;
    } else {
	port = strtol(portStr, &endPtr, 0);
	if (*endPtr) {
	    fprintf(stderr, _("error: ftpport must be a number\n"));
	    return -1;
	}
    }

    ftpconn = ftpOpen(machineName, userName, password, proxy, port);

    free(machineName);
    free(userName);
    free(password);

    if (ftpconn < 0) {
	free(fileName);
	return ftpconn;
    }

    *fileNamePtr = fileName;

    return ftpconn;
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

int ufdClose(FD_t fd)
{
    if (fd != NULL && fd->fd_url) {
	int fdno = ((urlContext *)fd->fd_url)->ftpControl;
	if (fdno >= 0)
	    ftpClose(fdno);
	free(fd->fd_url);
	fd->fd_url = NULL;
    }
    return fdClose(fd);
}

FD_t ufdOpen(const char *url, int flags, mode_t mode)
{
    FD_t fd;

    switch (urlIsURL(url)) {
    case URL_IS_FTP:
	if ((fd = fdNew()) == NULL)
	    return NULL;
	if ((fd->fd_url = malloc(sizeof(urlContext))) == NULL)
	    return NULL;;
	{   urlContext *context = (urlContext *)fd->fd_url;
	    char * fileName;
	    if ((context->ftpControl = urlFtpLogin(url, &fileName)) < 0) {
		ufdClose(fd);
		return NULL;
	    }
	    fd->fd_fd = ftpGetFileDesc(context->ftpControl, fileName);
	    free(fileName);
	}
	break;
    case URL_IS_DASH:
	fd = fdDup(STDIN_FILENO);
	break;
    case URL_IS_UNKNOWN:
    case URL_IS_PATH:
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
