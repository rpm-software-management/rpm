#include "spec.h"
#include "package.h"
#include "misc.h"
#include "rpmlib.h"
#include "files.h"

#include <stdlib.h>

static void freeTriggerFiles(struct TriggerFileEntry *p);
    
int lookupPackage(Spec spec, char *name, int flag, Package *pkg)
{
    char buf[BUFSIZ];
    char *n, *fullName;
    Package p;
    
    /* "main" package */
    if (! name) {
	if (pkg) {
	    *pkg = spec->packages;
	}
	return 0;
    }

    /* Construct package name */
    if (flag == PART_SUBNAME) {
	headerGetEntry(spec->packages->header, RPMTAG_NAME,
		       NULL, (void *) &n, NULL);
	sprintf(buf, "%s-%s", n, name);
	fullName = buf;
    } else {
	fullName = name;
    }

    p = spec->packages;
    while (p) {
	headerGetEntry(p->header, RPMTAG_NAME, NULL, (void *) &n, NULL);
	if (n && (! strcmp(fullName, n))) {
	    if (pkg) {
		*pkg = p;
	    }
	    return 0;
	}
	p = p->next;
    }

    if (pkg) {
	*pkg = NULL;
    }
    return 1;
}

Package newPackage(Spec spec)
{
    Package p;
    Package pp;

    p = malloc(sizeof(*p));

    p->header = headerNew();
    p->icon = NULL;
    p->autoReqProv = 1;
    
#if 0    
    p->reqProv = NULL;
    p->triggers = NULL;
    p->triggerScripts = NULL;
#endif

    p->triggerFiles = NULL;
    
    p->fileFile = NULL;
    p->fileList = NULL;
    p->next = NULL;

    p->cpioList = NULL;
    p->cpioCount = 0;

    p->preInFile = NULL;
    p->postInFile = NULL;
    p->preUnFile = NULL;
    p->postUnFile = NULL;
    p->verifyFile = NULL;

    p->specialDoc = NULL;

    if (! spec->packages) {
	spec->packages = p;
    } else {
	/* Always add package to end of list */
	pp = spec->packages;
	while (pp->next) {
	    pp = pp->next;
	}
	pp->next = p;
    }

    return p;
}

void freePackages(Spec spec)
{
    Package p;

    while (spec->packages) {
	p = spec->packages;
	spec->packages = p->next;
	freePackage(p);
    }
}

void freePackage(Package p)
{
    if (! p) {
	return;
    }
    
    FREE(p->preInFile);
    FREE(p->postInFile);
    FREE(p->preUnFile);
    FREE(p->postUnFile);
    FREE(p->verifyFile);

    headerFree(p->header);
    freeStringBuf(p->fileList);
    FREE(p->fileFile);
    freeCpioList(p->cpioList, p->cpioCount);

    freeStringBuf(p->specialDoc);

    freeTriggerFiles(p->triggerFiles);

    free(p);
}

static void freeTriggerFiles(struct TriggerFileEntry *p)
{
    struct TriggerFileEntry *o;
    
    while (p) {
	FREE(p->fileName);
	FREE(p->script);
	FREE(p->prog);
	o = p;
	p = p->next;
	free(o);
    }
}
