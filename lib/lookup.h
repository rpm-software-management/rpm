#ifndef H_LOOKUP
#define H_LOOKUP

#include <rpmlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XXX only for the benefit of runTransactions() */
int findMatches(rpmdb db, const char * name, const char * version,
	const char * release, /*@out@*/ dbiIndexSet * matches);

#ifdef __cplusplus
}
#endif

#endif
