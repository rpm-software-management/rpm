#include "system.h"

/** \ingroup rpmio
 * \file rpmio/messages.c
 */

#include <stdarg.h>

#include <rpmmessages.h>

static int minLevel = RPMMESS_NORMAL;

void rpmIncreaseVerbosity(void) {
    minLevel--;
}

void rpmSetVerbosity(int level) {
    minLevel = level;
}

int rpmGetVerbosity(void)
{
   return minLevel;
}

int rpmIsDebug(void)
{
    return (minLevel <= RPMMESS_DEBUG);
}

int rpmIsVerbose(void)
{
    return (minLevel <= RPMMESS_VERBOSE);
}

void rpmMessage(rpmmsgLevel level, const char * format, ...) {
    va_list args;

    va_start(args, format);
    if (level >= minLevel) {
	switch (level) {
	  case RPMMESS_VERBOSE:
	  case RPMMESS_NORMAL:
	    vfprintf(stdout, format, args);
	    fflush(stdout);
	    break;
	    
	  case RPMMESS_DEBUG:
	    if (*format != '+')
	        fprintf(stdout, "D: ");
	    else
		format++;
	    vfprintf(stdout, format, args);
	    fflush(stdout);
	    break;

	  case RPMMESS_WARNING:
	    if (*format != '+')
		fprintf(stderr, _("warning: "));
	    else
		format++;
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    break;

	  case RPMMESS_ERROR:
	    if (*format != '+')
		fprintf(stderr, _("error: "));
	    else
		format++;
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    break;

	  case RPMMESS_FATALERROR:
	    if (*format != '+')
		fprintf(stderr, _("fatal error: "));
	    else
		format++;
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    exit(EXIT_FAILURE);
	    /*@notreached@*/ break;

	  default:
	    fprintf(stderr, _("internal error (rpm bug?): "));
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    exit(EXIT_FAILURE);
	    /*@notreached@*/ break;

	}
    }
}
