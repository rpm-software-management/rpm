/* (C) 1998 Red Hat Software, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.redhat.com/pub/code/popt */

#ifndef H_POPTINT
#define H_POPTINT

struct optionStackEntry {
    int argc;
    const char ** argv;
    int next;
    const char * nextArg;
    const char * nextCharArg;
    struct poptAlias * currAlias;
    int stuffed;
};

struct execEntry {
    const char * longName;
    char shortName;
    const char * script;
};

struct poptContext_s {
    struct optionStackEntry optionStack[POPT_OPTION_DEPTH], * os;
    const char ** leftovers;
    int numLeftovers;
    int nextLeftover;
    const struct poptOption * options;
    int restLeftover;
    const char * appName;
    struct poptAlias * aliases;
    int numAliases;
    int flags;
    struct execEntry * execs;
    int numExecs;
    const char ** finalArgv;
    int finalArgvCount;
    int finalArgvAlloced;
    struct execEntry * doExec;
    const char * execPath;
    int execAbsolute;
    const char * otherHelp;
};

#define	xfree(_a)	free((void *)_a)

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#ifdef HAVE_GETTEXT
#define _(foo) gettext(foo)
#else
#define _(foo) (foo)
#endif

#ifdef HAVE_DGETTEXT
#define D_(dom, str) dgettext(dom, str)
#define POPT_(foo) D_("popt", foo)
#else
#define POPT_(foo) (foo)
#define D_(dom, str) (str)
#endif

#define N_(foo) (foo)

#endif
