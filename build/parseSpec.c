/** \ingroup rpmbuild
 * \file build/parseSpec.c
 *  Top level dispatcher for spec file parsing.
 */

#include "system.h"

#include <errno.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* RPM_MACHTABLE & related */
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"
#include "debug.h"

#define SKIPSPACE(s) { while (*(s) && risspace(*(s))) (s)++; }
#define SKIPNONSPACE(s) { while (*(s) && !risspace(*(s))) (s)++; }
#define ISMACRO(s,m) (rstreqn((s), (m), sizeof((m))-1) && !risalpha((s)[sizeof((m))-1]))
#define ISMACROWITHARG(s,m) (rstreqn((s), (m), sizeof((m))-1) && (risblank((s)[sizeof((m))-1]) || !(s)[sizeof((m))-1]))

#define LEN_AND_STR(_tag) (sizeof(_tag)-1), (_tag)

typedef struct OpenFileInfo {
    char * fileName;
    FILE *fp;
    int lineNum;
    char readBuf[BUFSIZ];
    const char * readPtr;
    struct OpenFileInfo * next;
} OFI_t;

static const struct PartRec {
    int part;
    size_t len;
    const char * token;
} partList[] = {
    { PART_PREAMBLE,      LEN_AND_STR("%package")},
    { PART_PREP,          LEN_AND_STR("%prep")},
    { PART_BUILD,         LEN_AND_STR("%build")},
    { PART_INSTALL,       LEN_AND_STR("%install")},
    { PART_CHECK,         LEN_AND_STR("%check")},
    { PART_CLEAN,         LEN_AND_STR("%clean")},
    { PART_PREUN,         LEN_AND_STR("%preun")},
    { PART_POSTUN,        LEN_AND_STR("%postun")},
    { PART_PRETRANS,      LEN_AND_STR("%pretrans")},
    { PART_POSTTRANS,     LEN_AND_STR("%posttrans")},
    { PART_PRE,           LEN_AND_STR("%pre")},
    { PART_POST,          LEN_AND_STR("%post")},
    { PART_FILES,         LEN_AND_STR("%files")},
    { PART_CHANGELOG,     LEN_AND_STR("%changelog")},
    { PART_DESCRIPTION,   LEN_AND_STR("%description")},
    { PART_TRIGGERPOSTUN, LEN_AND_STR("%triggerpostun")},
    { PART_TRIGGERPREIN,  LEN_AND_STR("%triggerprein")},
    { PART_TRIGGERUN,     LEN_AND_STR("%triggerun")},
    { PART_TRIGGERIN,     LEN_AND_STR("%triggerin")},
    { PART_TRIGGERIN,     LEN_AND_STR("%trigger")},
    { PART_VERIFYSCRIPT,  LEN_AND_STR("%verifyscript")},
    { PART_POLICIES,      LEN_AND_STR("%sepolicy")},
    { PART_FILETRIGGERIN,	    LEN_AND_STR("%filetriggerin")},
    { PART_FILETRIGGERIN,	    LEN_AND_STR("%filetrigger")},
    { PART_FILETRIGGERUN,	    LEN_AND_STR("%filetriggerun")},
    { PART_FILETRIGGERPOSTUN,	    LEN_AND_STR("%filetriggerpostun")},
    { PART_TRANSFILETRIGGERIN,	    LEN_AND_STR("%transfiletriggerin")},
    { PART_TRANSFILETRIGGERIN,	    LEN_AND_STR("%transfiletrigger")},
    { PART_TRANSFILETRIGGERUN,	    LEN_AND_STR("%transfiletriggerun")},
    { PART_TRANSFILETRIGGERUN,	    LEN_AND_STR("%transfiletriggerun")},
    { PART_TRANSFILETRIGGERPOSTUN,  LEN_AND_STR("%transfiletriggerpostun")},
    {0, 0, 0}
};

int isPart(const char *line)
{
    const struct PartRec *p;

    for (p = partList; p->token != NULL; p++) {
	char c;
	if (rstrncasecmp(line, p->token, p->len))
	    continue;
	c = *(line + p->len);
	if (c == '\0' || risspace(c))
	    break;
    }

    return (p->token ? p->part : PART_NONE);
}

/**
 */
static int matchTok(const char *token, const char *line)
{
    const char *b, *be = line;
    size_t toklen = strlen(token);
    int rc = 0;

    while ( *(b = be) != '\0' ) {
	SKIPSPACE(b);
	be = b;
	SKIPNONSPACE(be);
	if (be == b)
	    break;
	if (toklen != (be-b) || rstrncasecmp(token, b, (be-b)))
	    continue;
	rc = 1;
	break;
    }

    return rc;
}

int handleComments(char *s)
{
    SKIPSPACE(s);
    if (*s == '#') {
	*s = '\0';
	return 1;
    }
    return 0;
}

/* Push a file to spec's file stack, return the newly pushed entry */
static OFI_t * pushOFI(rpmSpec spec, const char *fn)
{
    OFI_t *ofi = xcalloc(1, sizeof(*ofi));

    ofi->fp = NULL;
    ofi->fileName = xstrdup(fn);
    ofi->lineNum = 0;
    ofi->readBuf[0] = '\0';
    ofi->readPtr = NULL;
    ofi->next = spec->fileStack;

    spec->fileStack = ofi;
    return spec->fileStack;
}

/* Pop from spec's file stack */
static OFI_t * popOFI(rpmSpec spec)
{
    if (spec->fileStack) {
	OFI_t * ofi = spec->fileStack;

	spec->fileStack = ofi->next;
	if (ofi->fp)
	    fclose(ofi->fp);
	free(ofi->fileName);
	free(ofi);
    }
    return spec->fileStack;
}

static int restoreFirstChar(rpmSpec spec)
{
    /* Restore 1st char in (possible) next line */
    if (spec->nextline != NULL && spec->nextpeekc != '\0') {
	*spec->nextline = spec->nextpeekc;
	spec->nextpeekc = '\0';
	return 1;
    }
    return 0;
}

static int expandMacrosInSpecBuf(rpmSpec spec, int strip)
{
    char *lbuf = NULL;
    int rc = 0, isComment = 0;

     /* Don't expand macros (eg. %define) in false branch of %if clause */
    if (!spec->readStack->reading)
	return 0;

    lbuf = spec->lbuf;
    SKIPSPACE(lbuf);
    if (lbuf[0] == '#')
	isComment = 1;

    lbuf = xstrdup(spec->lbuf);

    rc = expandMacros(spec, spec->macros, spec->lbuf, spec->lbufSize);
    if (rc) {
	rpmlog(RPMLOG_ERR, _("line %d: %s\n"),
		spec->lineNum, spec->lbuf);
	goto exit;
    }

    if (strip & STRIP_COMMENTS && isComment) {
	char *bufA = lbuf;
	char *bufB = spec->lbuf;

	while (*bufA != '\0' && *bufB != '\0') {
	    if (*bufA == '%' && *(bufA + 1) == '%')
		bufA++;

	    if (*bufA != *bufB)
		break;

	    bufA++;
	    bufB++;
	}

	if (*bufA != '\0' || *bufB != '\0')
	    rpmlog(RPMLOG_WARNING,
		_("Macro expanded in comment on line %d: %s\n"),
		spec->lineNum, lbuf);
    }

exit:
    free(lbuf);

    return rc;
}

/* Return zero on success, 1 if we need to read more and -1 on errors. */
static int copyNextLineFromOFI(rpmSpec spec, OFI_t *ofi, int strip)
{
    /* Expand next line from file into line buffer */
    if (!(spec->nextline && *spec->nextline)) {
	int pc = 0, bc = 0, nc = 0;
	const char *from = ofi->readPtr;
	char ch = ' ';
	while (from && *from && ch != '\n') {
	    ch = spec->lbuf[spec->lbufOff] = *from;
	    spec->lbufOff++; from++;

	    if (spec->lbufOff >= spec->lbufSize) {
		spec->lbufSize += BUFSIZ;
		spec->lbuf = realloc(spec->lbuf, spec->lbufSize);
	    }
	}
	spec->lbuf[spec->lbufOff] = '\0';
	ofi->readPtr = from;

	/* Check if we need another line before expanding the buffer. */
	for (const char *p = spec->lbuf; *p; p++) {
	    switch (*p) {
		case '\\':
		    switch (*(p+1)) {
			case '\n': p++, nc = 1; break;
			case '\0': break;
			default: p++; break;
		    }
		    break;
		case '\n': nc = 0; break;
		case '%':
		    switch (*(p+1)) {
			case '{': p++, bc++; break;
			case '(': p++, pc++; break;
			case '%': p++; break;
		    }
		    break;
		case '{': if (bc > 0) bc++; break;
		case '}': if (bc > 0) bc--; break;
		case '(': if (pc > 0) pc++; break;
		case ')': if (pc > 0) pc--; break;
	    }
	}
	
	/* If it doesn't, ask for one more line. */
	if (pc || bc || nc ) {
	    spec->nextline = "";
	    return 1;
	}
	spec->lbufOff = 0;

	if (expandMacrosInSpecBuf(spec, strip))
	    return -1;

	spec->nextline = spec->lbuf;
    }
    return 0;
}

static void copyNextLineFinish(rpmSpec spec, int strip)
{
    char *last;
    char ch;

    /* Find next line in expanded line buffer */
    spec->line = last = spec->nextline;
    ch = ' ';
    while (*spec->nextline && ch != '\n') {
	ch = *spec->nextline++;
	if (!risspace(ch))
	    last = spec->nextline;
    }

    /* Save 1st char of next line in order to terminate current line. */
    if (*spec->nextline != '\0') {
	spec->nextpeekc = *spec->nextline;
	*spec->nextline = '\0';
    }
    
    if (strip & STRIP_COMMENTS)
	handleComments(spec->line);
    
    if (strip & STRIP_TRAILINGSPACE)
	*last = '\0';
}

static int readLineFromOFI(rpmSpec spec, OFI_t *ofi)
{
retry:
    /* Make sure the current file is open */
    if (ofi->fp == NULL) {
	ofi->fp = fopen(ofi->fileName, "r");
	if (ofi->fp == NULL) {
	    rpmlog(RPMLOG_ERR, _("Unable to open %s: %s\n"),
		     ofi->fileName, strerror(errno));
	    return PART_ERROR;
	}
	spec->lineNum = ofi->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!(ofi->readPtr && *(ofi->readPtr))) {
	if (!fgets(ofi->readBuf, BUFSIZ, ofi->fp)) {
	    /* EOF, remove this file from the stack */
	    ofi = popOFI(spec);

	    /* only on last file do we signal EOF to caller */
	    if (ofi == NULL)
		return 1;

	    /* otherwise, go back and try the read again. */
	    goto retry;
	}
	ofi->readPtr = ofi->readBuf;
	ofi->lineNum++;
	spec->lineNum = ofi->lineNum;
    }
    return 0;
}

#define ARGMATCH(s,token,match) \
do { \
    char *os = s; \
    char *exp = rpmExpand(token, NULL); \
    while(*s && !risblank(*s)) s++; \
    while(*s && risblank(*s)) s++; \
    if (!*s) { \
	rpmlog(RPMLOG_ERR, _("%s:%d: Argument expected for %s\n"), ofi->fileName, ofi->lineNum, os); \
	free(exp); \
	return PART_ERROR; \
    } \
    match = matchTok(exp, s); \
    free(exp); \
} while (0)


int readLine(rpmSpec spec, int strip)
{
    char *s;
    int match;
    struct ReadLevelEntry *rl;
    OFI_t *ofi = spec->fileStack;
    int rc;
    int startLine = 0;

    if (!restoreFirstChar(spec)) {
    retry:
	if ((rc = readLineFromOFI(spec, ofi)) != 0) {
	    if (spec->readStack->next) {
		rpmlog(RPMLOG_ERR, _("line %d: Unclosed %%if\n"),
			spec->readStack->lineNum);
		rc = PART_ERROR;
	    } else if (startLine > 0) {
		rpmlog(RPMLOG_ERR,
		    _("line %d: unclosed macro or bad line continuation\n"),
		    startLine);
		rc = PART_ERROR;
	    }
	    return rc;
	}
	ofi = spec->fileStack;

	/* Copy next file line into the spec line buffer */
	rc = copyNextLineFromOFI(spec, ofi, strip);
	if (rc > 0) {
	    if (startLine == 0)
		startLine = spec->lineNum;
	    goto retry;
	} else if (rc < 0) {
	    return PART_ERROR;
	}
    }

    copyNextLineFinish(spec, strip);

    s = spec->line;
    SKIPSPACE(s);

    match = -1;
    if (!spec->readStack->reading && ISMACROWITHARG(s, "%if")) {
	match = 0;
    } else if (ISMACROWITHARG(s, "%ifarch")) {
	ARGMATCH(s, "%{_target_cpu}", match);
    } else if (ISMACROWITHARG(s, "%ifnarch")) {
	ARGMATCH(s, "%{_target_cpu}", match);
	match = !match;
    } else if (ISMACROWITHARG(s, "%ifos")) {
	ARGMATCH(s, "%{_target_os}", match);
    } else if (ISMACROWITHARG(s, "%ifnos")) {
	ARGMATCH(s, "%{_target_os}", match);
	match = !match;
    } else if (ISMACROWITHARG(s, "%if")) {
	s += 3;
        match = parseExpressionBoolean(s);
	if (match < 0) {
	    rpmlog(RPMLOG_ERR,
			_("%s:%d: bad %%if condition\n"),
			ofi->fileName, ofi->lineNum);
	    return PART_ERROR;
	}
    } else if (ISMACRO(s, "%else")) {
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmlog(RPMLOG_ERR,
			_("%s:%d: Got a %%else with no %%if\n"),
			ofi->fileName, ofi->lineNum);
	    return PART_ERROR;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (ISMACRO(s, "%endif")) {
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmlog(RPMLOG_ERR,
			_("%s:%d: Got a %%endif with no %%if\n"),
			ofi->fileName, ofi->lineNum);
	    return PART_ERROR;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
    } else if (spec->readStack->reading && ISMACROWITHARG(s, "%include")) {
	char *fileName, *endFileName, *p;

	fileName = s+8;
	SKIPSPACE(fileName);
	endFileName = fileName;
	SKIPNONSPACE(endFileName);
	p = endFileName;
	SKIPSPACE(p);
	if (*fileName == '\0' || *p != '\0') {
	    rpmlog(RPMLOG_ERR, _("%s:%d: malformed %%include statement\n"),
				ofi->fileName, ofi->lineNum);
	    return PART_ERROR;
	}
	*endFileName = '\0';

	ofi = pushOFI(spec, fileName);
	goto retry;
    }

    if (match != -1) {
	rl = xmalloc(sizeof(*rl));
	rl->reading = spec->readStack->reading && match;
	rl->next = spec->readStack;
	rl->lineNum = ofi->lineNum;
	spec->readStack = rl;
	spec->line[0] = '\0';
    }

    if (! spec->readStack->reading) {
	spec->line[0] = '\0';
    }

    /* Collect parsed line */
    if (spec->parsed == NULL)
	spec->parsed = newStringBuf();
    appendStringBufAux(spec->parsed, spec->line,(strip & STRIP_TRAILINGSPACE));

    /* FIX: spec->readStack->next should be dependent */
    return 0;
}

void closeSpec(rpmSpec spec)
{
    while (popOFI(spec)) {};
}

static const rpmTagVal sourceTags[] = {
    RPMTAG_NAME,
    RPMTAG_VERSION,
    RPMTAG_RELEASE,
    RPMTAG_EPOCH,
    RPMTAG_SUMMARY,
    RPMTAG_DESCRIPTION,
    RPMTAG_PACKAGER,
    RPMTAG_DISTRIBUTION,
    RPMTAG_DISTURL,
    RPMTAG_VENDOR,
    RPMTAG_LICENSE,
    RPMTAG_GROUP,
    RPMTAG_OS,
    RPMTAG_ARCH,
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    RPMTAG_URL,
    RPMTAG_BUGURL,
    RPMTAG_HEADERI18NTABLE,
    RPMTAG_VCS,
    0
};

static void initSourceHeader(rpmSpec spec)
{
    Package sourcePkg = spec->sourcePackage;
    struct Source *srcPtr;

    if (headerIsEntry(sourcePkg->header, RPMTAG_NAME))
	return;

    /* Only specific tags are added to the source package header */
    headerCopyTags(spec->packages->header, sourcePkg->header, sourceTags);

    /* Add the build restrictions */
    for (int i=0; i<PACKAGE_NUM_DEPS; i++) {
	rpmdsPutToHeader(sourcePkg->dependencies[i], sourcePkg->header);
    }

    {
	HeaderIterator hi = headerInitIterator(spec->buildRestrictions);
	struct rpmtd_s td;
	while (headerNext(hi, &td)) {
	    if (rpmtdCount(&td) > 0) {
		(void) headerPut(sourcePkg->header, &td, HEADERPUT_DEFAULT);
	    }
	    rpmtdFreeData(&td);
	}
	headerFreeIterator(hi);
    }

    if (spec->BANames && spec->BACount > 0) {
	headerPutStringArray(sourcePkg->header, RPMTAG_BUILDARCHS,
		  spec->BANames, spec->BACount);
    }

    /* Add tags for sources and patches */
    for (srcPtr = spec->sources; srcPtr != NULL; srcPtr = srcPtr->next) {
	if (srcPtr->flags & RPMBUILD_ISSOURCE) {
	    headerPutString(sourcePkg->header, RPMTAG_SOURCE, srcPtr->source);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerPutUint32(sourcePkg->header, RPMTAG_NOSOURCE,
				&srcPtr->num, 1);
	    }
	}
	if (srcPtr->flags & RPMBUILD_ISPATCH) {
	    headerPutString(sourcePkg->header, RPMTAG_PATCH, srcPtr->source);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerPutUint32(sourcePkg->header, RPMTAG_NOPATCH,
				&srcPtr->num, 1);
	    }
	}
    }
}

/* Add extra provides to package.  */
static void addPackageProvides(Package pkg)
{
    const char *arch, *name;
    char *evr, *isaprov;
    rpmsenseFlags pflags = RPMSENSE_EQUAL;

    /* <name> = <evr> provide */
    name = headerGetString(pkg->header, RPMTAG_NAME);
    arch = headerGetString(pkg->header, RPMTAG_ARCH);
    evr = headerGetAsString(pkg->header, RPMTAG_EVR);
    addReqProv(pkg, RPMTAG_PROVIDENAME, name, evr, pflags, 0);

    /*
     * <name>(<isa>) = <evr> provide
     * FIXME: noarch needs special casing for now as BuildArch: noarch doesn't
     * cause reading in the noarch macros :-/ 
     */
    isaprov = rpmExpand(name, "%{?_isa}", NULL);
    if (!rstreq(arch, "noarch") && !rstreq(name, isaprov)) {
	addReqProv(pkg, RPMTAG_PROVIDENAME, isaprov, evr, pflags, 0);
    }
    free(isaprov);
    free(evr);
}

static void addTargets(Package Pkgs)
{
    char *platform = rpmExpand("%{_target_platform}", NULL);
    char *arch = rpmExpand("%{_target_cpu}", NULL);
    char *os = rpmExpand("%{_target_os}", NULL);
    char *optflags = rpmExpand("%{optflags}", NULL);

    for (Package pkg = Pkgs; pkg != NULL; pkg = pkg->next) {
	headerPutString(pkg->header, RPMTAG_OS, os);
	/* noarch subpackages already have arch set here, leave it alone */
	if (!headerIsEntry(pkg->header, RPMTAG_ARCH)) {
	    headerPutString(pkg->header, RPMTAG_ARCH, arch);
	}
	headerPutString(pkg->header, RPMTAG_PLATFORM, platform);
	headerPutString(pkg->header, RPMTAG_OPTFLAGS, optflags);

	/* Add manual dependencies early for rpmspec etc to look at */
	addPackageProvides(pkg);
	for (int i=0; i<PACKAGE_NUM_DEPS; i++) {
	    rpmdsPutToHeader(pkg->dependencies[i], pkg->header);
	}

	pkg->ds = rpmdsThis(pkg->header, RPMTAG_REQUIRENAME, RPMSENSE_EQUAL);
    }
    free(platform);
    free(arch);
    free(os);
    free(optflags);
}

rpmRC checkForEncoding(Header h, int addtag)
{
    rpmRC rc = RPMRC_OK;
#if HAVE_ICONV
    const char *encoding = "utf-8";
    rpmTagVal tag;
    iconv_t ic = (iconv_t) -1;
    char *dest = NULL;
    size_t destlen = 0;
    int strict = rpmExpandNumeric("%{_invalid_encoding_terminates_build}");
    HeaderIterator hi = headerInitIterator(h);

    ic = iconv_open(encoding, encoding);
    if (ic == (iconv_t) -1) {
	rpmlog(RPMLOG_WARNING,
		_("encoding %s not supported by system\n"), encoding);
	goto exit;
    }

    while ((tag = headerNextTag(hi)) != RPMTAG_NOT_FOUND) {
	struct rpmtd_s td;
	const char *src = NULL;

	if (rpmTagGetClass(tag) != RPM_STRING_CLASS)
	    continue;

	headerGet(h, tag, &td, (HEADERGET_RAW|HEADERGET_MINMEM));
	while ((src = rpmtdNextString(&td)) != NULL) {
	    size_t srclen = strlen(src);
	    size_t outlen, inlen = srclen;
	    char *out, *in = (char *) src;

	    if (destlen < srclen) {
		destlen = srclen * 2;
		dest = xrealloc(dest, destlen);
	    }
	    out = dest;
	    outlen = destlen;

	    /* reset conversion state */
	    iconv(ic, NULL, &inlen, &out, &outlen);

	    if (iconv(ic, &in, &inlen, &out, &outlen) == (size_t) -1) {
		rpmlog(strict ? RPMLOG_ERR : RPMLOG_WARNING,
			_("Package %s: invalid %s encoding in %s: %s - %s\n"),
			headerGetString(h, RPMTAG_NAME),
			encoding, rpmTagGetName(tag), src, strerror(errno));
		rc = RPMRC_FAIL;
	    }

	}
	rpmtdFreeData(&td);
    }

    /* Stomp "known good utf" mark in header if requested */
    if (rc == RPMRC_OK && addtag)
	headerPutString(h, RPMTAG_ENCODING, encoding);
    if (!strict)
	rc = RPMRC_OK;

exit:
    if (ic != (iconv_t) -1)
	iconv_close(ic);
    headerFreeIterator(hi);
    free(dest);
#endif /* HAVE_ICONV */

    return rc;
}

static rpmSpec parseSpec(const char *specFile, rpmSpecFlags flags,
			 const char *buildRoot, int recursing)
{
    int parsePart = PART_PREAMBLE;
    int initialPackage = 1;
    rpmSpec spec;
    
    /* Set up a new Spec structure with no packages. */
    spec = newSpec();

    spec->specFile = rpmGetPath(specFile, NULL);
    pushOFI(spec, spec->specFile);
    /* If buildRoot not specified, use default %{buildroot} */
    if (buildRoot) {
	spec->buildRoot = xstrdup(buildRoot);
    } else {
	spec->buildRoot = rpmGetPath("%{?buildroot:%{buildroot}}", NULL);
    }
    addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
    addMacro(NULL, "_licensedir", NULL, "%{_defaultlicensedir}", RMIL_SPEC);
    spec->recursing = recursing;
    spec->flags = flags;

    /* All the parse*() functions expect to have a line pre-read */
    /* in the spec's line buffer.  Except for parsePreamble(),   */
    /* which handles the initial entry into a spec file.         */
    
    while (parsePart != PART_NONE) {
	int goterror = 0;
	switch (parsePart) {
	case PART_ERROR: /* fallthrough */
	default:
	    goterror = 1;
	    break;
	case PART_PREAMBLE:
	    parsePart = parsePreamble(spec, initialPackage);
	    initialPackage = 0;
	    break;
	case PART_PREP:
	    parsePart = parsePrep(spec);
	    break;
	case PART_BUILD:
	case PART_INSTALL:
	case PART_CHECK:
	case PART_CLEAN:
	    parsePart = parseBuildInstallClean(spec, parsePart);
	    break;
	case PART_CHANGELOG:
	    parsePart = parseChangelog(spec);
	    break;
	case PART_DESCRIPTION:
	    parsePart = parseDescription(spec);
	    break;

	case PART_PRE:
	case PART_POST:
	case PART_PREUN:
	case PART_POSTUN:
	case PART_PRETRANS:
	case PART_POSTTRANS:
	case PART_VERIFYSCRIPT:
	case PART_TRIGGERPREIN:
	case PART_TRIGGERIN:
	case PART_TRIGGERUN:
	case PART_TRIGGERPOSTUN:
	case PART_FILETRIGGERIN:
	case PART_FILETRIGGERUN:
	case PART_FILETRIGGERPOSTUN:
	case PART_TRANSFILETRIGGERIN:
	case PART_TRANSFILETRIGGERUN:
	case PART_TRANSFILETRIGGERPOSTUN:
	    parsePart = parseScript(spec, parsePart);
	    break;

	case PART_FILES:
	    parsePart = parseFiles(spec);
	    break;

	case PART_POLICIES:
	    parsePart = parsePolicies(spec);
	    break;

	case PART_NONE:		/* XXX avoid gcc whining */
	case PART_LAST:
	case PART_BUILDARCHITECTURES:
	    break;
	}

	if (goterror || parsePart >= PART_LAST) {
	    goto errxit;
	}

	if (parsePart == PART_BUILDARCHITECTURES) {
	    int index;
	    int x;

	    closeSpec(spec);

	    spec->BASpecs = xcalloc(spec->BACount, sizeof(*spec->BASpecs));
	    index = 0;
	    if (spec->BANames != NULL)
	    for (x = 0; x < spec->BACount; x++) {

		/* Skip if not arch is not compatible. */
		if (!rpmMachineScore(RPM_MACHTABLE_BUILDARCH, spec->BANames[x]))
		    continue;
		addMacro(NULL, "_target_cpu", NULL, spec->BANames[x], RMIL_RPMRC);
		spec->BASpecs[index] = parseSpec(specFile, flags, buildRoot, 1);
		if (spec->BASpecs[index] == NULL) {
			spec->BACount = index;
			goto errxit;
		}
		delMacro(NULL, "_target_cpu");
		index++;
	    }

	    spec->BACount = index;
	    if (! index) {
		rpmlog(RPMLOG_ERR,
			_("No compatible architectures found for build\n"));
		goto errxit;
	    }

	    /*
	     * Return the 1st child's fully parsed Spec structure.
	     * The restart of the parse when encountering BuildArch
	     * causes problems for "rpm -q --specfile". This is
	     * still a hack because there may be more than 1 arch
	     * specified (unlikely but possible.) There's also the
	     * further problem that the macro context, particularly
	     * %{_target_cpu}, disagrees with the info in the header.
	     */
	    if (spec->BACount >= 1) {
		rpmSpec nspec = spec->BASpecs[0];
		spec->BASpecs = _free(spec->BASpecs);
		rpmSpecFree(spec);
		spec = nspec;
	    }

	    goto exit;
	}
    }

    if (spec->clean == NULL) {
	char *body = rpmExpand("%{?buildroot: %{__rm} -rf %{buildroot}}", NULL);
	spec->clean = newStringBuf();
	appendLineStringBuf(spec->clean, body);
	free(body);
    }

    /* Check for description in each package */
    for (Package pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	if (!headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	    rpmlog(RPMLOG_ERR, _("Package has no %%description: %s\n"),
		   headerGetString(pkg->header, RPMTAG_NAME));
	    goto errxit;
	}
    }

    /* Add arch, os and platform, self-provides etc for each package */
    addTargets(spec->packages);

    /* Check for encoding in each package unless disabled */
    if (!(spec->flags & RPMSPEC_NOUTF8)) {
	int badenc = 0;
	for (Package pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	    if (checkForEncoding(pkg->header, 0) != RPMRC_OK) {
		badenc = 1;
	    }
	}
	if (badenc)
	    goto errxit;
    }

    closeSpec(spec);
exit:
    /* Assemble source header from parsed components */
    initSourceHeader(spec);

    return spec;

errxit:
    rpmSpecFree(spec);
    return NULL;
}

rpmSpec rpmSpecParse(const char *specFile, rpmSpecFlags flags,
		     const char *buildRoot)
{
    return parseSpec(specFile, flags, buildRoot, 0);
}
