/* 
 * This handles all HTTP transfers.
 * 
 * Written by Alex deVries <puffin@redhat.com>
 * 
 * To do:
 *  - HTTP proxy authentication
 *  - non-peek parsing of the header so that querying works
 */


#include <netinet/in.h>

#include "system.h"
#include "build/rpmbuild.h"
#include "url.h"
#include "ftp.h"
#include "http.h"

int httpProxySetup(const char * url, urlinfo ** uret)
{
    urlinfo *u;

    if (urlSplit(url, &u) < 0)
	return -1;

    if (!strcmp(u->service, "http")) {
        char *newpath;
	char *proxy;
	char *proxyport;

	rpmMessage(RPMMESS_DEBUG, _("logging into %s as pw %s\n"),
		u->host,
		u->user ? u->user : "(none)",
		u->password ? u->password : "(none)");

	if ((proxy = rpmGetVar(RPMVAR_HTTPPROXY)) != NULL) {
            if ((newpath = malloc(strlen((*uret)->host)+
                             strlen((*uret)->path) + 7 + 6 + 1 )) == NULL)
               return HTTPERR_UNKNOWN_ERROR;

            sprintf(newpath,"http://%s:%i%s",
                 (*uret)->host,(*uret)->port,(*uret)->path);
	    u->host = strdup(proxy);
            free(u->path);   /* Get rid of the old one */
            u->path = newpath;
	}

	if ((proxyport = rpmGetVar(RPMVAR_HTTPPORT)) != NULL) {
	    int port;
	    char *end;
	    port = strtol(proxyport, &end, 0);
	    if (*end) {
		fprintf(stderr, _("error: httport must be a number\n"));
		return HTTPERR_INVALID_PORT;
	    }
            u->port=port;
	}
    } else {
      *uret = NULL;
      return HTTPERR_UNSUPPORTED_PROTOCOL;
    }

    if (uret != NULL)
	*uret = u;
    return HTTPERR_OKAY;
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
    sprintf(buf,"GET %s HTTP 1.0\r\n\r\n",u->path);
/*
    strcpy(buf, "GET ");
    strcat(buf, u->path);
    strcat(buf, " HTTP 1.0\r\n"); 
    strcat(buf,"\r\n");
*/

    if(!(sockfile = fdopen(sock,"r+"))) {
        return HTTPERR_SERVER_IO_ERROR;
    }

    if (write(sock, buf, len) != len) {
        close(sock);
        return HTTPERR_SERVER_IO_ERROR;
    }

    rpmMessage(RPMMESS_DEBUG, _("Buffer: %s\n"), buf);

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
    unsigned int response;

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
		return HTTPERR_SERVER_TIMEOUT;
	    else
                return HTTPERR_SERVER_IO_ERROR;
	} else
	    rc = 0;

	*bytesRead = read(sfd->fd_fd, buf + bufLength, 
             *bytesRead - bufLength - 1);

        if (*bytesRead == -1) {
                return HTTPERR_SERVER_IO_ERROR;
        }

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

                if ((!strncmp(*start,"HTTP",4))) {
                   char *end;
                   *start = strchr(*start,' ');
                   response = strtol (*start,&end,0);
                   if ((*end != '\0') && (*end != ' ')) {
                      return HTTPERR_INVALID_SERVER_RESPONSE;
                   }
                   doesContinue = 1;
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

    switch (response) {
      case HTTP_OK:
          return 0;
      default:
          return HTTPERR_FILE_UNAVAILABLE;
    }
}

int httpGetFile(FD_t sfd, FD_t tfd) {

    static char buf[BUFFER_SIZE + 1];
    int bytesRead = BUFFER_SIZE, rc = 0;
    char * start;

    if ((rc = httpSkipHeader(sfd,buf,&bytesRead,&start)) != HTTPERR_OKAY)
            return rc;
    
    /* Write the buffer out to tfd */

    if (write (tfd->fd_fd,start,bytesRead-(start-buf))<0) 
        return HTTPERR_SERVER_IO_ERROR;
    do {
	bytesRead = read(sfd->fd_fd, buf, 
             sizeof(buf)-1);
        if (write (tfd->fd_fd,buf,bytesRead)<0) 
           return HTTPERR_SERVER_IO_ERROR;
    } while (bytesRead);
    return HTTPERR_OKAY;
}

