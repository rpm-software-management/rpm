// standard C++ includes
#include <fstream>

// our includes
#include "XMLParser.h"
#include "XMLSpec.h"

// display some usage
int usage()
{
		printf("Usage: xml2spec input [output]\n");
		printf("Converts the input pkg.spec.xml file to a pkg.spec\n");
		printf("file for use in an rpmbuild command.\n\n");
		return 1;
}

// program entry point
int main(int argc,
		 char** argv)
{
	printf("\nxml2spec, version 0.01\n");
	if (argc != 2 && argc != 3) {
		usage();
		return 1;
	}

	XMLSpec* pSpec = NULL;
	if (parseXMLSpec(argv[1], pSpec) == 0 && pSpec) {
		if (argc == 3) {
			printf("Writing spec to %s ... ", argv[2]);
			ofstream fOut(argv[2]);
			if (fOut.is_open()) {
				pSpec->toSpecFile(fOut);
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
			pSpec->toSpecFile(cout);
		}

		delete pSpec;
		return 0;
	}

	return 3;
}
