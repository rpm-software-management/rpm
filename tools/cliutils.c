#include "system.h"
#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmcli.h>
#include "cliutils.h"
#include "debug.h"

static pid_t pipeChild = 0;

RPM_GNUC_NORETURN
void argerror(const char * desc)
{
    fprintf(stderr, _("%s: %s\n"), xgetprogname(), desc);
    exit(EXIT_FAILURE);
}

static void printVersion(FILE * fp)
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(FILE * fp)
{
    fprintf(fp, _("Copyright (C) 1998-2002 - Red Hat, Inc.\n"));
    fprintf(fp, _("This program may be freely redistributed under the terms of the GNU GPL\n"));
}

void printUsage(poptContext con, FILE * fp, int flags)
{
    printVersion(fp);
    printBanner(fp);
    fprintf(fp, "\n");

    if (rpmIsVerbose())
	poptPrintHelp(con, fp, flags);
    else
	poptPrintUsage(con, fp, flags);
}

int initPipe(void)
{
    int p[2];

    if (pipe(p) < 0) {
	fprintf(stderr, _("creating a pipe for --pipe failed: %m\n"));
	return -1;
    }

    if (!(pipeChild = fork())) {
	(void) close(p[1]);
	(void) dup2(p[0], STDIN_FILENO);
	(void) close(p[0]);
	(void) execl("/bin/sh", "/bin/sh", "-c", rpmcliPipeOutput, NULL);
	fprintf(stderr, _("exec failed\n"));
	exit(EXIT_FAILURE);
    }

    (void) close(p[0]);
    (void) dup2(p[1], STDOUT_FILENO);
    (void) close(p[1]);
    return 0;
}

int finishPipe(void)
{
    int rc = 0;
    if (pipeChild) {
	int status;
	pid_t reaped;

	(void) fclose(stdout);
	do {
	    reaped = waitpid(pipeChild, &status, 0);
	} while (reaped == -1 && errno == EINTR);
	    
	if (reaped == -1 || !WIFEXITED(status) || WEXITSTATUS(status))
	    rc = 1;
    }
    return rc;
}
