#include <alloca.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "install.h"
#include "lib/rpmlib.h"
#include "lib/package.h"
#include "messages.h"
#include "query.h"

static int hashesPrinted = 0;

static void printHash(const unsigned long amount, const unsigned long total);
static void printPercent(const unsigned long amount, const unsigned long total);
static int getFtpURL(char * hostAndFile, char * dest);

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

static int installPackages(char * prefix, char ** packages, 
			    int numPackages, int installFlags, 
			    int interfaceFlags, rpmdb db) {
    int i, fd;
    int numFailed = 0;
    char ** filename;
    char * printFormat = NULL;
    char * chptr;
    int rc;
    notifyFunction fn;

    if (interfaceFlags & RPMINSTALL_PERCENT)
	fn = printPercent;
    else if (interfaceFlags & RPMINSTALL_HASH)
	fn = printHash;
    else
	fn = NULL;

    for (i = 0, filename = packages; i < numPackages; i++, filename++) {
	if (!*filename) continue;

	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, "error: cannot open %s\n", *filename);
	    numFailed++;
	    *filename = NULL;
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
	    rc = rpmInstallPackage(prefix, db, fd, installFlags, fn, 
				   printFormat);
	} else {
	    if (installFlags &= INSTALL_TEST) {
		message(MESS_DEBUG, "stopping source install as we're "
			"just testing\n");
		rc = 0;
	    } else
		rc = rpmInstallSourcePackage(prefix, fd, NULL);
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

int doInstall(char * prefix, char ** argv, int installFlags, 
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
	mode = O_RDWR;

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
	if (!strncmp(*filename, "ftp://", 6)) {
	    if (isVerbose()) {
		printf("Retrieving %s\n", *filename);
	    }
	    packages[i] = alloca(strlen(*filename) + 30 + strlen(prefix));
	    sprintf(packages[i], "%s/var/tmp/rpm-ftp-%d-%d.tmp", prefix, 
		    tmpnum++, getpid());
	    message(MESS_DEBUG, "getting %s as %s\n", *filename, packages[i]);
	    fd = getFtpURL(*filename + 6, packages[i]);
	    if (fd < 0) {
		fprintf(stderr, "error: skipping %s - ftp failed - %s\n", 
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
	    fprintf(stderr, "error: cannot open %s\n", *filename);
	    numFailed++;
	    *filename = NULL;
	}

	rc = pkgReadHeader(fd, &binaryHeaders[numBinaryPackages], &isSource);

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
	    freeHeader(binaryHeaders[numBinaryPackages]);
	    numSourcePackages++;
	} else {
	    numBinaryPackages++;
	}
    }

    message(MESS_DEBUG, "found %d source and %d binary packages\n", 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages) {
	message(MESS_DEBUG, "opening database mode: 0%o\n", mode | O_CREAT);
	if (rpmdbOpen(prefix, &db, mode | O_CREAT, 0644)) {
	    fprintf(stderr, "error: cannot open %s/var/lib/rpm/packages.rpm\n", 
			prefix);
	    exit(1);
	}

	if (!(interfaceFlags & RPMINSTALL_NODEPS)) {
	    rpmdep = rpmdepDependencies(db);
	    for (i = 0; i < numBinaryPackages; i++)
		rpmdepAddPackage(rpmdep, binaryHeaders[i]);

	    if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
		numFailed = numPackages;
		stopInstall = 1;
	    }

	    if (!stopInstall && conflicts) {
		fprintf(stderr, "failed dependencies:\n");
		for (i = 0; i < numConflicts; i++) {
		    fprintf(stderr, "\t%s is needed by %s-%s-%s\n",
			    conflicts[i].needsName, conflicts[i].byName,
			    conflicts[i].byVersion, conflicts[i].byRelease);
		}
		
		free(conflicts);
		numFailed = numPackages;
		stopInstall = 1;
	    }
	}
    }
    else
	db = NULL;

    if (!stopInstall) {
	hashesPrinted = 0;
	message(MESS_DEBUG, "installing binary packages\n");
	numFailed += installPackages(prefix, packages, numPackages, 
				     installFlags, interfaceFlags, db);
    }

    for (i = 0; i < numTmpPackages; i++)
	unlink(tmpPackages[i]);

    if (db) rpmdbClose(db);

    return numFailed;
}

int doUninstall(char * prefix, char ** argv, int uninstallFlags,
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
	
    if (rpmdbOpen(prefix, &db, mode, 0644)) {
	fprintf(stderr, "cannot open %s/var/lib/rpm/packages.rpm\n", prefix);
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

	if (!stopUninstall && conflicts) {
	    fprintf(stderr, "failed dependencies:\n");
	    for (i = 0; i < numConflicts; i++) {
		fprintf(stderr, "\t%s is needed by %s-%s-%s\n",
			conflicts[i].needsName, conflicts[i].byName,
			conflicts[i].byVersion, conflicts[i].byRelease);
	    }
	    
	    free(conflicts);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (!stopUninstall) {
	for (i = 0; i < numPackages; i++) {
	    message(MESS_DEBUG, "uninstalling record number %d\n",
			packageOffsets[i]);
	    rpmRemovePackage(prefix, db, packageOffsets[i], 0, uninstallFlags);
	}
    }

    rpmdbClose(db);

    return numFailed;
}

int doSourceInstall(char * prefix, char * arg, char ** specFile) {
    int fd;
    int rc;

    fd = open(arg, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "error: cannot open %s\n", arg);
	return 1;
    }

    if (isVerbose())
	printf("Installing %s\n", arg);

    rc = rpmInstallSourcePackage(prefix, fd, specFile);
    if (rc == 1) {
	fprintf(stderr, "error: %s cannot be installed\n", arg);
    }

    close(fd);

    return rc;
}

static int getFtpURL(char * hostAndFile, char * dest) {
    char * buf;
    char * chptr;
    int ftpconn;
    int fd;
    int rc;
   
    message(MESS_DEBUG, "getting %s via anonymous ftp\n", hostAndFile);

    buf = alloca(strlen(hostAndFile) + 1);
    strcpy(buf, hostAndFile);

    chptr = buf;
    while (*chptr && (*chptr != '/')) chptr++;
    if (!*chptr) return -1;

    *chptr = '\0';

    ftpconn = ftpOpen(buf, NULL, NULL);
    if (ftpconn < 0) return ftpconn;

    *chptr = '/';

    fd = creat(dest, 0600);

    if (fd < 0) {
	message(MESS_DEBUG, "failed to create %s\n", dest);
	unlink(dest);
	ftpClose(ftpconn);
	return FTPERR_UNKNOWN;
    }
    
    if ((rc = ftpGetFile(ftpconn, chptr, fd))) {
	unlink(dest);
	close(fd);
	ftpClose(ftpconn);
	return rc;
    }    

    ftpClose(ftpconn);

    return fd;
}
