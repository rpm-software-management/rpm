#ifndef _CLIUTIL_H
#define _CLIUTIL_H

#include <stdio.h>
#include <popt.h>
#include <rpm/rpmutil.h>

RPM_GNUC_NORETURN
void argerror(const char * desc);

void printUsage(poptContext con, FILE * fp, int flags);

/* Initialize cli-environment, returning parsed popt context caller */
poptContext initCli(const char *ctx, struct poptOption *optionsTable,
		    int argc, char *argv[]);

/* Free up common resources, return "normalized" exit code */
int finishCli(poptContext optCon, int rc);

int initPipe(void);

void finishPipe(void);

#endif /* _CLIUTIL_H */
