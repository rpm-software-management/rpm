#include <alloca.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "install.h"
#include "lib/rpmlib.h"
#include "messages.h"
#include "query.h"

static int hashesPrinted = 0;

static void printHash(const unsigned long amount, const unsigned long total);
static void printPercent(const unsigned long amount, const unsigned long total);
static int getFtpURL(char * hostAndFile, char * dest);
static void printDepFlags(FILE * f, char * version, int flags);
static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts);
static char * getFtpPassword(char * machine, char * account, int mustAsk);

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
				   printFormat);
	} else {
	    if (installFlags &= INSTALL_TEST) {
		message(MESS_DEBUG, "stopping source install as we're "
			"just testing\n");
		rc = 0;
	    } else {
		hashesPrinted = 0;
		rc = rpmInstallSourcePackage(rootdir, fd, NULL);
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
	if (!strncmp(*filename, "ftp://", 6)) {
	    if (isVerbose()) {
		printf("Retrieving %s\n", *filename);
	    }
	    packages[i] = alloca(strlen(*filename) + 30 + strlen(rootdir));
	    sprintf(packages[i], "%s/var/tmp/rpm-ftp-%d-%d.tmp", rootdir, 
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
	    fprintf(stderr, "error: cannot open file %s\n", *filename);
	    numFailed++;
	    *filename = NULL;
	    continue;
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

	    if (!stopInstall && conflicts) {
		fprintf(stderr, "failed dependencies:\n");
		printDepProblems(stderr, conflicts, numConflicts);
		
		free(conflicts);
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

	if (!stopUninstall && conflicts) {
	    fprintf(stderr, "removing these packages would break "
			    "dependencies:\n");
	    printDepProblems(stderr, conflicts, numConflicts);
	    
	    free(conflicts);
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

    rc = rpmInstallSourcePackage(rootdir, fd, specFile);
    if (rc == 1) {
	fprintf(stderr, "error: %s cannot be installed\n", arg);
    }

    close(fd);

    return rc;
}

struct pwcacheEntry {
    char * machine;
    char * account;
    char * pw;
} ;

static char * getFtpPassword(char * machine, char * account, int mustAsk) {
    static struct pwcacheEntry * pwCache = NULL;
    static int pwCount = 0;
    int i;
    char * prompt;

    for (i = 0; i < pwCount; i++) {
	if (!strcmp(pwCache[i].machine, machine) &&
	    !strcmp(pwCache[i].account, account))
		break;
    }

    if (i < pwCount && !mustAsk) {
	return pwCache[i].pw;
    } else if (i == pwCount) {
	pwCount++;
	if (pwCache)
	    pwCache = realloc(pwCache, sizeof(*pwCache) * pwCount);
	else
	    pwCache = malloc(sizeof(*pwCache));

	pwCache[i].machine = strdup(machine);
	pwCache[i].account = strdup(account);
    } else
	free(pwCache[i].pw);

    prompt = alloca(strlen(machine) + strlen(account) + 50);
    sprintf(prompt, "Password for %s@%s: ", account, machine);

    pwCache[i].pw = strdup(getpass(prompt));

    return pwCache[i].pw;
}

static int getFtpURL(char * hostAndFile, char * dest) {
    char * buf;
    char * chptr, * machineName, * fileName;
    char * userName = NULL;
    char * password = NULL;
    int ftpconn;
    int fd;
    int rc;
   
    message(MESS_DEBUG, "getting %s via anonymous ftp\n", hostAndFile);

    buf = alloca(strlen(hostAndFile) + 1);
    strcpy(buf, hostAndFile);

    chptr = buf;
    while (*chptr && (*chptr != '/')) chptr++;
    if (!*chptr) return -1;

    machineName = buf;		/* could still have user:pass@ though */
    fileName = chptr;
    *fileName = '\0';

    chptr = machineName;
    while (*chptr && *chptr != '@') chptr++;
    if (*chptr) {		/* we have a username */
	*chptr = '\0';
	userName = machineName;
	machineName = chptr + 1;
	
	chptr = userName;
	while (*chptr && *chptr != ':') chptr++;
	if (*chptr) {		/* we have a password */
	    *chptr = '\0';
	    password = chptr + 1;
	}
    }
	
    if (userName && !password) {
	password = getFtpPassword(machineName, userName, 0);
    }

    message(MESS_DEBUG, "logging into %s as %s, pw %s\n", machineName,
		userName ? userName : "ftp", 
		password ? password : "(username)");

    ftpconn = ftpOpen(machineName, userName, password);
    if (ftpconn < 0) return ftpconn;

    fd = creat(dest, 0600);

    if (fd < 0) {
	message(MESS_DEBUG, "failed to create %s\n", dest);
	unlink(dest);
	ftpClose(ftpconn);
	return FTPERR_UNKNOWN;
    }

    *fileName = '/';
    
    if ((rc = ftpGetFile(ftpconn, fileName, fd))) {
	unlink(dest);
	close(fd);
	ftpClose(ftpconn);
	return rc;
    }    

    ftpClose(ftpconn);

    return fd;
}

static void printDepFlags(FILE * f, char * version, int flags) {
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
	printDepFlags(stderr, conflicts[i].needsVersion, 
		      conflicts[i].needsFlags);
	fprintf(f, " is needed by %s-%s-%s\n", conflicts[i].byName, 
		conflicts[i].byVersion, conflicts[i].byRelease);
    }
}
