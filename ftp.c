#include <alloca.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/protocols.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define TIMEOUT_SECS 60
#define BUFFER_SIZE 4096

#include "ftp.h"
#include "lib/messages.h"

static int ftpCheckResponse(int sock);
static int ftpCommand(int sock, char * command, ...);
static int ftpReadData(int sock, int out);
static int getHostAddress(const char * host, struct in_addr * address);

static int ftpCheckResponse(int sock) {
    char buf[BUFFER_SIZE + 1];
    int bufLength = 0; 
    fd_set emptySet, readSet;
    char * chptr, * start;
    struct timeval timeout;
    int bytesRead, rc;
    int doesContinue = 1;
    
    while (doesContinue) {
	FD_ZERO(&emptySet);
	FD_ZERO(&readSet);
	FD_SET(sock, &readSet);

	timeout.tv_sec = TIMEOUT_SECS;
	timeout.tv_usec = 0;
    
	rc = select(sock + 1, &readSet, &emptySet, &emptySet, &timeout);
	if (rc < 1) {
	    if (rc==0) 
		return FTPERR_BAD_SERVER_RESPONSE;
	    else
		return FTPERR_UNKNOWN;
	}

	/* We got a response - make sure none of the response codes are in the
	   400's or 500's. That would indicate a problem */

	bytesRead = read(sock, buf + bufLength, sizeof(buf) - bufLength);

	bufLength += bytesRead;

	buf[bufLength] = '\0';

	start = chptr = buf;
	if (start[3] == '-') 
	    doesContinue = 1;
	else
	    doesContinue = 0;

	if (*start == '4' || *start == '5') {
	    return FTPERR_BAD_SERVER_RESPONSE;
	}
	while (chptr < (bufLength + buf)) {
	    if (*chptr == '\n') {
		start = chptr + 1;
		if ((start - buf) < bufLength) {
		    if (start[3] == '-') 
			doesContinue = 1;
		    else
			doesContinue = 0;
		    if (*start == '4' || *start == '5') {
		        return FTPERR_BAD_SERVER_RESPONSE;
		    }
		}
	    }
	    chptr++;
	}

	if (*(chptr - 1) != '\n') {
	    memcpy(buf, start, chptr - start);
	    bufLength = chptr - start;
	} else {
	    bufLength = 0;
	}
    }

    return 0;
}

int ftpCommand(int sock, char * command, ...) {
    va_list ap;
    int len;
    char * s;
    char * buf;
    int rc;

    va_start(ap, command);
    len = strlen(command) + 1;
    s = va_arg(ap, char *);
    while (s) {
	len += strlen(s) + 1;
	s = va_arg(ap, char *);
    }
    va_end(ap);

    buf = alloca(len + 1);

    va_start(ap, command);
    strcpy(buf, command);
    strcat(buf, " ");
    s = va_arg(ap, char *);
    while (s) {
	strcat(buf, s);
	strcat(buf, " ");
	s = va_arg(ap, char *);
    }
    va_end(ap);

    buf[len - 1] = '\n';
    buf[len] = '\0';
     
    if (write(sock, buf, len) != len) {
        return FTPERR_SERVER_IO_ERROR;
    }

    if ((rc = ftpCheckResponse(sock)))
	return rc;

    return 0;
}

static int getHostAddress(const char * host, struct in_addr * address) {
    struct hostent * hostinfo;

    if (isdigit(host[0])) {
      if (inet_aton(host, address)) {
	  return FTPERR_BAD_HOST_ADDR;
      }
    } else {
      hostinfo = gethostbyname(host);
      if (!hostinfo) {
	  errno = h_errno;
	  return FTPERR_BAD_HOSTNAME;
      }
      
      memcpy(address, hostinfo->h_addr_list[0], hostinfo->h_length);
    }
    
    return 0;
}

int ftpOpen(char * host, char * name, char * password) {
    static int sock;
    /*static char * lastHost = NULL;*/
    struct in_addr serverAddress;
    struct sockaddr_in destPort;
    struct passwd * pw;
    int rc;

    if ((rc = getHostAddress(host, &serverAddress))) return rc;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        return FTPERR_FAILED_CONNECT;
    }

    destPort.sin_family = AF_INET;
    destPort.sin_port = htons(IPPORT_FTP);
    destPort.sin_addr = serverAddress;

    message(MESS_DEBUG, "establishing connection\n");

    if (connect(sock, (struct sockaddr *) &destPort, sizeof(destPort))) {
	close(sock);
        return FTPERR_FAILED_CONNECT;
    }

    /* ftpCheckResponse() assumes the socket is nonblocking */
    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
	close(sock);
        return FTPERR_FAILED_CONNECT;
    }

    if ((rc = ftpCheckResponse(sock))) {
        return rc;     
    }

    if (!name)
	name = "anonymous";

    if (!password) {
	pw = getpwuid(getuid());
	password = alloca(strlen(pw->pw_name) + 2);
	strcpy(password, pw->pw_name);
	strcat(password, "@");
    }

    message(MESS_DEBUG, "logging in\n");

    if ((rc = ftpCommand(sock, "USER", name, NULL))) {
	close(sock);
	return rc;
    }

    if ((rc = ftpCommand(sock, "PASS", password, NULL))) {
	close(sock);
	return rc;
    }

    if ((rc = ftpCommand(sock, "TYPE", "I", NULL))) {
	close(sock);
	return rc;
    }

    message(MESS_DEBUG, "logged into ftp site\n");

    return sock;
}

int ftpReadData(int sock, int out) {
    char buf[BUFFER_SIZE];
    fd_set emptySet, readSet;
    struct timeval timeout;
    int bytesRead, rc;
    
    while (1) {
	FD_ZERO(&emptySet);
	FD_ZERO(&readSet);
	FD_SET(sock, &readSet);

	timeout.tv_sec = TIMEOUT_SECS;
	timeout.tv_usec = 0;
    
	rc = select(sock + 1, &readSet, &emptySet, &emptySet, &timeout);
	if (rc == 0) {
	    close(sock);
	    return FTPERR_SERVER_TIMEOUT;
	} else if (rc < 0) {
	    close(sock);
	    return FTPERR_UNKNOWN;
	}

	bytesRead = read(sock, buf, sizeof(buf));
	if (bytesRead == 0) {
	    close(sock);
	    return 0;
	}

	if (write(out, buf, bytesRead) != bytesRead) {
	    close(sock);
	    return FTPERR_FILE_IO_ERROR;
	}
    }
}

int ftpGetFile(int sock, char * remotename, int dest) {
    int dataSocket, trSocket;
    struct sockaddr_in myAddress, dataAddress;
    int i;
    char portbuf[64];
    char numbuf[20];
    char * dotAddress;
    char * chptr;
    unsigned short dataPort;
    int rc;

    i = sizeof(myAddress);
    if (getsockname(sock, (struct sockaddr *) &myAddress, &i)) {
        return FTPERR_UNKNOWN;
    }

    dataAddress.sin_family = AF_INET;
    dataAddress.sin_port = 0;
    dataAddress.sin_addr = myAddress.sin_addr;

    dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (dataSocket < 0) {
        return FTPERR_FAILED_CONNECT;
    }

    if (bind(dataSocket, (struct sockaddr *) &dataAddress, 
	     sizeof(dataAddress))) {
        close(dataSocket);
        return FTPERR_FAILED_CONNECT;
    }

    if (listen(dataSocket, 1)) {
	close(dataSocket);
	return FTPERR_UNKNOWN;
    }

    if (getsockname(dataSocket, (struct sockaddr *) &dataAddress, &i)) {
 	close(dataSocket);
	return FTPERR_UNKNOWN;
    }

    dotAddress = inet_ntoa(dataAddress.sin_addr);
    dataPort = ntohs(dataAddress.sin_port);

    strcpy(portbuf, dotAddress);
    for (chptr = portbuf; *chptr; chptr++)
	if (*chptr == '.') *chptr = ',';
   
    sprintf(numbuf, ",%d,%d", dataPort >> 8, dataPort & 0xFF);
    strcat(portbuf, numbuf);

    message(MESS_DEBUG, "sending PORT command\n");

    if ((rc = ftpCommand(sock, "PORT", portbuf, NULL))) {
	close(dataSocket);
	return rc;
    }

    message(MESS_DEBUG, "sending RETR command\n");

    if ((rc = ftpCommand(sock, "RETR", remotename, NULL))) {
        message(MESS_DEBUG, "RETR command failed\n");

	close(dataSocket);
	return rc;
    }

    i = sizeof(dataAddress);
    trSocket = accept(dataSocket, (struct sockaddr *) &dataAddress, &i);
    close(dataSocket);

    message(MESS_DEBUG, "data socket open\n");

    return ftpReadData(trSocket, dest);
}

void ftpClose(int sock) {
    close(sock);
}

const char *ftpStrerror(int errorNumber) {
  switch (errorNumber) {
    case FTPERR_BAD_SERVER_RESPONSE:
      return ("Bad FTP server response");

    case FTPERR_SERVER_IO_ERROR:
      return("FTP IO error");

    case FTPERR_SERVER_TIMEOUT:
      return("FTP server timeout");

    case FTPERR_BAD_HOST_ADDR:
      return("Unable to lookup FTP server host address");

    case FTPERR_BAD_HOSTNAME:
      return("Unable to lookup FTP server host name");

    case FTPERR_FAILED_CONNECT:
      return("Failed to connect to FTP server");

    case FTPERR_FILE_IO_ERROR:
      return("IO error to local file");

    case FTPERR_UNKNOWN:
    default:
      return("FTP Unknown or unexpected error");
  }
}
  
