/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006,2007 Oracle.  All rights reserved.
 *
 * $Id: getaddrinfo.c,v 1.8 2007/05/17 15:14:54 bostic Exp $
 */

#include "db_config.h"

#define	__INCLUDE_NETWORKING	1
#include "db_int.h"

/*
 * __db_getaddrinfo and __db_freeaddrinfo wrap the getaddrinfo and freeaddrinfo
 * calls, as well as the associated platform dependent error handling, mapping
 * the error return to a ANSI C/POSIX error return.
 */

/*
 * __db_getaddrinfo --
 *
 * PUBLIC: #if defined(HAVE_REPLICATION_THREADS)
 * PUBLIC: int __db_getaddrinfo __P((DB_ENV *, const char *, u_int,
 * PUBLIC:    const char *, const ADDRINFO *, ADDRINFO **));
 * PUBLIC: #endif
 */
int
__db_getaddrinfo(dbenv, nodename, port, servname, hints, res)
	DB_ENV *dbenv;
	const char *nodename, *servname;
	u_int port;
	const ADDRINFO *hints;
	ADDRINFO **res;
{
#ifdef HAVE_GETADDRINFO
	int ret;

	if ((ret = getaddrinfo(nodename, servname, hints, res)) == 0)
		return (0);

	__db_errx(dbenv, "%s(%u): host lookup failed: %s",
	    nodename == NULL ? "" : nodename, port,
#ifdef DB_WIN32
	    gai_strerrorA(ret));
#else
	    gai_strerror(ret));
#endif
	return (__os_posix_err(ret));
#else
	ADDRINFO *answer;
	struct hostent *hostaddr;
	struct sockaddr_in sin;
	u_int32_t tmpaddr;
	int ret;

	COMPQUIET(hints, NULL);
	COMPQUIET(servname, NULL);

	/* INADDR_NONE is not defined on Solaris 2.6, 2.7 or 2.8. */
#ifndef	INADDR_NONE
#define	INADDR_NONE	((u_long)0xffffffff)
#endif

	/*
	 * Basic implementation of IPv4 component of getaddrinfo.
	 * Limited to the functionality used by repmgr.
	 */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (nodename) {
		if (nodename[0] == '\0')
			sin.sin_addr.s_addr = htonl(INADDR_ANY);
		else if ((tmpaddr = inet_addr(nodename)) != INADDR_NONE) {
			sin.sin_addr.s_addr = tmpaddr;
		} else {
			hostaddr = gethostbyname(nodename);
			if (hostaddr == NULL) {
#ifdef DB_WIN32
				ret = __os_get_neterr();
				__db_syserr(dbenv, ret,
				    "%s(%u): host lookup failed",
				    nodename == NULL ? "" : nodename, port);
				return (__os_posix_err(ret));
#else
				/*
				 * Historic UNIX systems used the h_errno
				 * global variable to return gethostbyname
				 * errors.  The only function we currently
				 * use that needs h_errno is gethostbyname,
				 * so we deal with it here.
				 *
				 * hstrerror is not available on Solaris 2.6
				 * (it is in libresolv but is a private,
				 * unexported symbol).
				 */
#ifdef HAVE_HSTRERROR
				__db_errx(dbenv,
				    "%s(%u): host lookup failed: %s",
				    nodename == NULL ? "" : nodename, port,
				    hstrerror(h_errno));
#else
				__db_errx(dbenv,
				    "%s(%u): host lookup failed: %d",
				    nodename == NULL ? "" : nodename, port,
				    h_errno);
#endif
				switch (h_errno) {
				case HOST_NOT_FOUND:
				case NO_DATA:
					return (EHOSTUNREACH);
				case TRY_AGAIN:
					return (EAGAIN);
				case NO_RECOVERY:
				default:
					return (EFAULT);
				}
				/* NOTREACHED */
#endif
			}
			memcpy(&(sin.sin_addr),
			    hostaddr->h_addr, (size_t)hostaddr->h_length);
		}
	} else					/* No host specified. */
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons((u_int16_t)port);

	if ((ret = __os_calloc(dbenv, 1, sizeof(ADDRINFO), &answer)) != 0)
		return (ret);
	if ((ret = __os_malloc(dbenv, sizeof(sin), &answer->ai_addr)) != 0) {
		__os_free(dbenv, answer);
		return (ret);
	}

	answer->ai_family = AF_INET;
	answer->ai_protocol = IPPROTO_TCP;
	answer->ai_socktype = SOCK_STREAM;
	answer->ai_addrlen = sizeof(sin);
	memcpy(answer->ai_addr, &sin, sizeof(sin));
	*res = answer;

	return (0);
#endif /* HAVE_GETADDRINFO */
}

/*
 * __db_freeaddrinfo --
 *
 * PUBLIC: #if defined(HAVE_REPLICATION_THREADS)
 * PUBLIC: void __db_freeaddrinfo __P((DB_ENV *, ADDRINFO *));
 * PUBLIC: #endif
 */
void
__db_freeaddrinfo(dbenv, ai)
	DB_ENV *dbenv;
	ADDRINFO *ai;
{
#ifdef HAVE_GETADDRINFO
	COMPQUIET(dbenv, NULL);

	freeaddrinfo(ai);
#else
	ADDRINFO *next, *tmpaddr;

	for (next = ai; next != NULL; next = tmpaddr) {
		if (next->ai_canonname != NULL)
			__os_free(dbenv, next->ai_canonname);

		if (next->ai_addr != NULL)
			__os_free(dbenv, next->ai_addr);

		tmpaddr = next->ai_next;
		__os_free(dbenv, next);
	}
#endif
}
