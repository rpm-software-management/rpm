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

/*
 * A safe, read-only variant of popen(3) that passes untrusted input (arg) as a
 * positional argument to sh(1) where the command (cmd) can read it via $1.
 */
FILE *rpopen(const char *cmd, const char *arg);
int rpclose(FILE *stream);

#endif /* _CLIUTIL_H */
