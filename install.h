#ifndef _H_INSTALL
#define _H_INSTALL

#include "lib/rpmlib.h"

#define INSTALL_PERCENT         (1 << 0)
#define INSTALL_HASH            (1 << 1)
#define INSTALL_NODEPS          (1 << 2)
#define INSTALL_NOORDER         (1 << 3)
#define INSTALL_LABEL		(1 << 4)  /* set if we're being verbose */
#define INSTALL_UPGRADE		(1 << 5)

#define UNINSTALL_NODEPS        (1 << 0)
#define UNINSTALL_ALLMATCHES    (1 << 1)

int doInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, int probFilter, rpmRelocation * relocations);
int doSourceInstall(const char * prefix, const char * arg, const char ** specFile,
		    char ** cookie);
int doUninstall(const char * rootdir, const char ** argv, int uninstallFlags, 
		 int interfaceFlags);
void printDepFlags(FILE * f, const char * version, int flags);
void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts);

#endif

