/* Routine to "verify" a parsed spec file */

/***************

Here's what we do

. check for duplicate fields
. make sure a certain set of fields are present
. in subpackages, make sure a certain set of fields are present
. in subpackages, make sure certain fields are *not* present

. check for duplicate sources/patch numbers
. do some checking on the field values for legit characters

****************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
#define MUST 1

struct packageFieldsRec {
    int tag;
    int present;
    int shouldBePresent;
};

struct fieldNamesRec {
    int tag;
    char *name;
};

static int checkHeaderTags(Header inh, struct packageFieldsRec *pfr);
static char *tagName(int tag);

/* This is a list of tags related to the "main" package. */
/* Only list those that must or must not be present.     */

static struct packageFieldsRec packageFields[] = {
    { RPMTAG_NAME,            0, MUST },
    { RPMTAG_VERSION,         0, MUST },
    { RPMTAG_RELEASE,         0, MUST },
/*    { RPMTAG_SUMMARY,         0, MUST }, */
    { RPMTAG_DESCRIPTION,     0, MUST },
    { RPMTAG_COPYRIGHT,       0, MUST },
/*    { RPMTAG_PACKAGER,        0, MUST }, */
    { RPMTAG_GROUP,           0, MUST },
    { 0, 0, 0 },
};

/* This is a list of tags related to "sub" packages. */
/* Only list those that must or must not be present. */

static struct packageFieldsRec subpackageFields[] = {
    { RPMTAG_NAME,            0, MUST },  /* This is inserted automaticaly */
/*    { RPMTAG_SUMMARY,         0, MUST }, */
    { RPMTAG_DESCRIPTION,     0, MUST },
    { RPMTAG_GROUP,           0, MUST },
    { RPMTAG_NAME,            0, 0 },
    { RPMTAG_COPYRIGHT,       0, 0 },
    { RPMTAG_PACKAGER,        0, 0 },
    { 0, 0, 0 },
};

static struct packageFieldsRec *findTag(int tag, struct packageFieldsRec *pfr)
{
    struct packageFieldsRec *p = pfr;

    while (p->tag) {
	if (p->tag == tag)
	    return p;
	p++;
    }

    return NULL;
}

static char *tagName(int tag)
{
    int i = 0;
    static char nameBuf[1024];
    char *s;

    strcpy(nameBuf, "(unknown)");
    while (i < rpmTagTableSize) {
	if (tag == rpmTagTable[i].val) {
	    strcpy(nameBuf, rpmTagTable[i].name + 7);
	    s = nameBuf+1;
	    while (*s) {
		*s = tolower(*s);
		s++;
	    }
	}
	i++;
    }

    return nameBuf;
}

static int checkHeaderTags(Header inh, struct packageFieldsRec *pfr)
{
    Header h;
    HeaderIterator headerIter;
    int res = 0;
    int_32 tag, lastTag, type, c;
    void *ptr;
    struct packageFieldsRec *p;

    /* This actually sorts the index, so it'll be easy to catch dups */
    h = copyHeader(inh);

    headerIter = initIterator(h);
    lastTag = 0;
    while (nextIterator(headerIter, &tag, &type, &ptr, &c)) {
	if (tag == lastTag) {
	    error(RPMERR_BADSPEC, "Duplicate fields for     : %s",
		  tagName(tag));
	    res = 1;
	}
	p = findTag(tag, pfr);
	if (p) 
	    p->present = 1;
	lastTag = tag;
    }
    freeIterator(headerIter);
    freeHeader(h);

    p = pfr;
    while (p->tag) {
	if (p->shouldBePresent != p->present) {
	    error(RPMERR_BADSPEC, "Field must%s be present%s: %s",
		  p->present ? " NOT" : "", p->present ? "" : "    ",
		  tagName(p->tag));
	    res = 1;
	}
	p->present = 0;  /* hack to clear it for next time :-) */
	p++;
    }
    
    return res;
}

int verifySpec(Spec s)
{
    int res = 0;
    struct PackageRec *pr;
    struct packageFieldsRec *fields;
    char name[1024];
    
    if (EMPTY(s->name)) {
	OOPS("No Name field");
    }

    /* Check each package/subpackage */
    pr = s->packages;
    fields = packageFields;
    while (pr) {
	if (! pr->subname) {
	    if (pr->newname) {
		strcpy(name, pr->newname);
	    } else {
		strcpy(name, s->name);
	    }
	} else {
	    sprintf(name, "%s-%s", s->name, pr->subname);
	}
	printf("* Package: %s\n", name);
	
	if (checkHeaderTags(pr->header, fields)) {
	    res = 1;
	}
	pr = pr->next;
	fields = subpackageFields;
    }
    
    return res;
}
