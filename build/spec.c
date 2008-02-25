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

#include "build/buildio.h"
#include "rpmio/rpmlua.h"

#include "debug.h"

extern int specedit;

#define SKIPSPACE(s) { while (*(s) && xisspace(*(s))) (s)++; }
#define SKIPWHITE(_x)	{while(*(_x) && (xisspace(*_x) || *(_x) == ',')) (_x)++;}
#define SKIPNONWHITE(_x){while(*(_x) &&!(xisspace(*_x) || *(_x) == ',')) (_x)++;}

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
    const char *fullName;
    Package p;
    
    /* "main" package */
    if (name == NULL) {
	if (pkg)
	    *pkg = spec->packages;
	return RPMRC_OK;
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
    strcpy(n, name);
  }

    /* Locate package with fullName */
    for (p = spec->packages; p != NULL; p = p->next) {
	(void) headerNVR(p->header, &pname, NULL, NULL);
	if (pname && (! strcmp(fullName, pname))) {
	    break;
	}
    }

    if (pkg)
	*pkg = p;
    return ((p == NULL) ? RPMRC_FAIL : RPMRC_OK);
}

Package newPackage(rpmSpec spec)
{
    Package p;
    Package pp;

    p = xcalloc(1, sizeof(*p));

    p->header = headerNew();
    p->ds = NULL;
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
    
    pkg->preInFile = _constfree(pkg->preInFile);
    pkg->postInFile = _constfree(pkg->postInFile);
    pkg->preUnFile = _constfree(pkg->preUnFile);
    pkg->postUnFile = _constfree(pkg->postUnFile);
    pkg->verifyFile = _constfree(pkg->verifyFile);

    pkg->header = headerFree(pkg->header);
    pkg->ds = rpmdsFree(pkg->ds);
    pkg->fileList = freeStringBuf(pkg->fileList);
    pkg->fileFile = _constfree(pkg->fileFile);
    if (pkg->cpioList) {
	rpmfi fi = pkg->cpioList;
	pkg->cpioList = NULL;
	fi = rpmfiFree(fi);
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
static inline struct Source *findSource(rpmSpec spec, int num, int flag)
{
    struct Source *p;

    for (p = spec->sources; p != NULL; p = p->next)
	if ((num == p->num) && (p->flags & flag)) return p;

    return NULL;
}

int parseNoSource(rpmSpec spec, const char * field, rpmTag tag)
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
	    rpmlog(RPMLOG_ERR, _("line %d: Bad number: %s\n"),
		     spec->lineNum, f);
	    return RPMRC_FAIL;
	}

	if (! (p = findSource(spec, num, flag))) {
	    rpmlog(RPMLOG_ERR, _("line %d: Bad no%s number: %d\n"),
		     spec->lineNum, name, num);
	    return RPMRC_FAIL;
	}

	p->flags |= RPMBUILD_ISNO;

    }

    return 0;
}

int addSource(rpmSpec spec, Package pkg, const char *field, rpmTag tag)
{
    struct Source *p;
    int flag = 0;
    const char *name = NULL;
    char *nump;
    const char *fieldp = NULL;
    char buf[BUFSIZ];
    int num = 0;

    buf[0] = '\0';
    switch ((rpm_tag_t) tag) {
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
	if (nump == NULL || *nump == '\0') {
	    num = 0;
	} else {
	    if (parseNum(buf, &num)) {
		rpmlog(RPMLOG_ERR, _("line %d: Bad %s number: %s\n"),
			 spec->lineNum, name, spec->line);
		return RPMRC_FAIL;
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
	char *body = rpmGetPath("%{_sourcedir}/", p->source, NULL);

	sprintf(buf, "%s%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, body, RMIL_SPEC);
	sprintf(buf, "%sURL%d",
		(flag & RPMBUILD_ISPATCH) ? "PATCH" : "SOURCE", num);
	addMacro(spec->macros, buf, NULL, p->fullSource, RMIL_SPEC);
#ifdef WITH_LUA
	{
	rpmlua lua = NULL; /* global state */
	const char * what = (flag & RPMBUILD_ISPATCH) ? "patches" : "sources";
	rpmluaPushTable(lua, what);
	rpmluav var = rpmluavNew();
	rpmluavSetListMode(var, 1);
	rpmluavSetValue(var, RPMLUAV_STRING, body);
	rpmluaSetVar(lua, var);
	var = rpmluavFree(var);
	rpmluaPop(lua);
	}
#endif
	body = _free(body);
    }
    
    return 0;
}

/**
 */
static inline speclines newSl(void)
{
    speclines sl = NULL;
    if (specedit) {
	sl = xmalloc(sizeof(*sl));
	sl->sl_lines = NULL;
	sl->sl_nalloc = 0;
	sl->sl_nlines = 0;
    }
    return sl;
}

/**
 */
static inline speclines freeSl(speclines sl)
{
    int i;
    if (sl == NULL) return NULL;
    for (i = 0; i < sl->sl_nlines; i++)
	sl->sl_lines[i] = _free(sl->sl_lines[i]);
    sl->sl_lines = _free(sl->sl_lines);
    return _free(sl);
}

/**
 */
static inline spectags newSt(void)
{
    spectags st = NULL;
    if (specedit) {
	st = xmalloc(sizeof(*st));
	st->st_t = NULL;
	st->st_nalloc = 0;
	st->st_ntags = 0;
    }
    return st;
}

/**
 */
static inline spectags freeSt(spectags st)
{
    int i;
    if (st == NULL) return NULL;
    for (i = 0; i < st->st_ntags; i++) {
	spectag t = st->st_t + i;
	t->t_lang = _constfree(t->t_lang);
	t->t_msgid = _constfree(t->t_msgid);
    }
    st->st_t = _free(st->st_t);
    return _free(st);
}

rpmSpec newSpec(void)
{
    rpmSpec spec = xcalloc(1, sizeof(*spec));
    
    spec->specFile = NULL;

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
    spec->check = NULL;
    spec->clean = NULL;

    spec->sources = NULL;
    spec->packages = NULL;
    spec->noSource = 0;
    spec->numSources = 0;

    spec->sourceRpmName = NULL;
    spec->sourcePkgId = NULL;
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

rpmSpec freeSpec(rpmSpec spec)
{
    struct ReadLevelEntry *rl;

    if (spec == NULL) return NULL;

    spec->sl = freeSl(spec->sl);
    spec->st = freeSt(spec->st);

    spec->prep = freeStringBuf(spec->prep);
    spec->build = freeStringBuf(spec->build);
    spec->install = freeStringBuf(spec->install);
    spec->check = freeStringBuf(spec->check);
    spec->clean = freeStringBuf(spec->clean);

    spec->buildRootURL = _constfree(spec->buildRootURL);
    spec->buildSubdir = _constfree(spec->buildSubdir);
    spec->rootURL = _constfree(spec->rootURL);
    spec->specFile = _constfree(spec->specFile);

    closeSpec(spec);

    while (spec->readStack) {
	rl = spec->readStack;
	spec->readStack = rl->next;
	rl->next = NULL;
	rl = _free(rl);
    }
    
    spec->sourceRpmName = _constfree(spec->sourceRpmName);
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
			freeSpec(spec->BASpecs[spec->BACount]);
	}
	spec->BASpecs = _free(spec->BASpecs);
    }
    spec->BANames = _free(spec->BANames);

    spec->passPhrase = _free(spec->passPhrase);
    spec->cookie = _constfree(spec->cookie);

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

struct OpenFileInfo * newOpenFileInfo(void)
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

/**
 * Print copy of spec file, filling in Group/Description/Summary from specspo.
 * @param spec		spec file control structure
 */
static void
printNewSpecfile(rpmSpec spec)
{
    Header h;
    speclines sl = spec->sl;
    spectags st = spec->st;
    char * msgstr = NULL;
    int i, j;

    if (sl == NULL || st == NULL)
	return;

    for (i = 0; i < st->st_ntags; i++) {
	spectag t = st->st_t + i;
	const char * tn = rpmTagGetName(t->t_tag);
	const char * errstr;
	char fmt[1024];

	fmt[0] = '\0';
	if (t->t_msgid == NULL)
	    h = spec->packages->header;
	else {
	    Package pkg;
	    char *fe;

	    strcpy(fmt, t->t_msgid);
	    for (fe = fmt; *fe && *fe != '('; fe++)
		{} ;
	    if (*fe == '(') *fe = '\0';
	    h = NULL;
	    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
		const char *pkgname;
		h = pkg->header;
		(void) headerNVR(h, &pkgname, NULL, NULL);
		if (!strcmp(pkgname, fmt))
		    break;
	    }
	    if (pkg == NULL || h == NULL)
		h = spec->packages->header;
	}

	if (h == NULL)
	    continue;

	fmt[0] = '\0';
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tn), "}");
	msgstr = _free(msgstr);

	/* XXX this should use queryHeader(), but prints out tn as well. */
	msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr == NULL) {
	    rpmlog(RPMLOG_ERR, _("can't query %s: %s\n"), tn, errstr);
	    return;
	}

	switch(t->t_tag) {
	case RPMTAG_SUMMARY:
	case RPMTAG_GROUP:
	    sl->sl_lines[t->t_startx] = _free(sl->sl_lines[t->t_startx]);
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG))
		continue;
	    {   char *buf = xmalloc(strlen(tn) + sizeof(": ") + strlen(msgstr));
		(void) stpcpy( stpcpy( stpcpy(buf, tn), ": "), msgstr);
		sl->sl_lines[t->t_startx] = buf;
	    }
	    break;
	case RPMTAG_DESCRIPTION:
	    for (j = 1; j < t->t_nlines; j++) {
		if (*sl->sl_lines[t->t_startx + j] == '%')
		    continue;
		sl->sl_lines[t->t_startx + j] =
			_free(sl->sl_lines[t->t_startx + j]);
	    }
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG)) {
		sl->sl_lines[t->t_startx] = _free(sl->sl_lines[t->t_startx]);
		continue;
	    }
	    sl->sl_lines[t->t_startx + 1] = xstrdup(msgstr);
	    if (t->t_nlines > 2)
		sl->sl_lines[t->t_startx + 2] = xstrdup("\n\n");
	    break;
	}
    }
    msgstr = _free(msgstr);

    for (i = 0; i < sl->sl_nlines; i++) {
	const char * s = sl->sl_lines[i];
	if (s == NULL)
	    continue;
	printf("%s", s);
	if (strchr(s, '\n') == NULL && s[strlen(s)-1] != '\n')
	    printf("\n");
    }
}

int rpmspecQuery(rpmts ts, QVA_t qva, const char * arg)
{
    rpmSpec spec = NULL;
    Package pkg;
    char * buildRoot = NULL;
    int recursing = 0;
    char * passPhrase = "";
    char *cookie = NULL;
    int anyarch = 1;
    int force = 1;
    int res = 1;
    int xx;

    if (qva->qva_showPackage == NULL)
	goto exit;

    /* FIX: make spec abstract */
    if (parseSpec(ts, arg, "/", buildRoot, recursing, passPhrase,
		cookie, anyarch, force)
      || (spec = rpmtsSetSpec(ts, NULL)) == NULL)
    {
	rpmlog(RPMLOG_ERR,
	    		_("query of specfile %s failed, can't parse\n"), arg);
	goto exit;
    }

    res = 0;
    if (specedit) {
	printNewSpecfile(spec);
	goto exit;
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next)
	xx = qva->qva_showPackage(qva, ts, pkg->header);

exit:
    spec = freeSpec(spec);
    return res;
}
