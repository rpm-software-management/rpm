#include <fcntl.h>

#include "install.h"
#include "lib/rpmlib.h"

void doInstall(char * prefix, int test, int installFlags) {
    printf("I can't install packages yet");
}

void doUninstall(char * prefix, char * arg, int test, int uninstallFlags) {
    rpmdb db;
    dbIndexSet matches;
    int i;

    if (!rpmdbOpen(prefix, &db, O_RDWR | O_EXCL, 0644)) {
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
