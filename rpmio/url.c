/** \ingroup rpmio
 * \file rpmio/url.c
 */

#include "system.h"

#include <assert.h>
#include <netinet/in.h>

#include <rpm/rpmmacro.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmio.h>
#include <rpm/argv.h>
#include <rpm/rpmstring.h>

#include "debug.h"

#ifndef	IPPORT_FTP
#define	IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif
#ifndef	IPPORT_HTTPS
#define	IPPORT_HTTPS	443
#endif
#ifndef	IPPORT_PGPKEYSERVER
#define	IPPORT_PGPKEYSERVER	11371
#endif

#define	URLMAGIC	0xd00b1ed0
#define	URLSANE(u)	assert(u && u->magic == URLMAGIC)

urlinfo urlNew()
{
    urlinfo u;
    if ((u = xmalloc(sizeof(*u))) == NULL)
	return NULL;
    memset(u, 0, sizeof(*u));
    u->proxyp = -1;
    u->port = -1;
    u->urltype = URL_IS_UNKNOWN;
    u->magic = URLMAGIC;
    return u;
}

urlinfo urlFree(urlinfo u)
{
    URLSANE(u);
    u->url = _free(u->url);
    u->scheme = _free(u->scheme);
    u->user = _free(u->user);
    u->password = _free(u->password);
    u->host = _free(u->host);
    u->portstr = _free(u->portstr);
    u->proxyu = _free(u->proxyu);
    u->proxyh = _free(u->proxyh);

    u = _free(u);
    return NULL;
}

/**
 */
static struct urlstring {
    const char const * leadin;
    urltype	ret;
} const urlstrings[] = {
    { "file://",	URL_IS_PATH },
    { "ftp://",		URL_IS_FTP },
    { "hkp://",		URL_IS_HKP },
    { "http://",	URL_IS_HTTP },
    { "https://",	URL_IS_HTTPS },
    { NULL,		URL_IS_UNKNOWN }
};

urltype urlIsURL(const char * url)
{
    const struct urlstring *us;

    if (url && *url) {
	for (us = urlstrings; us->leadin != NULL; us++) {
	    if (!rstreqn(url, us->leadin, strlen(us->leadin)))
		continue;
	    return us->ret;
	}
	if (rstreq(url, "-")) 
	    return URL_IS_DASH;
    }

    return URL_IS_UNKNOWN;
}

/* Return path portion of url (or pointer to NUL if url == NULL) */
urltype urlPath(const char * url, const char ** pathp)
{
    const char *path;
    int urltype;

    path = url;
    urltype = urlIsURL(url);
    switch (urltype) {
    case URL_IS_FTP:
	url += sizeof("ftp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_PATH:
	url += sizeof("file://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HKP:
	url += sizeof("hkp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTP:
	url += sizeof("http://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTPS:
	url += sizeof("https://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_UNKNOWN:
	if (path == NULL) path = "";
	break;
    case URL_IS_DASH:
	path = "";
	break;
    }
    if (pathp)
	*pathp = path;
    return urltype;
}

/*
 * Split URL into components. The URL can look like
 *	scheme://user:password@host:port/path
  * or as in RFC2732 for IPv6 address
  *    service://user:password@[ip:v6:ad:dr:es:s]:port/path
 */
int urlSplit(const char * url, urlinfo *uret)
{
    urlinfo u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = urlNew()) == NULL)
	return -1;

    if ((se = s = myurl = xstrdup(url)) == NULL) {
	u = urlFree(u);
	return -1;
    }

    u->url = xstrdup(url);
    u->urltype = urlIsURL(url);

    while (1) {
	/* Point to end of next item */
	while (*se && *se != '/') se++;
	/* Item was scheme. Save scheme and go for the rest ...*/
    	if (*se && (se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->scheme = xstrdup(s);
	    se += 2;	/* skip over "//" */
	    s = se++;
	    continue;
	}
	
	/* Item was everything-but-path. Continue parse on rest */
	*se = '\0';
	break;
    }

    /* Look for ...@host... */
    fe = f = s;
    while (*fe && *fe != '@') fe++;
    if (*fe == '@') {
	s = fe + 1;
	*fe = '\0';
    	/* Look for user:password@host... */
	while (fe > f && *fe != ':') fe--;
	if (*fe == ':') {
	    *fe++ = '\0';
	    u->password = xstrdup(fe);
	}
	u->user = xstrdup(f);
    }

    /* Look for ...host:port or [v6addr]:port*/
    fe = f = s;
    if (strchr(fe, '[') && strchr(fe, ']'))
    {
	    fe = strchr(f, ']');
	    *f++ = '\0';
	    *fe++ = '\0';
    }
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = xstrdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (!(end && *end == '\0')) {
		rpmlog(RPMLOG_ERR, _("url port must be a number\n"));
		myurl = _free(myurl);
		u = urlFree(u);
		return -1;
	    }
	}
    }
    u->host = xstrdup(f);

    if (u->port < 0 && u->scheme != NULL) {
	struct servent *serv;
	/* HACK hkp:// might lookup "pgpkeyserver" */
	serv = getservbyname(u->scheme, "tcp");
	if (serv != NULL)
	    u->port = ntohs(serv->s_port);
	else if (u->urltype == URL_IS_FTP)
	    u->port = IPPORT_FTP;
	else if (u->urltype == URL_IS_HKP)
	    u->port = IPPORT_PGPKEYSERVER;
	else if (u->urltype == URL_IS_HTTP)
	    u->port = IPPORT_HTTP;
	else if (u->urltype == URL_IS_HTTPS)
	    u->port = IPPORT_HTTPS;
    }

    myurl = _free(myurl);
    if (uret) {
	*uret = u;
    }
    return 0;
}


int urlGetFile(const char * url, const char * dest)
{
    char *cmd = NULL;
    const char *target = NULL;
    char *urlhelper = NULL;
    int rc;
    pid_t pid, wait;

    urlhelper = rpmExpand("%{?_urlhelper}", NULL);

    if (dest == NULL) {
	urlPath(url, &target);
    } else {
	target = dest;
    }

    /* XXX TODO: sanity checks like target == dest... */

    rasprintf(&cmd, "%s %s %s", urlhelper, target, url);
    urlhelper = _free(urlhelper);

    if ((pid = fork()) == 0) {
        ARGV_t argv = NULL;
        argvSplit(&argv, cmd, " ");
        execvp(argv[0], argv);
        exit(127); /* exit with 127 for compatibility with bash(1) */
    }
    wait = waitpid(pid, &rc, 0);
    cmd = _free(cmd);

    return rc;

}
