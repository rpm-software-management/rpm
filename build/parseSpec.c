/** \ingroup rpmbuild
 * \file build/parseSpec.c
 *  Top level dispatcher for spec file parsing.
 */

#include "system.h"

/*@unchecked@*/
static int _debug = 0;

#include <rpmio_internal.h>
#include <rpmbuild.h>
#include "debug.h"

/*@access FD_t @*/	/* compared with NULL */

/**
 */
/*@unchecked@*/
static struct PartRec {
    int part;
    int len;
/*@observer@*/ /*@null@*/ const char * token;
} partList[] = {
    { PART_PREAMBLE,      0, "%package"},
    { PART_PREP,          0, "%prep"},
    { PART_BUILD,         0, "%build"},
    { PART_INSTALL,       0, "%install"},
    { PART_CLEAN,         0, "%clean"},
    { PART_PREUN,         0, "%preun"},
    { PART_POSTUN,        0, "%postun"},
    { PART_PRE,           0, "%pre"},
    { PART_POST,          0, "%post"},
    { PART_FILES,         0, "%files"},
    { PART_CHANGELOG,     0, "%changelog"},
    { PART_DESCRIPTION,   0, "%description"},
    { PART_TRIGGERPOSTUN, 0, "%triggerpostun"},
    { PART_TRIGGERUN,     0, "%triggerun"},
    { PART_TRIGGERIN,     0, "%triggerin"},
    { PART_TRIGGERIN,     0, "%trigger"},
    { PART_VERIFYSCRIPT,  0, "%verifyscript"},
    {0, 0, 0}
};

/**
 */
static inline void initParts(struct PartRec *p)
	/*@modifies p->len @*/
{
    for (; p->token != NULL; p++)
	p->len = strlen(p->token);
}

rpmParseState isPart(const char *line)
{
    struct PartRec *p;

    if (partList[0].len == 0)
	initParts(partList);
    
    for (p = partList; p->token != NULL; p++) {
	char c;
	if (xstrncasecmp(line, p->token, p->len))
	    continue;
	c = *(line + p->len);
	if (c == '\0' || xisspace(c))
	    break;
    }

    return (p->token ? p->part : PART_NONE);
}

/**
 */
static int matchTok(const char *token, const char *line)
	/*@*/
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
	if (toklen != (be-b) || xstrncasecmp(token, b, (be-b)))
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

/**
 */
static void forceIncludeFile(Spec spec, const char * fileName)
	/*@modifies spec->fileStack @*/
{
    OFI_t * ofi;

    ofi = newOpenFileInfo();
    ofi->fileName = xstrdup(fileName);
    ofi->next = spec->fileStack;
    spec->fileStack = ofi;
}

/**
 */
static int copyNextLine(Spec spec, OFI_t *ofi, int strip)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies spec->nextline, fileSystem @*/
{
    char *last;
    char ch;

    /* Restore 1st char in (possible) next line */
    if (spec->nextline != NULL && spec->nextpeekc != '\0') {
	*spec->nextline = spec->nextpeekc;
	spec->nextpeekc = '\0';
    }
    /* Expand next line from file into line buffer */
    if (!(spec->nextline && *spec->nextline)) {
	char *from, *to;
	to = last = spec->lbuf;
	from = ofi->readPtr;
	ch = ' ';
	while (*from && ch != '\n')
	    ch = *to++ = *from++;
	*to++ = '\0';
	ofi->readPtr = from;

	/* Don't expand macros (eg. %define) in false branch of %if clause */
	if (spec->readStack->reading &&
	    expandMacros(spec, spec->macros, spec->lbuf, sizeof(spec->lbuf))) {
		rpmError(RPMERR_BADSPEC, _("line %d: %s\n"),
			spec->lineNum, spec->lbuf);
		return RPMERR_BADSPEC;
	}
	spec->nextline = spec->lbuf;
    }

    /* Find next line in expanded line buffer */
    spec->line = last = spec->nextline;
    ch = ' ';
    while (*spec->nextline && ch != '\n') {
	ch = *spec->nextline++;
	if (!xisspace(ch))
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

    return 0;
}

int readLine(Spec spec, int strip)
{
#ifdef	DYING
    const char *arch;
    const char *os;
#endif
    char  *s;
    int match;
    struct ReadLevelEntry *rl;
    OFI_t *ofi = spec->fileStack;
    int rc;

retry:
    /* Make sure the current file is open */
    /*@-branchstate@*/
    if (ofi->fd == NULL) {
	ofi->fd = Fopen(ofi->fileName, "r.fpio");
	if (ofi->fd == NULL || Ferror(ofi->fd)) {
	    /* XXX Fstrerror */
	    rpmError(RPMERR_BADSPEC, _("Unable to open %s: %s\n"),
		     ofi->fileName, Fstrerror(ofi->fd));
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = ofi->lineNum = 0;
    }
    /*@=branchstate@*/

    /* Make sure we have something in the read buffer */
    if (!(ofi->readPtr && *(ofi->readPtr))) {
	FILE * f = fdGetFp(ofi->fd);
	if (f == NULL || !fgets(ofi->readBuf, BUFSIZ, f)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmError(RPMERR_UNMATCHEDIF, _("Unclosed %%if\n"));
	        return RPMERR_UNMATCHEDIF;
	    }

	    /* remove this file from the stack */
	    spec->fileStack = ofi->next;
	    (void) Fclose(ofi->fd);
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
	if (spec->sl) {
	    speclines sl = spec->sl;
	    if (sl->sl_nlines == sl->sl_nalloc) {
		sl->sl_nalloc += 100;
		sl->sl_lines = (char **) xrealloc(sl->sl_lines, 
			sl->sl_nalloc * sizeof(*(sl->sl_lines)));
	    }
	    sl->sl_lines[sl->sl_nlines++] = xstrdup(ofi->readBuf);
	}
    }
    
#ifdef	DYING
    arch = NULL;
    rpmGetArchInfo(&arch, NULL);
    os = NULL;
    rpmGetOsInfo(&os, NULL);
#endif

    /* Copy next file line into the spec line buffer */
    if ((rc = copyNextLine(spec, ofi, strip)) != 0)
	return rc;

    s = spec->line;
    SKIPSPACE(s);

    match = -1;
    if (! strncmp("%ifarch", s, sizeof("%ifarch")-1)) {
	const char *arch = rpmExpand("%{_target_cpu}", NULL);
	s += 7;
	match = matchTok(arch, s);
	arch = _free(arch);
    } else if (! strncmp("%ifnarch", s, sizeof("%ifnarch")-1)) {
	const char *arch = rpmExpand("%{_target_cpu}", NULL);
	s += 8;
	match = !matchTok(arch, s);
	arch = _free(arch);
    } else if (! strncmp("%ifos", s, sizeof("%ifos")-1)) {
	const char *os = rpmExpand("%{_target_os}", NULL);
	s += 5;
	match = matchTok(os, s);
	os = _free(os);
    } else if (! strncmp("%ifnos", s, sizeof("%ifnos")-1)) {
	const char *os = rpmExpand("%{_target_os}", NULL);
	s += 6;
	match = !matchTok(os, s);
	os = _free(os);
    } else if (! strncmp("%if", s, sizeof("%if")-1)) {
	s += 3;
        match = parseExpressionBoolean(spec, s);
	if (match < 0) {
	    rpmError(RPMERR_UNMATCHEDIF,
			_("%s:%d: parseExpressionBoolean returns %d\n"),
			ofi->fileName, ofi->lineNum, match);
	    return RPMERR_BADSPEC;
	}
    } else if (! strncmp("%else", s, sizeof("%else")-1)) {
	s += 5;
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF,
			_("%s:%d: Got a %%else with no %%if\n"),
			ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (! strncmp("%endif", s, sizeof("%endif")-1)) {
	s += 6;
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF,
			_("%s:%d: Got a %%endif with no %%if\n"),
			ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
    } else if (! strncmp("%include", s, sizeof("%include")-1)) {
	char *fileName, *endFileName, *p;

	s += 8;
	fileName = s;
	if (! xisspace(*fileName)) {
	    rpmError(RPMERR_BADSPEC, _("malformed %%include statement\n"));
	    return RPMERR_BADSPEC;
	}
	SKIPSPACE(fileName);
	endFileName = fileName;
	SKIPNONSPACE(endFileName);
	p = endFileName;
	SKIPSPACE(p);
	if (*p != '\0') {
	    rpmError(RPMERR_BADSPEC, _("malformed %%include statement\n"));
	    return RPMERR_BADSPEC;
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

    return 0;
}

void closeSpec(Spec spec)
{
    OFI_t *ofi;

    while (spec->fileStack) {
	ofi = spec->fileStack;
	spec->fileStack = spec->fileStack->next;
	if (ofi->fd) (void) Fclose(ofi->fd);
	ofi->fileName = _free(ofi->fileName);
	ofi = _free(ofi);
    }
}

/*@-redecl@*/
/*@unchecked@*/
extern int noLang;		/* XXX FIXME: pass as arg */
/*@=redecl@*/

/*@todo Skip parse recursion if os is not compatible. @*/
int parseSpec(Spec *specp, const char *specFile, const char *rootURL,
		const char *buildRootURL, int recursing, const char *passPhrase,
		char *cookie, int anyarch, int force)
{
    rpmParseState parsePart = PART_PREAMBLE;
    int initialPackage = 1;
#ifdef	DYING
    const char *saveArch;
#endif
    Package pkg;
    Spec spec;
    
    /* Set up a new Spec structure with no packages. */
    spec = newSpec();

    /*
     * Note: rpmGetPath should guarantee a "canonical" path. That means
     * that the following pathologies should be weeded out:
     *          //bin//sh
     *          //usr//bin/
     *          /.././../usr/../bin//./sh (XXX FIXME: dots not handled yet)
     */
    spec->specFile = rpmGetPath(specFile, NULL);
    spec->fileStack = newOpenFileInfo();
    spec->fileStack->fileName = xstrdup(spec->specFile);
    if (buildRootURL) {
	const char * buildRoot;
	(void) urlPath(buildRootURL, &buildRoot);
	/*@-branchstate@*/
	if (*buildRoot == '\0') buildRoot = "/";
	/*@=branchstate@*/
	if (!strcmp(buildRoot, "/")) {
            rpmError(RPMERR_BADSPEC,
                     _("BuildRoot can not be \"/\": %s\n"), buildRootURL);
            return RPMERR_BADSPEC;
        }
	spec->gotBuildRootURL = 1;
	spec->buildRootURL = xstrdup(buildRootURL);
	addMacro(spec->macros, "buildroot", NULL, buildRoot, RMIL_SPEC);
if (_debug)
fprintf(stderr, "*** PS buildRootURL(%s) %p macro set to %s\n", spec->buildRootURL, spec->buildRootURL, buildRoot);
    }
    addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
    spec->recursing = recursing;
    spec->anyarch = anyarch;
    spec->force = force;

    if (rootURL)
	spec->rootURL = xstrdup(rootURL);
    if (passPhrase)
	spec->passPhrase = xstrdup(passPhrase);
    if (cookie)
	spec->cookie = xstrdup(cookie);

    spec->timeCheck = rpmExpandNumeric("%{_timecheck}");

    /* All the parse*() functions expect to have a line pre-read */
    /* in the spec's line buffer.  Except for parsePreamble(),   */
    /* which handles the initial entry into a spec file.         */
    
    /*@-infloops@*/	/* LCL: parsePart is modified @*/
    while (parsePart < PART_LAST && parsePart != PART_NONE) {
	switch (parsePart) {
	case PART_PREAMBLE:
	    parsePart = parsePreamble(spec, initialPackage);
	    initialPackage = 0;
	    /*@switchbreak@*/ break;
	case PART_PREP:
	    parsePart = parsePrep(spec);
	    /*@switchbreak@*/ break;
	case PART_BUILD:
	case PART_INSTALL:
	case PART_CLEAN:
	    parsePart = parseBuildInstallClean(spec, parsePart);
	    /*@switchbreak@*/ break;
	case PART_CHANGELOG:
	    parsePart = parseChangelog(spec);
	    /*@switchbreak@*/ break;
	case PART_DESCRIPTION:
	    parsePart = parseDescription(spec);
	    /*@switchbreak@*/ break;

	case PART_PRE:
	case PART_POST:
	case PART_PREUN:
	case PART_POSTUN:
	case PART_VERIFYSCRIPT:
	case PART_TRIGGERIN:
	case PART_TRIGGERUN:
	case PART_TRIGGERPOSTUN:
	    parsePart = parseScript(spec, parsePart);
	    /*@switchbreak@*/ break;

	case PART_FILES:
	    parsePart = parseFiles(spec);
	    /*@switchbreak@*/ break;

	case PART_NONE:		/* XXX avoid gcc whining */
	case PART_LAST:
	case PART_BUILDARCHITECTURES:
	    /*@switchbreak@*/ break;
	}

	if (parsePart >= PART_LAST) {
	    spec = freeSpec(spec);
	    return parsePart;
	}

	if (parsePart == PART_BUILDARCHITECTURES) {
	    int index;
	    int x;

	    closeSpec(spec);

	    /* LCL: sizeof(spec->BASpecs[0]) -nullderef whine here */
	    spec->BASpecs = xcalloc(spec->BACount, sizeof(*spec->BASpecs));
	    index = 0;
	    if (spec->BANames != NULL)
	    for (x = 0; x < spec->BACount; x++) {

		/* Skip if not arch is not compatible. */
		if (!rpmMachineScore(RPM_MACHTABLE_BUILDARCH, spec->BANames[x]))
		    /*@innercontinue@*/ continue;
#ifdef	DYING
		rpmGetMachine(&saveArch, NULL);
		saveArch = xstrdup(saveArch);
		rpmSetMachine(spec->BANames[x], NULL);
#else
		addMacro(NULL, "_target_cpu", NULL, spec->BANames[x], RMIL_RPMRC);
#endif
		spec->BASpecs[index] = NULL;
		if (parseSpec(&(spec->BASpecs[index]),
				  specFile, spec->rootURL, buildRootURL, 1,
				  passPhrase, cookie, anyarch, force))
		{
			spec->BACount = index;
			spec = freeSpec(spec);
			return RPMERR_BADSPEC;
		}
#ifdef	DYING
		rpmSetMachine(saveArch, NULL);
		saveArch = _free(saveArch);
#else
		delMacro(NULL, "_target_cpu");
#endif
		index++;
	    }

	    spec->BACount = index;
	    if (! index) {
		spec = freeSpec(spec);
		rpmError(RPMERR_BADSPEC,
			_("No compatible architectures found for build\n"));
		return RPMERR_BADSPEC;
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
	    /*@-branchstate@*/
	    if (spec->BACount >= 1) {
		Spec nspec = spec->BASpecs[0];
		spec->BASpecs = _free(spec->BASpecs);
		spec = freeSpec(spec);
		spec = nspec;
	    }
	    /*@=branchstate@*/

	    *specp = spec;
	    return 0;
	}
    }
    /*@=infloops@*/	/* LCL: parsePart is modified @*/

    /* Check for description in each package and add arch and os */
  {
#ifdef	DYING
    const char *arch = NULL;
    const char *os = NULL;
    char *myos = NULL;

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
    /*
     * XXX Capitalizing the 'L' is needed to insure that old
     * XXX os-from-uname (e.g. "Linux") is compatible with the new
     * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
     * XXX A copy of this string is embedded in headers.
     */
    if (!strcmp(os, "linux")) {
	myos = xstrdup(os);
	*myos = 'L';
	os = myos;
    }
#else
    const char *arch = rpmExpand("%{_target_cpu}", NULL);
    const char *os = rpmExpand("%{_target_os}", NULL);
#endif

      for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	if (!headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	    const char * name;
	    (void) headerNVR(pkg->header, &name, NULL, NULL);
	    rpmError(RPMERR_BADSPEC, _("Package has no %%description: %s\n"),
			name);
	    spec = freeSpec(spec);
	    return RPMERR_BADSPEC;
	}

	(void) headerAddEntry(pkg->header, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
	(void) headerAddEntry(pkg->header, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);
      }
#ifdef	DYING
    myos = _free(myos);
#else
    arch = _free(arch);
    os = _free(os);
#endif
  }

    closeSpec(spec);
    *specp = spec;

    return 0;
}
