#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include "miscfn.h"
#include "install.h"
#include "intl.h"
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
    rpmNotifyFunction fn;
    char * netsharedPath = NULL;

    if (interfaceFlags & INSTALL_PERCENT)
	fn = printPercent;
    else if (interfaceFlags & INSTALL_HASH)
	fn = printHash;
    else
	fn = NULL;

    netsharedPath = rpmGetVar(RPMVAR_NETSHAREDPATH);

    for (i = 0, filename = packages; i < numPackages; i++, filename++) {
	if (!*filename) continue;

	hashesPrinted = 0;

	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, _("error: cannot open file %s\n"), *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
	} 

	if (interfaceFlags & INSTALL_PERCENT) 
	    printFormat = "%%f %s:%s:%s\n";
	else if (rpmIsVerbose() && (interfaceFlags & INSTALL_HASH)) {
	    chptr = strrchr(*filename, '/');
	    if (!chptr)
		chptr = *filename;
	    else
		chptr++;

	    printFormat = "%-28s";
	} else if (rpmIsVerbose())
	    printf(_("Installing %s\n"), *filename);

	if (db) {
	    rc = rpmInstallPackage(rootdir, db, fd, location, installFlags, fn, 
				   printFormat, netsharedPath);
	} else {
	    if (installFlags &= RPMINSTALL_TEST) {
		rpmMessage(RPMMESS_DEBUG, "stopping source install as we're "
			"just testing\n");
		rc = 0;
	    } else {
		rc = rpmInstallSourcePackage(rootdir, fd, NULL, fn,
					     printFormat);
	    }
	} 

	if (rc == 1) {
	    fprintf(stderr, 
		    _("error: %s does not appear to be a RPM package\n"), 
		    *filename);
	}
	    
	if (rc) {
	    fprintf(stderr, _("error: %s cannot be installed\n"), *filename);
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

    if (installFlags & RPMINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;

    rpmMessage(RPMMESS_DEBUG, "counting packages to install\n");
    for (filename = argv, numPackages = 0; *filename; filename++, numPackages++)
	;

    rpmMessage(RPMMESS_DEBUG, "found %d packages\n", numPackages);
    packages = alloca((numPackages + 1) * sizeof(char *));
    packages[numPackages] = NULL;
    tmpPackages = alloca((numPackages + 1) * sizeof(char *));
    binaryHeaders = alloca((numPackages + 1) * sizeof(Header));
	
    rpmMessage(RPMMESS_DEBUG, "looking for packages to download\n");
    for (filename = argv, i = 0; *filename; filename++) {
	if (urlIsURL(*filename)) {
	    if (rpmIsVerbose()) {
		printf(_("Retrieving %s\n"), *filename);
	    }
	    packages[i] = alloca(strlen(*filename) + 30 + strlen(rootdir));
	    sprintf(packages[i], "%s/var/tmp/rpm-ftp-%d-%d.tmp", rootdir, 
		    tmpnum++, (int) getpid());
	    rpmMessage(RPMMESS_DEBUG, "getting %s as %s\n", *filename, packages[i]);
	    fd = urlGetFile(*filename, packages[i]);
	    if (fd < 0) {
		fprintf(stderr, 
			_("error: skipping %s - transfer failed - %s\n"), 
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

    rpmMessage(RPMMESS_DEBUG, "retrieved %d packages\n", numTmpPackages);

    rpmMessage(RPMMESS_DEBUG, "finding source and binary packages\n");
    for (filename = packages; *filename; filename++) {
	fd = open(*filename, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, _("error: cannot open file %s\n"), *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &binaryHeaders[numBinaryPackages], &isSource,
			   NULL, NULL);

	close(fd);
	
	if (rc == 1) {
	    fprintf(stderr, 
			_("error: %s does not appear to be a RPM package\n"), 
			*filename);
	}
	    
	if (rc) {
	    fprintf(stderr, _("error: %s cannot be installed\n"), *filename);
	    numFailed++;
	    *filename = NULL;
	} else if (isSource) {
	    /* the header will be NULL if this is a v1 source package */
	    if (binaryHeaders[numBinaryPackages])
		headerFree(binaryHeaders[numBinaryPackages]);

	    numSourcePackages++;
	} else {
	    numBinaryPackages++;
	}
    }

    rpmMessage(RPMMESS_DEBUG, "found %d source and %d binary packages\n", 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages) {
	rpmMessage(RPMMESS_DEBUG, "opening database mode: 0%o\n", mode);
	if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	    fprintf(stderr, "error: cannot open %s%s/packages.rpm\n", 
			rpmGetVar(RPMVAR_DBPATH), rootdir);
	    exit(1);
	}

	if (!(interfaceFlags & INSTALL_NODEPS)) {
	    rpmdep = rpmdepDependencies(db);
	    for (i = 0; i < numBinaryPackages; i++)
		if (installFlags & RPMINSTALL_UPGRADE)
		    rpmdepUpgradePackage(rpmdep, binaryHeaders[i]);
		else
		    rpmdepAddPackage(rpmdep, binaryHeaders[i]);

	    if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
		numFailed = numPackages;
		stopInstall = 1;
	    }

	    rpmdepDone(rpmdep);

	    if (!stopInstall && conflicts) {
		fprintf(stderr, _("failed dependencies:\n"));
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
	rpmMessage(RPMMESS_DEBUG, "installing binary packages\n");
	numFailed += installPackages(rootdir, packages, location, numPackages, 
				     installFlags, interfaceFlags, db);
    }

    for (i = 0; i < numTmpPackages; i++)
	unlink(tmpPackages[i]);

    for (i = 0; i < numBinaryPackages; i++) 
	headerFree(binaryHeaders[i]);

    if (db) rpmdbClose(db);

    return numFailed;
}

int doUninstall(char * rootdir, char ** argv, int uninstallFlags,
		 int interfaceFlags) {
    rpmdb db;
    dbiIndexSet matches;
    int i, j;
    int mode;
    int rc;
    int count;
    int numPackages, packageOffsetsAlloced;
    int * packageOffsets;
    char ** arg;
    int numFailed = 0;
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopUninstall = 0;

    rpmMessage(RPMMESS_DEBUG, "counting packages to uninstall\n");
    for (arg = argv, numPackages = 0; *arg; arg++, numPackages++)
	;

    packageOffsetsAlloced = numPackages;
    packageOffsets = malloc(sizeof(int *) * packageOffsetsAlloced);

    if (uninstallFlags & RPMUNINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	fprintf(stderr, _("cannot open %s%s/packages.rpm\n"), 
		rpmGetVar(RPMVAR_DBPATH), rootdir);
	exit(1);
    }

    j = 0;
    numPackages = 0;
    for (arg = argv; *arg; arg++) {
	rc = rpmdbFindByLabel(db, *arg, &matches);
	if (rc == 1) {
	    fprintf(stderr, _("package %s is not installed\n"), *arg);
	    numFailed++;
	} else if (rc == 2) {
	    fprintf(stderr, _("error searching for package %s\n"), *arg);
	    numFailed++;
	} else {
	    count = 0;
	    for (i = 0; i < matches.count; i++)
		if (matches.recs[i].recOffset) count++;

	    if (count > 1 && !(interfaceFlags & UNINSTALL_ALLMATCHES)) {
		fprintf(stderr, _("\"%s\" specifies multiple packages\n"), 
			*arg);
		numFailed++;
	    }
	    else { 
		numPackages += count;
		if (numPackages > packageOffsetsAlloced) {
		    packageOffsetsAlloced = numPackages + 5;
		    packageOffsets = realloc(packageOffsets, 
				sizeof(int *) * packageOffsetsAlloced);
		}
		for (i = 0; i < matches.count; i++) {
		    if (matches.recs[i].recOffset) {
			packageOffsets[j++] = matches.recs[i].recOffset;
		    }
		}
	    }

	    dbiFreeIndexRecord(matches);
	}
    }

    rpmMessage(RPMMESS_DEBUG, "found %d packages to uninstall\n", numPackages);

    if (!(interfaceFlags & UNINSTALL_NODEPS)) {
	rpmdep = rpmdepDependencies(db);
	for (i = 0; i < numPackages; i++)
	    rpmdepRemovePackage(rpmdep, packageOffsets[i]);

	if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
	    numFailed = numPackages;
	    stopUninstall = 1;
	}

	rpmdepDone(rpmdep);

	if (!stopUninstall && conflicts) {
	    fprintf(stderr, _("removing these packages would break "
			      "dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (!stopUninstall) {
	for (i = 0; i < numPackages; i++) {
	    rpmMessage(RPMMESS_DEBUG, "uninstalling record number %d\n",
			packageOffsets[i]);
	    if (rpmRemovePackage(rootdir, db, packageOffsets[i], 
				 uninstallFlags))
		numFailed++;
	}
    }

    rpmdbClose(db);

    free(packageOffsets);

    return numFailed;
}

int doSourceInstall(char * rootdir, char * arg, char ** specFile) {
    int fd;
    int rc;

    fd = open(arg, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, _("error: cannot open %s\n"), arg);
	return 1;
    }

    if (rpmIsVerbose())
	printf("Installing %s\n", arg);

    rc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL);
    if (rc == 1) {
	fprintf(stderr, _("error: %s cannot be installed\n"), arg);
    }

    close(fd);

    return rc;
}

void printDepFlags(FILE * f, char * version, int flags) {
    if (flags)
	fprintf(f, " ");

    if (flags & RPMSENSE_LESS) 
	fprintf(f, "<");
    if (flags & RPMSENSE_GREATER)
	fprintf(f, ">");
    if (flags & RPMSENSE_EQUAL)
	fprintf(f, "=");
    if (flags & RPMSENSE_SERIAL)
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
	    fprintf(f, _(" is needed by %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
	else
	    fprintf(f, _(" conflicts with %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
    }
}
