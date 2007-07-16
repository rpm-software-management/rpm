/*
 * Exit success/failure macros.
 */
#ifndef	HAVE_EXIT_SUCCESS
#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0
#endif

/*
 * Don't step on the namespace.  Other libraries may have their own
 * implementations of these functions, we don't want to use their
 * implementations or force them to use ours based on the load order.
 */
#ifndef	HAVE_ATOI
#define	atoi		__db_Catoi
#endif
#ifndef	HAVE_ATOL
#define	atol		__db_Catol
#endif
#ifndef	HAVE_GETADDRINFO
#define	freeaddrinfo(a)		__db_Cfreeaddrinfo(a)
#define	getaddrinfo(a, b, c, d)	__db_Cgetaddrinfo(a, b, c, d)
#endif
#ifndef	HAVE_GETCWD
#define	getcwd		__db_Cgetcwd
#endif
#ifndef	HAVE_GETOPT
#define	getopt		__db_Cgetopt
#define	optarg		__db_Coptarg
#define	opterr		__db_Copterr
#define	optind		__db_Coptind
#define	optopt		__db_Coptopt
#define	optreset	__db_Coptreset
#endif
#ifndef	HAVE_ISSPACE
#define	isspace		__db_Cisspace
#endif
#ifndef	HAVE_MEMCMP
#define	memcmp		__db_Cmemcmp
#endif
#ifndef	HAVE_MEMCPY
#define	memcpy		__db_Cmemcpy
#endif
#ifndef	HAVE_MEMMOVE
#define	memmove		__db_Cmemmove
#endif
#ifndef	HAVE_PRINTF
#define	printf		__db_Cprintf
#define	fprintf		__db_Cfprintf
#endif
#ifndef	HAVE_RAISE
#define	raise		__db_Craise
#endif
#ifndef	HAVE_RAND
#define	rand		__db_Crand
#define	srand		__db_Csrand
#endif
#ifndef	HAVE_SNPRINTF
#define	snprintf	__db_Csnprintf
#endif
#ifndef	HAVE_STRCASECMP
#define	strcasecmp	__db_Cstrcasecmp
#define	strncasecmp	__db_Cstrncasecmp
#endif
#ifndef	HAVE_STRCAT
#define	strcat		__db_Cstrcat
#endif
#ifndef	HAVE_STRCHR
#define	strchr		__db_Cstrchr
#endif
#ifndef	HAVE_STRDUP
#define	strdup		__db_Cstrdup
#endif
#ifndef	HAVE_STRERROR
#define	strerror	__db_Cstrerror
#endif
#ifndef	HAVE_STRNCAT
#define	strncat		__db_Cstrncat
#endif
#ifndef	HAVE_STRNCMP
#define	strncmp		__db_Cstrncmp
#endif
#ifndef	HAVE_STRRCHR
#define	strrchr		__db_Cstrrchr
#endif
#ifndef	HAVE_STRSEP
#define	strsep		__db_Cstrsep
#endif
#ifndef	HAVE_STRTOL
#define	strtol		__db_Cstrtol
#endif
#ifndef	HAVE_STRTOUL
#define	strtoul		__db_Cstrtoul
#endif
#ifndef	HAVE_VSNPRINTF
#define	vsnprintf	__db_Cvsnprintf
#endif
