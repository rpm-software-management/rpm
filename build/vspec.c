/* Routine to "verify" a parsed spec file */

/***************

Here's what we do

. check for duplicate fields
. check for duplicate sources/patch numbers
. make sure a certain set of fields are present
. in subpackages, make sure a certain set of fields are present
. in subpackages, make sure certain fields are *not* present
. do some checking on the field values for legit characters

****************/

#include <stdio.h>
#include <stdlib.h>

#include "header.h"
#include "spec.h"
#include "specP.h"
#include "rpmerr.h"
#include "messages.h"
#include "rpmlib.h"
#include "stringbuf.h"
#include "misc.h"

#define EMPTY(s) ((!(s)) || (!(*(s))))
#define OOPS(s) {fprintf(stderr, "verifySpec: %s\n", s); res = 1;}

int checkHeaderDups(Header h)
{
    return 0;
}

int verifySpec(Spec s)
{
    int res;
    struct PackageRec *pr;
    
    if (EMPTY(s->name)) {
	OOPS("No Name field");
    }

    /* Check each package/subpackage */
    pr = s->packages;
    while (pr) {
	if (checkHeaderDups(pr->header)) {
	    res = 1;
	}
	pr = pr->next;
    }
    
    return 0;
}
