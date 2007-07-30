/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: db_codegen.c,v 1.2 2007/05/17 15:14:58 bostic Exp $
 */

#include "db_codegen.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996,2007 Oracle.  All rights reserved.\n";
#endif

static int usage __P((void));

int main __P((int, char *[]));

const char *progname;
struct __head_env env_tree;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	enum { C, CXX, JAVA } api;
	int ch, verbose;
	char *ofname;

	TAILQ_INIT(&env_tree);

	if ((progname = __db_rpath(argv[0])) == NULL)
		progname = argv[0];
	else
		++progname;

	api = C;
	ofname = NULL;
	verbose = 0;

	while ((ch = getopt(argc, argv, "a:i:o:Vv")) != EOF)
		switch (ch) {
		case 'a':
			if (strcasecmp(optarg, "c") == 0) {
				api = C;
			}
			else if (
			    strcasecmp(optarg, "c++") == 0 ||
			    strcasecmp(optarg, "cxx") == 0)
				api = CXX;
			else if (strcasecmp(optarg, "java") == 0)
				api = JAVA;
			else
				return (usage());
			break;
		case 'i':
			if (freopen(optarg, "r", stdin) == NULL) {
				fprintf(stderr, "%s: %s: %s\n",
				    progname, optarg, strerror(errno));
				return (EXIT_FAILURE);
			}
			break;
		case 'o':
			ofname = optarg;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		return (usage());

	if (parse_input(stdin))
		return (EXIT_FAILURE);

#ifdef DEBUG
	if (verbose && parse_dump())
		return (EXIT_FAILURE);
#endif

	if (TAILQ_FIRST(&env_tree) != NULL)
		switch (api) {
		case C:
			if (api_c(ofname))
				return (EXIT_FAILURE);
			break;
		case CXX:
		case JAVA:
			fprintf(stderr,
			    "C++ and Java APIs not yet supported\n");
			return (EXIT_FAILURE);
		}

	return (EXIT_SUCCESS);
}

static int
usage()
{
	(void)fprintf(stderr,
    "usage: %s [-Vv] [-a c] [-i input] [-o output]\n", progname);
	return (EXIT_FAILURE);
}
