/** \ingroup rpmbuild
 * \file build/spec.c
 * Handle spec data structure.
 */

#include "system.h"

#include "rpmbuild.h"
#include "buildio.h"
#include "debug.h"

extern int specedit;
extern MacroContext rpmGlobalMacroContext;

#define SKIPWHITE(_x)	{while(*(_x) && (isspace(*_x) || *(_x) == ',')) (_x)++;}
#define SKIPNONWHITE(_x){while(*(_x) &&!(isspace(*_x) || *(_x) == ',')) (_x)++;}

static inline void freeTriggerFiles(/*@only@*/ struct TriggerFileEntry *p)
{
    struct TriggerFileEntry *o, *q = p;
    
    while (q != NULL) {
	o = q;
	q = q->next;
	FREE(o->fileName);
	FREE(o->script);
	FREE(o->prog);
	free(o);
    }
}

static inline void freeCpioList(/*@only@*/ struct cpioFileMapping *cpioList, int cpioCount)
{
    struct cpioFileMapping *p = cpioList;

    while (cpioCount--) {
	rpmMessage(RPMMESS_DEBUG, _("archive = %s, fs = %s\n"),
		   p->archivePath, p->fsPath);
	FREE(p->archivePath);
	FREE(p->fsPath);
	p++;
    }
    FREE(cpioList);
}

static inline void freeSources(/*@only@*/ struct Source *s)
{
    struct Source *r, *t = s;

    while (t != NULL) {
	r = t;
	t = t->next;
	FREE(r->fullSource);
	free(r);
    }
}

/** */
int lookupPackage(Spec spec, const char *name, int flag, /*@out@*/Package *pkg)
{
    const char *pname;
    const char *fullName;
    Package p;
    
    /* "main" package */
    if (name == NULL) {
	if (pkg)
	    *pkg = spec->packages;
	return 0;
    }

    /* Construct package name */
  { char *n;
    if (flag == PART_SUBNAME) {
	headerNVR(spec->packages->header, &pname, NULL, NULL);
	fullName = n = alloca(strlen(pname) + 1 + strlen(name) + 1);
	while (*pname) *n++ = *pname++;
	*n++ = '-';
    } else {
	fullName = n = alloca(strlen(name)+1);
    }
    strcpy(n, name);
  }

    /* Locate package with fullName */
    for (p = spec->packages; p != NULL; p = p->next) {
	headerNVR(p->header, &pname, NULL, NULL);
	if (pname && (! strcmp(fullName, pname))) {
	    break;
	}
    }

    if (pkg)
	*pkg = p;
    return ((p == NULL) ? 1 : 0);
}

/** */
Package newPackage(Spec spec)
{
    Package p;
    Package pp;

    p = xmalloc(sizeof(*p));

    p->header = headerNew();
    p->icon = NULL;

    p->autoProv = 1;
    p->autoReq = 1;
    
#if 0    
    p->reqProv = NULL;
    p->triggers = NULL;
    p->triggerScripts = NULL;
#endif

    p->triggerFiles = NULL;
    
    p->fileFile = NULL;
    p->fileList = NULL;

    p->cpioList = NULL;
    p->cpioCount = 0;

    p->preInFile = NULL;
    p->postInFile = NULL;
    p->preUnFile = NULL;
    p->postUnFile = NULL;
    p->verifyFile = NULL;

    p->specialDoc = NULL;

    if (spec->packages == NULL) {
	spec->packages = p;
    } else {
	/* Always add package to end of list */
	for (pp = spec->packages; pp->next != NULL; pp = pp->next)
	    ;
	pp->next = p;
    }
    p->next = NULL;

    return p;
}

/** */
void freePackage(/*@only@*/ Package p)
{
    if (p == NULL)
	return;
    
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

    freeSources(p->icon);

    freeTriggerFiles(p->triggerFiles);

    free(p);
}

/** */
void freePackages(Spec spec)
{
    Package p;

    while ((p = spec->packages) != NULL) {
	spec->packages = p->next;
	p->next = NULL;
	freePackage(p);
    }
}

static inline /*@owned@*/ struct Source *findSource(Spec spec, int num, int flag)
{
    struct Source *p;

    for (p = spec->sources; p != NULL; p = p->next) {
	if ((num == p->num) && (p->flags & flag)) {
	    return p;
	}
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

/** */
int parseNoSource(Spec spec, const char *field, int tag)
{
    const char *f, *fe;
    const char *name;
    int num, flag;

    if (tag == RPMTAG_NOSOURCE) {
	flag = RPMBUILD_ISSOURCE;
	name = "source";
    } else {
	flag = RPMBUILD_ISPATCH;
	name = "patch";
    }
    
    fe = field;
    for (f = fe; *f; f = fe) {
        struct Source *p;

	SKIPWHITE(f);
	if (*f == '\0')
	    break;
	fe = f;
	SKIPNONWHITE(fe);
	if (*fe) fe++;

	if (parseNum(f, &num)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad number: %s"),
		     spec->lineNum, f);
	    return RPMERR_BADSPEC;
	}

	if (! (p = findSource(spec, num, flag))) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad no%s number: %d"),
		     spec->lineNum, name, num);
	    return RPMERR_BADSPEC;
	}

	p->flags |= RPMBUILD_ISNO;

    }

    return 0;
}

/** */
int addSource(Spec spec, Package pkg, const char *field, int tag)
{
    struct Source *p;
    int flag = 0;
    char *name = NULL;
    char *nump;
    const char *fieldp = NULL;
    char buf[BUFSIZ];
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
	fieldp = NULL;
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
		rpmError(RPMERR_BADSPEC, _("line %d: Bad %s number: %s\n"),
			 spec->lineNum, name, spec->line);
		return RPMERR_BADSPEC;
	    }
	}
    }

    /* Create the entry and link it in */
    p = xmalloc(sizeof(struct Source));
    p->num = num;
    p->fullSource = xstrdup(field);
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
	const char *body = rpmGetPath("%{_sourcedir}/", p->source, NULL);

	sprintf(buf, "%s%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, body, RMIL_SPEC);
	sprintf(buf, "%sURL%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, p->fullSource, RMIL_SPEC);
	free((void *)body);
    }
    
    return 0;
}

static inline struct speclines * newSl(void)
{
    struct speclines *sl = NULL;
    if (specedit) {
	sl = xmalloc(sizeof(struct speclines));
	sl->sl_lines = NULL;
	sl->sl_nalloc = 0;
	sl->sl_nlines = 0;
    }
    return sl;
}

static inline void freeSl(/*@only@*/struct speclines *sl)
{
    int i;
    if (sl == NULL)
	return;
    for (i = 0; i < sl->sl_nlines; i++)
	FREE(sl->sl_lines[i]);
    FREE(sl->sl_lines);
    free(sl);
}

static inline struct spectags * newSt(void)
{
    struct spectags *st = NULL;
    if (specedit) {
	st = xmalloc(sizeof(struct spectags));
	st->st_t = NULL;
	st->st_nalloc = 0;
	st->st_ntags = 0;
    }
    return st;
}

static inline void freeSt(/*@only@*/struct spectags *st)
{
    int i;
    if (st == NULL)
	return;
    for (i = 0; i < st->st_ntags; i++) {
	struct spectag *t = st->st_t + i;
	FREE(t->t_lang);
	FREE(t->t_msgid);
    }
    FREE(st->st_t);
    free(st);
}

/** */
Spec newSpec(void)
{
    Spec spec;

    spec = (Spec)xmalloc(sizeof *spec);
    
    spec->specFile = NULL;
    spec->sourceRpmName = NULL;

    spec->sl = newSl();
    spec->st = newSt();

    spec->fileStack = NULL;
    spec->lbuf[0] = '\0';
    spec->line = spec->lbuf;
    spec->nextline = NULL;
    spec->nextpeekc = '\0';
    spec->lineNum = 0;
    spec->readStack = xmalloc(sizeof(struct ReadLevelEntry));
    spec->readStack->next = NULL;
    spec->readStack->reading = 1;

    spec->rootURL = NULL;
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
    
    spec->gotBuildRootURL = 0;
    spec->buildRootURL = NULL;
    spec->buildSubdir = NULL;

    spec->passPhrase = NULL;
    spec->timeCheck = 0;
    spec->cookie = NULL;

    spec->buildRestrictions = headerNew();
    spec->buildArchitectures = NULL;
    spec->buildArchitectureCount = 0;
    spec->inBuildArchitectures = 0;
    spec->buildArchitectureSpecs = NULL;

    spec->force = 0;
    spec->anyarch = 0;

    spec->macros = &rpmGlobalMacroContext;
    
    return spec;
}

/** */
void freeSpec(/*@only@*/ Spec spec)
{
    struct OpenFileInfo *ofi;
    struct ReadLevelEntry *rl;

    freeSl(spec->sl);	spec->sl = NULL;
    freeSt(spec->st);	spec->st = NULL;

    freeStringBuf(spec->prep);	spec->prep = NULL;
    freeStringBuf(spec->build);	spec->build = NULL;
    freeStringBuf(spec->install); spec->install = NULL;
    freeStringBuf(spec->clean);	spec->clean = NULL;

    FREE(spec->buildRootURL);
    FREE(spec->buildSubdir);
    FREE(spec->rootURL);
    FREE(spec->specFile);
    FREE(spec->sourceRpmName);

    while (spec->fileStack) {
	ofi = spec->fileStack;
	spec->fileStack = spec->fileStack->next;
	ofi->next = NULL;
	FREE(ofi->fileName);
	free(ofi);
    }

    while (spec->readStack) {
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	rl->next = NULL;
	free(rl);
    }
    
    if (spec->sourceHeader != NULL) {
	headerFree(spec->sourceHeader);
	spec->sourceHeader = NULL;
    }

    freeCpioList(spec->sourceCpioList, spec->sourceCpioCount);
    spec->sourceCpioList = NULL;
    
    headerFree(spec->buildRestrictions);
    spec->buildRestrictions = NULL;

    if (!spec->inBuildArchitectures) {
	while (spec->buildArchitectureCount--) {
	    freeSpec(
		spec->buildArchitectureSpecs[spec->buildArchitectureCount]);
	}
	FREE(spec->buildArchitectureSpecs);
    }
    FREE(spec->buildArchitectures);

    FREE(spec->passPhrase);
    FREE(spec->cookie);

    freeSources(spec->sources);	spec->sources = NULL;
    freePackages(spec);
    closeSpec(spec);
    
    free(spec);
}

/** */
/*@only@*/ struct OpenFileInfo * newOpenFileInfo(void)
{
    struct OpenFileInfo *ofi;

    ofi = xmalloc(sizeof(struct OpenFileInfo));
    ofi->fd = NULL;
    ofi->fileName = NULL;
    ofi->lineNum = 0;
    ofi->readBuf[0] = '\0';
    ofi->readPtr = NULL;
    ofi->next = NULL;

    return ofi;
}
