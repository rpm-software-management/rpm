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

#define TIMEOUT_SECS 20
#define BUFFER_SIZE 4096

#include "ftp.h"

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
	if (rc < 1) return 1;
    
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

	if (*start == '4' || *start == '5') return 1;
	while (chptr < (bufLength + buf)) {
	    if (*chptr == '\n') {
		start = chptr + 1;
		if ((start - buf) < bufLength) {
		    if (start[3] == '-') 
			doesContinue = 1;
		    else
			doesContinue = 0;
		    if (*start == '4' || *start == '5') return 1;
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
     
    if (write(sock, buf, len) != len)
	return 1;

    if (ftpCheckResponse(sock))
	return 1;

    return 0;
}

static int getHostAddress(const char * host, struct in_addr * address) {
    struct hostent * hostinfo;

    if (isdigit(host[0])) {
	if (inet_aton(host, address))
	    return 1;
    } else {
	hostinfo = gethostbyname(host);
	if (!hostinfo) {
	    errno = h_errno;
	    return 1;
	}

	memcpy(address, hostinfo->h_addr_list[0], hostinfo->h_length);
    }

    return 0;
}

/* returns -1 on error, error code in errno */
int ftpOpen(char * host, char * name, char * password) {
    static int sock;
    /*static char * lastHost = NULL;*/
    struct in_addr serverAddress;
    struct sockaddr_in destPort;
    struct passwd * pw;

    if (getHostAddress(host, &serverAddress)) return -1;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return -1;

    destPort.sin_family = AF_INET;
    destPort.sin_port = htons(IPPORT_FTP);
    destPort.sin_addr = serverAddress;

    if (connect(sock, (struct sockaddr *) &destPort, sizeof(destPort))) {
	close(sock);
	return -1;
    }

    /* ftpCheckResponse() assumes the socket is nonblocking */
    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
	close(sock);
	return -1;
    }

    ftpCheckResponse(sock);

    if (!name)
	name = "anonymous";

    if (!password) {
	pw = getpwuid(getuid());
	password = alloca(strlen(pw->pw_name) + 2);
	strcpy(password, pw->pw_name);
	strcat(password, "@");
    }

    if (ftpCommand(sock, "USER", name, NULL)) {
	close(sock);
	return -1;
    }

    if (ftpCommand(sock, "PASS", password, NULL)) {
	close(sock);
	return -1;
    }

    if (ftpCommand(sock, "TYPE", "I", NULL)) {
	close(sock);
	return -1;
    }

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
	    return 0;
	}
	if (rc < 0) {
	    close(sock);
	    return 1;
	}

	bytesRead = read(sock, buf, sizeof(buf));
	if (bytesRead == 0) {
	    close(sock);
	    return 0;
	}

	if (write(out, buf, bytesRead) != bytesRead) {
	    close(sock);
	    return 1;
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

    i = sizeof(myAddress);
    if (getsockname(sock, (struct sockaddr *) &myAddress, &i))
	return 1;

    dataAddress.sin_family = AF_INET;
    dataAddress.sin_port = 0;
    dataAddress.sin_addr = myAddress.sin_addr;

    dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (dataSocket < 0)
	return 1;

    if (bind(dataSocket, (struct sockaddr *) &dataAddress, 
	     sizeof(dataAddress))) {
	close(dataSocket);
	return 1;
    }

    if (listen(dataSocket, 1)) {
	close(dataSocket);
	return 1;
    }

    if (getsockname(dataSocket, (struct sockaddr *) &dataAddress, &i)) {
 	close(dataSocket);
	return 1;
    }

    dotAddress = inet_ntoa(dataAddress.sin_addr);
    dataPort = ntohs(dataAddress.sin_port);

    strcpy(portbuf, dotAddress);
    for (chptr = portbuf; *chptr; chptr++)
	if (*chptr == '.') *chptr = ',';
   
    sprintf(numbuf, ",%d,%d", dataPort >> 8, dataPort & 0xFF);
    strcat(portbuf, numbuf);

    if (ftpCommand(sock, "PORT", portbuf, NULL)) {
	close(dataSocket);
	return 1;
    }

    if (ftpCommand(sock, "RETR", remotename, NULL)) {
	close(dataSocket);
	return 1;
    }

    i = sizeof(dataAddress);
    trSocket = accept(dataSocket, (struct sockaddr *) &dataAddress, &i);
    close(dataSocket);

    return ftpReadData(trSocket, dest);
}

void ftpClose(int sock) {
    close(sock);
}

