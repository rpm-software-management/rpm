/* handle triggers */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "trigger.h"
#include "header.h"
#include "spec.h"
#include "specP.h"
#include "messages.h"
#include "rpmlib.h"
#include "stringbuf.h"
#include "misc.h"
#include "miscfn.h"

#define FREE(x) { if (x) free(x); }
#define CHUNK 8

int addTrigger(struct PackageRec *package,
	       int sense, char *script, char *args)
{
    struct TriggerEntry *te;
    int i;
    char *arg = NULL;
    struct ReqComp *rc = NULL;
    char *version;

    /* add the script */
    i = package->trigger.used++;
    if (i == package->trigger.alloced) {
	/* extend */
	package->trigger.alloced += CHUNK;
	package->trigger.triggerScripts =
	    realloc(package->trigger.triggerScripts,
		    package->trigger.alloced *
		    sizeof(*(package->trigger.triggerScripts)));
    }
    package->trigger.triggerScripts[i] = strdup(script);
    rpmMessage(RPMMESS_DEBUG, "TRIGGER %d:\n%s", i, script);

    /* create the entry (or entries) */
    te = NULL;
    while (arg || (arg = strtok(args, " ,\t\n"))) {
	if (!te) {
	    te = malloc(sizeof(*te));
	    te->flags = sense;
	}
	if ((version = strtok(NULL, " ,\t\n"))) {
	    rc = ReqComparisons;
	    while (rc->token && strcmp(version, rc->token)) {
		rc++;
	    }
	    if (rc->token) {
		/* read a version */
		te->flags |= rc->flags;
		version = strtok(NULL, " ,\t\n");
	    }
	}
	if ((te->flags & RPMSENSE_SENSEMASK) && !version) {
	    rpmError(RPMERR_BADSPEC, "Version required in trigger");
	    return RPMERR_BADSPEC;
	}
	te->name = strdup(arg);
	te->version = (rc && rc->token && version) ? strdup(version) : NULL;
	te->index = i;

	rpmMessage(RPMMESS_DEBUG, "TRIGGER(%s): %s %s %s %d\n",
	       (sense == RPMSENSE_TRIGGER_IN) ? "in" : "un",
	       te->name,
	       (rc && rc->token) ? rc->token : "NONE",
	       te->version, te->index);
	
	/* link it in */
	te->next = package->trigger.trigger;
	package->trigger.trigger = te;
	package->trigger.triggerCount++;
	te = NULL;

	/* prepare for next round */
	arg = NULL;
	if (! (rc && rc->token)) {
	    /* No version -- we just read a name */
	    arg = version;
	}
	args = NULL;
    }

    return 0;
}

void generateTriggerEntries(Header h, struct PackageRec *p)
{
    struct TriggerEntry *te;
    int i;
    char **nameArray;
    char **versionArray;
    int_32 *flagArray;
    int_32 *indexArray;

    /* Add the scripts */
    
    if (p->trigger.used) {
	headerAddEntry(h, RPMTAG_TRIGGERSCRIPTS, RPM_STRING_ARRAY_TYPE,
		 p->trigger.triggerScripts, p->trigger.used);
    }

    /* Add the entries */

    if (p->trigger.triggerCount) {
	nameArray = malloc(p->trigger.triggerCount * sizeof(*nameArray));
	versionArray = malloc(p->trigger.triggerCount * sizeof(*versionArray));
	flagArray = malloc(p->trigger.triggerCount * sizeof(*flagArray));
	indexArray = malloc(p->trigger.triggerCount * sizeof(*indexArray));
	
	te = p->trigger.trigger;
	i = 0;
	while (te) {
	    nameArray[i] = te->name;
	    versionArray[i] = te->version ? te->version : "";
	    flagArray[i] = te->flags;
	    indexArray[i] = te->index;
	    i++;
	    te = te->next;
	}
	
	headerAddEntry(h, RPMTAG_TRIGGERNAME, RPM_STRING_ARRAY_TYPE, nameArray, i);
	headerAddEntry(h, RPMTAG_TRIGGERVERSION, RPM_STRING_ARRAY_TYPE, versionArray, i);
	headerAddEntry(h, RPMTAG_TRIGGERFLAGS, RPM_INT32_TYPE, flagArray, i);
	headerAddEntry(h, RPMTAG_TRIGGERINDEX, RPM_INT32_TYPE, indexArray, i);
    }
}

void freeTriggers(struct TriggerStruct t)
{
    char **s;
    struct TriggerEntry *te;
    
    s = t.triggerScripts;
    while (t.used--) {
	FREE(*s++);
    }
    FREE(t.triggerScripts);

    while (t.trigger) {
	te = t.trigger;
	t.trigger = t.trigger->next;
	FREE(te->name);
	FREE(te->version);
	free(te);
    }
}
