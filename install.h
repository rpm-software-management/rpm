#ifndef _H_INSTALL
#define _H_INSTALL

#define RPMINSTALL_PERCENT 1
#define RPMINSTALL_HASH	   2

void doInstall(char * prefix, char * arg, int installFlags,
	       int interfaceFlags);
void doUninstall(char * prefix, char * arg, int test, int uninstallFlags);

#endif

