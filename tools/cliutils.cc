#include "system.h"
#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmcli.h>
#include "cliutils.hh"
#include "debug.h"

static pid_t pipeChild = 0;

RPM_GNUC_NORETURN
void argerror(const char * desc)
{
    fprintf(stderr, _("%s: %s\n"), rgetprogname(), desc);
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

FILE* rpopen(const char *cmd, const char *arg)
{
    FILE *stream = NULL;
    ARGV_t argv = NULL;
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) < 0)
	goto exit;

    argvAdd(&argv, "/bin/sh");
    argvAdd(&argv, "-c");
    argvAdd(&argv, cmd);
    argvAdd(&argv, "sh");
    argvAdd(&argv, arg);

    if ((pid = fork()) == 0) {
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[0]);
	execv(argv[0], argv);
	_exit(EXIT_FAILURE);
    } else if (pid == -1) {
	goto exit;
    }

    close(pipefd[1]);
    stream = fdopen(pipefd[0], "r");

exit:
    argvFree(argv);
    return stream;
}

int rpclose(FILE *stream)
{
    int rc = -1;
    int status;

    if ((waitpid(-1, &status, 0) == -1) || !WIFEXITED(status))
	goto exit;

    rc = WEXITSTATUS(status);

exit:
    fclose(stream);
    return rc;
}
