#include <stdlib.h>
#include <string.h>

#include "rpmlib.h"

/* this is incredibly simple minded, but should be fine for what we need */

static char * values[RPMVAR_LASTVAR + 1];

char *rpmGetVar(int var)
{
    if (var > RPMVAR_LASTVAR) 
	return NULL;
    else 
	return values[var];
}

int rpmGetBooleanVar(int var) {
    char * val;
    int num;
    char * chptr;

    val = rpmGetVar(var);
    if (!val) return 0;

    if (val[0] == 'y' || val[0] == 'Y') return 1;

    num = strtol(val, &chptr, 0);
    if (chptr && *chptr == '\0') {
	return num != 0;
    }

    return 0;
}

void rpmSetVar(int var, char *val)
{
    if (var > RPMVAR_LASTVAR) 
	return ;		/* XXX should we go harey carey here? */
   
    if (values[var]) free(values[var]);

    if (val)
	values[var] = strdup(val);
    else
	values[var] = NULL;
}
