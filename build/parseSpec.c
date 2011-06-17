/** \ingroup rpmbuild
 * \file build/parseSpec.c
 *  Top level dispatcher for spec file parsing.
 */

#include "system.h"

#include <errno.h>

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

#define LEN_AND_STR(_tag) (sizeof(_tag)-1), (_tag)

typedef struct OpenFileInfo {
    char * fileName;
    FILE *fp;
    int lineNum;
    char readBuf[BUFSIZ];
    char * readPtr;
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

void handleComments(char *s)
{
    SKIPSPACE(s);
    if (*s == '#')
	*s = '\0';
}

static struct OpenFileInfo * newOpenFileInfo(void)
{
    struct OpenFileInfo *ofi;

    ofi = xmalloc(sizeof(*ofi));
    ofi->fp = NULL;
    ofi->fileName = NULL;
    ofi->lineNum = 0;
    ofi->readBuf[0] = '\0';
    ofi->readPtr = NULL;
    ofi->next = NULL;

    return ofi;
}

/**
 */
static void forceIncludeFile(rpmSpec spec, const char * fileName)
{
    OFI_t * ofi;

    ofi = newOpenFileInfo();
    ofi->fileName = xstrdup(fileName);
    ofi->next = spec->fileStack;
    spec->fileStack = ofi;
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

/* Return zero on success, 1 if we need to read more and -1 on errors. */
static int copyNextLineFromOFI(rpmSpec spec, OFI_t *ofi)
{
    char ch;

    /* Expand next line from file into line buffer */
    if (!(spec->nextline && *spec->nextline)) {
	int pc = 0, bc = 0, nc = 0;
	char *from, *to, *p;
	to = spec->lbufPtr ? spec->lbufPtr : spec->lbuf;
	from = ofi->readPtr;
	ch = ' ';
	while (from && *from && ch != '\n')
	    ch = *to++ = *from++;
	spec->lbufPtr = to;
	*to++ = '\0';
	ofi->readPtr = from;

	/* Check if we need another line before expanding the buffer. */
	for (p = spec->lbuf; *p; p++) {
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
	spec->lbufPtr = spec->lbuf;

	/* Don't expand macros (eg. %define) in false branch of %if clause */
	if (spec->readStack->reading &&
	    expandMacros(spec, spec->macros, spec->lbuf, sizeof(spec->lbuf))) {
		rpmlog(RPMLOG_ERR, _("line %d: %s\n"),
			spec->lineNum, spec->lbuf);
		return -1;
	}
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
	if (ofi->fp == NULL || ferror(ofi->fp)) {
	    /* XXX Fstrerror */
	    rpmlog(RPMLOG_ERR, _("Unable to open %s: %s\n"),
		     ofi->fileName, strerror(errno));
	    return PART_ERROR;
	}
	spec->lineNum = ofi->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!(ofi->readPtr && *(ofi->readPtr))) {
	if (!fgets(ofi->readBuf, BUFSIZ, ofi->fp)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmlog(RPMLOG_ERR, _("Unclosed %%if\n"));
	        return PART_ERROR;
	    }

	    /* remove this file from the stack */
	    spec->fileStack = ofi->next;
	    fclose(ofi->fp);
	    ofi->fileName = _free(ofi->fileName);
	    ofi = _free(ofi);

	    /* only on last file do we signal EOF to caller */
	    ofi = spec->fileStack;
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

int readLine(rpmSpec spec, int strip)
{
    char  *s;
    int match;
    struct ReadLevelEntry *rl;
    OFI_t *ofi = spec->fileStack;
    int rc;
    int startLine = 0;

    if (!restoreFirstChar(spec)) {
    retry:
	if ((rc = readLineFromOFI(spec, ofi)) != 0) {
	    if (startLine > 0) {
		rpmlog(RPMLOG_ERR,
		    _("line %d: unclosed macro or bad line continuation\n"),
		    startLine);
		rc = PART_ERROR;
	    }
	    return rc;
	}
	ofi = spec->fileStack;

	/* Copy next file line into the spec line buffer */
	rc = copyNextLineFromOFI(spec, ofi);
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
    if (!spec->readStack->reading && rstreqn("%if", s, sizeof("%if")-1)) {
	match = 0;
    } else if (rstreqn("%ifarch", s, sizeof("%ifarch")-1)) {
	char *arch = rpmExpand("%{_target_cpu}", NULL);
	s += 7;
	match = matchTok(arch, s);
	arch = _free(arch);
    } else if (rstreqn("%ifnarch", s, sizeof("%ifnarch")-1)) {
	char *arch = rpmExpand("%{_target_cpu}", NULL);
	s += 8;
	match = !matchTok(arch, s);
	arch = _free(arch);
    } else if (rstreqn("%ifos", s, sizeof("%ifos")-1)) {
	char *os = rpmExpand("%{_target_os}", NULL);
	s += 5;
	match = matchTok(os, s);
	os = _free(os);
    } else if (rstreqn("%ifnos", s, sizeof("%ifnos")-1)) {
	char *os = rpmExpand("%{_target_os}", NULL);
	s += 6;
	match = !matchTok(os, s);
	os = _free(os);
    } else if (rstreqn("%if", s, sizeof("%if")-1)) {
	s += 3;
        match = parseExpressionBoolean(spec, s);
	if (match < 0) {
	    rpmlog(RPMLOG_ERR,
			_("%s:%d: parseExpressionBoolean returns %d\n"),
			ofi->fileName, ofi->lineNum, match);
	    return PART_ERROR;
	}
    } else if (rstreqn("%else", s, sizeof("%else")-1)) {
	s += 5;
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
    } else if (rstreqn("%endif", s, sizeof("%endif")-1)) {
	s += 6;
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
    } else if (rstreqn("%include", s, sizeof("%include")-1)) {
	char *fileName, *endFileName, *p;

	s += 8;
	fileName = s;
	if (! risspace(*fileName)) {
	    rpmlog(RPMLOG_ERR, _("malformed %%include statement\n"));
	    return PART_ERROR;
	}
	SKIPSPACE(fileName);
	endFileName = fileName;
	SKIPNONSPACE(endFileName);
	p = endFileName;
	SKIPSPACE(p);
	if (*p != '\0') {
	    rpmlog(RPMLOG_ERR, _("malformed %%include statement\n"));
	    return PART_ERROR;
	}
	*endFileName = '\0';

	forceIncludeFile(spec, fileName);

	ofi = spec->fileStack;
	goto retry;
    }

    if (match != -1) {
	rl = xmalloc(sizeof(*rl));
	rl->reading = spec->readStack->reading && match;
	rl->next = spec->readStack;
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
    OFI_t *ofi;

    while (spec->fileStack) {
	ofi = spec->fileStack;
	spec->fileStack = spec->fileStack->next;
	if (ofi->fp) (void) fclose(ofi->fp);
	ofi->fileName = _free(ofi->fileName);
	ofi = _free(ofi);
    }
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
    0
};

static void initSourceHeader(rpmSpec spec)
{
    HeaderIterator hi;
    struct rpmtd_s td;
    struct Source *srcPtr;

    spec->sourceHeader = headerNew();
    /* Only specific tags are added to the source package header */
    headerCopyTags(spec->packages->header, spec->sourceHeader, sourceTags);

    /* Add the build restrictions */
    hi = headerInitIterator(spec->buildRestrictions);
    while (headerNext(hi, &td)) {
	if (rpmtdCount(&td) > 0) {
	    (void) headerPut(spec->sourceHeader, &td, HEADERPUT_DEFAULT);
	}
	rpmtdFreeData(&td);
    }
    hi = headerFreeIterator(hi);

    if (spec->BANames && spec->BACount > 0) {
	headerPutStringArray(spec->sourceHeader, RPMTAG_BUILDARCHS,
		  spec->BANames, spec->BACount);
    }

    /* Add tags for sources and patches */
    for (srcPtr = spec->sources; srcPtr != NULL; srcPtr = srcPtr->next) {
	if (srcPtr->flags & RPMBUILD_ISSOURCE) {
	    headerPutString(spec->sourceHeader, RPMTAG_SOURCE, srcPtr->source);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerPutUint32(spec->sourceHeader, RPMTAG_NOSOURCE,
				&srcPtr->num, 1);
	    }
	}
	if (srcPtr->flags & RPMBUILD_ISPATCH) {
	    headerPutString(spec->sourceHeader, RPMTAG_PATCH, srcPtr->source);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerPutUint32(spec->sourceHeader, RPMTAG_NOPATCH,
				&srcPtr->num, 1);
	    }
	}
    }
}

static void addTargets(Package Pkgs)
{
    char *platform = rpmExpand("%{_target_platform}", NULL);
    char *arch = rpmExpand("%{_target_cpu}", NULL);
    char *os = rpmExpand("%{_target_os}", NULL);

    for (Package pkg = Pkgs; pkg != NULL; pkg = pkg->next) {
	headerPutString(pkg->header, RPMTAG_OS, os);
	/* noarch subpackages already have arch set here, leave it alone */
	if (!headerIsEntry(pkg->header, RPMTAG_ARCH)) {
	    headerPutString(pkg->header, RPMTAG_ARCH, arch);
	}
	headerPutString(pkg->header, RPMTAG_PLATFORM, platform);

	pkg->ds = rpmdsThis(pkg->header, RPMTAG_REQUIRENAME, RPMSENSE_EQUAL);
    }
    free(platform);
    free(arch);
    free(os);
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
    spec->fileStack = newOpenFileInfo();
    spec->fileStack->fileName = xstrdup(spec->specFile);
    /* If buildRoot not specified, use default %{buildroot} */
    if (buildRoot) {
	spec->buildRoot = xstrdup(buildRoot);
    } else {
	spec->buildRoot = rpmGetPath("%{?buildroot:%{buildroot}}", NULL);
    }
    addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
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
		spec = rpmSpecFree(spec);
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

    /* Add arch, os and platform for each package */
    addTargets(spec->packages);

    closeSpec(spec);
exit:
    /* Assemble source header from parsed components */
    initSourceHeader(spec);

    return spec;

errxit:
    spec = rpmSpecFree(spec);
    return NULL;
}

rpmSpec rpmSpecParse(const char *specFile, rpmSpecFlags flags,
		     const char *buildRoot)
{
    return parseSpec(specFile, flags, buildRoot, 0);
}
