#ifndef H_VERIFY
#define H_VERIFY

#include <rpmlib.h>

#define VERIFY_FILES		(1 << 1)
#define VERIFY_DEPS		(1 << 2)
#define VERIFY_SCRIPT		(1 << 3)
#define VERIFY_MD5		(1 << 4)

extern struct poptOption rpmVerifyPoptTable[];

int rpmVerify(const char * prefix, enum rpmQVSources source, int verifyFlags,
	const char *arg);

#endif
