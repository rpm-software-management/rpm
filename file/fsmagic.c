/*
 * fsmagic - magic based on filesystem info - directory, special files, etc.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include "system.h"
#include "file.h"
#include "debug.h"

FILE_RCSID("@(#)Id: fsmagic.c,v 1.36 2002/07/03 19:00:41 christos Exp ")

/*@access fmagic @*/

int
fmagicD(fmagic fm)
{
	const char * fn = fm->fn;
	struct stat * st = &fm->sb;
	int ret = 0;
	int xx;

	/*
	 * Fstat is cheaper but fails for files you don't have read perms on.
	 * On 4.2BSD and similar systems, use lstat() to identify symlinks.
	 */
#if defined(S_IFLNK) || defined(__LCLINT__)
	if (!(fm->flags & FMAGIC_FLAGS_FOLLOW))
		ret = lstat(fn, st);
	else
#endif
	ret = stat(fn, st);	/* don't merge into if; see "ret =" above */

	if (ret) {
		/* Yes, I do mean stdout. */
		/* No \n, caller will provide. */
		fmagicPrintf(fm, "can't stat `%s' (%s).", fn, strerror(errno));
		return 1;
	}

	if ((fm->flags & FMAGIC_FLAGS_MIME)) {
		if ((st->st_mode & S_IFMT) != S_IFREG) {
			fmagicPrintf(fm, "application/x-not-regular-file");
			return 1;
		}
	}
	else {
#if defined(S_ISUID) || defined(__LCLINT__)
		if (st->st_mode & S_ISUID) fmagicPrintf(fm, "setuid ");
#endif
#if defined(S_ISGID) || defined(__LCLINT__)
		if (st->st_mode & S_ISGID) fmagicPrintf(fm, "setgid ");
#endif
#if defined(S_ISVTX) || defined(__LCLINT__)
		if (st->st_mode & S_ISVTX) fmagicPrintf(fm, "sticky ");
#endif
	}
	
	switch (st->st_mode & S_IFMT) {
	case S_IFDIR:
		fmagicPrintf(fm, "directory");
		return 1;
#if defined(S_IFCHR) || defined(__LCLINT__)
	case S_IFCHR:
		/* 
		 * If -s has been specified, treat character special files
		 * like ordinary files.  Otherwise, just report that they
		 * are block special files and go on to the next file.
		 */
		if ((fm->flags & FMAGIC_FLAGS_SPECIAL))
			break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
# ifdef dv_unit
		fmagicPrintf(fm, "character special (%d/%d/%d)",
			major(st->st_rdev),
			dv_unit(st->st_rdev),
			dv_subunit(st->st_rdev));
# else
		fmagicPrintf(fm, "character special (%ld/%ld)",
			(long) major(st->st_rdev), (long) minor(st->st_rdev));
# endif
#else
		fmagicPrintf(fm, "character special");
#endif
		return 1;
#endif
#if defined(S_IFBLK) || defined(__LCLINT__)
	case S_IFBLK:
		/* 
		 * If -s has been specified, treat block special files
		 * like ordinary files.  Otherwise, just report that they
		 * are block special files and go on to the next file.
		 */
		if ((fm->flags & FMAGIC_FLAGS_SPECIAL))
			break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
# ifdef dv_unit
		fmagicPrintf(fm, "block special (%d/%d/%d)",
			major(st->st_rdev),
			dv_unit(st->st_rdev),
			dv_subunit(st->st_rdev));
# else
		fmagicPrintf(fm, "block special (%ld/%ld)",
			(long) major(st->st_rdev), (long) minor(st->st_rdev));
# endif
#else
		fmagicPrintf(fm, "block special");
#endif
		return 1;
#endif
	/* TODO add code to handle V7 MUX and Blit MUX files */
#if defined(S_IFIFO) || defined(__LCLINT__)
	case S_IFIFO:
		fmagicPrintf(fm, "fifo (named pipe)");
		return 1;
#endif
#if defined(S_IFDOOR)
	case S_IFDOOR:
		fmagicPrintf(fm, "door");
		return 1;
#endif
#if defined(S_IFLNK) || defined(__LCLINT__)
	case S_IFLNK:
		{
			char buf[BUFSIZ+4];
			int nch;
			struct stat tstatbuf;

			buf[0] = '\0';
			if ((nch = readlink(fn, buf, BUFSIZ-1)) <= 0) {
				fmagicPrintf(fm, "unreadable symlink (%s).", strerror(errno));
				return 1;
			}
			buf[nch] = '\0';	/* readlink(2) needs this */

			/* If broken symlink, say so and quit early. */
/*@-branchstate@*/
			if (*buf == '/') {
			    if (stat(buf, &tstatbuf) < 0) {
				fmagicPrintf(fm, "broken symbolic link to %s", buf);
				return 1;
			    }
			}
			else {
			    char *tmp;
			    char buf2[BUFSIZ+BUFSIZ+4];

			    if ((tmp = strrchr(fn,  '/')) == NULL) {
				tmp = buf; /* in current directory anyway */
			    }
			    else {
				strcpy (buf2, fn);  /* take directory part */
				buf2[tmp-fn+1] = '\0';
				strcat (buf2, buf); /* plus (relative) symlink */
				tmp = buf2;
			    }
			    if (stat(tmp, &tstatbuf) < 0) {
				fmagicPrintf(fm, "broken symbolic link to %s", buf);
				return 1;
			    }
                        }
/*@=branchstate@*/

			/* Otherwise, handle it. */
			if ((fm->flags & FMAGIC_FLAGS_FOLLOW)) {
				fmagicPrintf(fm, "\n");
				xx = fmagicProcess(fm, buf, strlen(buf));
				return 1;
			} else { /* just print what it points to */
				fmagicPrintf(fm, "symbolic link to %s", buf);
			}
		}
		return 1;
#endif
#if defined(S_IFSOCK)
#ifndef __COHERENT__
	case S_IFSOCK:
		fmagicPrintf(fm, "socket");
		return 1;
#endif
#endif
	case S_IFREG:
		break;
	default:
		error(EXIT_FAILURE, 0, "invalid mode 0%o.\n", st->st_mode);
		/*@notreached@*/
	}

	/*
	 * regular file, check next possibility
	 *
	 * If stat() tells us the file has zero length, report here that
	 * the file is empty, so we can skip all the work of opening and 
	 * reading the file.
	 * But if the -s option has been given, we skip this optimization,
	 * since on some systems, stat() reports zero size for raw disk
	 * partitions.  (If the block special device really has zero length,
	 * the fact that it is empty will be detected and reported correctly
	 * when we read the file.)
	 */
	if (!(fm->flags & FMAGIC_FLAGS_SPECIAL) && st->st_size == 0) {
		fmagicPrintf(fm, ((fm->flags & FMAGIC_FLAGS_MIME)
			? "application/x-empty" : "empty"));
		return 1;
	}
	return 0;
}

int
fmagicF(fmagic fm, int zfl)
{
	/*
	 * The main work is done here!
	 * We have the file name and/or the data buffer to be identified. 
	 */

#ifdef __EMX__
	/*
	 * Ok, here's the right place to add a call to some os-specific
	 * routine, e.g.
	 */
	if (os2_apptype(fn, buf, nb) == 1)
	       return 'o';
#endif
	/* try compression stuff */
	if (zfl && fmagicZ(fm))
		return 'z';

	/* try tests in /etc/magic (or surrogate magic file) */
	if (fmagicS(fm))
		return 's';

	/* try known keywords, check whether it is ASCII */
	if (fmagicA(fm))
		return 'a';

	/* abandon hope, all ye who remain here */
	fmagicPrintf(fm, ((fm->flags & FMAGIC_FLAGS_MIME)
		? "application/octet-stream" : "data"));
	return '\0';
}

/*
 * fmagicProcess - process input file
 */
int
fmagicProcess(fmagic fm, const char *fn, int wid)
{
	static const char stdname[] = "standard input";
	char match = '\0';
	int ret = 0;

/*@-assignexpose -temptrans @*/
	fm->fn = fn;
/*@=assignexpose =temptrans @*/
	fm->buf = xmalloc(HOWMANY+1);
	fm->buf[0] = '\0';
	fm->nb = 0;

/*@-branchstate@*/
	if (strcmp("-", fn) == 0) {
		if (fstat(0, &fm->sb)<0) {
			error(EXIT_FAILURE, 0, "cannot fstat `%s' (%s).\n", stdname,
			      strerror(errno));
			/*@notreached@*/
		}
		fm->fn = stdname;
	}
/*@=branchstate@*/

	if (wid > 0 && !(fm->flags & FMAGIC_FLAGS_BRIEF))
	     fmagicPrintf(fm, "%s:%*s ", fm->fn, 
			   (int) (wid - strlen(fm->fn)), "");

	if (fm->fn != stdname) {
		/*
		 * first try judging the file based on its filesystem status
		 */
		if (fmagicD(fm) != 0)
			goto exit;

		if ((fm->fd = open(fm->fn, O_RDONLY)) < 0) {
			/* We can't open it, but we were able to stat it. */
			if (fm->sb.st_mode & 0002)
				fmagicPrintf(fm, "writeable, ");
			if (fm->sb.st_mode & 0111)
				fmagicPrintf(fm, "executable, ");
			fmagicPrintf(fm, "can't read `%s' (%s).", fm->fn, strerror(errno));
			goto exit;
		}
	}


	/*
	 * try looking at the first HOWMANY bytes
	 */
	if ((fm->nb = read(fm->fd, (char *)fm->buf, HOWMANY)) == -1) {
		error(EXIT_FAILURE, 0, "read failed (%s).\n", strerror(errno));
		/*@notreached@*/
	}

	if (fm->nb == 0)
		fmagicPrintf(fm, ((fm->flags & FMAGIC_FLAGS_MIME)
			? "application/x-empty" : "empty"), fm);
	else {
		fm->buf[fm->nb++] = '\0';	/* null-terminate data buffer */
		match = fmagicF(fm, (fm->flags & FMAGIC_FLAGS_UNCOMPRESS));
	}

#ifdef BUILTIN_ELF
	if (match == 's' && fm->nb > 5) {
		/*
		 * We matched something in the file, so this *might*
		 * be an ELF file, and the file is at least 5 bytes long,
		 * so if it's an ELF file it has at least one byte
		 * past the ELF magic number - try extracting information
		 * from the ELF headers that can't easily be extracted
		 * with rules in the magic file.
		 */
		fmagicE(fm);
	}
#endif

	if (fm->fn != stdname) {
#ifdef RESTORE_TIME
		/*
		 * Try to restore access, modification times if read it.
		 * This is really *bad* because it will modify the status
		 * time of the file... And of course this will affect
		 * backup programs
		 */
# ifdef USE_UTIMES
		struct timeval  utsbuf[2];
		utsbuf[0].tv_sec = fm->sb.st_atime;
		utsbuf[1].tv_sec = fm->sb.st_mtime;

		(void) utimes(fm->fn, utsbuf); /* don't care if loses */
# else
		struct utimbuf  utbuf;

		utbuf.actime = fm->sb.st_atime;
		utbuf.modtime = fm->sb.st_mtime;
		(void) utime(fm->fn, &utbuf); /* don't care if loses */
# endif
#endif
		(void) close(fm->fd);
		fm->fd = -1;
	}

exit:
	if (fm->buf != NULL)
		free(fm->buf);
	fm->buf = NULL;
	fm->nb = 0;
	return ret;
}
