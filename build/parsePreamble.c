/** \ingroup rpmbuild
 * \file build/parsePreamble.c
 *  Parse tags in global section from spec file.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmbuild.h>
#include "debug.h"

/*@access FD_t @*/	/* compared with NULL */

/**
 */
static int_32 copyTagsDuringParse[] = {
    RPMTAG_EPOCH,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_LICENSE,
    RPMTAG_PACKAGER,
    RPMTAG_DISTRIBUTION,
    RPMTAG_DISTURL,
    RPMTAG_VENDOR,
    RPMTAG_ICON,
    RPMTAG_URL,
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    RPMTAG_PREFIXES,
    0
};

/**
 */
static int requiredTags[] = {
    RPMTAG_NAME,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_SUMMARY,
    RPMTAG_GROUP,
    RPMTAG_LICENSE,
    0
};

/**
 */
static void addOrAppendListEntry(Header h, int_32 tag, char *line)
{
    int argc;
    const char **argv;

    (void) poptParseArgvString(line, &argc, &argv);
    if (argc)
	(void) headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, argv, argc);
    argv = _free(argv);
}

/* Parse a simple part line that only take -n <pkg> or <pkg> */
/* <pkg> is return in name as a pointer into a static buffer */

/**
 */
static int parseSimplePart(char *line, /*@out@*/char **name, /*@out@*/int *flag)
{
    char *tok;
    char linebuf[BUFSIZ];
    static char buf[BUFSIZ];

    strcpy(linebuf, line);

    /* Throw away the first token (the %xxxx) */
    (void)strtok(linebuf, " \t\n");
    
    if (!(tok = strtok(NULL, " \t\n"))) {
	*name = NULL;
	return 0;
    }
    
    if (!strcmp(tok, "-n")) {
	if (!(tok = strtok(NULL, " \t\n")))
	    return 1;
	*flag = PART_NAME;
    } else {
	*flag = PART_SUBNAME;
    }
    strcpy(buf, tok);
    *name = buf;

    return (strtok(NULL, " \t\n")) ? 1 : 0;
}

/**
 */
static inline int parseYesNo(const char *s)
{
    return ((!s || (s[0] == 'n' || s[0] == 'N' || s[0] == '0') ||
	!xstrcasecmp(s, "false") || !xstrcasecmp(s, "off"))
	    ? 0 : 1);
}

typedef struct tokenBits_s {
    const char * name;
    rpmsenseFlags bits;
} * tokenBits;

/**
 */
static struct tokenBits_s installScriptBits[] = {
    { "interp",		RPMSENSE_INTERP },
    { "prereq",		RPMSENSE_PREREQ },
    { "preun",		RPMSENSE_SCRIPT_PREUN },
    { "pre",		RPMSENSE_SCRIPT_PRE },
    { "postun",		RPMSENSE_SCRIPT_POSTUN },
    { "post",		RPMSENSE_SCRIPT_POST },
    { "rpmlib",		RPMSENSE_RPMLIB },
    { "verify",		RPMSENSE_SCRIPT_VERIFY },
    { NULL, 0 }
};

/**
 */
static struct tokenBits_s buildScriptBits[] = {
    { "prep",		RPMSENSE_SCRIPT_PREP },
    { "build",		RPMSENSE_SCRIPT_BUILD },
    { "install",	RPMSENSE_SCRIPT_INSTALL },
    { "clean",		RPMSENSE_SCRIPT_CLEAN },
    { NULL, 0 }
};

/**
 */
static int parseBits(const char * s, const tokenBits tokbits,
		/*@out@*/ rpmsenseFlags * bp)
{
    tokenBits tb;
    const char * se;
    rpmsenseFlags bits = RPMSENSE_ANY;
    int c = 0;

    if (s) {
	while (*s != '\0') {
	    while ((c = *s) && xisspace(c)) s++;
	    se = s;
	    while ((c = *se) && xisalpha(c)) se++;
	    if (s == se)
		break;
	    for (tb = tokbits; tb->name; tb++) {
		if (strlen(tb->name) == (se-s) && !strncmp(tb->name, s, (se-s)))
		    break;
	    }
	    if (tb->name == NULL)
		break;
	    bits |= tb->bits;
	    while ((c = *se) && xisspace(c)) se++;
	    if (c != ',')
		break;
	    s = ++se;
	}
    }
    if (c == 0 && bp) *bp = bits;
    return (c ? RPMERR_BADSPEC : 0);
}

/**
 */
static inline char * findLastChar(char * s)
{
    char *res = s;

    while (*s != '\0') {
	if (! xisspace(*s))
	    res = s;
	s++;
    }

    /*@-temptrans@*/
    return res;
    /*@=temptrans@*/
}

/**
 */
static int isMemberInEntry(Header header, const char *name, int tag)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** names;
    int type, count;

    if (!hge(header, tag, &type, (void **)&names, &count))
	return -1;
    while (count--) {
	if (!xstrcasecmp(names[count], name))
	    break;
    }
    names = hfd(names, type);
    return (count >= 0 ? 1 : 0);
}

/**
 */
static int checkForValidArchitectures(Spec spec)
{
#ifndef	DYING
    const char *arch = NULL;
    const char *os = NULL;

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
#else
    const char *arch = rpmExpand("%{_target_cpu}", NULL);
    const char *os = rpmExpand("%{_target_os}", NULL);
#endif
    
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUDEARCH) == 1) {
	rpmError(RPMERR_BADSPEC, _("Architecture is excluded: %s\n"), arch);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUSIVEARCH) == 0) {
	rpmError(RPMERR_BADSPEC, _("Architecture is not included: %s\n"), arch);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUDEOS) == 1) {
	rpmError(RPMERR_BADSPEC, _("OS is excluded: %s\n"), os);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUSIVEOS) == 0) {
	rpmError(RPMERR_BADSPEC, _("OS is not included: %s\n"), os);
	return RPMERR_BADSPEC;
    }

    return 0;
}

/**
 */
static int checkForRequired(Header h, const char *name)
{
    int res = 0;
    int *p;

    for (p = requiredTags; *p != 0; p++) {
	if (!headerIsEntry(h, *p)) {
	    rpmError(RPMERR_BADSPEC,
			_("%s field must be present in package: %s\n"),
			tagName(*p), name);
	    res = 1;
	}
    }

    return res;
}

/**
 */
static int checkForDuplicates(Header h, const char *name)
{
    int res = 0;
    int lastTag, tag;
    HeaderIterator hi;
    
#if 0	/* XXX harmless, but headerInitIterator() does this anyways */
    headerSort(h);
#endif

    for (hi = headerInitIterator(h), lastTag = 0;
	headerNextIterator(hi, &tag, NULL, NULL, NULL);
	lastTag = tag)
    {
	if (tag != lastTag)
	    continue;
	rpmError(RPMERR_BADSPEC, _("Duplicate %s entries in package: %s\n"),
		     tagName(tag), name);
	res = 1;
    }
    headerFreeIterator(hi);

    return res;
}

/**
 */
static struct optionalTag {
    int		ot_tag;
    const char *ot_mac;
} optionalTags[] = {
    { RPMTAG_VENDOR,		"%{vendor}" },
    { RPMTAG_PACKAGER,		"%{packager}" },
    { RPMTAG_DISTRIBUTION,	"%{distribution}" },
    { RPMTAG_DISTURL,		"%{disturl}" },
    { -1, NULL }
};

/**
 */
static void fillOutMainPackage(Header h)
{
    struct optionalTag *ot;

    for (ot = optionalTags; ot->ot_mac != NULL; ot++) {
	if (!headerIsEntry(h, ot->ot_tag)) {
	    const char *val = rpmExpand(ot->ot_mac, NULL);
	    if (val && *val != '%')
		(void) headerAddEntry(h, ot->ot_tag, RPM_STRING_TYPE, (void *)val, 1);
	    val = _free(val);
	}
    }
}

/**
 */
static int readIcon(Header h, const char *file)
{
    const char *fn = NULL;
    char *icon;
    FD_t fd;
    int rc = 0;
    off_t size;
    size_t nb, iconsize;

    /* XXX use rpmGenPath(rootdir, "%{_sourcedir}/", file) for icon path. */
    fn = rpmGetPath("%{_sourcedir}/", file, NULL);

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_BADSPEC, _("Unable to open icon %s: %s\n"),
		fn, Fstrerror(fd));
	rc = RPMERR_BADSPEC;
	goto exit;
    }
    size = fdSize(fd);
    iconsize = (size >= 0 ? size : (8 * BUFSIZ));
    if (iconsize == 0) {
	(void) Fclose(fd);
	rc = 0;
	goto exit;
    }

    icon = xmalloc(iconsize + 1);
    *icon = '\0';

    nb = Fread(icon, sizeof(char), iconsize, fd);
    if (Ferror(fd) || (size >= 0 && nb != size)) {
	rpmError(RPMERR_BADSPEC, _("Unable to read icon %s: %s\n"),
		fn, Fstrerror(fd));
	rc = RPMERR_BADSPEC;
    }
    (void) Fclose(fd);
    if (rc)
	goto exit;

    if (! strncmp(icon, "GIF", sizeof("GIF")-1)) {
	(void) headerAddEntry(h, RPMTAG_GIF, RPM_BIN_TYPE, icon, iconsize);
    } else if (! strncmp(icon, "/* XPM", sizeof("/* XPM")-1)) {
	(void) headerAddEntry(h, RPMTAG_XPM, RPM_BIN_TYPE, icon, iconsize);
    } else {
	rpmError(RPMERR_BADSPEC, _("Unknown icon type: %s\n"), file);
	rc = RPMERR_BADSPEC;
	goto exit;
    }
    icon = _free(icon);
    
exit:
    fn = _free(fn);
    return rc;
}

struct spectag *
stashSt(Spec spec, Header h, int tag, const char *lang)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    struct spectag *t = NULL;

    if (spec->st) {
	struct spectags *st = spec->st;
	if (st->st_ntags == st->st_nalloc) {
	    st->st_nalloc += 10;
	    st->st_t = xrealloc(st->st_t, st->st_nalloc * sizeof(*(st->st_t)));
	}
	t = st->st_t + st->st_ntags++;
	t->t_tag = tag;
	t->t_startx = spec->lineNum - 1;
	t->t_nlines = 1;
	t->t_lang = xstrdup(lang);
	t->t_msgid = NULL;
	if (!(t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG))) {
	    char *n;
	    if (hge(h, RPMTAG_NAME, NULL, (void **) &n, NULL)) {
		char buf[1024];
		sprintf(buf, "%s(%s)", n, tagName(tag));
		t->t_msgid = xstrdup(buf);
	    }
	}
    }
    /*@-usereleased -compdef@*/
    return t;
    /*@=usereleased =compdef@*/
}

#define SINGLE_TOKEN_ONLY \
if (multiToken) { \
    rpmError(RPMERR_BADSPEC, _("line %d: Tag takes single token only: %s\n"), \
	     spec->lineNum, spec->line); \
    return RPMERR_BADSPEC; \
}

extern int noLang;	/* XXX FIXME: pass as arg */

/**
 */
static int handlePreambleTag(Spec spec, Package pkg, int tag, const char *macro,
			     const char *lang)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    char *field = spec->line;
    char *end;
    char **array;
    int multiToken = 0;
    rpmsenseFlags tagflags;
    int type;
    int len;
    int num;
    int rc;
    
    if (field == NULL) return RPMERR_BADSPEC;	/* XXX can't happen */
    /* Find the start of the "field" and strip trailing space */
    while ((*field) && (*field != ':'))
	field++;
    if (*field != ':') {
	rpmError(RPMERR_BADSPEC, _("line %d: Malformed tag: %s\n"),
		 spec->lineNum, spec->line);
	return RPMERR_BADSPEC;
    }
    field++;
    SKIPSPACE(field);
    if (!*field) {
	/* Empty field */
	rpmError(RPMERR_BADSPEC, _("line %d: Empty tag: %s\n"),
		 spec->lineNum, spec->line);
	return RPMERR_BADSPEC;
    }
    end = findLastChar(field);
    *(end+1) = '\0';

    /* See if this is multi-token */
    end = field;
    SKIPNONSPACE(end);
    if (*end != '\0')
	multiToken = 1;

    switch (tag) {
      case RPMTAG_NAME:
      case RPMTAG_VERSION:
      case RPMTAG_RELEASE:
      case RPMTAG_URL:
	SINGLE_TOKEN_ONLY;
	/* These macros are for backward compatibility */
	if (tag == RPMTAG_VERSION) {
	    if (strchr(field, '-') != NULL) {
		rpmError(RPMERR_BADSPEC, _("line %d: Illegal char '-' in %s: %s\n"),
		    spec->lineNum, "version", spec->line);
		return RPMERR_BADSPEC;
	    }
	    addMacro(spec->macros, "PACKAGE_VERSION", NULL, field, RMIL_OLDSPEC);
	} else if (tag == RPMTAG_RELEASE) {
	    if (strchr(field, '-') != NULL) {
		rpmError(RPMERR_BADSPEC, _("line %d: Illegal char '-' in %s: %s\n"),
		    spec->lineNum, "release", spec->line);
		return RPMERR_BADSPEC;
	    }
	    addMacro(spec->macros, "PACKAGE_RELEASE", NULL, field, RMIL_OLDSPEC-1);
	}
	(void) headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, field, 1);
	break;
      case RPMTAG_GROUP:
      case RPMTAG_SUMMARY:
	(void) stashSt(spec, pkg->header, tag, lang);
	/*@fallthrough@*/
      case RPMTAG_DISTRIBUTION:
      case RPMTAG_VENDOR:
      case RPMTAG_LICENSE:
      case RPMTAG_PACKAGER:
	if (!*lang)
	    (void) headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, field, 1);
	else if (!(noLang && strcmp(lang, RPMBUILD_DEFAULT_LANG)))
	    (void) headerAddI18NString(pkg->header, tag, field, lang);
	break;
      case RPMTAG_BUILDROOT:
	SINGLE_TOKEN_ONLY;
      {	const char * buildRoot = NULL;
	const char * buildRootURL = spec->buildRootURL;

	/*
	 * Note: rpmGenPath should guarantee a "canonical" path. That means
	 * that the following pathologies should be weeded out:
	 *          //bin//sh
	 *          //usr//bin/
	 *          /.././../usr/../bin//./sh
	 */
	if (buildRootURL == NULL) {
	    buildRootURL = rpmGenPath(NULL, "%{?buildroot:%{buildroot}}", NULL);
	    if (strcmp(buildRootURL, "/")) {
		spec->buildRootURL = buildRootURL;
		macro = NULL;
	    } else {
		const char * specURL = field;

		buildRootURL = _free(buildRootURL);
		(void) urlPath(specURL, (const char **)&field);
		if (*field == '\0') field = "/";
		buildRootURL = rpmGenPath(spec->rootURL, field, NULL);
		spec->buildRootURL = buildRootURL;
		field = (char *) buildRootURL;
	    }
	    spec->gotBuildRootURL = 1;
	} else {
	    macro = NULL;
	}
	buildRootURL = rpmGenPath(NULL, spec->buildRootURL, NULL);
	(void) urlPath(buildRootURL, &buildRoot);
	if (*buildRoot == '\0') buildRoot = "/";
	if (!strcmp(buildRoot, "/")) {
	    rpmError(RPMERR_BADSPEC,
		     _("BuildRoot can not be \"/\": %s\n"), spec->buildRootURL);
	    buildRootURL = _free(buildRootURL);
	    return RPMERR_BADSPEC;
	}
	buildRootURL = _free(buildRootURL);
      }	break;
      case RPMTAG_PREFIXES:
	addOrAppendListEntry(pkg->header, tag, field);
	(void) hge(pkg->header, tag, &type, (void **)&array, &num);
	while (num--) {
	    len = strlen(array[num]);
	    if (array[num][len - 1] == '/' && len > 1) {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: Prefixes must not end with \"/\": %s\n"),
			 spec->lineNum, spec->line);
		array = hfd(array, type);
		return RPMERR_BADSPEC;
	    }
	}
	array = hfd(array, type);
	break;
      case RPMTAG_DOCDIR:
	SINGLE_TOKEN_ONLY;
	if (field[0] != '/') {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Docdir must begin with '/': %s\n"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	macro = NULL;
	delMacro(NULL, "_docdir");
	addMacro(NULL, "_docdir", NULL, field, RMIL_SPEC);
	break;
      case RPMTAG_EPOCH:
	SINGLE_TOKEN_ONLY;
	if (parseNum(field, &num)) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Epoch/Serial field must be a number: %s\n"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	(void) headerAddEntry(pkg->header, tag, RPM_INT32_TYPE, &num, 1);
	break;
      case RPMTAG_AUTOREQPROV:
	pkg->autoReq = parseYesNo(field);
	pkg->autoProv = pkg->autoReq;
	break;
      case RPMTAG_AUTOREQ:
	pkg->autoReq = parseYesNo(field);
	break;
      case RPMTAG_AUTOPROV:
	pkg->autoProv = parseYesNo(field);
	break;
      case RPMTAG_SOURCE:
      case RPMTAG_PATCH:
	SINGLE_TOKEN_ONLY;
	macro = NULL;
	if ((rc = addSource(spec, pkg, field, tag)))
	    return rc;
	break;
      case RPMTAG_ICON:
	SINGLE_TOKEN_ONLY;
	if ((rc = addSource(spec, pkg, field, tag)))
	    return rc;
	if ((rc = readIcon(pkg->header, field)))
	    return RPMERR_BADSPEC;
	break;
      case RPMTAG_NOSOURCE:
      case RPMTAG_NOPATCH:
	spec->noSource = 1;
	if ((rc = parseNoSource(spec, field, tag)))
	    return rc;
	break;
      case RPMTAG_BUILDPREREQ:
      case RPMTAG_BUILDREQUIRES:
	if ((rc = parseBits(lang, buildScriptBits, &tagflags))) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Bad %s: qualifiers: %s\n"),
		     spec->lineNum, tagName(tag), spec->line);
	    return rc;
	}
	if ((rc = parseRCPOT(spec, pkg, field, tag, 0, tagflags)))
	    return rc;
	break;
      case RPMTAG_REQUIREFLAGS:
      case RPMTAG_PREREQ:
	if ((rc = parseBits(lang, installScriptBits, &tagflags))) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Bad %s: qualifiers: %s\n"),
		     spec->lineNum, tagName(tag), spec->line);
	    return rc;
	}
	if ((rc = parseRCPOT(spec, pkg, field, tag, 0, tagflags)))
	    return rc;
	break;
      case RPMTAG_BUILDCONFLICTS:
      case RPMTAG_CONFLICTFLAGS:
      case RPMTAG_OBSOLETEFLAGS:
      case RPMTAG_PROVIDEFLAGS:
	tagflags = RPMSENSE_ANY;
	if ((rc = parseRCPOT(spec, pkg, field, tag, 0, tagflags)))
	    return rc;
	break;
      case RPMTAG_EXCLUDEARCH:
      case RPMTAG_EXCLUSIVEARCH:
      case RPMTAG_EXCLUDEOS:
      case RPMTAG_EXCLUSIVEOS:
	addOrAppendListEntry(spec->buildRestrictions, tag, field);
	break;
      case RPMTAG_BUILDARCHS:
	if ((rc = poptParseArgvString(field,
				      &(spec->buildArchitectureCount),
				      &(spec->buildArchitectures)))) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Bad BuildArchitecture format: %s\n"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	if (!spec->buildArchitectureCount)
	    spec->buildArchitectures = _free(spec->buildArchitectures);
	break;

      default:
	rpmError(RPMERR_INTERNAL, _("Internal error: Bogus tag %d\n"), tag);
	return RPMERR_INTERNAL;
    }

    if (macro)
	addMacro(spec->macros, macro, NULL, field, RMIL_SPEC);
    
    return 0;
}

/* This table has to be in a peculiar order.  If one tag is the */
/* same as another, plus a few letters, it must come first.     */

/**
 */
typedef struct PreambleRec_s {
    int tag;
    int len;
    int multiLang;
    const char * token;
} * PreambleRec;
static struct PreambleRec_s preambleList[] = {
    {RPMTAG_NAME,		0, 0, "name"},
    {RPMTAG_VERSION,		0, 0, "version"},
    {RPMTAG_RELEASE,		0, 0, "release"},
    {RPMTAG_EPOCH,		0, 0, "epoch"},
    {RPMTAG_EPOCH,		0, 0, "serial"},
    {RPMTAG_SUMMARY,		0, 1, "summary"},
    {RPMTAG_LICENSE,		0, 0, "copyright"},
    {RPMTAG_LICENSE,		0, 0, "license"},
    {RPMTAG_DISTRIBUTION,	0, 0, "distribution"},
    {RPMTAG_DISTURL,		0, 0, "disturl"},
    {RPMTAG_VENDOR,		0, 0, "vendor"},
    {RPMTAG_GROUP,		0, 1, "group"},
    {RPMTAG_PACKAGER,		0, 0, "packager"},
    {RPMTAG_URL,		0, 0, "url"},
    {RPMTAG_SOURCE,		0, 0, "source"},
    {RPMTAG_PATCH,		0, 0, "patch"},
    {RPMTAG_NOSOURCE,		0, 0, "nosource"},
    {RPMTAG_NOPATCH,		0, 0, "nopatch"},
    {RPMTAG_EXCLUDEARCH,	0, 0, "excludearch"},
    {RPMTAG_EXCLUSIVEARCH,	0, 0, "exclusivearch"},
    {RPMTAG_EXCLUDEOS,		0, 0, "excludeos"},
    {RPMTAG_EXCLUSIVEOS,	0, 0, "exclusiveos"},
    {RPMTAG_ICON,		0, 0, "icon"},
    {RPMTAG_PROVIDEFLAGS,	0, 0, "provides"},
    {RPMTAG_REQUIREFLAGS,	0, 1, "requires"},
    {RPMTAG_PREREQ,		0, 1, "prereq"},
    {RPMTAG_CONFLICTFLAGS,	0, 0, "conflicts"},
    {RPMTAG_OBSOLETEFLAGS,	0, 0, "obsoletes"},
    {RPMTAG_PREFIXES,		0, 0, "prefixes"},
    {RPMTAG_PREFIXES,		0, 0, "prefix"},
    {RPMTAG_BUILDROOT,		0, 0, "buildroot"},
    {RPMTAG_BUILDARCHS,		0, 0, "buildarchitectures"},
    {RPMTAG_BUILDARCHS,		0, 0, "buildarch"},
    {RPMTAG_BUILDCONFLICTS,	0, 0, "buildconflicts"},
    {RPMTAG_BUILDPREREQ,	0, 1, "buildprereq"},
    {RPMTAG_BUILDREQUIRES,	0, 1, "buildrequires"},
    {RPMTAG_AUTOREQPROV,	0, 0, "autoreqprov"},
    {RPMTAG_AUTOREQ,		0, 0, "autoreq"},
    {RPMTAG_AUTOPROV,		0, 0, "autoprov"},
    {RPMTAG_DOCDIR,		0, 0, "docdir"},
    /*@-nullassign@*/	/* LCL: can't add null annotation */
    {0, 0, 0, 0}
    /*@=nullassign@*/
};

/**
 */
static inline void initPreambleList(void)
{
    PreambleRec p;
    for (p = preambleList; p->token; p++)
	p->len = strlen(p->token);
}

/**
 */
static int findPreambleTag(Spec spec, /*@out@*/int * tag,
	/*@null@*/ /*@out@*/ const char ** macro, char *lang)
{
    char *s;
    PreambleRec p;

    if (preambleList[0].len == 0)
	initPreambleList();

    for (p = preambleList; p->token; p++) {
	if (!xstrncasecmp(spec->line, p->token, p->len))
	    break;
    }
    if (p->token == NULL)
	return 1;

    s = spec->line + p->len;
    SKIPSPACE(s);

    switch (p->multiLang) {
    default:
    case 0:
	/* Unless this is a source or a patch, a ':' better be next */
	if (p->tag != RPMTAG_SOURCE && p->tag != RPMTAG_PATCH) {
	    if (*s != ':') return 1;
	}
	*lang = '\0';
	break;
    case 1:	/* Parse optional ( <token> ). */
	if (*s == ':') {
	    strcpy(lang, RPMBUILD_DEFAULT_LANG);
	    break;
	}
	if (*s != '(') return 1;
	s++;
	SKIPSPACE(s);
	while (!xisspace(*s) && *s != ')')
	    *lang++ = *s++;
	*lang = '\0';
	SKIPSPACE(s);
	if (*s != ')') return 1;
	s++;
	SKIPSPACE(s);
	if (*s != ':') return 1;
	break;
    }

    *tag = p->tag;
    if (macro)
	/*@-onlytrans@*/	/* FIX: observer, but double indirection. */
	*macro = p->token;
	/*@=onlytrans@*/
    return 0;
}

int parsePreamble(Spec spec, int initialPackage)
{
    int nextPart;
    int tag, rc;
    char *name, *linep;
    int flag;
    Package pkg;
    char fullName[BUFSIZ];
    char lang[BUFSIZ];

    strcpy(fullName, "(main package)");

    pkg = newPackage(spec);
	
    if (! initialPackage) {
	/* There is one option to %package: <pkg> or -n <pkg> */
	if (parseSimplePart(spec->line, &name, &flag)) {
	    rpmError(RPMERR_BADSPEC, _("Bad package specification: %s\n"),
			spec->line);
	    return RPMERR_BADSPEC;
	}
	
	if (!lookupPackage(spec, name, flag, NULL)) {
	    rpmError(RPMERR_BADSPEC, _("Package already exists: %s\n"),
			spec->line);
	    return RPMERR_BADSPEC;
	}
	
	/* Construct the package */
	if (flag == PART_SUBNAME) {
	    const char * mainName;
	    (void) headerNVR(spec->packages->header, &mainName, NULL, NULL);
	    sprintf(fullName, "%s-%s", mainName, name);
	} else
	    strcpy(fullName, name);
	(void) headerAddEntry(pkg->header, RPMTAG_NAME, RPM_STRING_TYPE, fullName, 1);
    }

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc)
	    return rc;
	while (! (nextPart = isPart(spec->line))) {
	    const char * macro;
	    /* Skip blank lines */
	    linep = spec->line;
	    SKIPSPACE(linep);
	    if (*linep != '\0') {
		if (findPreambleTag(spec, &tag, &macro, lang)) {
		    rpmError(RPMERR_BADSPEC, _("line %d: Unknown tag: %s\n"),
				spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
		if (handlePreambleTag(spec, pkg, tag, macro, lang))
		    return RPMERR_BADSPEC;
		if (spec->buildArchitectures && !spec->inBuildArchitectures)
		    return PART_BUILDARCHITECTURES;
	    }
	    if ((rc =
		 readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc)
		return rc;
	}
    }

    /* Do some final processing on the header */
    
    if (!spec->gotBuildRootURL && spec->buildRootURL) {
	rpmError(RPMERR_BADSPEC, _("Spec file can't use BuildRoot\n"));
	return RPMERR_BADSPEC;
    }

    /* XXX Skip valid arch check if not building binary package */
    if (!spec->anyarch && checkForValidArchitectures(spec))
	return RPMERR_BADSPEC;

    if (pkg == spec->packages)
	fillOutMainPackage(pkg->header);

    if (checkForDuplicates(pkg->header, fullName))
	return RPMERR_BADSPEC;

    if (pkg != spec->packages)
	headerCopyTags(spec->packages->header, pkg->header, copyTagsDuringParse);

    if (checkForRequired(pkg->header, fullName))
	return RPMERR_BADSPEC;

    return nextPart;
}
