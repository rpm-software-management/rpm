#include <stdlib.h>
#include <string.h>

#include "rpmlib.h"

/* this is incredibly simple minded, but should be fine for what we need */

static char * values[RPMVAR_LASTVAR + 1];

char *getVar(int var)
{
    if (var > RPMVAR_LASTVAR) 
	return NULL;
    else 
	return values[var];
}

void setVar(int var, char *val)
{
    if (var > RPMVAR_LASTVAR) 
	return ;		/* XXX should we go harey carey here? */
   
    if (values[var]) free(values[var]);
    values[var] = strdup(val);
}
