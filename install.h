#ifndef _H_INSTALL
#define _H_INSTALL

#include <stdio.h>

#define INSTALL_PERCENT         (1 << 0)
#define INSTALL_HASH            (1 << 1)
#define INSTALL_NODEPS          (1 << 2)

#define UNINSTALL_NODEPS        (1 << 0)

int doInstall(char * rootdir, char ** argv, char * prefix, int installFlags, 
	      int interfaceFlags);
int doSourceInstall(char * prefix, char * arg, char ** specFile);
int doUninstall(char * rootdir, char ** argv, int uninstallFlags, 
		 int interfaceFlags);
void printDepFlags(FILE * f, char * version, int flags);

#endif

