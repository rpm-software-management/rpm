// standard C++ includes
#include <fstream>

// our includes
#include "XMLSpec.h"

// rpm includes
#include <rpmbuild.h>

// display some usage
int usage()
{
		printf("Usage: spec2xml input [output]\n");
		printf("Converts the input pkg.spec file to a pkg.spec.xml\n");
		printf("file for use in an rpmbuild command.\n\n");
		return 1;
}

// program entry point
int main(int argc,
		 char** argv)
{
	printf("\nspec2xml, version 0.01\n");
	if (argc != 2 && argc != 3) {
		usage();
		return 1;
	}

	Spec pSpec = NULL;
	printf("Parsing RPM spec %s:\n", argv[1]);
	if (!parseSpec(&pSpec, argv[1], "/", "/var/tmp", 0, NULL, NULL, 1, 0)) {
		printf("\tOk, spec parsed.\n");
		printf("Creating XML structures:\n");
		XMLSpec* pXSpec = XMLSpec::structCreate(pSpec);
		if (pXSpec) {
			printf("\tOk, structures created.\n");
			if (argc == 3) {
				printf("Writing XML to %s ... ", argv[2]);
				ofstream fOut(argv[2]);
				if (fOut.is_open()) {
					pXSpec->toXMLFile(fOut);
					fOut.close();
					printf("Ok.\n");
				}
				else {
					delete pSpec;
					printf("Failed.\n");
					return 2;
				}
			}
			else if (argc == 2) {
				pXSpec->toXMLFile(cout);
			}

			delete pSpec;
			return 0;
		}
	}

	printf("\tFailed.\n");
	return 3;
}
