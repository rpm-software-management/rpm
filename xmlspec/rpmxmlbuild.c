#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "rpmbuild.h"

#include "xmlbuild.h"

#define RUN_HELP  "\n       Run \"rpmxmlbuild --help\" for a list of valid options."

#define MASK_BP   (RPMBUILD_PREP)
#define MASK_BC   (RPMBUILD_BUILD|MASK_BP)
#define MASK_BI   (RPMBUILD_INSTALL|MASK_BC)
#define MASK_BL   (RPMBUILD_FILECHECK)
#define MASK_BB   (RPMBUILD_PACKAGEBINARY|RPMBUILD_CLEAN|MASK_BI)
#define MASK_BS   (RPMBUILD_PACKAGESOURCE)
#define MASK_BA   (MASK_BB|MASK_BS)

int usage()
{
	printf("Usage  : rpmxmlbuild [options] <specfile>\n");
	printf("General Options:\n");
	printf("\t--help|-h|-?       - Display this message\n");
	printf("\t--quiet|-q         - Don't display build information\n");
	printf("\t--verbose|-v       - Display verbose information\n");
	printf("\n");
	printf("Build Options:\n");
	printf("\t-b{p|c|i|l|a|b|s}  - Build spec file to the options specified.\n");
	printf("\t                     (Options as per rpmbuild.)\n");
	printf("\n");

	return 0;
}

int main(int argc, char** argv)
{
	char* szSpec = NULL;
	int i = 0;
	int nBuild = 0;
	int nVerbose = 1;
	int nQuiet = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!strcasecmp(argv[i], "--help") ||
			    !strcasecmp(argv[i], "-h") ||
			    !strcasecmp(argv[i], "-?"))
				return usage();
			else if ((argv[i][1] == 'b') &&
				 (argv[i][2] != '\0') &&
				 (argv[i][3] == '\0')) {
				switch (argv[i][2]) {
					case 'a': nBuild = MASK_BA; break;
					case 'b': nBuild = MASK_BB; break;
					case 's': nBuild = MASK_BS; break;
					case 'p': nBuild = MASK_BP; break;
					case 'c': nBuild = MASK_BC; break;
					case 'i': nBuild = MASK_BI; break;
					case 'l': nBuild = MASK_BL; break;
					default:
						printf("error: Unknown option \"%s\". %s\n",
							argv[i], RUN_HELP);
						return -1;
				}
			}
			else if (!strcasecmp(argv[i], "--verbose") ||
				 !strcasecmp(argv[i], "-v")) {
				nVerbose = 1;
				nQuiet = 0;
			}
			else if (!strcasecmp(argv[i], "--quiet") ||
				 !strcasecmp(argv[i], "-q")) {
				nVerbose = 0;
				nQuiet = 1;
			}
			else {
				printf("error: Unknown option \"%s\". %s\n",
					argv[i], RUN_HELP);
				return -1;
			}
		}
		else if (!szSpec)
			szSpec = argv[i];
		else {
			printf("error: Extra characters \"%s\" on the command-line. %s\n",
				argv[i], RUN_HELP);
			return -2;
		}
	}

	if (!szSpec) {
		printf("error: No XML RPM spec specified. %s\n", RUN_HELP);
		return -3;
	}

	rpmReadConfigFiles(NULL, NULL);
	if (!nQuiet)
		rpmIncreaseVerbosity();

	return parseBuildXMLSpec(szSpec, nBuild, 0, nVerbose);
}
