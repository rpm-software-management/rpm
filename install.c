#include <alloca.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "install.h"
#include "lib/rpmlib.h"
#include "messages.h"
#include "query.h"
#include "url.h"

static int hashesPrinted = 0;

static void printHash(const unsigned long amount, const unsigned long total);
static void printPercent(const unsigned long amount, const unsigned long total);
static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts);

static void printHash(const unsigned long amount, const unsigned long total) {
    int hashesNeeded;

    if (hashesPrinted != 50) {
	hashesNeeded = 50 * (total ? (((float) amount) / total) : 1);
	while (hashesNeeded > hashesPrinted) {
	    printf("#");
	    hashesPrinted++;
	}
	fflush(stdout);
	hashesPrinted = hashesNeeded;

	if (hashesPrinted == 50)
	    printf("\n");
    }
}

static void printPercent(const unsigned long amount, const unsigned long total) 
{
    printf("%%%% %f\n", (total
                        ? ((float) ((((float) amount) / total) * 100))
                        : 100.0));
    fflush(stdout);
}

static int installPackages(char * rootdir, char ** packages, char * location,
			    int numPackages, int installFlags, 
			    int interfaceFlags, rpmdb db) {
    int i, fd;
    int numFailed = 0;
    char ** filename;
    char * printFormat = NULL;
    char * chptr;
    int rc;
    notifyFunction fn;
    char * netsharedPath = NULL;

    if (interfaceFlags & RPMINSTALL_PERCENT)
	fn = printPercent;
    else if (interfaceFlags & RPMINSTALL_HASH)
	fn = printHash;
    else
	fn = NULL;

    netsharedPath = getVar(RPMVAR_NETSHAREDPATH);

    for (i = 0, filename = packages; i < numPackages; i++, filename++) {
	if (!*filename) continue;

	hashesPrinted = 0;

	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, "error: cannot open file %s\n", *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
	} 

	if (interfaceFlags & RPMINSTALL_PERCENT) 
	    printFormat = "%%f %s:%s:%s\n";
	else if (isVerbose() && (interfaceFlags & RPMINSTALL_HASH)) {
	    chptr = strrchr(*filename, '/');
	    if (!chptr)
		chptr = *filename;
	    else
		chptr++;

	    printFormat = "%-28s";
	} else if (isVerbose())
	    printf("Installing %s\n", *filename);

	if (db) {
	    rc = rpmInstallPackage(rootdir, db, fd, location, installFlags, fn, 
				   printFormat, netsharedPath);
	} else {
	    if (installFlags &= INSTALL_TEST) {
		message(MESS_DEBUG, "stopping source install as we're "
			"just testing\n");
		rc = 0;
	    } else {
		rc = rpmInstallSourcePackage(rootdir, fd, NULL, fn,
					     printFormat);
	    }
	} 

	if (rc == 1) {
	    fprintf(stderr, "error: %s does not appear to be a RPM package\n", 
			*filename);
	}
	    
	if (rc) {
	    fprintf(stderr, "error: %s cannot be installed\n", *filename);
	    numFailed++;
	}

	close(fd);
    }

    return numFailed;
}

int doInstall(char * rootdir, char ** argv, char * location, int installFlags, 
	      int interfaceFlags) {
    rpmdb db;
    int fd, i;
    int mode, rc;
    char ** packages, ** tmpPackages;
    char ** filename;
    int numPackages;
    int numTmpPackages = 0, numBinaryPackages = 0, numSourcePackages = 0;
    int numFailed = 0;
    Header * binaryHeaders;
    int isSource;
    int tmpnum = 0;
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopInstall = 0;

    if (installFlags & INSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;

    message(MESS_DEBUG, "counting packages to install\n");
    for (filename = argv, numPackages = 0; *filename; filename++, numPackages++)
	;

    message(MESS_DEBUG, "found %d packages\n", numPackages);
    packages = alloca((numPackages + 1) * sizeof(char *));
    packages[numPackages] = NULL;
    tmpPackages = alloca((numPackages + 1) * sizeof(char *));
    binaryHeaders = alloca((numPackages + 1) * sizeof(Header));
	
    message(MESS_DEBUG, "looking for packages to download\n");
    for (filename = argv, i = 0; *filename; filename++) {
	if (urlIsURL(*filename)) {
	    if (isVerbose()) {
		printf("Retrieving %s\n", *filename);
	    }
	    packages[i] = alloca(strlen(*filename) + 30 + strlen(rootdir));
	    sprintf(packages[i], "%s/var/tmp/rpm-ftp-%d-%d.tmp", rootdir, 
		    tmpnum++, (int) getpid());
	    message(MESS_DEBUG, "getting %s as %s\n", *filename, packages[i]);
	    fd = urlGetFile(*filename, packages[i]);
	    if (fd < 0) {
		fprintf(stderr, "error: skipping %s - transfer failed - %s\n", 
			*filename, ftpStrerror(fd));
		numFailed++;
	    } else {
		tmpPackages[numTmpPackages++] = packages[i];
		i++;
	    }
	} else {
	    packages[i++] = *filename;
	}
    }

    message(MESS_DEBUG, "retrieved %d packages\n", numTmpPackages);

    message(MESS_DEBUG, "finding source and binary packages\n");
    for (filename = packages; *filename; filename++) {
	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, "error: cannot open file %s\n", *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
	}

	rc = pkgReadHeader(fd, &binaryHeaders[numBinaryPackages], &isSource,
			   NULL, NULL);

	close(fd);
	
	if (rc == 1) {
	    fprintf(stderr, "error: %s does not appear to be a RPM package\n", 
			*filename);
	}
	    
	if (rc) {
	    fprintf(stderr, "error: %s cannot be installed\n", *filename);
	    numFailed++;
	    *filename = NULL;
	} else if (isSource) {
	    /* the header will be NULL if this is a v1 source package */
	    if (binaryHeaders[numBinaryPackages])
		freeHeader(binaryHeaders[numBinaryPackages]);

	    numSourcePackages++;
	} else {
	    numBinaryPackages++;
	}
    }

    message(MESS_DEBUG, "found %d source and %d binary packages\n", 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages) {
	message(MESS_DEBUG, "opening database mode: 0%o\n", mode);
	if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	    fprintf(stderr, "error: cannot open %s/var/lib/rpm/packages.rpm\n", 
			rootdir);
	    exit(1);
	}

	if (!(interfaceFlags & RPMINSTALL_NODEPS)) {
	    rpmdep = rpmdepDependencies(db);
	    for (i = 0; i < numBinaryPackages; i++)
		if (installFlags & INSTALL_UPGRADE)
		    rpmdepUpgradePackage(rpmdep, binaryHeaders[i]);
		else
		    rpmdepAddPackage(rpmdep, binaryHeaders[i]);

	    if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
		numFailed = numPackages;
		stopInstall = 1;
	    }

	    rpmdepDone(rpmdep);

	    if (!stopInstall && conflicts) {
		fprintf(stderr, "failed dependencies:\n");
		printDepProblems(stderr, conflicts, numConflicts);
		rpmdepFreeConflicts(conflicts, numConflicts);
		numFailed = numPackages;
		stopInstall = 1;
	    }
	}
    }
    else
	db = NULL;

    if (!stopInstall) {
	message(MESS_DEBUG, "installing binary packages\n");
	numFailed += installPackages(rootdir, packages, location, numPackages, 
				     installFlags, interfaceFlags, db);
    }

    for (i = 0; i < numTmpPackages; i++)
	unlink(tmpPackages[i]);

    for (i = 0; i < numBinaryPackages; i++) 
	freeHeader(binaryHeaders[i]);

    if (db) rpmdbClose(db);

    return numFailed;
}

int doUninstall(char * rootdir, char ** argv, int uninstallFlags,
		 int interfaceFlags) {
    rpmdb db;
    dbIndexSet matches;
    int i, j;
    int mode;
    int rc;
    int count;
    int numPackages;
    int * packageOffsets;
    char ** arg;
    int numFailed = 0;
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopUninstall = 0;

    message(MESS_DEBUG, "counting packages to uninstall\n");
    for (arg = argv, numPackages = 0; *arg; arg++, numPackages++)
	;
    message(MESS_DEBUG, "found %d packages to uninstall\n", numPackages);

    packageOffsets = alloca(sizeof(int *) * numPackages);

    if (uninstallFlags & UNINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	fprintf(stderr, "cannot open %s/var/lib/rpm/packages.rpm\n", rootdir);
	exit(1);
    }

    j = 0;
    for (arg = argv, numPackages = 0; *arg; arg++, numPackages++) {
	rc = findPackageByLabel(db, *arg, &matches);
	if (rc == 1) {
	    fprintf(stderr, "package %s is not installed\n", *arg);
	    numFailed++;
	} else if (rc == 2) {
	    fprintf(stderr, "error searching for package %s\n", *arg);
	    numFailed++;
	} else {
	    count = 0;
	    for (i = 0; i < matches.count; i++)
		if (matches.recs[i].recOffset) count++;

	    if (count > 1) {
		fprintf(stderr, "\"%s\" specifies multiple packages\n", *arg);
		numFailed++;
	    }
	    else { 
		for (i = 0; i < matches.count; i++) {
		    if (matches.recs[i].recOffset) {
			packageOffsets[j++] = matches.recs[i].recOffset;
		    }
		}
	    }

	    freeDBIndexRecord(matches);
	}
    }
    numPackages = j;

    if (!(interfaceFlags & RPMUNINSTALL_NODEPS)) {
	rpmdep = rpmdepDependencies(db);
	for (i = 0; i < numPackages; i++)
	    rpmdepRemovePackage(rpmdep, packageOffsets[i]);

	if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
	    numFailed = numPackages;
	    stopUninstall = 1;
	}

	rpmdepDone(rpmdep);

	if (!stopUninstall && conflicts) {
	    fprintf(stderr, "removing these packages would break "
			    "dependencies:\n");
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (!stopUninstall) {
	for (i = 0; i < numPackages; i++) {
	    message(MESS_DEBUG, "uninstalling record number %d\n",
			packageOffsets[i]);
	    rpmRemovePackage(rootdir, db, packageOffsets[i], uninstallFlags);
	}
    }

    rpmdbClose(db);

    return numFailed;
}

int doSourceInstall(char * rootdir, char * arg, char ** specFile) {
    int fd;
    int rc;

    fd = open(arg, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "error: cannot open %s\n", arg);
	return 1;
    }

    if (isVerbose())
	printf("Installing %s\n", arg);

    rc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL);
    if (rc == 1) {
	fprintf(stderr, "error: %s cannot be installed\n", arg);
    }

    close(fd);

    return rc;
}

void printDepFlags(FILE * f, char * version, int flags) {
    if (flags)
	fprintf(f, " ");

    if (flags & REQUIRE_LESS) 
	fprintf(f, "<");
    if (flags & REQUIRE_GREATER)
	fprintf(f, ">");
    if (flags & REQUIRE_EQUAL)
	fprintf(f, "=");
    if (flags & REQUIRE_SERIAL)
	fprintf(f, "S");

    if (flags)
	fprintf(f, " %s", version);
}

static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts) {
    int i;

    for (i = 0; i < numConflicts; i++) {
	fprintf(f, "\t%s", conflicts[i].needsName);
	if (conflicts[i].needsFlags) {
	    printDepFlags(stderr, conflicts[i].needsVersion, 
			  conflicts[i].needsFlags);
	}

	if (conflicts[i].sense == RPMDEP_SENSE_REQUIRES) 
	    fprintf(f, " is needed by %s-%s-%s\n", conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
	else
	    fprintf(f, " conflicts with %s-%s-%s\n", conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
    }
}
