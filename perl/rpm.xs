/*
 * Perl interface to rpmlib
 *
 * $Id: rpm.xs,v 1.4 1999/07/16 08:44:54 gafton Exp $
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <rpm/rpmio.h>
#include <rpm/dbindex.h>
#include <rpm/header.h>
#include <popt.h>
#include <rpm/rpmlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * External functions
 */
extern double constant(char *name, int arg);


MODULE = rpm		PACKAGE = rpm		

PROTOTYPES: ENABLE

BOOT:
# The following message will be printed when the
# bootstrap function executes.
	if (rpmReadConfigFiles(NULL, NULL) != 0) {
	  XSRETURN_NO;
	}

double
constant(name,arg)
	char *		name
	int		arg

Header
Header(package)
  const char *	package
  PREINIT:
	FD_t file_desc = NULL;
	int rc;
	int isSource;
	int had_error = 0;
  CODE:
	/* New(1,RETVAL,1,Header); */
	file_desc = fdOpen(package, O_RDONLY, 0);
	if (file_desc != NULL && RETVAL != NULL) {
	  rc = rpmReadPackageHeader(file_desc, &RETVAL, &isSource, NULL, NULL);
	  if (rc != 0) {
	    had_error++;
	  }
	  if (file_desc != NULL) {
	    fdClose(file_desc);
	  }
	} else {
	  had_error++;
	}
	ST(0) = sv_newmortal();
	if (had_error) {
	  ST(0) = &PL_sv_undef;
	} else {
	  sv_setref_pv(ST(0), "Header", (void*)RETVAL);
	}

rpmTransactionSet
Transaction(header)
  Header header
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

rpmdb
dbOpen(root = "", forWrite = 0)
  const char * root
  int forWrite
  PREINIT:
	int retval;
  CODE:
	retval = rpmdbOpen(root, &RETVAL, forWrite ? O_RDWR | O_CREAT : O_RDONLY, 0644);
	printf("\nretval is %d\n", retval);
	ST(0) = sv_newmortal();
	if (retval != 0) {
	  ST(0) = &PL_sv_undef;
	} else {
	  sv_setref_pv(ST(0), "rpmdb", (void *)RETVAL);
	}


int
dbInit(root = NULL)
  char * root
  CODE:
	RETVAL = rpmdbInit(root, 0);
  OUTPUT:
	RETVAL

int
dbRebuild(root = NULL)
  char * root
  CODE:
	RETVAL = rpmdbRebuild(root);
  OUTPUT:
	RETVAL


int
Error()
  CODE:
	RETVAL = rpmErrorCode();
  OUTPUT:
	RETVAL

void
Debug(enable = 1)
  int enable
  CODE:
	if (enable) {
	  rpmSetVerbosity(RPMMESS_DEBUG);
	} else {
	  rpmSetVerbosity(RPMMESS_QUIET);
	}

char *
GetVar(var)
  int var
  CODE:
	RETVAL = rpmGetVar(var);
  OUTPUT:
	RETVAL

void
SetVar(var, value)
  int var
  char * value
  CODE:
	rpmSetVar(var, value);

INCLUDE: db.xs
INCLUDE: header.xs
INCLUDE: transaction.xs


