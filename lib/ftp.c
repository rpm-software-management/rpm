#include "system.h"

#include <assert.h>

#if !defined(HAVE_CONFIG_H)
#define HAVE_MACHINE_TYPES_H 1
#define HAVE_ALLOCA_H 1
#define HAVE_NETINET_IN_SYSTM_H 1
#define HAVE_SYS_SOCKET_H 1
#endif

#if ! HAVE_HERRNO
extern int h_errno;
#endif

#include <stdarg.h>

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
typedef	unsigned int		uint32_t;
#define	INADDR_ANY		((uint32_t) 0x00000000)
#define	IPPROTO_IP		0
extern int h_errno;

#else	/* __LCLINT__ */

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#if HAVE_NETINET_IN_SYSTM_H
# include <sys/types.h>
# include <netinet/in_systm.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#endif	/* __LCLINT__ */

#include <rpmlib.h>

#if !defined(HAVE_INET_ATON)
int inet_aton(const char *cp, struct in_addr *inp);
#endif

#if defined(USE_ALT_DNS) && USE_ALT_DNS 
#include "dns.h"
#endif

#include <rpmurl.h>
/*@access urlinfo@*/

#ifdef __MINT__
# ifndef EAGAIN
#  define EAGAIN EWOULDBLOCK
# endif
# ifndef O_NONBLOCK
#  define O_NONBLOCK O_NDELAY
# endif
#endif

static int ftp_debug = 0;
#define DBG(_f, _x)     if ((ftp_debug | (_f))) fprintf _x

static int checkResponse(urlinfo u, int fdno, int secs, int *ecp,
		/*@out@*/ char ** str)
{
    static char buf[BUFSIZ + 1];	/* XXX Yuk! */
    int bufLength = 0; 
    fd_set emptySet, readSet;
    const char *s;
    char *se;
    struct timeval timeout, *tvp = (secs < 0 ? NULL : &timeout);
    int bytesRead, rc = 0;
    int doesContinue = 1;
    char errorCode[4];
 
    assert(secs > 0);
    errorCode[0] = '\0';
    u->httpContentLength = -1;
    
    do {
	/*
	 * XXX In order to preserve both getFile and getFd methods with
	 * XXX HTTP, the response is read 1 char at a time with breaks on
	 * XXX newlines.
	 */
	do {
	    FD_ZERO(&emptySet);
	    FD_ZERO(&readSet);
	    FD_SET(fdno, &readSet);

	    if (tvp) {
		tvp->tv_sec = secs;
		tvp->tv_usec = 0;
	    }
    
	    rc = select(fdno + 1, &readSet, &emptySet, &emptySet, tvp);
	    if (rc < 1) {
		if (rc == 0) 
		    return FTPERR_BAD_SERVER_RESPONSE;
		else
		    rc = FTPERR_UNKNOWN;
	    } else
		rc = 0;

	    se = buf + bufLength;
	    bytesRead = read(fdno, se, 1);
	    bufLength += bytesRead;
	    buf[bufLength] = '\0';
	} while (bufLength < sizeof(buf) && *se != '\n');

	/*
	 * Divide the response into lines. Skip continuation lines.
	 */
	for (s = se = buf; *s != '\0'; s = se) {
		const char *e;

		while (*se && *se != '\n') se++;

		if (se > s && se[-1] == '\r')
		   se[-1] = '\0';
		if (*se == '\0')
		    break;

		DBG(0, (stderr, "<- %s\n", s));

		/* HTTP: header termination on empty line */
		if (*s == '\0') {
		    doesContinue = 0;
		    break;
		}
		*se++ = '\0';

		/* HTTP: look for "HTTP/1.1 123 ..." */
		if (!strncmp(s, "HTTP", sizeof("HTTP")-1)) {
		    if ((e = strchr(s, '.')) != NULL) {
			e++;
			u->httpVersion = *e - '0';
			if (u->httpVersion < 1 || u->httpVersion > 2)
			    u->httpPersist = u->httpVersion = 0;
		    }
		    if ((e = strchr(s, ' ')) != NULL) {
			e++;
			if (strchr("0123456789", *e))
			    strncpy(errorCode, e, 3);
			errorCode[3] = '\0';
		    }
		    continue;
		}

		/* HTTP: look for "<token>: ..." */
		for (e = s; *e && !(*e == ' ' || *e == ':'); e++)
		    ;
		if (e > s && *e++ == ':') {
		    size_t ne = (e - s);
		    while (*e && *e == ' ') e++;
#if 0
		    if (!strncmp(s, "Date:", ne)) {
		    } else
		    if (!strncmp(s, "Server:", ne)) {
		    } else
		    if (!strncmp(s, "Last-Modified:", ne)) {
		    } else
		    if (!strncmp(s, "ETag:", ne)) {
		    } else
#endif
		    if (!strncmp(s, "Accept-Ranges:", ne)) {
			if (!strcmp(e, "bytes"))
			    u->httpHasRange = 1;
			if (!strcmp(e, "none"))
			    u->httpHasRange = 0;
		    } else
		    if (!strncmp(s, "Content-Length:", ne)) {
			if (strchr("0123456789", *e))
			    u->httpContentLength = atoi(e);
		    } else
		    if (!strncmp(s, "Connection:", ne)) {
			if (!strcmp(e, "close"))
			    u->httpPersist = 0;
		    } else
#if 0
		    if (!strncmp(s, "Content-Type:", ne)) {
		    } else
		    if (!strncmp(s, "Transfer-Encoding:", ne)) {
		    } else
		    if (!strncmp(s, "Allow:", ne)) {
		    } else
#endif
			;
		    continue;
		}

		/* FTP: look for "123-" and/or "123 " */
		if (strchr("0123456789", *s)) {
		    if (errorCode[0]) {
			if (!strncmp(s, errorCode, sizeof("123")-1) && s[3] == ' ')
			    doesContinue = 0;
		    } else {
			strncpy(errorCode, s, sizeof("123")-1);
			errorCode[3] = '\0';
			if (s[3] != '-')
			    doesContinue = 0;
		    }
		}
	}

	if (doesContinue && se > s) {
	    bufLength = se - s - 1;
	    if (s != buf)
		memcpy(buf, s, bufLength);
	} else {
	    bufLength = 0;
	}
    } while (doesContinue && !rc);

    if (str)	*str = buf;
    if (ecp)	*ecp = atoi(errorCode);

    return rc;
}

int ftpCheckResponse(urlinfo u, /*@out@*/ char ** str)
{
    int ec = 0;
    int secs = fdGetRdTimeoutSecs(u->ftpControl);
    int rc =  checkResponse(u, fdio->fileno(u->ftpControl), secs, &ec, str);

    switch (ec) {
    case 550:
	return FTPERR_FILE_NOT_FOUND;
	/*@notreached@*/ break;
    case 552:
	return FTPERR_NIC_ABORT_IN_PROGRESS;
	/*@notreached@*/ break;
    default:
	if (ec >= 400 && ec <= 599)
	    return FTPERR_BAD_SERVER_RESPONSE;
	break;
    }
    return rc;
}

static int ftpCommand(urlinfo u, char * command, ...)
{
    va_list ap;
    int len;
    char * s;
    char * req;

    va_start(ap, command);
    len = strlen(command) + 2;
    s = va_arg(ap, char *);
    while (s) {
	len += strlen(s) + 1;
	s = va_arg(ap, char *);
    }
    va_end(ap);

    req = alloca(len + 1);

    va_start(ap, command);
    strcpy(req, command);
    strcat(req, " ");
    s = va_arg(ap, char *);
    while (s) {
	strcat(req, s);
	strcat(req, " ");
	s = va_arg(ap, char *);
    }
    va_end(ap);

    req[len - 2] = '\r';
    req[len - 1] = '\n';
    req[len] = '\0';

    DBG(0, (stderr, "-> %s", req));
    if (fdio->write(u->ftpControl, req, len) != len)
	return FTPERR_SERVER_IO_ERROR;

    return ftpCheckResponse(u, NULL);
}

#if !defined(USE_ALT_DNS) || !USE_ALT_DNS 
static int mygethostbyname(const char * host, struct in_addr * address)
{
    struct hostent * hostinfo;

    hostinfo = /*@-unrecog@*/ gethostbyname(host) /*@=unrecog@*/;
    if (!hostinfo) return 1;

    memcpy(address, hostinfo->h_addr_list[0], sizeof(*address));
    return 0;
}
#endif

static int getHostAddress(const char * host, struct in_addr * address)
{
    if (isdigit(host[0])) {
      if (! /*@-unrecog@*/ inet_aton(host, address) /*@=unrecog@*/ ) {
	  return FTPERR_BAD_HOST_ADDR;
      }
    } else {
      if (mygethostbyname(host, address)) {
	  errno = h_errno;
	  return FTPERR_BAD_HOSTNAME;
      }
    }
    
    return 0;
}

static int tcpConnect(const char *host, int port)
{
    struct sockaddr_in sin;
    int sock = -1;
    int rc;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;
    
  do {
    if ((rc = getHostAddress(host, &sin.sin_addr)) < 0)
	break;

    if ((sock = socket(sin.sin_family, SOCK_STREAM, IPPROTO_IP)) < 0) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }

    if (connect(sock, (struct sockaddr *) &sin, sizeof(sin))) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }
  } while (0);

    if (rc < 0 && sock >= 0) {
	close(sock);
	return rc;
    }

    DBG(0, (stderr,"++ connect %s:%d on fd %d\n",
	/*@-unrecog@*/ inet_ntoa(sin.sin_addr) /*@=unrecog@*/ ,
	ntohs(sin.sin_port), sock));

    return sock;
}

int httpOpen(urlinfo u, const char *httpcmd)
{
    int sock;
    const char *host;
    const char *path;
    int port;
    char *req;
    size_t len;

    if (u == NULL || ((host = (u->proxyh ? u->proxyh : u->host)) == NULL))
	return FTPERR_BAD_HOSTNAME;

    if ((port = (u->proxyp > 0 ? u->proxyp : u->port)) < 0) port = 80;

    path = (u->proxyh || u->proxyp > 0) ? u->url : u->path;
    if ((sock = tcpConnect(host, port)) < 0)
	return sock;

    len = sizeof("\
req x HTTP/1.0\r\n\
User-Agent: rpm/3.0.4\r\n\
Host: y:z\r\n\
Accept: text/plain\r\n\
\r\n\
") + strlen(httpcmd) + strlen(path) + sizeof(VERSION) + strlen(host) + 20;

    req = alloca(len);
    *req = '\0';

    sprintf(req, "\
%s %s HTTP/1.%d\r\n\
User-Agent: rpm/%s\r\n\
Host: %s:%d\r\n\
Accept: text/plain\r\n\
\r\n\
",	httpcmd, path, (u->httpVersion ? 1 : 0), VERSION, host, port);

    if (write(sock, req, len) != len) {
	close(sock);
	return FTPERR_SERVER_IO_ERROR;
    }

    DBG(0, (stderr, "-> %s", req));

  { int ec = 0;
    int secs = fdGetRdTimeoutSecs(u->ftpControl);
    int rc;
    rc = checkResponse(u, sock, secs, &ec, NULL);

    switch (ec) {
    default:
	if (rc == 0 && ec != 200)	/* not HTTP_OK */
	    rc = FTPERR_FILE_NOT_FOUND;
	break;
    }

    if (rc < 0) {
	close(sock);
	return rc;
    }
  }

    return sock;
}

int ftpOpen(urlinfo u)
{
    const char * host;
    const char * user;
    const char * password;
    int port;
    int secs;
    int rc;

    if (u == NULL || ((host = (u->proxyh ? u->proxyh : u->host)) == NULL))
	return FTPERR_BAD_HOSTNAME;

    if ((port = (u->proxyp > 0 ? u->proxyp : u->port)) < 0) port = IPPORT_FTP;

    if ((user = (u->proxyu ? u->proxyu : u->user)) == NULL)
	user = "anonymous";

    if ((password = u->password) == NULL) {
	if (getuid()) {
	    struct passwd * pw = getpwuid(getuid());
	    char *myp = alloca(strlen(pw->pw_name) + sizeof("@"));
	    strcpy(myp, pw->pw_name);
	    strcat(myp, "@");
	    password = myp;
	} else {
	    password = "root@";
	}
    }

    secs = fdGetRdTimeoutSecs(u->ftpControl);
    fdSetFdno(u->ftpControl, tcpConnect(host, port));
    if (fdio->fileno(u->ftpControl) < 0)
	return fdio->fileno(u->ftpControl);

    /* ftpCheckResponse() assumes the socket is nonblocking */
    if (fcntl(fdio->fileno(u->ftpControl), F_SETFL, O_NONBLOCK)) {
	rc = FTPERR_FAILED_CONNECT;
	goto errxit;
    }

    if ((rc = ftpCheckResponse(u, NULL))) {
	return rc;     
    }

    if ((rc = ftpCommand(u, "USER", user, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, "PASS", password, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, "TYPE", "I", NULL)))
	goto errxit;

    fdLink(u->ftpControl, "open ftpControl");
    return fdio->fileno(u->ftpControl);

errxit:
    fdio->close(u->ftpControl);
    return rc;
}

int ftpFileDone(urlinfo u)
{
    if (u == NULL)
	return FTPERR_UNKNOWN;	/* XXX W2DO? */

    if (u->ftpFileDoneNeeded) {
	u->ftpFileDoneNeeded = 0;
	fdFree(u->ftpControl, "ftpFileDone (from ftpFileDone)");
	if (ftpCheckResponse(u, NULL))
	    return FTPERR_BAD_SERVER_RESPONSE;
    }
    return 0;
}

int ftpFileDesc(urlinfo u, const char *cmd, FD_t fd)
{
    struct sockaddr_in dataAddress;
    int i, j;
    char * passReply;
    char * chptr;
    int rc;

    if (u == NULL)
	return FTPERR_UNKNOWN;	/* XXX W2DO? */

/*
 * XXX When ftpFileDesc() is called, there may be a lurking
 * XXX transfer complete message (if ftpFileDone() was not
 * XXX called to clear that message). Clear that message now.
 */

    if (u->ftpFileDoneNeeded)
	rc = ftpFileDone(u);

    DBG(0, (stderr, "-> PASV\n"));
    if (fdio->write(u->ftpControl, "PASV\r\n", 6) != 6)
	return FTPERR_SERVER_IO_ERROR;

    if ((rc = ftpCheckResponse(u, &passReply)))
	return FTPERR_PASSIVE_ERROR;

    chptr = passReply;
    while (*chptr && *chptr != '(') chptr++;
    if (*chptr != '(') return FTPERR_PASSIVE_ERROR; 
    chptr++;
    passReply = chptr;
    while (*chptr && *chptr != ')') chptr++;
    if (*chptr != ')') return FTPERR_PASSIVE_ERROR;
    *chptr-- = '\0';

    while (*chptr && *chptr != ',') chptr--;
    if (*chptr != ',') return FTPERR_PASSIVE_ERROR;
    chptr--;
    while (*chptr && *chptr != ',') chptr--;
    if (*chptr != ',') return FTPERR_PASSIVE_ERROR;
    *chptr++ = '\0';
    
    /* now passReply points to the IP portion, and chptr points to the
       port number portion */

    dataAddress.sin_family = AF_INET;
    if (sscanf(chptr, "%d,%d", &i, &j) != 2)
	return FTPERR_PASSIVE_ERROR;
    dataAddress.sin_port = htons((((unsigned)i) << 8) + j);

    chptr = passReply;
    while (*chptr++) {
	if (*chptr == ',') *chptr = '.';
    }

    if (!inet_aton(passReply, &dataAddress.sin_addr))
	return FTPERR_PASSIVE_ERROR;

    fdSetFdno(fd, socket(AF_INET, SOCK_STREAM, IPPROTO_IP));
    if (fdio->fileno(fd) < 0)
	return FTPERR_FAILED_CONNECT;
    /* XXX setsockopt SO_LINGER */
    /* XXX setsockopt SO_KEEPALIVE */
    /* XXX setsockopt SO_TOS IPTOS_THROUGHPUT */

    while (connect(fdio->fileno(fd), (struct sockaddr *) &dataAddress, 
	        sizeof(dataAddress)) < 0) {
	if (errno == EINTR)
	    continue;
	fdio->close(fd);
	return FTPERR_FAILED_DATA_CONNECT;
    }

    DBG(0, (stderr, "-> %s", cmd));
    i = strlen(cmd);
    if (fdio->write(u->ftpControl, cmd, i) != i)
	return FTPERR_SERVER_IO_ERROR;

    if ((rc = ftpCheckResponse(u, NULL))) {
	fdio->close(fd);
	return rc;
    }

    u->ftpFileDoneNeeded = 1;
    fdLink(u->ftpControl, "ftpFileDone");
    return 0;
}
