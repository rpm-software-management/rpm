#ifndef H_VERIFY
#define H_VERIFY

#include <rpmlib.h>

enum verifysources { VERIFY_PATH, VERIFY_PACKAGE, VERIFY_EVERY, VERIFY_SPATH,
		     VERIFY_SPACKAGE, VERIFY_RPM, VERIFY_SRPM, VERIFY_GRP,
		     VERIFY_SGROUP };

void doVerify(char * prefix, enum verifysources source, char ** argv);

#endif
