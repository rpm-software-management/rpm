#ifndef _H_INSTALL
#define _H_INSTALL

#define RPMINSTALL_PERCENT 1
#define RPMINSTALL_HASH	   2

int doInstall(char * prefix, char * arg, int installFlags, int interfaceFlags);
int doSourceInstall(char * prefix, char * arg, char ** specFile);
void doUninstall(char * prefix, char * arg, int test, int uninstallFlags);

#endif

