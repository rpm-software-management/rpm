/* (C) 1998 Red Hat Software, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.redhat.com/pub/code/popt */

#ifndef H_POPTINT
#define H_POPTINT

struct optionStackEntry {
    int argc;
    /*@keep@*/ const char ** argv;
    int next;
    /*@keep@*/ const char * nextArg;
    /*@keep@*/ const char * nextCharArg;
    /*@dependent@*/ struct poptAlias * currAlias;
    int stuffed;
};

struct execEntry {
    const char * longName;
    char shortName;
    const char * script;
};

struct poptContext_s {
    struct optionStackEntry optionStack[POPT_OPTION_DEPTH];
    /*@dependent@*/ struct optionStackEntry * os;
    /*@owned@*/ const char ** leftovers;
    int numLeftovers;
    int nextLeftover;
    /*@keep@*/ const struct poptOption * options;
    int restLeftover;
    /*@owned@*/ const char * appName;
    /*@owned@*/ struct poptAlias * aliases;
    int numAliases;
    int flags;
    struct execEntry * execs;
    int numExecs;
    /*@owned@*/ const char ** finalArgv;
    int finalArgvCount;
    int finalArgvAlloced;
    /*@dependent@*/ struct execEntry * doExec;
    /*@owned@*/ const char * execPath;
    int execAbsolute;
    /*@owned@*/ const char * otherHelp;
};

#define	xfree(_a)	free((void *)_a)

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#if defined(HAVE_GETTEXT) && !defined(__LCLINT__)
#define _(foo) gettext(foo)
#else
#define _(foo) (foo)
#endif

#if defined(HAVE_DGETTEXT) && !defined(__LCLINT__)
#define D_(dom, str) dgettext(dom, str)
#define POPT_(foo) D_("popt", foo)
#else
#define POPT_(foo) (foo)
#define D_(dom, str) (str)
#endif

#define N_(foo) (foo)

#endif
