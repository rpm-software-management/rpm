/* 
 * This handles all HTTP transfers.
 * 
 * Written by Alex deVries <puffin@redhat.com>
 * 
 * To do:
 *  - better HTTP response code handling
 *  - HTTP proxy authentication
 *  - non-peek parsing of the header so that querying works
 */


#include <netinet/in.h>

#include "system.h"
#include "build/rpmbuild.h"
#include "url.h"
#include "ftp.h"
#include "http.h"

static int httpDebug = 0;

int httpProxySetup(const char * url, urlinfo ** uret)
{
    urlinfo *u;

    if (urlSplit(url, &u) < 0)
	return -1;

    if (!strcmp(u->service, "http")) {
        char *newpath;
	char *proxy;
	char *proxyport;

	rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
		u->host,
		u->user ? u->user : "ftp",
		u->password ? u->password : "(username)");

	if ((proxy = rpmGetVar(RPMVAR_HTTPPROXY)) != NULL) {
            newpath = malloc(strlen((*uret)->host)+
                             strlen((*uret)->path) + 7 + 6 + 1 );

            sprintf(newpath,"http://%s:%i%s",
                 (*uret)->host,(*uret)->port,(*uret)->path);
	    u->host = strdup(proxy);
            free(u->path);
            u->path = newpath;
	}

	if ((proxyport = rpmGetVar(RPMVAR_HTTPPORT)) != NULL) {
	    int port;
	    char *end;
	    port = strtol(proxyport, &end, 0);
	    if (*end) {
		fprintf(stderr, _("error: httport must be a number\n"));
		return -1;
	    }
            u->port=port;
	}
    }

    if (uret != NULL)
	*uret = u;
    return 0;
}

int httpOpen(urlinfo *u)
{
    int sock;
    FILE * sockfile;
    const char *host;
    int port;
    char *buf;
    size_t len;
    
    if (u == NULL || ((host = u->host) == NULL))
        return HTTPERR_BAD_HOSTNAME;

    if ((port = u->port) < 0) port = 80;
    
    if ((sock = tcpConnect(host, port)) < 0)
        return sock;

    len = strlen(u->path) + sizeof("GET  HTTP 1.0\r\n\r\n"); 
    buf = alloca(len);
    strcpy(buf, "GET ");
    strcat(buf, u->path);
    strcat(buf, " HTTP 1.0\r\n"); 
    strcat(buf,"\r\n");

    sockfile = fdopen(sock,"r+");

    if (write(sock, buf, len) != len) {
        close(sock);
        return HTTPERR_SERVER_IO_ERROR;
    }
    
    if (httpDebug) fprintf(stderr, "-> %s", buf);

    return sock;
}

#define httpTimeoutSecs 15
#define BUFFER_SIZE 2048

int httpSkipHeader(FD_t sfd, char *buf,int * bytesRead, char ** start) {

    int doesContinue = 1;
    int dataHere = 0;
    char errorCode[4];
    fd_set emptySet, readSet;
    char * chptr ;
    struct timeval timeout;
    int rc = 0;
    int bufLength = 0;

    errorCode[0] = '\0';


    do {
	FD_ZERO(&emptySet);
	FD_ZERO(&readSet);
 	FD_SET(sfd->fd_fd, &readSet);

	timeout.tv_sec = httpTimeoutSecs;
	timeout.tv_usec = 0;
    
	rc = select(sfd->fd_fd+1, &readSet, &emptySet, &emptySet, &timeout);
	if (rc < 1) {
	    if (rc==0) 
		return HTTPERR_BAD_SERVER_RESPONSE;
	    else
		rc = HTTPERR_UNKNOWN;
	} else
	    rc = 0;

	*bytesRead = read(sfd->fd_fd, buf + bufLength, 
             *bytesRead - bufLength - 1);

	bufLength += (*bytesRead);

	buf[bufLength] = '\0';

	/* divide the response into lines, checking each one to see if 
	   we are finished or need to continue */

	*start = chptr =buf;

        if (!dataHere) {
          do {
              while (*chptr != '\n' && *chptr) chptr++;

	        if (*chptr == '\n') {
		   *chptr = '\0';
		if (*(chptr - 1) == '\r') *(chptr - 1) = '\0';

                if ((!strncmp(*start,"HTTP",4)) && 
                    (strchr(*start,' '))) {
                   *start = strchr(*start,' ')+1;
                   if (!strncmp(*start,"200",3))  {
			doesContinue = 1;
                   }
		} else {
                    if (**start == '\0')  {
			dataHere = 1;
                    }
		}
		*start = chptr + 1;
		chptr++;
	    } else {
		chptr++;
	    }
	  } while (chptr && !dataHere);
        }
    } while (doesContinue && !rc && !dataHere);
    return 0;
}

int httpGetFile(FD_t sfd, FD_t tfd) {

    static char buf[BUFFER_SIZE + 1];
    int bufLength = 0; 
    int bytesRead = BUFFER_SIZE, rc = 0;
    char * start;

    httpSkipHeader(sfd,buf,&bytesRead,&start); 
    
    /* Write the buffer out to tfd */

    if (write (tfd->fd_fd,start,bytesRead-(start-buf))<0) {
        return HTTPERR_SERVER_IO_ERROR;
    }

    while (1) {
	bytesRead = read(sfd->fd_fd, buf, 
             sizeof(buf)-1);
        if (!bytesRead)  return 0;
        if (write (tfd->fd_fd,buf,bytesRead)<0) {
           return HTTPERR_SERVER_IO_ERROR;
         }
    }    
    return 0;
}

