/** \ingroup rpmbuild
 * \file build/spec.c
 * Handle spec data structure.
 */

#include "system.h"

#include "buildio.h"
#include "debug.h"

/*@-redecl@*/
extern int specedit;
/*@=redecl@*/

#define SKIPWHITE(_x)	{while(*(_x) && (xisspace(*_x) || *(_x) == ',')) (_x)++;}
#define SKIPNONWHITE(_x){while(*(_x) &&!(xisspace(*_x) || *(_x) == ',')) (_x)++;}

/*@access Header @*/	/* compared with NULL */
/*@access TFI_t @*/	/* compared with NULL */

/**
 * @return		NULL always
 */
static inline
/*@null@*/ struct TriggerFileEntry * freeTriggerFiles(/*@only@*/ /*@null@*/ struct TriggerFileEntry * p)
	/*@modifies p @*/
{
    struct TriggerFileEntry *o, *q = p;
    
    while (q != NULL) {
	o = q;
	q = q->next;
	o->fileName = _free(o->fileName);
	o->script = _free(o->script);
	o->prog = _free(o->prog);
	o = _free(o);
    }
    return NULL;
}

/**
 * Destroy source component chain.
 * @param		source component chain
 * @return		NULL always
 */
static inline
/*@null@*/ struct Source * freeSources(/*@only@*/ /*@null@*/ struct Source * s)
	/*@modifies s @*/
{
    struct Source *r, *t = s;

    while (t != NULL) {
	r = t;
	t = t->next;
	r->fullSource = _free(r->fullSource);
	r = _free(r);
    }
    return NULL;
}

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
	(void) headerNVR(spec->packages->header, &pname, NULL, NULL);
	fullName = n = alloca(strlen(pname) + 1 + strlen(name) + 1);
	while (*pname != '\0') *n++ = *pname++;
	*n++ = '-';
    } else {
	fullName = n = alloca(strlen(name)+1);
    }
    /*@-mayaliasunique@*/
    strcpy(n, name);
    /*@=mayaliasunique@*/
  }

    /* Locate package with fullName */
    for (p = spec->packages; p != NULL; p = p->next) {
	(void) headerNVR(p->header, &pname, NULL, NULL);
	if (pname && (! strcmp(fullName, pname))) {
	    break;
	}
    }

    if (pkg)
	/*@-dependenttrans@*/ *pkg = p; /*@=dependenttrans@*/
    return ((p == NULL) ? 1 : 0);
}

Package newPackage(Spec spec)
{
    Package p;
    Package pp;

    p = xcalloc(1, sizeof(*p));

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
	    {};
	pp->next = p;
    }
    p->next = NULL;

    return p;
}

Package freePackage(Package pkg)
{
    if (pkg == NULL) return NULL;
    
    pkg->preInFile = _free(pkg->preInFile);
    pkg->postInFile = _free(pkg->postInFile);
    pkg->preUnFile = _free(pkg->preUnFile);
    pkg->postUnFile = _free(pkg->postUnFile);
    pkg->verifyFile = _free(pkg->verifyFile);

    pkg->header = headerFree(pkg->header);
    pkg->fileList = freeStringBuf(pkg->fileList);
    pkg->fileFile = _free(pkg->fileFile);
    if (pkg->cpioList) {
	TFI_t fi = pkg->cpioList;
	pkg->cpioList = NULL;
	freeFi(fi);
	/*@-refcounttrans@*/ /* FIX: fi needs to be only */
	fi = _free(fi);
	/*@=refcounttrans@*/
    }

    pkg->specialDoc = freeStringBuf(pkg->specialDoc);
    pkg->icon = freeSources(pkg->icon);
    pkg->triggerFiles = freeTriggerFiles(pkg->triggerFiles);

    pkg = _free(pkg);
    return NULL;
}

Package freePackages(Package packages)
{
    Package p;

    while ((p = packages) != NULL) {
	packages = p->next;
	p->next = NULL;
	p = freePackage(p);
    }
    return NULL;
}

/**
 */
static inline /*@owned@*/ struct Source *findSource(Spec spec, int num, int flag)
	/*@*/
{
    struct Source *p;

    for (p = spec->sources; p != NULL; p = p->next)
	if ((num == p->num) && (p->flags & flag)) return p;

    return NULL;
}

int parseNoSource(Spec spec, const char * field, int tag)
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
    for (f = fe; *f != '\0'; f = fe) {
        struct Source *p;

	SKIPWHITE(f);
	if (*f == '\0')
	    break;
	fe = f;
	SKIPNONWHITE(fe);
	if (*fe != '\0') fe++;

	if (parseNum(f, &num)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad number: %s\n"),
		     spec->lineNum, f);
	    return RPMERR_BADSPEC;
	}

	if (! (p = findSource(spec, num, flag))) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad no%s number: %d\n"),
		     spec->lineNum, name, num);
	    return RPMERR_BADSPEC;
	}

	p->flags |= RPMBUILD_ISNO;

    }

    return 0;
}

int addSource(Spec spec, Package pkg, const char *field, int tag)
{
    struct Source *p;
    int flag = 0;
    char *name = NULL;
    char *nump;
    const char *fieldp = NULL;
    char buf[BUFSIZ];
    int num = 0;

    buf[0] = '\0';
    /*@-branchstate@*/
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
    /*@=branchstate@*/

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
	if (nump == NULL || *nump == '\0') {
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
    p = xmalloc(sizeof(*p));
    p->num = num;
    p->fullSource = xstrdup(field);
    p->flags = flag;
    p->source = strrchr(p->fullSource, '/');
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
	/*@-nullpass@*/		/* LCL: varargs needs null annotate. */
	const char *body = rpmGetPath("%{_sourcedir}/", p->source, NULL);
	/*@=nullpass@*/

	sprintf(buf, "%s%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, body, RMIL_SPEC);
	sprintf(buf, "%sURL%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, p->fullSource, RMIL_SPEC);
	body = _free(body);
    }
    
    return 0;
}

/**
 */
static inline /*@only@*/ /*@null@*/ speclines newSl(void)
	/*@*/
{
    speclines sl = NULL;
    /*@-branchstate@*/
    if (specedit) {
	sl = xmalloc(sizeof(*sl));
	sl->sl_lines = NULL;
	sl->sl_nalloc = 0;
	sl->sl_nlines = 0;
    }
    /*@=branchstate@*/
    return sl;
}

/**
 */
static inline /*@null@*/ speclines freeSl(/*@only@*/ /*@null@*/ speclines sl)
	/*@modifies sl @*/
{
    int i;
    if (sl == NULL) return NULL;
    for (i = 0; i < sl->sl_nlines; i++)
	/*@-unqualifiedtrans@*/
	sl->sl_lines[i] = _free(sl->sl_lines[i]);
	/*@=unqualifiedtrans@*/
    sl->sl_lines = _free(sl->sl_lines);
    return _free(sl);
}

/**
 */
static inline /*@only@*/ /*@null@*/ spectags newSt(void)
	/*@*/
{
    spectags st = NULL;
    /*@-branchstate@*/
    if (specedit) {
	st = xmalloc(sizeof(*st));
	st->st_t = NULL;
	st->st_nalloc = 0;
	st->st_ntags = 0;
    }
    /*@=branchstate@*/
    return st;
}

/**
 */
static inline /*@null@*/ spectags freeSt(/*@only@*/ /*@null@*/ spectags st)
	/*@modifies st @*/
{
    int i;
    if (st == NULL) return NULL;
    for (i = 0; i < st->st_ntags; i++) {
	spectag t = st->st_t + i;
	t->t_lang = _free(t->t_lang);
	t->t_msgid = _free(t->t_msgid);
    }
    st->st_t = _free(st->st_t);
    return _free(st);
}

Spec newSpec(void)
{
    Spec spec = xcalloc(1, sizeof(*spec));
    
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
    spec->readStack = xcalloc(1, sizeof(*spec->readStack));
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

    spec->sourceCpioList = NULL;
    
    spec->gotBuildRootURL = 0;
    spec->buildRootURL = NULL;
    spec->buildSubdir = NULL;

    spec->passPhrase = NULL;
    spec->timeCheck = 0;
    spec->cookie = NULL;

    spec->buildRestrictions = headerNew();
    spec->BANames = NULL;
    spec->BACount = 0;
    spec->recursing = 0;
    spec->BASpecs = NULL;

    spec->force = 0;
    spec->anyarch = 0;

/*@i@*/	spec->macros = rpmGlobalMacroContext;
    
    return spec;
}

Spec freeSpec(Spec spec)
{
    struct ReadLevelEntry *rl;

    if (spec == NULL) return NULL;

    spec->sl = freeSl(spec->sl);
    spec->st = freeSt(spec->st);

    spec->prep = freeStringBuf(spec->prep);
    spec->build = freeStringBuf(spec->build);
    spec->install = freeStringBuf(spec->install);
    spec->clean = freeStringBuf(spec->clean);

    spec->buildRootURL = _free(spec->buildRootURL);
    spec->buildSubdir = _free(spec->buildSubdir);
    spec->rootURL = _free(spec->rootURL);
    spec->specFile = _free(spec->specFile);
    spec->sourceRpmName = _free(spec->sourceRpmName);

#ifdef	DEAD
  { struct OpenFileInfo *ofi;
    while (spec->fileStack) {
	ofi = spec->fileStack;
	spec->fileStack = ofi->next;
	ofi->next = NULL;
	ofi->fileName = _free(ofi->fileName);
	ofi = _free(ofi);
    }
  }
#else
    closeSpec(spec);
#endif

    while (spec->readStack) {
	rl = spec->readStack;
	/*@-dependenttrans@*/
	spec->readStack = rl->next;
	/*@=dependenttrans@*/
	rl->next = NULL;
	rl = _free(rl);
    }
    
    spec->sourceHeader = headerFree(spec->sourceHeader);

    if (spec->sourceCpioList) {
	TFI_t fi = spec->sourceCpioList;
	spec->sourceCpioList = NULL;
	freeFi(fi);
	/*@-refcounttrans@*/ /* FIX: fi needs to be only */
	fi = _free(fi);
	/*@=refcounttrans@*/
    }
    
    spec->buildRestrictions = headerFree(spec->buildRestrictions);

    if (!spec->recursing) {
	if (spec->BASpecs != NULL)
	while (spec->BACount--) {
	    /*@-unqualifiedtrans@*/
	    spec->BASpecs[spec->BACount] =
			freeSpec(spec->BASpecs[spec->BACount]);
	    /*@=unqualifiedtrans@*/
	}
	/*@-compdef@*/
	spec->BASpecs = _free(spec->BASpecs);
	/*@=compdef@*/
    }
    spec->BANames = _free(spec->BANames);

    spec->passPhrase = _free(spec->passPhrase);
    spec->cookie = _free(spec->cookie);

    spec->sources = freeSources(spec->sources);
    spec->packages = freePackages(spec->packages);
    
    spec = _free(spec);

    return spec;
}

/*@only@*/ struct OpenFileInfo * newOpenFileInfo(void)
{
    struct OpenFileInfo *ofi;

    ofi = xmalloc(sizeof(*ofi));
    ofi->fd = NULL;
    ofi->fileName = NULL;
    ofi->lineNum = 0;
    ofi->readBuf[0] = '\0';
    ofi->readPtr = NULL;
    ofi->next = NULL;

    return ofi;
}
