#ifndef H_QUERY
#define H_QUERY

#include <rpmlib.h>

enum querysources { QUERY_PATH, QUERY_PACKAGE, QUERY_ALL, QUERY_SPATH,
		    QUERY_SPACKAGE, QUERY_RPM, QUERY_SRPM, QUERY_GROUP,
		    QUERY_SGROUP };

#define QUERY_FOR_INFO 		(1 << 0)
#define QUERY_FOR_LIST		(1 << 1)
#define QUERY_FOR_STATE		(1 << 2)
#define QUERY_FOR_DOCS		(1 << 3)
#define QUERY_FOR_CONFIG	(1 << 4)
#define QUERY_FOR_SCRIPTS	(1 << 5)

int doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg, char * queryFormat);
void queryPrintTags(void);

/* 0 found matches */
/* 1 no matches */
/* 2 error */
int findPackageByLabel(rpmdb db, char * arg, dbIndexSet * matches);

#endif
