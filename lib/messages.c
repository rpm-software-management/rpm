#include <stdarg.h>
#include <stdio.h>

#include "messages.h"

static minLevel = MESS_NORMAL;

void increaseVerbosity(void) {
    minLevel--;
}

void setVerbosity(int level) {
    minLevel = level;
}

int getVerbosity(void)
{
   return minLevel;
}

int isDebug(void)
{
    return (minLevel <= MESS_DEBUG);
}

int isVerbose(void)
{
    return (minLevel <= MESS_VERBOSE);
}

void message(int level, char * format, ...) {
    va_list args;

    va_start(args, format);
    if (level >= minLevel) {
	switch (level) {
	  case MESS_VERBOSE:
	  case MESS_NORMAL:
	    vfprintf(stdout, format, args);
	    break;
	    
	  case MESS_DEBUG:
	    fprintf(stdout, "D: ");
	    vfprintf(stdout, format, args);
	    break;

	  case MESS_WARNING:
	    fprintf(stderr, "warning: ");
	    vfprintf(stderr, format, args);
	    break;

	  case MESS_ERROR:
	    fprintf(stderr, "error: ");
	    vfprintf(stderr, format, args);
	    break;

	  case MESS_FATALERROR:
	    fprintf(stderr, "fatal error: ");
	    vfprintf(stderr, format, args);
	    exit(1);
	    break;
	}
    }
}
