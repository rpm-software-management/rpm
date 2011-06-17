/** \ingroup rpmbuild
 * \file build/spec.c
 * Handle spec data structure.
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "rpmio/rpmlua.h"
#include "build/rpmbuild_internal.h"

#include "debug.h"

#define SKIPSPACE(s) { while (*(s) && risspace(*(s))) (s)++; }

/**
 * @param p		trigger entry chain
 * @return		NULL always
 */
static inline
struct TriggerFileEntry * freeTriggerFiles(struct TriggerFileEntry * p)
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
 * @param s		source component chain
 * @return		NULL always
 */
static inline
struct Source * freeSources(struct Source * s)
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

rpmRC lookupPackage(rpmSpec spec, const char *name, int flag,Package *pkg)
{
    const char *pname;
    char *fullName = NULL;
    Package p;

    /* "main" package */
    if (name == NULL) {
	if (pkg)
	    *pkg = spec->packages;
	return RPMRC_OK;
    }

    /* Construct package name */
    if (flag == PART_SUBNAME) {
	pname = headerGetString(spec->packages->header, RPMTAG_NAME);
	rasprintf(&fullName, "%s-%s", pname, name);
    } else {
	fullName = xstrdup(name);
    }

    /* Locate package with fullName */
    for (p = spec->packages; p != NULL; p = p->next) {
	pname = headerGetString(p->header, RPMTAG_NAME);
	if (pname && (rstreq(fullName, pname))) {
	    break;
	}
    }
    free(fullName);

    if (pkg)
	*pkg = p;
    return ((p == NULL) ? RPMRC_FAIL : RPMRC_OK);
}

Package newPackage(rpmSpec spec)
{
    Package p = xcalloc(1, sizeof(*p));
    p->header = headerNew();
    p->autoProv = 1;
    p->autoReq = 1;
    p->fileList = NULL;
    p->fileFile = NULL;
    p->policyList = NULL;

    if (spec->packages == NULL) {
	spec->packages = p;
    } else {
	Package pp;
	/* Always add package to end of list */
	for (pp = spec->packages; pp->next != NULL; pp = pp->next)
	    {};
	pp->next = p;
    }
    p->next = NULL;

    return p;
}

static Package freePackage(Package pkg)
{
    if (pkg == NULL) return NULL;
    
    pkg->preInFile = _free(pkg->preInFile);
    pkg->postInFile = _free(pkg->postInFile);
    pkg->preUnFile = _free(pkg->preUnFile);
    pkg->postUnFile = _free(pkg->postUnFile);
    pkg->verifyFile = _free(pkg->verifyFile);

    pkg->header = headerFree(pkg->header);
    pkg->ds = rpmdsFree(pkg->ds);
    pkg->fileList = argvFree(pkg->fileList);
    pkg->fileFile = argvFree(pkg->fileFile);
    pkg->policyList = argvFree(pkg->policyList);
    if (pkg->cpioList) {
	rpmfi fi = pkg->cpioList;
	pkg->cpioList = NULL;
	fi = rpmfiFree(fi);
    }

    pkg->specialDoc = freeStringBuf(pkg->specialDoc);
    pkg->specialDocDir = _free(pkg->specialDocDir);
    pkg->icon = freeSources(pkg->icon);
    pkg->triggerFiles = freeTriggerFiles(pkg->triggerFiles);

    pkg = _free(pkg);
    return NULL;
}

static Package freePackages(Package packages)
{
    Package p;

    while ((p = packages) != NULL) {
	packages = p->next;
	p->next = NULL;
	p = freePackage(p);
    }
    return NULL;
}

rpmSpec newSpec(void)
{
    rpmSpec spec = xcalloc(1, sizeof(*spec));
    
    spec->specFile = NULL;

    spec->fileStack = NULL;
    spec->lbuf[0] = '\0';
    spec->line = spec->lbuf;
    spec->nextline = NULL;
    spec->nextpeekc = '\0';
    spec->lineNum = 0;
    spec->readStack = xcalloc(1, sizeof(*spec->readStack));
    spec->readStack->next = NULL;
    spec->readStack->reading = 1;

    spec->rootDir = NULL;
    spec->prep = NULL;
    spec->build = NULL;
    spec->install = NULL;
    spec->check = NULL;
    spec->clean = NULL;
    spec->parsed = NULL;

    spec->sources = NULL;
    spec->packages = NULL;
    spec->noSource = 0;
    spec->numSources = 0;

    spec->sourceRpmName = NULL;
    spec->sourcePkgId = NULL;
    spec->sourceHeader = NULL;
    spec->sourceCpioList = NULL;
    
    spec->buildRoot = NULL;
    spec->buildSubdir = NULL;

    spec->buildRestrictions = headerNew();
    spec->BANames = NULL;
    spec->BACount = 0;
    spec->recursing = 0;
    spec->BASpecs = NULL;

    spec->flags = RPMSPEC_NONE;

    spec->macros = rpmGlobalMacroContext;
    
#ifdef WITH_LUA
    {
    /* make sure patches and sources tables always exist */
    rpmlua lua = NULL; /* global state */
    rpmluaPushTable(lua, "patches");
    rpmluaPushTable(lua, "sources");
    rpmluaPop(lua);
    rpmluaPop(lua);
    }
#endif
    return spec;
}

rpmSpec rpmSpecFree(rpmSpec spec)
{

    if (spec == NULL) return NULL;

    spec->prep = freeStringBuf(spec->prep);
    spec->build = freeStringBuf(spec->build);
    spec->install = freeStringBuf(spec->install);
    spec->check = freeStringBuf(spec->check);
    spec->clean = freeStringBuf(spec->clean);
    spec->parsed = freeStringBuf(spec->parsed);

    spec->buildRoot = _free(spec->buildRoot);
    spec->buildSubdir = _free(spec->buildSubdir);
    spec->specFile = _free(spec->specFile);

    closeSpec(spec);

    while (spec->readStack) {
	struct ReadLevelEntry *rl = spec->readStack;
	spec->readStack = rl->next;
	rl->next = NULL;
	free(rl);
    }
    
    spec->sourceRpmName = _free(spec->sourceRpmName);
    spec->sourcePkgId = _free(spec->sourcePkgId);
    spec->sourceHeader = headerFree(spec->sourceHeader);

    if (spec->sourceCpioList) {
	rpmfi fi = spec->sourceCpioList;
	spec->sourceCpioList = NULL;
	fi = rpmfiFree(fi);
    }
    
    spec->buildRestrictions = headerFree(spec->buildRestrictions);

    if (!spec->recursing) {
	if (spec->BASpecs != NULL)
	while (spec->BACount--) {
	    spec->BASpecs[spec->BACount] =
			rpmSpecFree(spec->BASpecs[spec->BACount]);
	}
	spec->BASpecs = _free(spec->BASpecs);
    }
    spec->BANames = _free(spec->BANames);

#ifdef WITH_LUA
    rpmlua lua = NULL; /* global state */
    rpmluaDelVar(lua, "patches");
    rpmluaDelVar(lua, "sources");	
#endif

    spec->sources = freeSources(spec->sources);
    spec->packages = freePackages(spec->packages);
    
    spec = _free(spec);

    return spec;
}

Header rpmSpecSourceHeader(rpmSpec spec)
{
	return spec->sourceHeader;
}

rpmds rpmSpecDS(rpmSpec spec, rpmTagVal tag)
{
    return (spec != NULL) ? rpmdsNew(spec->buildRestrictions, tag, 0) : NULL;
}

rpmps rpmSpecCheckDeps(rpmts ts, rpmSpec spec)
{
    rpmps probs = NULL;

    rpmtsEmpty(ts);

    rpmtsAddInstallElement(ts, rpmSpecSourceHeader(spec), NULL, 0, NULL);
    rpmtsCheck(ts);
    probs = rpmtsProblems(ts);

    rpmtsEmpty(ts);
    return probs;
}

struct rpmSpecIter_s {
    void *next;
};

#define SPEC_LISTITER_INIT(_itertype, _iteritem)	\
    _itertype iter = NULL;				\
    if (spec) {						\
	iter = xcalloc(1, sizeof(*iter));		\
	iter->next = spec->_iteritem;			\
    }							\
    return iter

#define SPEC_LISTITER_NEXT(_valuetype)			\
    _valuetype item = NULL;				\
    if (iter) {						\
	item = iter->next;				\
	iter->next = (item) ? item->next : NULL;	\
    }							\
    return item

#define SPEC_LISTITER_FREE()				\
    free(iter);						\
    return NULL


rpmSpecPkgIter rpmSpecPkgIterInit(rpmSpec spec)
{
    SPEC_LISTITER_INIT(rpmSpecPkgIter, packages);
}

rpmSpecPkgIter rpmSpecPkgIterFree(rpmSpecPkgIter iter)
{
    SPEC_LISTITER_FREE();
}

rpmSpecPkg rpmSpecPkgIterNext(rpmSpecPkgIter iter)
{
    SPEC_LISTITER_NEXT(rpmSpecPkg);
}

Header rpmSpecPkgHeader(rpmSpecPkg pkg)
{
    return (pkg != NULL) ? pkg->header : NULL;
}

rpmSpecSrcIter rpmSpecSrcIterInit(rpmSpec spec)
{
    SPEC_LISTITER_INIT(rpmSpecSrcIter, sources);
}

rpmSpecSrcIter rpmSpecSrcIterFree(rpmSpecSrcIter iter)
{
    SPEC_LISTITER_FREE();
}

rpmSpecSrc rpmSpecSrcIterNext(rpmSpecSrcIter iter)
{
    SPEC_LISTITER_NEXT(rpmSpecSrc);
}

rpmSourceFlags rpmSpecSrcFlags(rpmSpecSrc src)
{
    return (src != NULL) ? src->flags : 0;
}

int rpmSpecSrcNum(rpmSpecSrc src)
{
    return (src != NULL) ? src->num : -1;
}

const char * rpmSpecSrcFilename(rpmSpecSrc src, int full)
{
    const char *source = NULL;
    if (src) {
	source = full ? src->fullSource : src->source;
    }
    return source;
}

const char * rpmSpecGetSection(rpmSpec spec, int section)
{
    if (spec) {
	switch (section) {
	case RPMBUILD_NONE:	return getStringBuf(spec->parsed);
	case RPMBUILD_PREP:	return getStringBuf(spec->prep);
	case RPMBUILD_BUILD:	return getStringBuf(spec->build);
	case RPMBUILD_INSTALL:	return getStringBuf(spec->install);
	case RPMBUILD_CHECK:	return getStringBuf(spec->check);
	case RPMBUILD_CLEAN:	return getStringBuf(spec->clean);
	}
    }
    return NULL;
}

int rpmspecQuery(rpmts ts, QVA_t qva, const char * arg)
{
    rpmSpec spec = NULL;
    int res = 1;
    int xx;

    if (qva->qva_showPackage == NULL)
	goto exit;

    spec = rpmSpecParse(arg, (RPMSPEC_ANYARCH|RPMSPEC_FORCE), NULL);
    if (spec == NULL) {
	rpmlog(RPMLOG_ERR,
	    		_("query of specfile %s failed, can't parse\n"), arg);
	goto exit;
    }

    res = 0;
    if (qva->qva_source == RPMQV_SPECRPMS) {
	for (Package pkg = spec->packages; pkg != NULL; pkg = pkg->next)
	    xx = qva->qva_showPackage(qva, ts, pkg->header);
    } else {
	xx = qva->qva_showPackage(qva, ts, spec->sourceHeader);
    }

exit:
    spec = rpmSpecFree(spec);
    return res;
}
