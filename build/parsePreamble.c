#include "system.h"

#include "rpmbuild.h"

#include "popt/popt.h"

static int copyTagsDuringParse[] = {
    RPMTAG_EPOCH,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_LICENSE,
    RPMTAG_PACKAGER,
    RPMTAG_DISTRIBUTION,
    RPMTAG_VENDOR,
    RPMTAG_ICON,
    RPMTAG_URL,
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    RPMTAG_PREFIXES,
    0
};

static int requiredTags[] = {
    RPMTAG_NAME,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_SUMMARY,
    RPMTAG_GROUP,
    RPMTAG_LICENSE,
#if 0	/* XXX You really ought to have these, but many people don't: */
    RPMTAG_PACKAGER,
    RPMTAG_DISTRIBUTION,
    RPMTAG_VENDOR,
#endif
    0
};

#ifdef DYING
static int handlePreambleTag(Spec spec, Package pkg, int tag, char *macro,
			     char *lang);
static void initPreambleList(void);
static int findPreambleTag(Spec spec, int *tag, char **macro, char *lang);
static int checkForRequired(Header h, char *name);
static int checkForDuplicates(Header h, char *name);
static void fillOutMainPackage(Header h);
static int checkForValidArchitectures(Spec spec);
static int isMemberInEntry(Header header, char *name, int tag);
static int readIcon(Header h, char *file);
#endif	/* DYING */
    
static void addOrAppendListEntry(Header h, int_32 tag, char *line)
{
    int argc;
    char **argv;

    poptParseArgvString(line, &argc, &argv);
    if (argc) {
	headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, argv, argc);
    }
    FREE(argv);
}

/* Parse a simple part line that only take -n <pkg> or <pkg> */
/* <pkg> is return in name as a pointer into a static buffer */

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
	if (!(tok = strtok(NULL, " \t\n"))) {
	    return 1;
	}
	*flag = PART_NAME;
    } else {
	*flag = PART_SUBNAME;
    }
    strcpy(buf, tok);
    *name = buf;

    return (strtok(NULL, " \t\n")) ? 1 : 0;
}

static int parseYesNo(char *s)
{
    if (!s || (s[0] == 'n' || s[0] == 'N') ||
	!strcasecmp(s, "false") ||
	!strcasecmp(s, "off") ||
	!strcmp(s, "0")) {
	return 0;
    }

    return 1;
}

static char *findLastChar(char *s)
{
    char *res = s;

    while (*s) {
	if (! isspace(*s)) {
	    res = s;
	}
	s++;
    }

    return res;
}

static int isMemberInEntry(Header header, char *name, int tag)
{
    char **names;
    int count;

    /*
     * XXX The strcasecmp below is necessary so the old (rpm < 2.90) style
     * XXX os-from-uname (e.g. "Linux") is compatible with the new
     * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
     */
    if (headerGetEntry(header, tag, NULL, (void **)&names, &count)) {
	while (count--) {
	    if (!strcasecmp(names[count], name)) {
		FREE(names);
		return 1;
	    }
	}
	FREE(names);
	return 0;
    }

    return -1;
}

static int checkForValidArchitectures(Spec spec)
{
    char *arch, *os;

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
    
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUDEARCH) == 1) {
	rpmError(RPMERR_BADSPEC, _("Architecture is excluded: %s"), arch);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUSIVEARCH) == 0) {
	rpmError(RPMERR_BADSPEC, _("Architecture is not included: %s"), arch);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUDEOS) == 1) {
	rpmError(RPMERR_BADSPEC, _("OS is excluded: %s"), os);
	return RPMERR_BADSPEC;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUSIVEOS) == 0) {
	rpmError(RPMERR_BADSPEC, _("OS is not included: %s"), os);
	return RPMERR_BADSPEC;
    }

    return 0;
}

const char *tagName(int tag)
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

static int checkForRequired(Header h, char *name)
{
    int res = 0;
    int *p;

    for (p = requiredTags; *p != 0; p++) {
	if (!headerIsEntry(h, *p)) {
	    rpmError(RPMERR_BADSPEC, _("%s field must be present in package: %s"),
		     tagName(*p), name);
	    res = 1;
	}
    }

    return res;
}

static int checkForDuplicates(Header h, char *name)
{
    int res = 0;
    int lastTag, tag;
    HeaderIterator hi;
    
#if 0	/* XXX harmless, but headerInitIterator() does this anyways */
    headerSort(h);
#endif
    hi = headerInitIterator(h);
    lastTag = 0;
    while (headerNextIterator(hi, &tag, NULL, NULL, NULL)) {
	if (tag == lastTag) {
	    rpmError(RPMERR_BADSPEC, _("Duplicate %s entries in package: %s"),
		     tagName(tag), name);
	    res = 1;
	}
	lastTag = tag;
    }
    headerFreeIterator(hi);

    return res;
}

static struct optionalTag {
    int		ot_tag;
    const char *ot_mac;
} optionalTags[] = {
    { RPMTAG_VENDOR,		"%{vendor}" },
    { RPMTAG_PACKAGER,		"%{packager}" },
    { RPMTAG_DISTRIBUTION,	"%{distribution}" },
    { -1, NULL }
};

static void fillOutMainPackage(Header h)
{
    struct optionalTag *ot;

    for (ot = optionalTags; ot->ot_mac != NULL; ot++) {
	if (!headerIsEntry(h, ot->ot_tag)) {
	    const char *val = rpmExpand(ot->ot_mac, NULL);
	    if (val && *val != '%')
		headerAddEntry(h, ot->ot_tag, RPM_STRING_TYPE, (void *)val, 1);
	    xfree(val);
	}
    }
}

static int readIcon(Header h, const char *file)
{
    const char *fn = NULL;
    char *icon;
    struct stat statbuf;
    FD_t fd;
    int rc;
    int nb;

    fn = rpmGetPath("%{_sourcedir}/", file, NULL);

    if (stat(fn, &statbuf)) {
	rpmError(RPMERR_BADSPEC, _("Unable to stat icon: %s"), fn);
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    icon = malloc(statbuf.st_size);
    *icon = '\0';
    fd = fdOpen(fn, O_RDONLY, 0);
    nb = fdRead(fd, icon, statbuf.st_size);
    fdClose(fd);
    if (nb != statbuf.st_size) {
	rpmError(RPMERR_BADSPEC, _("Unable to read icon: %s"), fn);
	rc = RPMERR_BADSPEC;
	goto exit;
    }

    if (! strncmp(icon, "GIF", 3)) {
	headerAddEntry(h, RPMTAG_GIF, RPM_BIN_TYPE, icon, statbuf.st_size);
    } else if (! strncmp(icon, "/* XPM", 6)) {
	headerAddEntry(h, RPMTAG_XPM, RPM_BIN_TYPE, icon, statbuf.st_size);
    } else {
	rpmError(RPMERR_BADSPEC, _("Unknown icon type: %s"), file);
	rc = RPMERR_BADSPEC;
	goto exit;
    }
    free(icon);
    rc = 0;
    
exit:
    FREE(fn);
    return rc;
}

struct spectag *
stashSt(Spec spec, Header h, int tag, const char *lang)
{
    struct spectags *st = spec->st;
    struct spectag *t = NULL;
    if (st) {
	if (st->st_ntags == st->st_nalloc) {
	    st->st_nalloc += 10;
	    st->st_t = realloc(st->st_t, st->st_nalloc * sizeof(*(st->st_t)));
	}
	t = st->st_t + st->st_ntags++;
	t->t_tag = tag;
	t->t_startx = spec->lineNum - 1;
	t->t_nlines = 1;
	t->t_lang = strdup(lang);
	t->t_msgid = NULL;
	if (!(t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG))) {
	    char *n;
	    if (headerGetEntry(h, RPMTAG_NAME, NULL, (void *) &n, NULL)) {
		char buf[1024];
		sprintf(buf, "%s(%s)", n, tagName(tag));
		t->t_msgid = strdup(buf);
	    }
	}
    }
    return t;
}

#define SINGLE_TOKEN_ONLY \
if (multiToken) { \
    rpmError(RPMERR_BADSPEC, _("line %d: Tag takes single token only: %s"), \
	     spec->lineNum, spec->line); \
    return RPMERR_BADSPEC; \
}

extern int noLang;	/* XXX FIXME: pass as arg */

static int handlePreambleTag(Spec spec, Package pkg, int tag, char *macro,
			     char *lang)
{
    char *field = spec->line;
    char *end;
    char **array;
    int multiToken = 0;
    int num, rc, len;
    
    /* Find the start of the "field" and strip trailing space */
    while ((*field) && (*field != ':')) {
	field++;
    }
    if (*field != ':') {
	rpmError(RPMERR_BADSPEC, _("line %d: Malformed tag: %s"),
		 spec->lineNum, spec->line);
	return RPMERR_BADSPEC;
    }
    field++;
    SKIPSPACE(field);
    if (! *field) {
	/* Empty field */
	rpmError(RPMERR_BADSPEC, _("line %d: Empty tag: %s"),
		 spec->lineNum, spec->line);
	return RPMERR_BADSPEC;
    }
    end = findLastChar(field);
    *(end+1) = '\0';

    /* See if this is multi-token */
    end = field;
    SKIPNONSPACE(end);
    if (*end) {
	multiToken = 1;
    }

    switch (tag) {
      case RPMTAG_NAME:
      case RPMTAG_VERSION:
      case RPMTAG_RELEASE:
      case RPMTAG_URL:
	SINGLE_TOKEN_ONLY;
	/* These are for backward compatibility */
	if (tag == RPMTAG_VERSION) {
	    if (strchr(field, '-') != NULL) {
		rpmError(RPMERR_BADSPEC, _("line %d: Illegal char '-' in %s: %s"),
		    spec->lineNum, "version", spec->line);
		return RPMERR_BADSPEC;
	    }
	    addMacro(spec->macros, "PACKAGE_VERSION", NULL, field, RMIL_OLDSPEC);
	} else if (tag == RPMTAG_RELEASE) {
	    if (strchr(field, '-') != NULL) {
		rpmError(RPMERR_BADSPEC, _("line %d: Illegal char '-' in %s: %s"),
		    spec->lineNum, "release", spec->line);
		return RPMERR_BADSPEC;
	    }
	    addMacro(spec->macros, "PACKAGE_RELEASE", NULL, field, RMIL_OLDSPEC-1);
	}
	headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, field, 1);
	break;
      case RPMTAG_GROUP:
      case RPMTAG_SUMMARY:
	stashSt(spec, pkg->header, tag, lang);
	/* fall thru */
      case RPMTAG_DISTRIBUTION:
      case RPMTAG_VENDOR:
      case RPMTAG_LICENSE:
      case RPMTAG_PACKAGER:
	if (! *lang) {
	    headerAddEntry(pkg->header, tag, RPM_STRING_TYPE, field, 1);
	} else if (!(noLang && strcmp(lang, RPMBUILD_DEFAULT_LANG))) {
	    headerAddI18NString(pkg->header, tag, field, lang);
	}
	break;
      case RPMTAG_BUILDROOT:
	SINGLE_TOKEN_ONLY;
	if (spec->buildRoot == NULL) {
	    const char *buildroot = rpmGetPath("%{buildroot}", NULL);
	    if (buildroot && *buildroot != '%') {
		spec->buildRoot = strdup(cleanFileName(buildroot));
	    } else {
		spec->buildRoot = strdup(cleanFileName(field));
	    }
	    xfree(buildroot);
	}
	if (!strcmp(spec->buildRoot, "/")) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: BuildRoot can not be \"/\": %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	spec->gotBuildRoot = 1;
	break;
      case RPMTAG_PREFIXES:
	addOrAppendListEntry(pkg->header, tag, field);
	headerGetEntry(pkg->header, tag, NULL, (void **)&array, &num);
	while (num--) {
	    len = strlen(array[num]);
	    if (array[num][len - 1] == '/' && len > 1) {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: Prefixes must not end with \"/\": %s"),
			 spec->lineNum, spec->line);
		FREE(array);
		return RPMERR_BADSPEC;
	    }
	}
	FREE(array);
	break;
      case RPMTAG_DOCDIR:
	SINGLE_TOKEN_ONLY;
	if (field[0] != '/') {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Docdir must begin with '/': %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	delMacro(&globalMacroContext, "_docdir");
	addMacro(&globalMacroContext, "_docdir", NULL, field, RMIL_SPEC);
	break;
      case RPMTAG_EPOCH:
	SINGLE_TOKEN_ONLY;
	if (parseNum(field, &num)) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Epoch/Serial field must be a number: %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	headerAddEntry(pkg->header, tag, RPM_INT32_TYPE, &num, 1);
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
	if ((rc = addSource(spec, pkg, field, tag))) {
	    return rc;
	}
	break;
      case RPMTAG_ICON:
	SINGLE_TOKEN_ONLY;
	if ((rc = addSource(spec, pkg, field, tag))) {
	    return rc;
	}
	if ((rc = readIcon(pkg->header, field))) {
	    return RPMERR_BADSPEC;
	}
	break;
      case RPMTAG_NOSOURCE:
      case RPMTAG_NOPATCH:
	spec->noSource = 1;
	if ((rc = parseNoSource(spec, field, tag))) {
	    return rc;
	}
	break;
      case RPMTAG_OBSOLETES:
      case RPMTAG_PROVIDES:
	if ((rc = parseProvidesObsoletes(spec, pkg, field, tag))) {
	    return rc;
	}
	break;
      case RPMTAG_BUILDPREREQ:
      case RPMTAG_REQUIREFLAGS:
      case RPMTAG_CONFLICTFLAGS:
      case RPMTAG_PREREQ:
	if ((rc = parseRequiresConflicts(spec, pkg, field, tag, 0))) {
	    return rc;
	}
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
		     _("line %d: Bad BuildArchitecture format: %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
	if (! spec->buildArchitectureCount) {
	    FREE(spec->buildArchitectures);
	}
	break;

      default:
	rpmError(RPMERR_INTERNAL, _("Internal error: Bogus tag %d"), tag);
	return RPMERR_INTERNAL;
    }

    if (macro) {
	addMacro(spec->macros, macro, NULL, field, RMIL_SPEC);
    }
    
    return 0;
}

/* This table has to be in a peculiar order.  If one tag is the */
/* same as another, plus a few letters, it must come first.     */

static struct PreambleRec {
    int tag;
    int len;
    int multiLang;
    char *token;
} preambleList[] = {
    {RPMTAG_NAME,		0, 0, "name"},
    {RPMTAG_VERSION,		0, 0, "version"},
    {RPMTAG_RELEASE,		0, 0, "release"},
    {RPMTAG_EPOCH,		0, 0, "epoch"},
    {RPMTAG_EPOCH,		0, 0, "serial"},
/*    {RPMTAG_DESCRIPTION,	0, 0, "description"}, */
    {RPMTAG_SUMMARY,		0, 1, "summary"},
    {RPMTAG_LICENSE,		0, 0, "copyright"},
    {RPMTAG_LICENSE,		0, 0, "license"},
    {RPMTAG_DISTRIBUTION,	0, 0, "distribution"},
    {RPMTAG_VENDOR,		0, 0, "vendor"},
    {RPMTAG_GROUP,		0, 1, "group"},
    {RPMTAG_PACKAGER,		0, 0, "packager"},
    {RPMTAG_URL,		0, 0, "url"},
/*    {RPMTAG_ROOT,		0, 0, "root"}, */
    {RPMTAG_SOURCE,		0, 0, "source"},
    {RPMTAG_PATCH,		0, 0, "patch"},
    {RPMTAG_NOSOURCE,		0, 0, "nosource"},
    {RPMTAG_NOPATCH,		0, 0, "nopatch"},
    {RPMTAG_EXCLUDEARCH,	0, 0, "excludearch"},
    {RPMTAG_EXCLUSIVEARCH,	0, 0, "exclusivearch"},
    {RPMTAG_EXCLUDEOS,		0, 0, "excludeos"},
    {RPMTAG_EXCLUSIVEOS,	0, 0, "exclusiveos"},
/*    {RPMTAG_EXCLUDE,		0, 0, "exclude"}, */
/*    {RPMTAG_EXCLUSIVE,	0, 0, "exclusive"}, */
    {RPMTAG_ICON,		0, 0, "icon"},
    {RPMTAG_PROVIDES,		0, 0, "provides"},
    {RPMTAG_REQUIREFLAGS,	0, 0, "requires"},
    {RPMTAG_PREREQ,		0, 0, "prereq"},
    {RPMTAG_CONFLICTFLAGS,	0, 0, "conflicts"},
    {RPMTAG_OBSOLETES,		0, 0, "obsoletes"},
    {RPMTAG_PREFIXES,		0, 0, "prefixes"},
    {RPMTAG_PREFIXES,		0, 0, "prefix"},
    {RPMTAG_BUILDROOT,		0, 0, "buildroot"},
    {RPMTAG_BUILDARCHS,		0, 0, "buildarchitectures"},
    {RPMTAG_BUILDARCHS,		0, 0, "buildarch"},
    {RPMTAG_BUILDPREREQ,	0, 0, "buildprereq"},
    {RPMTAG_AUTOREQPROV,	0, 0, "autoreqprov"},
    {RPMTAG_AUTOREQ,		0, 0, "autoreq"},
    {RPMTAG_AUTOPROV,		0, 0, "autoprov"},
    {RPMTAG_DOCDIR,		0, 0, "docdir"},
    {0, 0, 0, 0}
};

static void initPreambleList(void)
{
    struct PreambleRec *p = preambleList;

    while (p->token) {
	p->len = strlen(p->token);
	p++;
    }
}

static int findPreambleTag(Spec spec, /*@out@*/int *tag, /*@out@*/char **macro, char *lang)
{
    char *s;
    struct PreambleRec *p = preambleList;

    if (! p->len) {
	initPreambleList();
    }

    while (p->token && strncasecmp(spec->line, p->token, p->len)) {
	p++;
    }

    if (!p->token) {
	return 1;
    }

    s = spec->line + p->len;
    SKIPSPACE(s);

    if (! p->multiLang) {
	/* Unless this is a source or a patch, a ':' better be next */
	if (p->tag != RPMTAG_SOURCE && p->tag != RPMTAG_PATCH) {
	    if (*s != ':') {
		return 1;
	    }
	}
	*lang = '\0';
    } else {
	if (*s != ':') {
	    if (*s != '(') return 1;
	    s++;
	    SKIPSPACE(s);
	    while (! isspace(*s) && *s != ')') {
		*lang++ = *s++;
	    }
	    *lang = '\0';
	    SKIPSPACE(s);
	    if (*s != ')') return 1;
	    s++;
	    SKIPSPACE(s);
	    if (*s != ':') return 1;
	} else {
	    strcpy(lang, RPMBUILD_DEFAULT_LANG);
	}
    }

    *tag = p->tag;
    if (macro) {
	*macro = p->token;
    }
    return 0;
}

int parsePreamble(Spec spec, int initialPackage)
{
    int nextPart;
    int tag, rc;
    char *name, *mainName, *linep, *macro;
    int flag;
    Package pkg;
    char fullName[BUFSIZ];
    char lang[BUFSIZ];

    strcpy(fullName, "(main package)");

    pkg = newPackage(spec);
	
    if (! initialPackage) {
	/* There is one option to %package: <pkg> or -n <pkg> */
	if (parseSimplePart(spec->line, &name, &flag)) {
	    rpmError(RPMERR_BADSPEC, _("Bad package specification: %s"),
		     spec->line);
	    return RPMERR_BADSPEC;
	}
	
	if (!lookupPackage(spec, name, flag, NULL)) {
	    rpmError(RPMERR_BADSPEC, _("Package already exists: %s"), spec->line);
	    return RPMERR_BADSPEC;
	}
	
	/* Construct the package */
	if (flag == PART_SUBNAME) {
	    headerGetEntry(spec->packages->header, RPMTAG_NAME,
			   NULL, (void *) &mainName, NULL);
	    sprintf(fullName, "%s-%s", mainName, name);
	} else {
	    strcpy(fullName, name);
	}
	headerAddEntry(pkg->header, RPMTAG_NAME, RPM_STRING_TYPE, fullName, 1);
    }

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    return rc;
	}
	while (! (nextPart = isPart(spec->line))) {
	    /* Skip blank lines */
	    linep = spec->line;
	    SKIPSPACE(linep);
	    if (*linep) {
		if (findPreambleTag(spec, &tag, &macro, lang)) {
		    rpmError(RPMERR_BADSPEC, _("line %d: Unknown tag: %s"),
			     spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
		if (handlePreambleTag(spec, pkg, tag, macro, lang)) {
		    return RPMERR_BADSPEC;
		}
		if (spec->buildArchitectures && !spec->inBuildArchitectures) {
		    return PART_BUILDARCHITECTURES;
		}
	    }
	    if ((rc =
		 readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		return rc;
	    }
	}
    }

    /* Do some final processing on the header */
    
    if (!spec->gotBuildRoot && spec->buildRoot) {
	rpmError(RPMERR_BADSPEC, _("Spec file can't use BuildRoot"));
	return RPMERR_BADSPEC;
    }

    /* XXX Skip valid arch check if not building binary package */
    if (!spec->anyarch && checkForValidArchitectures(spec)) {
	    return RPMERR_BADSPEC;
    }

    if (pkg == spec->packages) {
	fillOutMainPackage(pkg->header);
    }

    if (checkForDuplicates(pkg->header, fullName)) {
	return RPMERR_BADSPEC;
    }

    if (pkg != spec->packages) {
	headerCopyTags(spec->packages->header, pkg->header, copyTagsDuringParse);
    }

    if (checkForRequired(pkg->header, fullName)) {
	return RPMERR_BADSPEC;
    }

    return nextPart;
}
