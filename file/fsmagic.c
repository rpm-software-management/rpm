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

int
fsmagic(const char *fn, struct stat *sb)
{
	int ret = 0;

	/*
	 * Fstat is cheaper but fails for files you don't have read perms on.
	 * On 4.2BSD and similar systems, use lstat() to identify symlinks.
	 */
#if defined(S_IFLNK) || defined(__LCLINT__)
	if (!lflag)
		ret = lstat(fn, sb);
	else
#endif
	ret = stat(fn, sb);	/* don't merge into if; see "ret =" above */

	if (ret) {
		ckfprintf(stdout,
			/* Yes, I do mean stdout. */
			/* No \n, caller will provide. */
			"can't stat `%s' (%s).", fn, strerror(errno));
		return 1;
	}

	if (iflag) {
		if ((sb->st_mode & S_IFMT) != S_IFREG) {
			ckfputs("application/x-not-regular-file", stdout);
			return 1;
		}
	}
	else {
#if defined(S_ISUID) || defined(__LCLINT__)
		if (sb->st_mode & S_ISUID) ckfputs("setuid ", stdout);
#endif
#if defined(S_ISGID) || defined(__LCLINT__)
		if (sb->st_mode & S_ISGID) ckfputs("setgid ", stdout);
#endif
#if defined(S_ISVTX) || defined(__LCLINT__)
		if (sb->st_mode & S_ISVTX) ckfputs("sticky ", stdout);
#endif
	}
	
	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		ckfputs("directory", stdout);
		return 1;
#if defined(S_IFCHR) || defined(__LCLINT__)
	case S_IFCHR:
		/* 
		 * If -s has been specified, treat character special files
		 * like ordinary files.  Otherwise, just report that they
		 * are block special files and go on to the next file.
		 */
		if (sflag)
			break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
# ifdef dv_unit
		(void) printf("character special (%d/%d/%d)",
			major(sb->st_rdev),
			dv_unit(sb->st_rdev),
			dv_subunit(sb->st_rdev));
# else
		(void) printf("character special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
# endif
#else
		(void) printf("character special");
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
		if (sflag)
			break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
# ifdef dv_unit
		(void) printf("block special (%d/%d/%d)",
			major(sb->st_rdev),
			dv_unit(sb->st_rdev),
			dv_subunit(sb->st_rdev));
# else
		(void) printf("block special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
# endif
#else
		(void) printf("block special");
#endif
		return 1;
#endif
	/* TODO add code to handle V7 MUX and Blit MUX files */
#if defined(S_IFIFO) || defined(__LCLINT__)
	case S_IFIFO:
		ckfputs("fifo (named pipe)", stdout);
		return 1;
#endif
#if defined(S_IFDOOR)
	case S_IFDOOR:
		ckfputs("door", stdout);
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
				ckfprintf(stdout, "unreadable symlink (%s).", 
				      strerror(errno));
				return 1;
			}
			buf[nch] = '\0';	/* readlink(2) forgets this */

			/* If broken symlink, say so and quit early. */
/*@-branchstate@*/
			if (*buf == '/') {
			    if (stat(buf, &tstatbuf) < 0) {
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
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
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
				return 1;
			    }
                        }
/*@=branchstate@*/

			/* Otherwise, handle it. */
			if (lflag) {
				process(buf, strlen(buf));
				return 1;
			} else { /* just print what it points to */
				ckfputs("symbolic link to ", stdout);
				ckfputs(buf, stdout);
			}
		}
		return 1;
#endif
#if defined(S_IFSOCK)
#ifndef __COHERENT__
	case S_IFSOCK:
		ckfputs("socket", stdout);
		return 1;
#endif
#endif
	case S_IFREG:
		break;
	default:
		error("invalid mode 0%o.\n", sb->st_mode);
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
	if (!sflag && sb->st_size == 0) {
		ckfputs(iflag ? "application/x-empty" : "empty", stdout);
		return 1;
	}
	return 0;
}

int
tryit(const char *fn, unsigned char *buf, int nb, int zfl)
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
	if (zfl && zmagic(fn, buf, nb))
		return 'z';

	/* try tests in /etc/magic (or surrogate magic file) */
	if (softmagic(buf, nb))
		return 's';

	/* try known keywords, check whether it is ASCII */
	if (ascmagic(buf, nb))
		return 'a';

	/* abandon hope, all ye who remain here */
	ckfputs(iflag ? "application/octet-stream" : "data", stdout);
		return '\0';
}

/*
 * process - process input file
 */
void
process(const char *inname, int wid)
{
	int	fd = 0;
	static  const char stdname[] = "standard input";
	unsigned char	buf[HOWMANY+1];	/* one extra for terminating '\0' */
	struct stat	sb;
	int nbytes = 0;	/* number of bytes read from a datafile */
	char match = '\0';

	if (strcmp("-", inname) == 0) {
		if (fstat(0, &sb)<0) {
			error("cannot fstat `%s' (%s).\n", stdname,
			      strerror(errno));
			/*@notreached@*/
		}
		inname = stdname;
	}

	if (wid > 0 && !bflag)
	     (void) printf("%s:%*s ", inname, 
			   (int) (wid - strlen(inname)), "");

	if (inname != stdname) {
		/*
		 * first try judging the file based on its filesystem status
		 */
		if (fsmagic(inname, &sb) != 0) {
			(void) putchar('\n');
			return;
		}

		if ((fd = open(inname, O_RDONLY)) < 0) {
			/* We can't open it, but we were able to stat it. */
			if (sb.st_mode & 0002) ckfputs("writeable, ", stdout);
			if (sb.st_mode & 0111) ckfputs("executable, ", stdout);
			ckfprintf(stdout, "can't read `%s' (%s).\n",
			    inname, strerror(errno));
			return;
		}
	}


	/*
	 * try looking at the first HOWMANY bytes
	 */
	if ((nbytes = read(fd, (char *)buf, HOWMANY)) == -1) {
		error("read failed (%s).\n", strerror(errno));
		/*@notreached@*/
	}

	if (nbytes == 0)
		ckfputs(iflag ? "application/x-empty" : "empty", stdout);
	else {
		buf[nbytes++] = '\0';	/* null-terminate it */
		match = tryit(inname, buf, nbytes, zflag);
	}

#ifdef BUILTIN_ELF
	if (match == 's' && nbytes > 5) {
		/*
		 * We matched something in the file, so this *might*
		 * be an ELF file, and the file is at least 5 bytes long,
		 * so if it's an ELF file it has at least one byte
		 * past the ELF magic number - try extracting information
		 * from the ELF headers that can't easily be extracted
		 * with rules in the magic file.
		 */
		tryelf(fd, buf, nbytes);
	}
#endif

	if (inname != stdname) {
#ifdef RESTORE_TIME
		/*
		 * Try to restore access, modification times if read it.
		 * This is really *bad* because it will modify the status
		 * time of the file... And of course this will affect
		 * backup programs
		 */
# ifdef USE_UTIMES
		struct timeval  utsbuf[2];
		utsbuf[0].tv_sec = sb.st_atime;
		utsbuf[1].tv_sec = sb.st_mtime;

		(void) utimes(inname, utsbuf); /* don't care if loses */
# else
		struct utimbuf  utbuf;

		utbuf.actime = sb.st_atime;
		utbuf.modtime = sb.st_mtime;
		(void) utime(inname, &utbuf); /* don't care if loses */
# endif
#endif
		(void) close(fd);
	}
	(void) putchar('\n');
}
