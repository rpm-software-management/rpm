/** \ingroup rpmbuild
 * \file build/parsePreamble.c
 *  Parse tags in global section from spec file.
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "debug.h"

#define SKIPSPACE(s) { while (*(s) && risspace(*(s))) (s)++; }
#define SKIPNONSPACE(s) { while (*(s) && !risspace(*(s))) (s)++; }

/**
 */
static const rpmTag copyTagsDuringParse[] = {
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
    RPMTAG_DISTTAG,
    RPMTAG_BUGURL,
    RPMTAG_GROUP,
    0
};

/**
 */
static const rpmTag requiredTags[] = {
    RPMTAG_NAME,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_SUMMARY,
    RPMTAG_LICENSE,
    0
};

/**
 */
static void addOrAppendListEntry(Header h, rpmTag tag, const char * line)
{
    int xx;
    int argc;
    const char **argv;

    xx = poptParseArgvString(line, &argc, &argv);
    if (argc) 
	headerPutStringArray(h, tag, argv, argc);
    argv = _free(argv);
}

/* Parse a simple part line that only take -n <pkg> or <pkg> */
/* <pkg> is returned in name as a pointer into a dynamic buffer */

/**
 */
static int parseSimplePart(const char *line, char **name, int *flag)
{
    char *tok;
    char *linebuf = xstrdup(line);
    int rc;

    /* Throw away the first token (the %xxxx) */
    (void)strtok(linebuf, " \t\n");
    *name = NULL;

    if (!(tok = strtok(NULL, " \t\n"))) {
	rc = 0;
	goto exit;
    }
    
    if (rstreq(tok, "-n")) {
	if (!(tok = strtok(NULL, " \t\n"))) {
	    rc = 1;
	    goto exit;
	}
	*flag = PART_NAME;
    } else {
	*flag = PART_SUBNAME;
    }
    *name = xstrdup(tok);
    rc = strtok(NULL, " \t\n") ? 1 : 0;

exit:
    free(linebuf);
    return rc;
}

/**
 */
static inline int parseYesNo(const char * s)
{
    return ((!s || (s[0] == 'n' || s[0] == 'N' || s[0] == '0') ||
	!rstrcasecmp(s, "false") || !rstrcasecmp(s, "off"))
	    ? 0 : 1);
}

typedef const struct tokenBits_s {
    const char * name;
    rpmsenseFlags bits;
} * tokenBits;

/**
 */
static struct tokenBits_s const installScriptBits[] = {
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
static const struct tokenBits_s const buildScriptBits[] = {
    { "prep",		RPMSENSE_SCRIPT_PREP },
    { "build",		RPMSENSE_SCRIPT_BUILD },
    { "install",	RPMSENSE_SCRIPT_INSTALL },
    { "clean",		RPMSENSE_SCRIPT_CLEAN },
    { NULL, 0 }
};

/**
 */
static int parseBits(const char * s, const tokenBits tokbits,
		rpmsenseFlags * bp)
{
    tokenBits tb;
    const char * se;
    rpmsenseFlags bits = RPMSENSE_ANY;
    int c = 0;

    if (s) {
	while (*s != '\0') {
	    while ((c = *s) && risspace(c)) s++;
	    se = s;
	    while ((c = *se) && risalpha(c)) se++;
	    if (s == se)
		break;
	    for (tb = tokbits; tb->name; tb++) {
		if (tb->name != NULL &&
		    strlen(tb->name) == (se-s) && rstreqn(tb->name, s, (se-s)))
		    break;
	    }
	    if (tb->name == NULL)
		break;
	    bits |= tb->bits;
	    while ((c = *se) && risspace(c)) se++;
	    if (c != ',')
		break;
	    s = ++se;
	}
    }
    if (c == 0 && bp) *bp = bits;
    return (c ? RPMRC_FAIL : RPMRC_OK);
}

/**
 */
static inline char * findLastChar(char * s)
{
    char *res = s;

    while (*s != '\0') {
	if (! risspace(*s))
	    res = s;
	s++;
    }

    return res;
}

/**
 */
static int isMemberInEntry(Header h, const char *name, rpmTag tag)
{
    struct rpmtd_s td;
    int found = 0;
    const char *str;

    if (!headerGet(h, tag, &td, HEADERGET_MINMEM))
	return -1;

    while ((str = rpmtdNextString(&td))) {
	if (!rstrcasecmp(str, name)) {
	    found = 1;
	    break;
	}
    }
    rpmtdFreeData(&td);

    return found;
}

/**
 */
static rpmRC checkForValidArchitectures(rpmSpec spec)
{
    char *arch = rpmExpand("%{_target_cpu}", NULL);
    char *os = rpmExpand("%{_target_os}", NULL);
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUDEARCH) == 1) {
	rpmlog(RPMLOG_ERR, _("Architecture is excluded: %s\n"), arch);
	goto exit;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			arch, RPMTAG_EXCLUSIVEARCH) == 0) {
	rpmlog(RPMLOG_ERR, _("Architecture is not included: %s\n"), arch);
	goto exit;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUDEOS) == 1) {
	rpmlog(RPMLOG_ERR, _("OS is excluded: %s\n"), os);
	goto exit;
    }
    if (isMemberInEntry(spec->buildRestrictions,
			os, RPMTAG_EXCLUSIVEOS) == 0) {
	rpmlog(RPMLOG_ERR, _("OS is not included: %s\n"), os);
	goto exit;
    }
    rc = RPMRC_OK;

exit:
    arch = _free(arch);
    os = _free(os);

    return rc;
}

/**
 * Check that required tags are present in header.
 * @param h		header
 * @param NVR		package name-version-release
 * @return		RPMRC_OK if OK
 */
static int checkForRequired(Header h, const char * NVR)
{
    int res = RPMRC_OK;
    const rpmTag * p;

    for (p = requiredTags; *p != 0; p++) {
	if (!headerIsEntry(h, *p)) {
	    rpmlog(RPMLOG_ERR,
			_("%s field must be present in package: %s\n"),
			rpmTagGetName(*p), NVR);
	    res = RPMRC_FAIL;
	}
    }

    return res;
}

/**
 * Check that no duplicate tags are present in header.
 * @param h		header
 * @param NVR		package name-version-release
 * @return		RPMRC_OK if OK
 */
static int checkForDuplicates(Header h, const char * NVR)
{
    int res = RPMRC_OK;
    rpmTag lastTag, tag;
    HeaderIterator hi;
    struct rpmtd_s td;
    
    for (hi = headerInitIterator(h), lastTag = 0;
	headerNext(hi, &td), tag = rpmtdTag(&td);
	lastTag = tag)
    {
	if (tag == lastTag) {
	    rpmlog(RPMLOG_ERR, _("Duplicate %s entries in package: %s\n"),
		     rpmTagGetName(tag), NVR);
	    res = RPMRC_FAIL;
	}
	rpmtdFreeData(&td);
    }
    hi = headerFreeIterator(hi);

    return res;
}

/**
 */
static struct optionalTag {
    rpmTag	ot_tag;
    const char * ot_mac;
} const optionalTags[] = {
    { RPMTAG_VENDOR,		"%{vendor}" },
    { RPMTAG_PACKAGER,		"%{packager}" },
    { RPMTAG_DISTRIBUTION,	"%{distribution}" },
    { RPMTAG_DISTURL,		"%{disturl}" },
    { RPMTAG_BUGURL,		"%{bugurl}" },
    { -1, NULL }
};

/**
 */
static void fillOutMainPackage(Header h)
{
    const struct optionalTag *ot;

    for (ot = optionalTags; ot->ot_mac != NULL; ot++) {
	if (!headerIsEntry(h, ot->ot_tag)) {
	    char *val = rpmExpand(ot->ot_mac, NULL);
	    if (val && *val != '%') {
		headerPutString(h, ot->ot_tag, val);
	    }
	    val = _free(val);
	}
    }
}

static int getSpecialDocDir(Package pkg)
{
    const char *errstr, *docdir_fmt = "%{NAME}-%{VERSION}";
    char *fmt_macro, *fmt; 
    int rc = -1;

    fmt_macro = rpmExpand("%{?_docdir_fmt}", NULL);
    if (fmt_macro && strlen(fmt_macro) > 0) {
	docdir_fmt = fmt_macro;
    }
    fmt = headerFormat(pkg->header, docdir_fmt, &errstr);
    if (!fmt) {
	rpmlog(RPMLOG_ERR, _("illegal _docdir_fmt: %s\n"), errstr);
	goto exit;
    }
    pkg->specialDocDir = rpmGetPath("%{_docdir}/", fmt, NULL);
    rc = 0;

exit:
    free(fmt);
    free(fmt_macro);
    return rc;
}

/**
 */
static rpmRC readIcon(Header h, const char * file)
{
    char *fn = NULL;
    uint8_t *icon;
    FD_t fd;
    rpmRC rc = RPMRC_OK;
    off_t size;
    size_t nb, iconsize;

    /* XXX use rpmGenPath(rootdir, "%{_sourcedir}/", file) for icon path. */
    fn = rpmGetPath("%{_sourcedir}/", file, NULL);

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open icon %s: %s\n"),
		fn, Fstrerror(fd));
	rc = RPMRC_FAIL;
	goto exit;
    }
    size = fdSize(fd);
    iconsize = (size >= 0 ? size : (8 * BUFSIZ));
    if (iconsize == 0) {
	(void) Fclose(fd);
	rc = RPMRC_OK;
	goto exit;
    }

    icon = xmalloc(iconsize + 1);
    *icon = '\0';

    nb = Fread(icon, sizeof(icon[0]), iconsize, fd);
    if (Ferror(fd) || (size >= 0 && nb != size)) {
	rpmlog(RPMLOG_ERR, _("Unable to read icon %s: %s\n"),
		fn, Fstrerror(fd));
	rc = RPMRC_FAIL;
    }
    (void) Fclose(fd);
    if (rc != RPMRC_OK)
	goto exit;

    if (rstreqn((char*)icon, "GIF", sizeof("GIF")-1)) {
	headerPutBin(h, RPMTAG_GIF, icon, iconsize);
    } else if (rstreqn((char*)icon, "/* XPM", sizeof("/* XPM")-1)) {
	headerPutBin(h, RPMTAG_XPM, icon, iconsize);
    } else {
	rpmlog(RPMLOG_ERR, _("Unknown icon type: %s\n"), file);
	rc = RPMRC_FAIL;
	goto exit;
    }
    icon = _free(icon);
    
exit:
    fn = _free(fn);
    return rc;
}

spectag stashSt(rpmSpec spec, Header h, rpmTag tag, const char * lang)
{
    spectag t = NULL;

    if (spec->st) {
	spectags st = spec->st;
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
	if (!(t->t_lang && !rstreq(t->t_lang, RPMBUILD_DEFAULT_LANG))) {
	    rasprintf(&t->t_msgid, "%s(%s)", 
		      headerGetString(h, RPMTAG_NAME), rpmTagGetName(tag));
	}
    }
    return t;
}

#define SINGLE_TOKEN_ONLY \
if (multiToken) { \
    rpmlog(RPMLOG_ERR, _("line %d: Tag takes single token only: %s\n"), \
	     spec->lineNum, spec->line); \
    return RPMRC_FAIL; \
}

extern int noLang;

/**
 * Check for inappropriate characters. All alphanums are considered sane.
 * @param spec		spec
 * @param field		string to check
 * @param fsize		size of string to check
 * @param whitelist	string of permitted characters
 * @return		RPMRC_OK if OK
 */
rpmRC rpmCharCheck(rpmSpec spec, char *field, size_t fsize, const char *whitelist)
{
    char *ch, *stop = &field[fsize];

    for (ch=field; *ch && ch < stop; ch++) {
	if (risalnum(*ch) || strchr(whitelist, *ch)) continue;
	if (isprint(*ch)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Illegal char '%c' in: %s\n"),
		spec->lineNum, *ch, spec->line);
	} else {
	    rpmlog(RPMLOG_ERR, _("line %d: Illegal char in: %s\n"),
		spec->lineNum, spec->line);
	}
	return RPMRC_FAIL;
    }
    if (strstr(field, "..") != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: Illegal sequence \"..\" in: %s\n"),
	    spec->lineNum, spec->line);
	return RPMRC_FAIL;
    }
    
    return RPMRC_OK;
}

/**
 */
static int handlePreambleTag(rpmSpec spec, Package pkg, rpmTag tag,
		const char *macro, const char *lang)
{
    char * field = spec->line;
    char * end;
    int multiToken = 0;
    rpmsenseFlags tagflags;
    int rc;
    int xx;
    
    if (field == NULL) return RPMRC_FAIL;	/* XXX can't happen */
    /* Find the start of the "field" and strip trailing space */
    while ((*field) && (*field != ':'))
	field++;
    if (*field != ':') {
	rpmlog(RPMLOG_ERR, _("line %d: Malformed tag: %s\n"),
		 spec->lineNum, spec->line);
	return RPMRC_FAIL;
    }
    field++;
    SKIPSPACE(field);
    if (!*field) {
	/* Empty field */
	rpmlog(RPMLOG_ERR, _("line %d: Empty tag: %s\n"),
		 spec->lineNum, spec->line);
	return RPMRC_FAIL;
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
	SINGLE_TOKEN_ONLY;
	if (rpmCharCheck(spec, field, strlen(field), ".-_+%{}") != RPMRC_OK) return RPMRC_FAIL;
	headerPutString(pkg->header, tag, field);
	break;
    case RPMTAG_VERSION:
    case RPMTAG_RELEASE:
	SINGLE_TOKEN_ONLY;
	if (rpmCharCheck(spec, field, strlen(field), "._+%{}") != RPMRC_OK) return RPMRC_FAIL;
	headerPutString(pkg->header, tag, field);
	break;
    case RPMTAG_URL:
    case RPMTAG_DISTTAG:
    case RPMTAG_BUGURL:
	SINGLE_TOKEN_ONLY;
	headerPutString(pkg->header, tag, field);
	break;
    case RPMTAG_GROUP:
    case RPMTAG_SUMMARY:
	(void) stashSt(spec, pkg->header, tag, lang);
    case RPMTAG_DISTRIBUTION:
    case RPMTAG_VENDOR:
    case RPMTAG_LICENSE:
    case RPMTAG_PACKAGER:
	if (!*lang) {
	    headerPutString(pkg->header, tag, field);
	} else if (!(noLang && !rstreq(lang, RPMBUILD_DEFAULT_LANG)))
	    (void) headerAddI18NString(pkg->header, tag, field, lang);
	break;
    case RPMTAG_BUILDROOT:
	/* just silently ignore BuildRoot */
	macro = NULL;
	break;
    case RPMTAG_PREFIXES: {
	struct rpmtd_s td;
	const char *str;
	addOrAppendListEntry(pkg->header, tag, field);
	xx = headerGet(pkg->header, tag, &td, HEADERGET_MINMEM);
	while ((str = rpmtdNextString(&td))) {
	    size_t len = strlen(str);
	    if (len > 1 && str[len-1] == '/') {
		rpmlog(RPMLOG_ERR,
			 _("line %d: Prefixes must not end with \"/\": %s\n"),
			 spec->lineNum, spec->line);
		rpmtdFreeData(&td);
		return RPMRC_FAIL;
	    }
	}
	rpmtdFreeData(&td);
	break;
    }
    case RPMTAG_DOCDIR:
	SINGLE_TOKEN_ONLY;
	if (field[0] != '/') {
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Docdir must begin with '/': %s\n"),
		     spec->lineNum, spec->line);
	    return RPMRC_FAIL;
	}
	macro = NULL;
	delMacro(NULL, "_docdir");
	addMacro(NULL, "_docdir", NULL, field, RMIL_SPEC);
	break;
    case RPMTAG_EPOCH: {
	SINGLE_TOKEN_ONLY;
	uint32_t epoch;
	if (parseUnsignedNum(field, &epoch)) {
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Epoch field must be an unsigned number: %s\n"),
		     spec->lineNum, spec->line);
	    return RPMRC_FAIL;
	}
	headerPutUint32(pkg->header, tag, &epoch, 1);
	break;
    }
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
	macro = NULL;
	if ((rc = addSource(spec, pkg, field, tag)))
	    return rc;
	break;
    case RPMTAG_ICON:
	SINGLE_TOKEN_ONLY;
	if ((rc = addSource(spec, pkg, field, tag)))
	    return rc;
	if ((rc = readIcon(pkg->header, field)))
	    return RPMRC_FAIL;
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
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Bad %s: qualifiers: %s\n"),
		     spec->lineNum, rpmTagGetName(tag), spec->line);
	    return rc;
	}
	if ((rc = parseRCPOT(spec, pkg, field, tag, 0, tagflags)))
	    return rc;
	break;
    case RPMTAG_REQUIREFLAGS:
    case RPMTAG_PREREQ:
	if ((rc = parseBits(lang, installScriptBits, &tagflags))) {
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Bad %s: qualifiers: %s\n"),
		     spec->lineNum, rpmTagGetName(tag), spec->line);
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
    case RPMTAG_BUILDARCHS: {
	int BACount;
	const char **BANames = NULL;
	if ((rc = poptParseArgvString(field, &BACount, &BANames))) {
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Bad BuildArchitecture format: %s\n"),
		     spec->lineNum, spec->line);
	    return RPMRC_FAIL;
	}
	if (spec->packages == pkg) {
	    spec->BACount = BACount;
	    spec->BANames = BANames;
	} else {
	    if (BACount != 1 || !rstreq(BANames[0], "noarch")) {
		rpmlog(RPMLOG_ERR,
			 _("line %d: Only noarch subpackages are supported: %s\n"),
			spec->lineNum, spec->line);
		BANames = _free(BANames);
		return RPMRC_FAIL;
	    }
	    headerPutString(pkg->header, RPMTAG_ARCH, "noarch");
	}
	if (!BACount)
	    spec->BANames = _free(spec->BANames);
	break;
    }
    default:
	rpmlog(RPMLOG_ERR, _("Internal error: Bogus tag %d\n"), tag);
	return RPMRC_FAIL;
    }

    if (macro)
	addMacro(spec->macros, macro, NULL, field, RMIL_SPEC);
    
    return RPMRC_OK;
}

/* This table has to be in a peculiar order.  If one tag is the */
/* same as another, plus a few letters, it must come first.     */

/**
 */
typedef const struct PreambleRec_s {
    rpmTag tag;
    int multiLang;
    int deprecated;
    size_t len;
    const char * token;
} * PreambleRec;

#define LEN_AND_STR(_tag) (sizeof(_tag)-1), _tag

static struct PreambleRec_s const preambleList[] = {
    {RPMTAG_NAME,		0, 0, LEN_AND_STR("name")},
    {RPMTAG_VERSION,		0, 0, LEN_AND_STR("version")},
    {RPMTAG_RELEASE,		0, 0, LEN_AND_STR("release")},
    {RPMTAG_EPOCH,		0, 0, LEN_AND_STR("epoch")},
    {RPMTAG_SUMMARY,		1, 0, LEN_AND_STR("summary")},
    {RPMTAG_LICENSE,		0, 0, LEN_AND_STR("license")},
    {RPMTAG_DISTRIBUTION,	0, 0, LEN_AND_STR("distribution")},
    {RPMTAG_DISTURL,		0, 0, LEN_AND_STR("disturl")},
    {RPMTAG_VENDOR,		0, 0, LEN_AND_STR("vendor")},
    {RPMTAG_GROUP,		1, 0, LEN_AND_STR("group")},
    {RPMTAG_PACKAGER,		0, 0, LEN_AND_STR("packager")},
    {RPMTAG_URL,		0, 0, LEN_AND_STR("url")},
    {RPMTAG_SOURCE,		0, 0, LEN_AND_STR("source")},
    {RPMTAG_PATCH,		0, 0, LEN_AND_STR("patch")},
    {RPMTAG_NOSOURCE,		0, 0, LEN_AND_STR("nosource")},
    {RPMTAG_NOPATCH,		0, 0, LEN_AND_STR("nopatch")},
    {RPMTAG_EXCLUDEARCH,	0, 0, LEN_AND_STR("excludearch")},
    {RPMTAG_EXCLUSIVEARCH,	0, 0, LEN_AND_STR("exclusivearch")},
    {RPMTAG_EXCLUDEOS,		0, 0, LEN_AND_STR("excludeos")},
    {RPMTAG_EXCLUSIVEOS,	0, 0, LEN_AND_STR("exclusiveos")},
    {RPMTAG_ICON,		0, 0, LEN_AND_STR("icon")},
    {RPMTAG_PROVIDEFLAGS,	0, 0, LEN_AND_STR("provides")},
    {RPMTAG_REQUIREFLAGS,	1, 0, LEN_AND_STR("requires")},
    {RPMTAG_PREREQ,		1, 1, LEN_AND_STR("prereq")},
    {RPMTAG_CONFLICTFLAGS,	0, 0, LEN_AND_STR("conflicts")},
    {RPMTAG_OBSOLETEFLAGS,	0, 0, LEN_AND_STR("obsoletes")},
    {RPMTAG_PREFIXES,		0, 0, LEN_AND_STR("prefixes")},
    {RPMTAG_PREFIXES,		0, 0, LEN_AND_STR("prefix")},
    {RPMTAG_BUILDROOT,		0, 0, LEN_AND_STR("buildroot")},
    {RPMTAG_BUILDARCHS,		0, 0, LEN_AND_STR("buildarchitectures")},
    {RPMTAG_BUILDARCHS,		0, 0, LEN_AND_STR("buildarch")},
    {RPMTAG_BUILDCONFLICTS,	0, 0, LEN_AND_STR("buildconflicts")},
    {RPMTAG_BUILDPREREQ,	1, 1, LEN_AND_STR("buildprereq")},
    {RPMTAG_BUILDREQUIRES,	1, 0, LEN_AND_STR("buildrequires")},
    {RPMTAG_AUTOREQPROV,	0, 0, LEN_AND_STR("autoreqprov")},
    {RPMTAG_AUTOREQ,		0, 0, LEN_AND_STR("autoreq")},
    {RPMTAG_AUTOPROV,		0, 0, LEN_AND_STR("autoprov")},
    {RPMTAG_DOCDIR,		0, 0, LEN_AND_STR("docdir")},
    {RPMTAG_DISTTAG,		0, 0, LEN_AND_STR("disttag")},
    {RPMTAG_BUGURL,		0, 0, LEN_AND_STR("bugurl")},
    {0, 0, 0, 0}
};

/**
 */
static int findPreambleTag(rpmSpec spec,rpmTag * tag,
		const char ** macro, char * lang)
{
    PreambleRec p;
    char *s;

    for (p = preambleList; p->token != NULL; p++) {
	if (!(p->token && !rstrncasecmp(spec->line, p->token, p->len)))
	    continue;
	if (p->deprecated) {
	    rpmlog(RPMLOG_WARNING, _("line %d: %s is deprecated: %s\n"),
			spec->lineNum, p->token, spec->line);
	}
	break;
    }
    if (p == NULL || p->token == NULL)
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
	while (!risspace(*s) && *s != ')')
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
	*macro = p->token;
    return 0;
}

int parsePreamble(rpmSpec spec, int initialPackage)
{
    int nextPart = PART_ERROR;
    int res = PART_ERROR; /* assume failure */
    int rc;
    char *name, *linep;
    int flag = 0;
    Package pkg;
    char *NVR = NULL;
    char lang[BUFSIZ];

    pkg = newPackage(spec);
	
    if (! initialPackage) {
	/* There is one option to %package: <pkg> or -n <pkg> */
	if (parseSimplePart(spec->line, &name, &flag)) {
	    rpmlog(RPMLOG_ERR, _("Bad package specification: %s\n"),
			spec->line);
	    goto exit;
	}
	
	if (!lookupPackage(spec, name, flag, NULL)) {
	    rpmlog(RPMLOG_ERR, _("Package already exists: %s\n"), spec->line);
	    free(name);
	    goto exit;
	}
	
	/* Construct the package */
	if (flag == PART_SUBNAME) {
	    rasprintf(&NVR, "%s-%s", 
		    headerGetString(spec->packages->header, RPMTAG_NAME), name);
	} else
	    NVR = xstrdup(name);
	free(name);
	headerPutString(pkg->header, RPMTAG_NAME, NVR);
    } else {
	NVR = xstrdup("(main package)");
    }

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else if (rc < 0) {
	goto exit;
    } else {
	while (! (nextPart = isPart(spec->line))) {
	    const char * macro;
	    rpmTag tag;

	    /* Skip blank lines */
	    linep = spec->line;
	    SKIPSPACE(linep);
	    if (*linep != '\0') {
		if (findPreambleTag(spec, &tag, &macro, lang)) {
		    rpmlog(RPMLOG_ERR, _("line %d: Unknown tag: %s\n"),
				spec->lineNum, spec->line);
		    goto exit;
		}
		if (handlePreambleTag(spec, pkg, tag, macro, lang)) {
		    goto exit;
		}
		if (spec->BANames && !spec->recursing) {
		    res = PART_BUILDARCHITECTURES;
		    goto exit;
		}
	    }
	    if ((rc =
		 readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		goto exit;
	    }
	}
    }

    /* 
     * Expand buildroot one more time to get %{version} and the like
     * from the main package, validate sanity. The spec->buildRoot could
     * still contain unexpanded macros but it cannot be empty or '/', and it
     * can't be messed with by anything spec does beyond this point.
     */
    if (initialPackage) {
	char *buildRoot = rpmGetPath(spec->buildRoot, NULL);
	if (*buildRoot == '\0') {
	    rpmlog(RPMLOG_ERR, _("%%{buildroot} couldn't be empty\n"));
	    goto exit;
	}
	if (rstreq(buildRoot, "/")) {
	    rpmlog(RPMLOG_ERR, _("%%{buildroot} can not be \"/\"\n"));
	    goto exit;
	}
	free(spec->buildRoot);
	spec->buildRoot = buildRoot;
	addMacro(spec->macros, "buildroot", NULL, spec->buildRoot, RMIL_SPEC);
    }

    /* XXX Skip valid arch check if not building binary package */
    if (!spec->anyarch && checkForValidArchitectures(spec)) {
	goto exit;
    }

    /* It is the main package */
    if (pkg == spec->packages) {
	fillOutMainPackage(pkg->header);
	/* Define group tag to something when group is undefined in main package*/
	if (!headerIsEntry(pkg->header, RPMTAG_GROUP)) {
	    headerPutString(pkg->header, RPMTAG_GROUP, "Unspecified");
	}
    }

    if (checkForDuplicates(pkg->header, NVR)) {
	goto exit;
    }

    if (pkg != spec->packages) {
	headerCopyTags(spec->packages->header, pkg->header,
			(rpmTag *)copyTagsDuringParse);
    }

    if (checkForRequired(pkg->header, NVR)) {
	goto exit;
    }

    if (getSpecialDocDir(pkg)) {
	goto exit;
    }
	
    /* if we get down here nextPart has been set to non-error */
    res = nextPart;

exit:
    free(NVR);
    return res;
}
