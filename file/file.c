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

/*@access fmagic @*/

/*@unchecked@*/
extern fmagic global_fmagic;

/*@unchecked@*/ /*@observer@*/
extern const char * default_magicfile;

#ifdef S_IFLNK
# define USAGE  "Usage: %s [-bciknsvzL] [-f namefile] [-m magicfiles] file...\n"
#else
# define USAGE  "Usage: %s [-bciknsvz] [-f namefile] [-m magicfiles] file...\n"
#endif

#ifdef __EMX__
static char *apptypeName = NULL;
int os2_apptype (const char *fn, char *buf, int nb);
#endif /* __EMX__ */

#ifndef MAXPATHLEN
#define	MAXPATHLEN	512
#endif

			/* Global command-line options 		*/
/*@unchecked@*/
static	int	nobuffer = 0;   /* Do not buffer stdout */

/*@unchecked@*/
char *progname;		/* used throughout 			*/

/*
 * unwrap -- read a file of filenames, do each one.
 */
static void
unwrap(fmagic fm, char *fn)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/
{
	char buf[MAXPATHLEN];
	FILE *f;
	int wid = 0, cwid;
	int xx;

	if (strcmp("-", fn) == 0) {
		f = stdin;
		wid = 1;
	} else {
		if ((f = fopen(fn, "r")) == NULL) {
			error(EXIT_FAILURE, 0, "Cannot open `%s' (%s).\n", fn, strerror(errno));
			/*@notreached@*/
		}

		while (fgets(buf, sizeof(buf), f) != NULL) {
			cwid = strlen(buf) - 1;
			if (cwid > wid)
				wid = cwid;
		}

		rewind(f);
	}

	while (fgets(buf, sizeof(buf), f) != NULL) {
		buf[strlen(buf)-1] = '\0';
		fm->obp = fm->obuf;
		*fm->obp = '\0';
		fm->nob = sizeof(fm->obuf);
		xx = fmagicProcess(fm, buf, wid);
		fprintf(stdout, "%s\n", fm->obuf);
		if (nobuffer)
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
	/*@globals global_fmagic, nobuffer,
		default_magicfile, optind, progname,
		fileSystem, internalState @*/
	/*@modifies argv, global_fmagic, nobuffer,
		default_magicfile, optind, progname,
		fileSystem, internalState @*/
{
	int xx;
	int c;
	int action = 0, didsomefiles = 0, errflg = 0, ret = 0, app = 0;
	char *mime, *home, *usermagic;
	fmagic fm = global_fmagic;
	struct stat sb;
#define OPTSTRING	"bcdf:ikm:nsvzCL"
#ifdef HAVE_GETOPT_H
	int longindex = 0;
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

#if HAVE_MCHECK_H && HAVE_MTRACE
	/*@-noeffect@*/
	mtrace(); /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
	/*@=noeffect@*/
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

/*@-assignexpose@*/
	fm->magicfile = default_magicfile;
/*@=assignexpose@*/
	if ((usermagic = getenv("MAGIC")) != NULL)
		fm->magicfile = usermagic;
	else {
		if ((home = getenv("HOME")) != NULL) {
			size_t nb = strlen(home) + 8;
			usermagic = xmalloc(nb);
			(void)strcpy(usermagic, home);
			(void)strcat(usermagic, "/.magic");
			if (stat(usermagic, &sb)<0) 
				free(usermagic);
			else
				fm->magicfile = usermagic;
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
			fm->flags |= FMAGIC_FLAGS_BRIEF;
			/*@switchbreak@*/ break;
		case 'c':
			action = CHECK;
			/*@switchbreak@*/ break;
		case 'C':
			action = COMPILE;
			/*@switchbreak@*/ break;
		case 'd':
			fm->flags |= FMAGIC_FLAGS_DEBUG;
			/*@switchbreak@*/ break;
		case 'f':
			if (!app) {
				ret = fmagicSetup(fm, fm->magicfile, action);
				if (action)
					exit(ret);
				app = 1;
			}
			unwrap(fm, optarg);
			++didsomefiles;
			/*@switchbreak@*/ break;
		case 'i':
			fm->flags |= FMAGIC_FLAGS_MIME;
			mime = malloc(strlen(fm->magicfile) + sizeof(".mime"));
			if (mime != NULL) {
				(void)strcpy(mime, fm->magicfile);
				(void)strcat(mime, ".mime");
			}
			fm->magicfile = mime;
			/*@switchbreak@*/ break;
		case 'k':
			fm->flags |= FMAGIC_FLAGS_CONTINUE;
			/*@switchbreak@*/ break;
		case 'm':
/*@-assignexpose@*/
			fm->magicfile = optarg;
/*@=assignexpose@*/
			/*@switchbreak@*/ break;
		case 'n':
			++nobuffer;
			/*@switchbreak@*/ break;
		case 's':
			fm->flags |= FMAGIC_FLAGS_SPECIAL;
			/*@switchbreak@*/ break;
		case 'v':
			(void) fprintf(stdout, "%s-%d.%d\n", progname,
				       FILE_VERSION_MAJOR, patchlevel);
			(void) fprintf(stdout, "magic file from %s\n",
				       fm->magicfile);
			return 1;
		case 'z':
			fm->flags |= FMAGIC_FLAGS_UNCOMPRESS;
			/*@switchbreak@*/ break;
#ifdef S_IFLNK
		case 'L':
			fm->flags |= FMAGIC_FLAGS_FOLLOW;
			/*@switchbreak@*/ break;
#endif
		case '?':
		default:
			errflg++;
			/*@switchbreak@*/ break;
		}
	}

	if (errflg)
		usage();

	if (!app) {
		ret = fmagicSetup(fm, fm->magicfile, action);
		if (action)
			exit(ret);
		app = 1;
	}

	if (optind == argc) {
		if (!didsomefiles)
			usage();
	} else {
		int i, wid, nw;
		for (wid = 0, i = optind; i < argc; i++) {
			nw = strlen(argv[i]);
			if (nw > wid)
				wid = nw;
		}
		for (; optind < argc; optind++) {
			fm->obp = fm->obuf;
			*fm->obp = '\0';
			fm->nob = sizeof(fm->obuf);
			xx = fmagicProcess(fm, argv[optind], wid);
			fprintf(stdout, "%s\n", fm->obuf);
			if (nobuffer)
				(void) fflush(stdout);
		}
	}

#if HAVE_MCHECK_H && HAVE_MTRACE
	/*@-noeffect@*/
	muntrace(); /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
	/*@=noeffect@*/
#endif

	return 0;
}
