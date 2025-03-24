#ifndef _CLIUTIL_H
#define _CLIUTIL_H

/** \file cliutils.h
 *
 *  Misc helpers for RPM CLI tools
 */

#include <stdio.h>
#include <popt.h>
#include <rpm/rpmutil.h>

/* "normalized" exit: avoid overflowing and xargs special value 255 */
#define RETVAL(rc) (((rc) > 254) ? 254 : (rc))

RPM_GNUC_NORETURN
void argerror(const char * desc);

void printUsage(poptContext con, FILE * fp, int flags);

int initPipe(void);

int finishPipe(void);

#endif /* _CLIUTIL_H */
