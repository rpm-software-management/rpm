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

int _ftp_debug = 0;
#define DBG(_f, _x)     if ((_ftp_debug | (_f))) fprintf _x

static int checkResponse(urlinfo u, /*@out@*/ int *ecp, /*@out@*/ char ** str)
{
    char *buf;
    size_t bufAlloced;
    int bufLength = 0; 
    const char *s;
    char *se;
    int ec = 0;
    int moretodo = 1;
    char errorCode[4];
 
    URLSANE(u);
    buf = u->buf;
    bufAlloced = u->bufAlloced;
    *buf = '\0';

    errorCode[0] = '\0';
    
    do {
	int rc;

	/*
	 * Read next line from server.
	 */
	se = buf + bufLength;
	*se = '\0';
	rc = fdRdline(u->ctrl, se, (bufAlloced - bufLength));
	if (rc < 0) {
	    ec = FTPERR_BAD_SERVER_RESPONSE;
	    continue;
	} else if (rc == 0 || fdWritable(u->ctrl, 0) < 1)
	    moretodo = 0;

	/*
	 * Process next line from server.
	 */
	for (s = se; *s != '\0'; s = se) {
		const char *e;

		while (*se && *se != '\n') se++;

		if (se > s && se[-1] == '\r')
		   se[-1] = '\0';
		if (*se == '\0')
		    break;

		DBG(0, (stderr, "<- %s\n", s));

		/* HTTP: header termination on empty line */
		if (*s == '\0') {
		    moretodo = 0;
		    break;
		}
		*se++ = '\0';

		/* HTTP: look for "HTTP/1.1 123 ..." */
		if (!strncmp(s, "HTTP", sizeof("HTTP")-1)) {
		    u->httpContentLength = -1;
		    if ((e = strchr(s, '.')) != NULL) {
			e++;
			u->httpVersion = *e - '0';
			if (u->httpVersion < 1 || u->httpVersion > 2)
			    u->httpPersist = u->httpVersion = 0;
			else
			    u->httpPersist = 1;
		    }
		    if ((e = strchr(s, ' ')) != NULL) {
			e++;
			if (strchr("0123456789", *e))
			    strncpy(errorCode, e, 3);
			errorCode[3] = '\0';
		    }
		    continue;
		}

		/* HTTP: look for "token: ..." */
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

		/* HTTP: look for "<TITLE>501 ... </TITLE>" */
		if (!strncmp(s, "<TITLE>", sizeof("<TITLE>")-1))
		    s += sizeof("<TITLE>") - 1;

		/* FTP: look for "123-" and/or "123 " */
		if (strchr("0123456789", *s)) {
		    if (errorCode[0]) {
			if (!strncmp(s, errorCode, sizeof("123")-1) && s[3] == ' ')
			    moretodo = 0;
		    } else {
			strncpy(errorCode, s, sizeof("123")-1);
			errorCode[3] = '\0';
			if (s[3] != '-')
			    moretodo = 0;
		    }
		}
	}

	if (moretodo && se > s) {
	    bufLength = se - s - 1;
	    if (s != buf)
		memcpy(buf, s, bufLength);
	} else {
	    bufLength = 0;
	}
    } while (moretodo && ec == 0);

    if (str)	*str = buf;
    if (ecp)	*ecp = atoi(errorCode);

    return ec;
}

int ftpCheckResponse(urlinfo u, /*@out@*/ char ** str)
{
    int ec = 0;
    int rc;

    URLSANE(u);
    rc = checkResponse(u, &ec, str);

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

int ftpCommand(urlinfo u, ...)
{
    va_list ap;
    int len = 0;
    const char * s, * t;
    char * te;
    int rc;

    URLSANE(u);
    va_start(ap, u);
    while ((s = va_arg(ap, const char *)) != NULL) {
	if (len) len++;
	len += strlen(s);
    }
    len += sizeof("\r\n")-1;
    va_end(ap);

    t = te = alloca(len + 1);

    va_start(ap, u);
    while ((s = va_arg(ap, const char *)) != NULL) {
	if (te > t) *te++ = ' ';
	te = stpcpy(te, s);
    }
    te = stpcpy(te, "\r\n");
    va_end(ap);

    DBG(0, (stderr, "-> %s", t));
    if (fdio->write(u->ctrl, t, (te-t)) != (te-t))
	return FTPERR_SERVER_IO_ERROR;

    rc = ftpCheckResponse(u, NULL);
    return rc;
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
    int fdno = -1;
    int rc;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;
    
  do {
    if ((rc = getHostAddress(host, &sin.sin_addr)) < 0)
	break;

    if ((fdno = socket(sin.sin_family, SOCK_STREAM, IPPROTO_IP)) < 0) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }

    if (connect(fdno, (struct sockaddr *) &sin, sizeof(sin))) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }
  } while (0);

    if (rc < 0)
	goto errxit;

    DBG(0, (stderr,"++ connect %s:%d on fdno %d\n",
	/*@-unrecog@*/ inet_ntoa(sin.sin_addr) /*@=unrecog@*/ ,
	ntohs(sin.sin_port), fdno));

    return fdno;

errxit:
    if (fdno >= 0)
	close(fdno);
    return rc;
}

int httpOpen(urlinfo u, FD_t ctrl, const char *httpcmd)
{
    const char *host;
    const char *path;
    int port;
    int rc;
    char *req;
    size_t len;
    int retrying = 0;

    URLSANE(u);
    assert(ctrl != NULL);

    if (((host = (u->proxyh ? u->proxyh : u->host)) == NULL))
	return FTPERR_BAD_HOSTNAME;

    if ((port = (u->proxyp > 0 ? u->proxyp : u->port)) < 0) port = 80;
    path = (u->proxyh || u->proxyp > 0) ? u->url : u->path;

reopen:
    if (fdio->fileno(ctrl) >= 0 && fdWritable(ctrl, 0) < 1)
	fdio->close(ctrl);

    if (fdio->fileno(ctrl) < 0) {
	rc = tcpConnect(host, port);
	fdSetFdno(ctrl, (rc >= 0 ? rc : -1));
	if (rc < 0)
	    goto errxit;

	ctrl = fdLink(ctrl, "open ctrl (httpOpen)");
    }

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

    DBG(0, (stderr, "-> %s", req));

    if (fdio->write(ctrl, req, len) != len) {
	rc = FTPERR_SERVER_IO_ERROR;
	goto errxit;
    }

  { int ec = 0;
    rc = checkResponse(u, &ec, NULL);

if (_ftp_debug && !(rc == 0 && ec == 200))
fprintf(stderr, "*** httpOpen: rc %d ec %d\n", rc, ec);

    switch (rc) {
    case 0:
	if (ec == 200)
	    break;
	/*@fallthrough@*/
    default:
	if (!retrying) {	/* not HTTP_OK */
if (_ftp_debug)
fprintf(stderr, "*** httpOpen ctrl %p reopening ...\n", ctrl);
	    retrying = 1;
	    fdio->close(ctrl);
	    goto reopen;
	}
	rc = FTPERR_FILE_NOT_FOUND;
	goto errxit;
	/*@notreached@*/ break;
    }
  }

    ctrl = fdLink(ctrl, "open data (httpOpen)");
    return fdio->fileno(ctrl);

errxit:
    if (fdio->fileno(ctrl) >= 0)
	fdio->close(ctrl);
    return rc;
}

int ftpOpen(urlinfo u)
{
    const char * host;
    const char * user;
    const char * password;
    int port;
    int rc;

    URLSANE(u);
    if (((host = (u->proxyh ? u->proxyh : u->host)) == NULL))
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

    if (fdio->fileno(u->ctrl) >= 0 && fdWritable(u->ctrl, 0) < 1)
	fdio->close(u->ctrl);

    if (fdio->fileno(u->ctrl) < 0) {
	rc = tcpConnect(host, port);
	fdSetFdno(u->ctrl, (rc >= 0 ? rc : -1));
	if (rc < 0)
	    goto errxit;
    }

    if ((rc = ftpCheckResponse(u, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, "USER", user, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, "PASS", password, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, "TYPE", "I", NULL)))
	goto errxit;

    u->ctrl = fdLink(u->ctrl, "open ctrl");
    return fdio->fileno(u->ctrl);

errxit:
    if (fdio->fileno(u->ctrl) >= 0)
	fdio->close(u->ctrl);
    return rc;
}

int ftpFileDone(urlinfo u, FD_t data)
{
    int rc = 0;
    int ftpFileDoneNeeded;

    URLSANE(u);
    ftpFileDoneNeeded = fdGetFtpFileDoneNeeded(data);
    assert(ftpFileDoneNeeded);

    if (ftpFileDoneNeeded) {
	fdSetFtpFileDoneNeeded(data, 0);
	u->ctrl = fdFree(u->ctrl, "open data (ftpFileDone)");
	u->ctrl = fdFree(u->ctrl, "grab data (ftpFileDone)");
	rc = ftpCheckResponse(u, NULL);
    }
    return rc;
}

int ftpFileDesc(urlinfo u, const char *cmd, FD_t data)
{
    struct sockaddr_in dataAddress;
    int cmdlen;
    char * passReply;
    char * chptr;
    int rc;
    int ftpFileDoneNeeded;

    URLSANE(u);
    if (cmd == NULL)
	return FTPERR_UNKNOWN;	/* XXX W2DO? */

    cmdlen = strlen(cmd);

/*
 * XXX When ftpFileDesc() is called, there may be a lurking
 * XXX transfer complete message (if ftpFileDone() was not
 * XXX called to clear that message). Detect that condition now.
 */
    ftpFileDoneNeeded = fdGetFtpFileDoneNeeded(data);
    assert(ftpFileDoneNeeded == 0);

/*
 * Get the ftp version of the Content-Length.
 */
    if (!strncmp(cmd, "RETR", 4)) {
	char * req = strcpy(alloca(cmdlen + 1), cmd);
	unsigned cl;

	memcpy(req, "SIZE", 4);
	DBG(0, (stderr, "-> %s", req));
	if (fdio->write(u->ctrl, req, cmdlen) != cmdlen) {
	    rc = FTPERR_SERVER_IO_ERROR;
	    goto errxit;
	}
	if ((rc = ftpCheckResponse(u, &passReply)))
	    goto errxit;
	if (sscanf(passReply, "%d %u", &rc, &cl) != 2) {
	    rc = FTPERR_BAD_SERVER_RESPONSE;
	    goto errxit;
	}
	rc = 0;
	u->httpContentLength = cl;
    }

    DBG(0, (stderr, "-> PASV\n"));
    if (fdio->write(u->ctrl, "PASV\r\n", 6) != 6) {
	rc = FTPERR_SERVER_IO_ERROR;
	goto errxit;
    }

    if ((rc = ftpCheckResponse(u, &passReply))) {
	rc = FTPERR_PASSIVE_ERROR;
	goto errxit;
    }

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

    {	int i, j;
	dataAddress.sin_family = AF_INET;
	if (sscanf(chptr, "%d,%d", &i, &j) != 2) {
	    rc = FTPERR_PASSIVE_ERROR;
	    goto errxit;
	}
	dataAddress.sin_port = htons((((unsigned)i) << 8) + j);
    }

    chptr = passReply;
    while (*chptr++) {
	if (*chptr == ',') *chptr = '.';
    }

    if (!inet_aton(passReply, &dataAddress.sin_addr)) {
	rc = FTPERR_PASSIVE_ERROR;
	goto errxit;
    }

    rc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    fdSetFdno(data, (rc >= 0 ? rc : -1));
    if (rc < 0) {
	rc = FTPERR_FAILED_CONNECT;
	goto errxit;
    }
    data = fdLink(data, "open data (ftpFileDesc)");

    /* XXX setsockopt SO_LINGER */
    /* XXX setsockopt SO_KEEPALIVE */
    /* XXX setsockopt SO_TOS IPTOS_THROUGHPUT */

    while (connect(fdio->fileno(data), (struct sockaddr *) &dataAddress, 
	        sizeof(dataAddress)) < 0) {
	if (errno == EINTR)
	    continue;
	fdio->close(data);
	rc = FTPERR_FAILED_DATA_CONNECT;
	goto errxit;
    }

    DBG(0, (stderr, "-> %s", cmd));
    if (fdio->write(u->ctrl, cmd, cmdlen) != cmdlen) {
	fdio->close(data);
	rc = FTPERR_SERVER_IO_ERROR;
	goto errxit;
    }

    if ((rc = ftpCheckResponse(u, NULL))) {
	fdio->close(data);
	goto errxit;
    }

    fdSetFtpFileDoneNeeded(data, 1);
    u->ctrl = fdLink(u->ctrl, "grab data (ftpFileDesc)");
    u->ctrl = fdLink(u->ctrl, "open data (ftpFileDesc)");
    return 0;

errxit:
    return rc;
}
