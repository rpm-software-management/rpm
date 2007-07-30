/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_oflags.c,v 12.10 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#ifdef HAVE_SYSTEM_INCLUDE_FILES
#ifdef HAVE_SHMGET
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#endif

/*
 * Ensure that POSIX defined codes are available.
 */
#ifndef O_CREAT
#define	O_CREAT  0x0200
#endif
#ifndef O_TRUNC
#define	O_TRUNC  0x0400
#endif
#ifndef O_RDONLY
#define	O_RDONLY 0x0000
#endif
#ifndef O_RDWR
#define	O_RDWR   0x0002
#endif
#ifndef O_WRONLY
#define	O_WRONLY 0x0001
#endif

#ifndef S_IREAD
#define	S_IREAD  0000400
#endif
#ifndef S_IWRITE
#define	S_IWRITE 0000200
#endif

/*
 * __db_oflags --
 *	Convert open(2) flags to DB flags.
 *
 * PUBLIC: u_int32_t __db_oflags __P((int));
 */
u_int32_t
__db_oflags(oflags)
	int oflags;
{
	u_int32_t dbflags;

	dbflags = 0;

	if (oflags & O_CREAT)
		dbflags |= DB_CREATE;

	if (oflags & O_TRUNC)
		dbflags |= DB_TRUNCATE;

	/*
	 * !!!
	 * Convert POSIX 1003.1 open(2) mode flags to DB flags.  This isn't
	 * an exact science as few POSIX implementations have a flag value
	 * for O_RDONLY, it's simply the lack of a write flag.
	 */
#ifndef	O_ACCMODE
#define	O_ACCMODE	(O_RDONLY | O_RDWR | O_WRONLY)
#endif
	switch (oflags & O_ACCMODE) {
	case O_RDWR:
	case O_WRONLY:
		break;
	default:
		dbflags |= DB_RDONLY;
		break;
	}
	return (dbflags);
}

#ifdef DB_WIN32
#ifndef	S_IRUSR
#define	S_IRUSR	S_IREAD		/* R for owner */
#endif
#ifndef	S_IWUSR
#define	S_IWUSR	S_IWRITE	/* W for owner */
#endif
#ifndef	S_IXUSR
#define	S_IXUSR	0		/* X for owner */
#endif
#ifndef	S_IRGRP
#define	S_IRGRP	0		/* R for group */
#endif
#ifndef	S_IWGRP
#define	S_IWGRP	0		/* W for group */
#endif
#ifndef	S_IXGRP
#define	S_IXGRP	0		/* X for group */
#endif
#ifndef	S_IROTH
#define	S_IROTH	0		/* R for other */
#endif
#ifndef	S_IWOTH
#define	S_IWOTH	0		/* W for other */
#endif
#ifndef	S_IXOTH
#define	S_IXOTH	0		/* X for other */
#endif
#else
#ifndef	S_IRUSR
#define	S_IRUSR	0000400		/* R for owner */
#endif
#ifndef	S_IWUSR
#define	S_IWUSR	0000200		/* W for owner */
#endif
#ifndef	S_IXUSR
#define	S_IXUSR	0000100		/* X for owner */
#endif
#ifndef	S_IRGRP
#define	S_IRGRP	0000040		/* R for group */
#endif
#ifndef	S_IWGRP
#define	S_IWGRP	0000020		/* W for group */
#endif
#ifndef	S_IXGRP
#define	S_IXGRP	0000010		/* X for group */
#endif
#ifndef	S_IROTH
#define	S_IROTH	0000004		/* R for other */
#endif
#ifndef	S_IWOTH
#define	S_IWOTH	0000002		/* W for other */
#endif
#ifndef	S_IXOTH
#define	S_IXOTH	0000001		/* X for other */
#endif
#endif /* DB_WIN32 */

/*
 * __db_omode --
 *	Convert a permission string to the correct open(2) flags.
 *
 * PUBLIC: int __db_omode __P((const char *));
 */
int
__db_omode(perm)
	const char *perm;
{
	int mode;
	mode = 0;
	if (perm[0] == 'r')
		mode |= S_IRUSR;
	if (perm[1] == 'w')
		mode |= S_IWUSR;
	if (perm[2] == 'x')
		mode |= S_IXUSR;
	if (perm[3] == 'r')
		mode |= S_IRGRP;
	if (perm[4] == 'w')
		mode |= S_IWGRP;
	if (perm[5] == 'x')
		mode |= S_IXGRP;
	if (perm[6] == 'r')
		mode |= S_IROTH;
	if (perm[7] == 'w')
		mode |= S_IWOTH;
	if (perm[8] == 'x')
		mode |= S_IXOTH;
	return (mode);
}
