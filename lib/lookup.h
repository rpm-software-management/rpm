#ifndef H_LOOKUP
#define H_LOOKUP

#include "rpmlib.h"

int findMatches(rpmdb db, const char * name, const char * version,
	const char * release, dbiIndexSet * matches);

#endif
