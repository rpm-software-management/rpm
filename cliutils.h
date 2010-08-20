#ifndef _CLIUTIL_H
#define _CLIUTIL_H

#include <stdio.h>
#include <popt.h>
#include <rpm/rpmutil.h>

RPM_GNUC_NORETURN
void argerror(const char * desc);

void printUsage(poptContext con, FILE * fp, int flags);

#endif /* _CLIUTIL_H */
