#include <alloca.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "header.h"
#include "install.h"
#include "package.h"
#include "rpmerr.h"
#include "rpmlib.h"

int installArchive(char * prefix, int fd);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallPackage(char * prefix, rpmdb db, int fd, int test) {
    int rc, isSource;
    Header h;

    rc = pkgReadHeader(fd, &h, &isSource);
    if (rc) return rc;

    message(MESS_DEBUG, "running preinstall script (if any)\n");
    if (runScript(prefix, h, RPMTAG_PREIN)) {
	return 2;
    }

    /* the file pointer for fd is pointing at the cpio archive */
    if (installArchive(prefix, fd)) {
	freeHeader(h);
	return 2;
    }
    message(MESS_DEBUG, "running postinstall script (if any)\n");

    if (runScript(prefix, h, RPMTAG_POSTIN)) {
	return 1;
    }

    if (rpmdbAdd(db, h)) {
	freeHeader(h);
	return 2;
    }

    return 0;
}

#define BLOCKSIZE 1024

int installArchive(char * prefix, int fd) {
    gzFile stream;
    char buf[BLOCKSIZE];
    pid_t child;
    int p[2];
    int bytesRead;
    int status;
    int cpioFailed = 0;
    void * oldhandler;

    /* fd should be a gzipped cpio archive */
    
    stream = gzdopen(fd, "r");
    pipe(p);

    if (access("/bin/cpio",  X_OK))  {
	error(RPMERR_CPIO, "/bin/cpio does not exist");
	return 1;
    }

    oldhandler = signal(SIGPIPE, SIG_IGN);

    child = fork();
    if (!child) {
	close(p[1]);   /* we don't need to write to it */
	close(0);      /* stdin will come from the pipe instead */
	dup2(p[0], 0);
	close(p[0]);

	chdir(prefix);
	execl("/bin/cpio", "/bin/cpio", "--extract", "--verbose",
	      "--unconditional", "--preserve-modification-time",
	      "--make-directories", "--quiet", "-t", NULL);
	exit(-1);
    }

    close(p[0]);

    do {
	bytesRead = gzread(stream, buf, sizeof(buf));
	if (write(p[1], buf, bytesRead) != bytesRead) {
	     cpioFailed = 1;
	     kill(SIGTERM, child);
	}
    } while (bytesRead);

    gzclose(stream);
    close(p[1]);
    signal(SIGPIPE, oldhandler);
    waitpid(child, &status, 0);

    if (cpioFailed || !WIFEXITED(status) || WEXITSTATUS(status)) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	error(RPMERR_CPIO, "unpacking of archive failed");
	return 1;
    }

    return 0;
}
