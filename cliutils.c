#include "system.h"
#if HAVE_MCHECK_H
#include <mcheck.h>
#endif
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
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
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

poptContext initCli(const char *ctx, struct poptOption *optionsTable,
		    int argc, char *argv[])
{
    const char *optArg;
    int arg;
    poptContext optCon;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    
#if defined(ENABLE_NLS)
    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);	/* XXX silly use by showrc */

    /* Make a first pass through the arguments, looking for --rcfile */
    /* We need to handle that before dealing with the rest of the arguments. */
    /* XXX popt argv definition should be fixed instead of casting... */
    optCon = poptGetContext(ctx, argc, (const char **)argv, optionsTable, 0);
    {
	char *poptfile = rpmGenPath(rpmConfigDir(), LIBRPMALIAS_FILENAME, NULL);
	(void) poptReadConfigFile(optCon, poptfile);
	free(poptfile);
    }
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, rpmConfigDir(), 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
	default:
	    fprintf(stderr, _("Internal error in argument processing (%d) :-(\n"), arg);
	    exit(EXIT_FAILURE);
	}
    }

    if (arg < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	exit(EXIT_FAILURE);
    }

    rpmcliConfigured();

    return optCon;
}

int finishCli(poptContext optCon, int rc)
{
    poptFreeContext(optCon);
    rpmFreeMacros(NULL);
    rpmFreeMacros(rpmCLIMacroContext);
    rpmFreeRpmrc();
    rpmlogClose();

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    /* XXX Avoid exit status overflow. Status 255 is special to xargs(1) */
    return (rc > 254) ? 254 : rc;
}

int initPipe(void)
{
    int p[2];

    if (pipe(p) < 0) {
	fprintf(stderr, _("creating a pipe for --pipe failed: %m\n"));
	return -1;
    }

    if (!(pipeChild = fork())) {
	(void) signal(SIGPIPE, SIG_DFL);
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

void finishPipe(void)
{
    int status;
    if (pipeChild) {
	(void) fclose(stdout);
	(void) waitpid(pipeChild, &status, 0);
    }
}
