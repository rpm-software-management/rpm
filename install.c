#include <alloca.h>
#include <fcntl.h>
#include <stdio.h>
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
static int getFtpURL(char * prefix, char * hostAndFile);

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

int doInstall(char * prefix, char * arg, int installFlags, int interfaceFlags) {
    rpmdb db;
    int fd;
    int mode, rc;
    char * chptr;
    notifyFunction fn;
    char * printFormat = NULL;

    hashesPrinted = 0;

    if (installFlags & INSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR;

    if (interfaceFlags & RPMINSTALL_PERCENT)
	fn = printPercent;
    else if (interfaceFlags & RPMINSTALL_HASH)
	fn = printHash;
    else
	fn = NULL;
	
    if (rpmdbOpen(prefix, &db, mode | O_CREAT, 0644)) {
	fprintf(stderr, "error: cannot open %s/var/lib/rpm/packages.rpm\n", 
		    prefix);
	exit(1);
    }

    message(MESS_DEBUG, "installing %s\n", arg);

    if (!strncmp(arg, "ftp://", 6)) {
	if (isVerbose()) {
	    printf("Retrieving %s\n", arg);
	}
	fd = getFtpURL(prefix, arg + 6);
	if (fd < 0) {
	    fprintf(stderr, "error: ftp of %s failed - %s\n", arg,
			ftpStrerror(fd));
	    return 1;
	}
    } else {
	fd = open(arg, O_RDONLY);
	if (fd < 0) {
	    rpmdbClose(db);
	    fprintf(stderr, "error: cannot open %s\n", arg);
	    return 1;
	}
    }

    if (interfaceFlags & RPMINSTALL_PERCENT) 
	printFormat = "%%f %s:%s:%s\n";
    else if (isVerbose() && (interfaceFlags & RPMINSTALL_HASH)) {
	chptr = strrchr(arg, '/');
	if (!chptr)
	    chptr = arg;
	else
	    chptr++;

	printFormat = "%-28s";
    } else if (isVerbose())
	printf("Installing %s\n", arg);

    rc = rpmInstallPackage(prefix, db, fd, installFlags, fn, printFormat);
    if (rc == 1) {
	fprintf(stderr, "error: %s cannot be installed\n", arg);
    }

    close(fd);
    rpmdbClose(db);

    return rc;
}

void doUninstall(char * prefix, char * arg, int test, int uninstallFlags) {
    rpmdb db;
    dbIndexSet matches;
    int i;
    int mode;
    int rc;
    int count;

    if (test) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(prefix, &db, mode, 0644)) {
	fprintf(stderr, "cannot open %s/var/lib/rpm/packages.rpm\n", prefix);
	exit(1);
    }
   
    rc = findPackageByLabel(db, arg, &matches);
    if (rc == 1) 
	fprintf(stderr, "package %s is not installed\n", arg);
    else if (rc == 2) 
	fprintf(stderr, "error searching for package %s\n", arg);
    else {
	count = 0;
	for (i = 0; i < matches.count; i++)
	    if (matches.recs[i].recOffset) count++;

	if (count > 1) {
	    fprintf(stderr, "\"%s\" specifies multiple packages\n", arg);
	}
	else { 
	    for (i = 0; i < matches.count; i++) {
		if (matches.recs[i].recOffset) {
		    message(MESS_DEBUG, "uninstalling record number %d\n",
				matches.recs[i].recOffset);
		    rpmRemovePackage(prefix, db, matches.recs[i].recOffset, 
				     test);
		}
	    }
	}

	freeDBIndexRecord(matches);
    }

    rpmdbClose(db);
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

static int getFtpURL(char * prefix, char * hostAndFile) {
    char * buf;
    char * chptr;
    int ftpconn;
    int fd;
    int rc;
   
    char * tmp;

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

    tmp = alloca(strlen(prefix) + 60);
    sprintf(tmp, "%s/var/tmp/rpm.ftp.%d", prefix, getpid());
    fd = creat(tmp, 0600);

    if (fd < 0) {
	unlink(tmp);
	ftpClose(ftpconn);
	return FTPERR_UNKNOWN;
    }
    
    if ((rc = ftpGetFile(ftpconn, chptr, fd))) {
	unlink(tmp);
	close(fd);
	ftpClose(ftpconn);
	return rc;
    }    

    ftpClose(ftpconn);

    close(fd);
    fd = open(tmp, O_RDONLY);

    unlink(tmp);

    return fd;
}
