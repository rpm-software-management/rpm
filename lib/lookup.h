#ifndef H_LOOKUP
#define H_LOOKUP

#include "rpmlib.h"

int findMatches(rpmdb db, char * name, char * version, char * release,
		       dbiIndexSet * matches);


#endif
