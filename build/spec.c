#include "system.h"

#include "rpmbuild.h"
#include "buildio.h"

#ifdef	DYING
static void freeTriggerFiles(struct TriggerFileEntry *p);
#endif
    
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

void freeCpioList(struct cpioFileMapping *cpioList, int cpioCount)
{
    struct cpioFileMapping *p = cpioList;

    while (cpioCount--) {
	rpmMessage(RPMMESS_DEBUG, "archive = %s, fs = %s\n",
		   p->archivePath, p->fsPath);
	FREE(p->archivePath);
	FREE(p->fsPath);
	p++;
    }
    FREE(cpioList);
}

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

    for (p = spec->packages; p != NULL; p = p->next) {
	headerGetEntry(p->header, RPMTAG_NAME, NULL, (void *) &n, NULL);
	if (n && (! strcmp(fullName, n))) {
	    break;
	}
    }

    if (pkg) {
	*pkg = p;
    }
    return ((p == NULL) ? 1 : 0);
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

void freePackages(Spec spec)
{
    Package p;

    while (spec->packages) {
	p = spec->packages;
	spec->packages = p->next;
	freePackage(p);
    }
}

#ifdef	DYING
static char *getSourceAux(Spec spec, int num, int flag, int full);
static struct Source *findSource(Spec spec, int num, int flag);
#endif

static struct Source *findSource(Spec spec, int num, int flag)
{
    struct Source *p = spec->sources;

    while (p) {
	if ((num == p->num) && (p->flags & flag)) {
	    return p;
	}
	p = p->next;
    }

    return NULL;
}

#ifdef	UNUSED
static char *getSourceAux(Spec spec, int num, int flag, int full)
{
    struct Source *p = spec->sources;

    p = findSource(spec, num, flag);

    return (p) ? (full ? p->fullSource : p->source) : NULL;
}

static char *getSource(Spec spec, int num, int flag)
{
    return getSourceAux(spec, num, flag, 0);
}

static char *getFullSource(Spec spec, int num, int flag)
{
    return getSourceAux(spec, num, flag, 1);
}
#endif	/* UNUSED */

int parseNoSource(Spec spec, char *field, int tag)
{
    char buf[BUFSIZ];
    char *s, *name;
    int num, flag;
    struct Source *p;

    if (tag == RPMTAG_NOSOURCE) {
	flag = RPMBUILD_ISSOURCE;
	name = "source";
    } else {
	flag = RPMBUILD_ISPATCH;
	name = "patch";
    }
    
    strcpy(buf, field);
    field = buf;
    while ((s = strtok(field, ", \t"))) {
	if (parseNum(s, &num)) {
	    rpmError(RPMERR_BADSPEC, "line %d: Bad number: %s",
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}

	if (! (p = findSource(spec, num, flag))) {
	    rpmError(RPMERR_BADSPEC, "line %d: Bad no%s number: %d",
		     spec->lineNum, name, num);
	    return RPMERR_BADSPEC;
	}

	p->flags |= RPMBUILD_ISNO;

	field = NULL;
    }

    return 0;
}

int addSource(Spec spec, Package pkg, char *field, int tag)
{
    struct Source *p;
    int flag = 0;
    char *name = NULL;
    char *nump, *fieldp = NULL;
    char buf[BUFSIZ];
    char body[BUFSIZ];
    int num = 0;

    switch (tag) {
      case RPMTAG_SOURCE:
	flag = RPMBUILD_ISSOURCE;
	name = "source";
	fieldp = spec->line + 6;
	break;
      case RPMTAG_PATCH:
	flag = RPMBUILD_ISPATCH;
	name = "patch";
	fieldp = spec->line + 5;
	break;
      case RPMTAG_ICON:
	flag = RPMBUILD_ISICON;
	break;
    }

    /* Get the number */
    if (tag != RPMTAG_ICON) {
	/* We already know that a ':' exists, and that there */
	/* are no spaces before it.                          */
	/* This also now allows for spaces and tabs between  */
	/* the number and the ':'                            */

	nump = buf;
	while ((*fieldp != ':') && (*fieldp != ' ') && (*fieldp != '\t')) {
	    *nump++ = *fieldp++;
	}
	*nump = '\0';

	nump = buf;
	SKIPSPACE(nump);
	if (! *nump) {
	    num = 0;
	} else {
	    if (parseNum(buf, &num)) {
		rpmError(RPMERR_BADSPEC, "line %d: Bad %s number: %s\n",
			 spec->lineNum, name, spec->line);
		return RPMERR_BADSPEC;
	    }
	}
    }

    /* Create the entry and link it in */
    p = malloc(sizeof(struct Source));
    p->num = num;
    p->fullSource = strdup(field);
    p->source = strrchr(p->fullSource, '/');
    p->flags = flag;
    if (p->source) {
	p->source++;
    } else {
	p->source = p->fullSource;
    }

    if (tag != RPMTAG_ICON) {
	p->next = spec->sources;
	spec->sources = p;
    } else {
	p->next = pkg->icon;
	pkg->icon = p;
    }

    spec->numSources++;

    if (tag != RPMTAG_ICON) {
	strcpy(body, "%{_sourcedir}/");
	expandMacros(spec, spec->macros, body, sizeof(body));	/* W2DO? */
	strcat(body, p->source);

	sprintf(buf, "%s%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, body, RMIL_SPEC);
	sprintf(buf, "%sURL%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, p->fullSource, RMIL_SPEC);
    }
    
    return 0;
}

Spec newSpec(void)
{
    Spec spec;

    spec = (Spec)malloc(sizeof *spec);
    
    spec->specFile = NULL;
    spec->sourceRpmName = NULL;

    spec->file = NULL;
    spec->readBuf[0] = '\0';
    spec->readPtr = NULL;
    spec->line[0] = '\0';
    spec->readStack = malloc(sizeof(struct ReadLevelEntry));
    spec->readStack->next = NULL;
    spec->readStack->reading = 1;

    spec->prep = NULL;
    spec->build = NULL;
    spec->install = NULL;
    spec->clean = NULL;

    spec->sources = NULL;
    spec->packages = NULL;
    spec->noSource = 0;
    spec->numSources = 0;

    spec->sourceHeader = NULL;

    spec->sourceCpioCount = 0;
    spec->sourceCpioList = NULL;
    
    spec->gotBuildRoot = 0;
    spec->buildRoot = NULL;
    
    spec->buildSubdir = NULL;

    spec->passPhrase = NULL;
    spec->timeCheck = 0;
    spec->cookie = NULL;

    spec->buildRestrictions = headerNew();
    spec->buildArchitectures = NULL;
    spec->buildArchitectureCount = 0;
    spec->inBuildArchitectures = 0;
    spec->buildArchitectureSpecs = NULL;

  { extern struct MacroContext globalMacroContext;
    spec->macros = &globalMacroContext;
  }
    
    spec->autoReq = 1;
    spec->autoProv = 1;

    return spec;
}

static void freeSources(Spec spec)
{
    struct Source *p1, *p2;

    p1 = spec->sources;
    while (p1) {
	p2 = p1;
	p1 = p1->next;
	FREE(p2->fullSource);
	free(p2);
    }
}

void freeSpec(Spec spec)
{
    struct ReadLevelEntry *rl;
    
    freeStringBuf(spec->prep);
    freeStringBuf(spec->build);
    freeStringBuf(spec->install);
    freeStringBuf(spec->clean);

    FREE(spec->buildRoot);
    FREE(spec->buildSubdir);
    FREE(spec->specFile);
    FREE(spec->sourceRpmName);

    while (spec->readStack) {
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
    }
    
    if (spec->sourceHeader) {
	headerFree(spec->sourceHeader);
    }

    freeCpioList(spec->sourceCpioList, spec->sourceCpioCount);
    
    headerFree(spec->buildRestrictions);
    FREE(spec->buildArchitectures);

    if (!spec->inBuildArchitectures) {
	while (spec->buildArchitectureCount--) {
	    freeSpec(
		spec->buildArchitectureSpecs[spec->buildArchitectureCount]);
	}
    }
    FREE(spec->buildArchitectures);

    FREE(spec->passPhrase);
    FREE(spec->cookie);

#ifdef	DEAD
    freeMacros(spec->macros);
#endif
    
    freeSources(spec);
    freePackages(spec);
    closeSpec(spec);
    
    free(spec);
}
