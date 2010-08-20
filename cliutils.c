#include "system.h"
#include <rpm/rpmlog.h>
#include <rpm/rpmlib.h>
#include "cliutils.h"
#include "debug.h"

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

