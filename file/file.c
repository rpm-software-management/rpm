/*
 * file - find type of a file or files - main program.
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
#include "patchlevel.h"
#include "debug.h"

FILE_RCSID("@(#)Id: file.c,v 1.66 2002/07/03 19:00:41 christos Exp ")


#ifdef S_IFLNK
# define USAGE  "Usage: %s [-bciknsvzL] [-f namefile] [-m magicfiles] file...\n"
#else
# define USAGE  "Usage: %s [-bciknsvz] [-f namefile] [-m magicfiles] file...\n"
#endif

#ifdef __EMX__
static char *apptypeName = NULL;
int os2_apptype (const char *fn, char *buf, int nb);
#endif /* __EMX__ */

#ifndef MAGIC
# define MAGIC "/etc/magic"
#endif

#ifndef MAXPATHLEN
#define	MAXPATHLEN	512
#endif

/*@unchecked@*/
int 			/* Global command-line options 		*/
	debug = 0, 	/* debugging 				*/
	lflag = 0,	/* follow Symlinks (BSD only) 		*/
	bflag = 0,	/* brief output format	 		*/
	zflag = 0,	/* follow (uncompress) compressed files */
	sflag = 0,	/* read block special files		*/
	iflag = 0,
	nobuffer = 0,   /* Do not buffer stdout */
	kflag = 0;	/* Keep going after the first match	*/

/*@unchecked@*/ /*@null@*/
const char *magicfile = 0;	/* where the magic is		*/
/*@unchecked@*/ /*@observer@*/
const char *default_magicfile = MAGIC;

/*@unchecked@*/
char *progname;		/* used throughout 			*/
/*@unchecked@*/
int lineno;		/* line number in the magic file	*/

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

/*
 * unwrap -- read a file of filenames, do each one.
 */
static void
unwrap(char *fn)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
	char buf[MAXPATHLEN];
	FILE *f;
	int wid = 0, cwid;

	if (strcmp("-", fn) == 0) {
		f = stdin;
		wid = 1;
	} else {
		if ((f = fopen(fn, "r")) == NULL) {
			error("Cannot open `%s' (%s).\n", fn, strerror(errno));
			/*@notreached@*/
		}

		while (fgets(buf, MAXPATHLEN, f) != NULL) {
			cwid = strlen(buf) - 1;
			if (cwid > wid)
				wid = cwid;
		}

		rewind(f);
	}

	while (fgets(buf, MAXPATHLEN, f) != NULL) {
		buf[strlen(buf)-1] = '\0';
		process(buf, wid);
		if(nobuffer)
			(void) fflush(stdout);
	}

	(void) fclose(f);
}

/*@exits@*/
static void
usage(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	(void)fprintf(stderr, USAGE, progname);
	(void)fprintf(stderr, "Usage: %s -C [-m magic]\n", progname);
#ifdef HAVE_GETOPT_H
	(void)fputs("Try `file --help' for more information.\n", stderr);
#endif
	exit(EXIT_FAILURE);
}

#ifdef HAVE_GETOPT_H
/*@exits@*/
static void
help(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	(void) puts(
"Usage: file [OPTION]... [FILE]...\n"
"Determine file type of FILEs.\n"
"\n"
"  -m, --magic-file LIST      use LIST as a colon-separated list of magic\n"
"                               number files\n"
"  -z, --uncompress           try to look inside compressed files\n"
"  -b, --brief                do not prepend filenames to output lines\n"
"  -c, --checking-printout    print the parsed form of the magic file, use in\n"
"                               conjunction with -m to debug a new magic file\n"
"                               before installing it\n"
"  -f, --files-from FILE      read the filenames to be examined from FILE\n"
"  -i, --mime                 output mime type strings\n"
"  -k, --keep-going           don't stop at the first match\n"
"  -L, --dereference          causes symlinks to be followed\n"
"  -n, --no-buffer            do not buffer output\n"
"  -s, --special-files        treat special (block/char devices) files as\n"
"                             ordinary ones\n"
"      --help                 display this help and exit\n"
"      --version              output version information and exit\n"
);
	exit(0);
}
#endif

/*
 * main - parse arguments and handle options
 */
int
main(int argc, char **argv)
	/*@globals debug, bflag, zflag, sflag, iflag, nobuffer, kflag,
		default_magicfile, lineno, magicfile, mlist, optind, progname,
		fileSystem, internalState @*/
	/*@modifies argv, debug, bflag, zflag, sflag, iflag, nobuffer, kflag,
		default_magicfile, lineno, magicfile, mlist, optind, progname,
		fileSystem, internalState @*/
{
	int c;
	int action = 0, didsomefiles = 0, errflg = 0, ret = 0, app = 0;
	char *mime, *home, *usermagic;
	struct stat sb;
#define OPTSTRING	"bcdf:ikm:nsvzCL"
#ifdef HAVE_GETOPT_H
	int longindex;
/*@-nullassign -readonlytrans@*/
	static struct option long_options[] =
	{
		{"version", 0, 0, 'v'},
		{"help", 0, 0, 0},
		{"brief", 0, 0, 'b'},
		{"checking-printout", 0, 0, 'c'},
		{"debug", 0, 0, 'd'},
		{"files-from", 1, 0, 'f'},
		{"mime", 0, 0, 'i'},
		{"keep-going", 0, 0, 'k'},
#ifdef S_IFLNK
		{"dereference", 0, 0, 'L'},
#endif
		{"magic-file", 1, 0, 'm'},
		{"uncompress", 0, 0, 'z'},
		{"no-buffer", 0, 0, 'n'},
		{"special-files", 0, 0, 's'},
		{"compile", 0, 0, 'C'},
		{0, 0, 0, 0},
	};
/*@=nullassign =readonlytrans@*/
#endif

#ifdef LC_CTYPE
	setlocale(LC_CTYPE, ""); /* makes islower etc work for other langs */
#endif

#ifdef __EMX__
	/* sh-like wildcard expansion! Shouldn't hurt at least ... */
	_wildcard(&argc, &argv);
#endif

/*@-modobserver@*/
	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];
/*@=modobserver@*/

	magicfile = default_magicfile;
	if ((usermagic = getenv("MAGIC")) != NULL)
		magicfile = usermagic;
	else {
		if ((home = getenv("HOME")) != NULL) {
			if ((usermagic = malloc(strlen(home) + 8)) != NULL) {
				(void)strcpy(usermagic, home);
				(void)strcat(usermagic, "/.magic");
				if (stat(usermagic, &sb)<0) 
					free(usermagic);
				else
					magicfile = usermagic;
			}
		}
	}

#ifndef HAVE_GETOPT_H
	while ((c = getopt(argc, argv, OPTSTRING)) != -1)
#else
	while ((c = getopt_long(argc, argv, OPTSTRING, long_options,
	    &longindex)) != -1)
#endif
	{
		switch (c) {
#ifdef HAVE_GETOPT_H
		case 0 :
			if (longindex == 1)
				help();
			/*@switchbreak@*/ break;
#endif
		case 'b':
			++bflag;
			/*@switchbreak@*/ break;
		case 'c':
			action = CHECK;
			/*@switchbreak@*/ break;
		case 'C':
			action = COMPILE;
			/*@switchbreak@*/ break;
		case 'd':
			++debug;
			/*@switchbreak@*/ break;
		case 'f':
			if (!app) {
				ret = apprentice(magicfile, action);
				if (action)
					exit(ret);
				app = 1;
			}
			unwrap(optarg);
			++didsomefiles;
			/*@switchbreak@*/ break;
		case 'i':
			iflag++;
			mime = malloc(strlen(magicfile) + sizeof(".mime"));
			if (mime != NULL) {
				(void)strcpy(mime, magicfile);
				(void)strcat(mime, ".mime");
				magicfile = mime;
			}
			/*@switchbreak@*/ break;
		case 'k':
			kflag = 1;
			/*@switchbreak@*/ break;
		case 'm':
			magicfile = optarg;
			/*@switchbreak@*/ break;
		case 'n':
			++nobuffer;
			/*@switchbreak@*/ break;
		case 's':
			sflag++;
			/*@switchbreak@*/ break;
		case 'v':
			(void) fprintf(stdout, "%s-%d.%d\n", progname,
				       FILE_VERSION_MAJOR, patchlevel);
			(void) fprintf(stdout, "magic file from %s\n",
				       magicfile);
			return 1;
		case 'z':
			zflag++;
			/*@switchbreak@*/ break;
#ifdef S_IFLNK
		case 'L':
			++lflag;
			/*@switchbreak@*/ break;
#endif
		case '?':
		default:
			errflg++;
			/*@switchbreak@*/ break;
		}
	}

	if (errflg) {
		usage();
	}

	if (!app) {
		ret = apprentice(magicfile, action);
		if (action)
			exit(ret);
		app = 1;
	}

	if (optind == argc) {
		if (!didsomefiles) {
			usage();
		}
	}
	else {
		int i, wid, nw;
		for (wid = 0, i = optind; i < argc; i++) {
			nw = strlen(argv[i]);
			if (nw > wid)
				wid = nw;
		}
		for (; optind < argc; optind++)
			process(argv[optind], wid);
	}

	return 0;
}
