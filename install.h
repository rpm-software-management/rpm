#ifndef _H_INSTALL
#define _H_INSTALL

#define RPMINSTALL_PERCENT 		(1 << 0)
#define RPMINSTALL_HASH	   		(1 << 1)
#define RPMINSTALL_NODEPS	   	(1 << 2)

#define RPMUNINSTALL_NODEPS		(1 << 0)

int doInstall(char * prefix, char ** argv, int installFlags, 
	      int interfaceFlags);
int doSourceInstall(char * prefix, char * arg, char ** specFile);
int doUninstall(char * prefix, char ** argv, int uninstallFlags, 
		 int interfaceFlags);

#endif

