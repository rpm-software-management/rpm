#ifndef H_VERIFY
#define H_VERIFY

#include <rpmlib.h>

#define VERIFY_FILES		(1 << 0)
#define VERIFY_DEPS		(1 << 1)
#define VERIFY_SCRIPT		(1 << 2)
#define VERIFY_MD5		(1 << 3)

enum verifysources { VERIFY_PATH, VERIFY_PACKAGE, VERIFY_EVERY, VERIFY_RPM, 
			VERIFY_GRP };

int doVerify(const char * prefix, enum verifysources source, const char ** argv,
	      int verifyFlags);

#endif
