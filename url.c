#include <alloca.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ftp.h"
#include "messages.h"
#include "rpmlib.h"
#include "url.h"

struct pwcacheEntry {
    char * machine;
    char * account;
    char * pw;
} ;

static char * getFtpPassword(char * machine, char * account, int mustAsk);
static int urlFtpLogin(char * url, char ** fileNamePtr);
static int urlFtpSplit(char * url, char ** user, char ** pw, char ** host, 
		char ** path);

static char * getFtpPassword(char * machine, char * account, int mustAsk) {
    static struct pwcacheEntry * pwCache = NULL;
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
    sprintf(prompt, "Password for %s@%s: ", account, machine);

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

static int urlFtpLogin(char * url, char ** fileNamePtr) {
    char * buf;
    char * machineName, * fileName;
    char * userName = NULL;
    char * password = NULL;
    char * proxy;
    char * portStr, * endPtr;
    int port;
    int ftpconn;
   
    rpmMessage(RPMMESS_DEBUG, "getting %s via anonymous ftp\n", url);

    buf = alloca(strlen(url) + 1);
    strcpy(buf, url);

    urlFtpSplit(buf, &userName, &password, &machineName, &fileName);

    rpmMessage(RPMMESS_DEBUG, "logging into %s as %s, pw %s\n", machineName,
		userName ? userName : "ftp", 
		password ? password : "(username)");

    proxy = rpmGetVar(RPMVAR_FTPPROXY);
    portStr = rpmGetVar(RPMVAR_FTPPORT);
    if (!portStr) {
	port = -1;
    } else {
	port = strtol(portStr, &endPtr, 0);
	if (*endPtr) {
	    fprintf(stderr, "error: ftpport must be a number\n");
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

int urlGetFd(char * url, struct urlContext * context) {
    char * fileName;
    int fd;

    rpmMessage(RPMMESS_DEBUG, "getting %s via anonymous ftp\n", url);

    if ((context->ftpControl = urlFtpLogin(url, &fileName)) < 0) 
	return context->ftpControl;

    fd = ftpGetFileDesc(context->ftpControl, fileName);

    free(fileName);

    if (fd < 0) ftpClose(context->ftpControl);

    return fd;
}

int urlFinishedFd(struct urlContext * context) {
    ftpClose(context->ftpControl);

    return 0;
}

int urlGetFile(char * url, char * dest) {
    char * fileName;
    int ftpconn;
    int rc;
    int fd;

    rpmMessage(RPMMESS_DEBUG, "getting %s via anonymous ftp\n", url);

    if ((ftpconn = urlFtpLogin(url, &fileName)) < 0) return ftpconn;

    fd = creat(dest, 0600);

    if (fd < 0) {
	rpmMessage(RPMMESS_DEBUG, "failed to create %s\n", dest);
	ftpClose(ftpconn);
	free(fileName);
	return FTPERR_UNKNOWN;
    }

    if ((rc = ftpGetFile(ftpconn, fileName, fd))) {
	free(fileName);
	unlink(dest);
	close(fd);
	ftpClose(ftpconn);
	return rc;
    }    

    free(fileName);

    ftpClose(ftpconn);

    return rc;
}

int urlIsURL(char * url) {
    if (!strncmp(url, "ftp://", 6)) return 1;

    return 0;
}
