#include <fcntl.h>
#include <unistd.h>

#include "install.h"
#include "lib/rpmlib.h"
#include "messages.h"

void doInstall(char * prefix, char * arg, int test, int installFlags) {
    rpmdb db;
    int fd;
    int mode, rc;

    if (test) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (!rpmdbOpen(prefix, &db, mode, 0644)) {
	fprintf(stderr, "error: cannot open %s/var/lib/rpm/packages.rpm\n", prefix);
	exit(1);
    }

    message(MESS_DEBUG, "installing %s\n", arg);
    fd = open(arg, O_RDONLY);
    if (fd < 0) {
	rpmdbClose(db);
	fprintf(stderr, "error: cannot open %s\n", arg);
	return;
    }

    rc = rpmInstallPackage(prefix, db, fd, test);
    if (rc == 1) {
	fprintf(stderr, "error: %s is not a RPM package\n", arg);
    }

    close(fd);
    rpmdbClose(db);
}

void doUninstall(char * prefix, char * arg, int test, int uninstallFlags) {
    rpmdb db;
    dbIndexSet matches;
    int i;
    int mode;

    if (test) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (!rpmdbOpen(prefix, &db, mode, 0644)) {
	fprintf(stderr, "cannot open %s/var/lib/rpm/packages.rpm\n", prefix);
	exit(1);
    }
   
    if (rpmdbFindPackage(db, arg, &matches)) {
	fprintf(stderr, "package %s is not installed\n", arg);
    } else {
	if (matches.count > 1) {
	    fprintf(stderr, "\"%s\" specifies multiple packages\n", arg);
	}
	else { 
	    for (i = 0; i < matches.count; i++) {
		rpmRemovePackage(prefix, db, matches.recs[i].recOffset, test);
	    }
	}

	freeDBIndexRecord(matches);
    }

    rpmdbClose(db);
}
