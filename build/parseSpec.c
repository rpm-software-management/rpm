#include "system.h"

#include "rpmbuild.h"

static struct PartRec {
    int part;
    int len;
    char *token;
} partList[] = {
    {PART_PREAMBLE,      0, "%package"},
    {PART_PREP,          0, "%prep"},
    {PART_BUILD,         0, "%build"},
    {PART_INSTALL,       0, "%install"},
    {PART_CLEAN,         0, "%clean"},
    {PART_PREUN,         0, "%preun"},
    {PART_POSTUN,        0, "%postun"},
    {PART_PRE,           0, "%pre"},
    {PART_POST,          0, "%post"},
    {PART_FILES,         0, "%files"},
    {PART_CHANGELOG,     0, "%changelog"},
    {PART_DESCRIPTION,   0, "%description"},
    {PART_TRIGGERPOSTUN, 0, "%triggerpostun"},
    {PART_TRIGGERUN,     0, "%triggerun"},
    {PART_TRIGGERIN,     0, "%triggerin"},
    {PART_TRIGGERIN,     0, "%trigger"},
    {PART_VERIFYSCRIPT,  0, "%verifyscript"},
    {0, 0, 0}
};

static inline void initParts(struct PartRec *p)
{
    for (; p->token != NULL; p++)
	p->len = strlen(p->token);
}

int isPart(char *line)
{
    char c;
    struct PartRec *p;

    if (partList[0].len == 0)
	initParts(partList);
    
    for (p = partList; p->token != NULL; p++) {
	if (! strncasecmp(line, p->token, p->len)) {
	    c = *(line + p->len);
	    if (c == '\0' || isspace(c)) {
		break;
	    }
	}
    }

    if (p->token) {
	return p->part;
    } else {
	return PART_NONE;
    }
}

static int matchTok(const char *token, const char *line)
{
    const char *b, *be = line;
    int rc = 0;

    while ( *(b = be) ) {
	SKIPSPACE(b);
	be = b;
	SKIPNONSPACE(be);
	if (be == b)
	    break;
    /*
     * XXX The strncasecmp below is necessary so the old (rpm < 2.90) style
     * XXX os-from-uname (e.g. "Linux") is compatible with the new
     * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
     */
	if (strncasecmp(token, b, (be-b)))
	    continue;
	rc = 1;
	break;
    }

    return rc;
}

void handleComments(char *s)
{
    SKIPSPACE(s);
    if (*s == '#') {
	*s = '\0';
    }
}

static void forceIncludeFile(Spec spec, const char * fileName)
{
    OFI_t * ofi;

    ofi = newOpenFileInfo();
    ofi->fileName = strdup(fileName);
    ofi->next = spec->fileStack;
    spec->fileStack = ofi;
}

static int copyNextLine(Spec spec, OFI_t *ofi, int strip)
{
    char *last;
    char ch;

    /* Expand next line from file into line buffer */
    if (!(spec->nextline && *spec->nextline)) {
	char *from, *to;
	to = last = spec->nextline = spec->lbuf;
	from = ofi->readPtr;
	while ((ch = *from) && ch != '\n')
	    *to++ = *from++;
	*to++ = '\0';
	ofi->readPtr = from;

	if (expandMacros(spec, spec->macros, spec->lbuf, sizeof(spec->lbuf))) {
		rpmError(RPMERR_BADSPEC, _("line %d: %s"), spec->lineNum, spec->lbuf);
		return RPMERR_BADSPEC;
	}
    }

    /* Find next line in expanded line buffer */
    spec->line = spec->nextline;
    while ((ch = *spec->nextline) && ch != '\n') {
	spec->nextline++;
	if (!isspace(ch))
	    last = spec->nextline;
    }
    if (ch == '\n')
	*spec->nextline++ = '\0';
    
    if (strip & STRIP_COMMENTS)
	handleComments(spec->line);
    
    if (strip & STRIP_TRAILINGSPACE)
	*last = '\0';

    return 0;
}

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

int readLine(Spec spec, int strip)
{
    char  *s, *arch, *os;
    int match;
    struct ReadLevelEntry *rl;
    OFI_t *ofi = spec->fileStack;
    int rc;

retry:
    /* Make sure the current file is open */
    if (ofi->file == NULL) {
	if (!(ofi->file = fopen(ofi->fileName, "r"))) {
	    rpmError(RPMERR_BADSPEC, _("Unable to open: %s\n"),
		     ofi->fileName);
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = ofi->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!(ofi->readPtr && *(ofi->readPtr))) {
	if (!fgets(ofi->readBuf, BUFSIZ, ofi->file)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmError(RPMERR_UNMATCHEDIF, _("Unclosed %%if"));
	        return RPMERR_UNMATCHEDIF;
	    }

	    /* remove this file from the stack */
	    spec->fileStack = ofi->next;
	    fclose(ofi->file);
	    FREE(ofi->fileName);
	    free(ofi);

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
	    struct speclines *sl = spec->sl;
	    if (sl->sl_nlines == sl->sl_nalloc) {
		sl->sl_nalloc += 100;
		sl->sl_lines = (char **)realloc(sl->sl_lines, 
			sl->sl_nalloc * sizeof(*(sl->sl_lines)));
	    }
	    sl->sl_lines[sl->sl_nlines++] = strdup(ofi->readBuf);
	}
    }
    
    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);

    /* Copy next file line into the spec line buffer */
    if ((rc = copyNextLine(spec, ofi, strip)) != 0)
	return rc;

    s = spec->line;
    SKIPSPACE(s);

    match = -1;
    if (! strncmp("%ifarch", s, 7)) {
	s += 7;
	match = matchTok(arch, s);
    } else if (! strncmp("%ifnarch", s, 8)) {
	s += 8;
	match = !matchTok(arch, s);
    } else if (! strncmp("%ifos", s, 5)) {
	s += 5;
	match = matchTok(os, s);
    } else if (! strncmp("%ifnos", s, 6)) {
	s += 6;
	match = !matchTok(os, s);
    } else if (! strncmp("%if", s, 3)) {
	s += 3;
        match = parseExpressionBoolean(spec, s);
	if (match < 0) {
	    rpmError(RPMERR_UNMATCHEDIF, _("%s:%d: parseExpressionBoolean returns %d"),
		     ofi->fileName, ofi->lineNum, match);
	    return RPMERR_BADSPEC;
	}
    } else if (! strncmp("%else", s, 5)) {
	s += 5;
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, _("%s:%d: Got a %%else with no if"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (! strncmp("%endif", s, 6)) {
	s += 6;
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, _("%s:%d: Got a %%endif with no if"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
    } else if (! strncmp("%include", s, 8)) {
	char *fileName, *endFileName, *p;

	s += 8;
	fileName = s;
	if (! isspace(*fileName)) {
	    rpmError(RPMERR_BADSPEC, _("malformed %%include statement"));
	    return RPMERR_BADSPEC;
	}
	SKIPSPACE(fileName);
	endFileName = fileName;
	SKIPNONSPACE(endFileName);
	p = endFileName;
	SKIPSPACE(p);
	if (*p != '\0') {
	    rpmError(RPMERR_BADSPEC, _("malformed %%include statement"));
	    return RPMERR_BADSPEC;
	}
	*endFileName = '\0';

	forceIncludeFile(spec, fileName);

	ofi = spec->fileStack;
	goto retry;
    }

    if (match != -1) {
	rl = malloc(sizeof(struct ReadLevelEntry));
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
	if (ofi->file) fclose(ofi->file);
	FREE(ofi->fileName);
	free(ofi);
    }
}

int noLang = 0;		/* XXX FIXME: pass as arg */

int parseSpec(Spec *specp, const char *specFile, const char *buildRoot,
	      int inBuildArch, const char *passPhrase, char *cookie,
	      int anyarch, int force)
{
    int parsePart = PART_PREAMBLE;
    int initialPackage = 1;
    char *name;
    char *saveArch;
    Package pkg;
    int x, index;
    Spec spec;
    
    /* Set up a new Spec structure with no packages. */
    spec = newSpec();

    spec->fileStack = newOpenFileInfo();
    spec->fileStack->fileName = strdup(specFile);

    spec->specFile = strdup(specFile);
    if (buildRoot) {
	spec->gotBuildRoot = 1;
	spec->buildRoot = strdup(buildRoot);
	addMacro(spec->macros, "buildroot", NULL, buildRoot, RMIL_SPEC);
    }
    addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
    spec->inBuildArchitectures = inBuildArch;
    spec->anyarch = anyarch;
    spec->force = force;

    if (passPhrase) {
	spec->passPhrase = strdup(passPhrase);
    }
    if (cookie) {
	spec->cookie = strdup(cookie);
    }

    {	const char *timecheck = rpmExpand("%{_timecheck}", NULL);
	if (timecheck && *timecheck != '%') {
	    if (parseNum(timecheck, &(spec->timeCheck))) {
		rpmError(RPMERR_BADSPEC,
		    _("Timecheck value must be an integer: %s"), timecheck);
		xfree(timecheck);
		freeSpec(spec);
		return RPMERR_BADSPEC;
	    }
	} else {
	    spec->timeCheck = 0;
	}
	xfree(timecheck);
    }

    /* All the parse*() functions expect to have a line pre-read */
    /* in the spec's line buffer.  Except for parsePreamble(),   */
    /* which handles the initial entry into a spec file.         */
    
    while (parsePart != PART_NONE) {
	switch (parsePart) {
	  case PART_PREAMBLE:
	    parsePart = parsePreamble(spec, initialPackage);
	    initialPackage = 0;
	    break;
	  case PART_PREP:
	    parsePart = parsePrep(spec);
	    break;
	  case PART_BUILD:
	  case PART_INSTALL:
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
	  case PART_VERIFYSCRIPT:
	  case PART_TRIGGERIN:
	  case PART_TRIGGERUN:
	  case PART_TRIGGERPOSTUN:
	    parsePart = parseScript(spec, parsePart);
	    break;

	  case PART_FILES:
	    parsePart = parseFiles(spec);
	    break;

	}

	if (parsePart < 0) {
	    freeSpec(spec);
	    return parsePart;
	}

	if (parsePart == PART_BUILDARCHITECTURES) {
	    spec->buildArchitectureSpecs =
		malloc(sizeof(Spec) * spec->buildArchitectureCount);
	    index = 0;
	    for (x = 0; x < spec->buildArchitectureCount; x++) {
		if (rpmMachineScore(RPM_MACHTABLE_BUILDARCH,
				    spec->buildArchitectures[x])) {
		    rpmGetMachine(&saveArch, NULL);
		    saveArch = strdup(saveArch);
		    rpmSetMachine(spec->buildArchitectures[x], NULL);
		    if (parseSpec(&(spec->buildArchitectureSpecs[index]),
				  specFile, buildRoot, 1,
				  passPhrase, cookie, anyarch, force)) {
			spec->buildArchitectureCount = index;
			freeSpec(spec);
			return RPMERR_BADSPEC;
		    }
		    rpmSetMachine(saveArch, NULL);
		    free(saveArch);
		    index++;
		}
	    }
	    spec->buildArchitectureCount = index;
	    if (! index) {
		freeSpec(spec);
		rpmError(RPMERR_BADSPEC, _("No buildable architectures"));
		return RPMERR_BADSPEC;
	    }

	    /* XXX HACK: Swap sl/st with child.
	     * The restart of the parse when encountering BuildArch
	     * causes problems for "rpm -q --specfile --specedit"
	     * which needs to keep track of the entire spec file.
	     */

	    if (spec->sl && spec->st) {
		Spec nspec = *spec->buildArchitectureSpecs;
		struct speclines *sl = spec->sl;
		struct spectags *st = spec->st;
		spec->sl = nspec->sl;
		spec->st = nspec->st;
		nspec->sl = sl;
		nspec->st = st;
	    }

	    closeSpec(spec);
	    *specp = spec;
	    return 0;
	}
    }

    /* Check for description in each package and add arch and os */
    { char *arch, *os, *myos = NULL;

      rpmGetArchInfo(&arch, NULL);
      rpmGetOsInfo(&os, NULL);
      /*
       * XXX Capitalizing the 'L' is needed to insure that old
       * XXX os-from-uname (e.g. "Linux") is compatible with the new
       * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
       * XXX A copy of this string is embedded in headers.
       */
      if (!strcmp(os, "linux")) {
	os = myos = strdup(os);
	*os = 'L';
      }

      for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	headerGetEntry(pkg->header, RPMTAG_NAME, NULL, (void **) &name, NULL);
	if (!headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	    rpmError(RPMERR_BADSPEC, _("Package has no %%description: %s"), name);
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}

	headerAddEntry(pkg->header, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
	headerAddEntry(pkg->header, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);
      }
      FREE(myos);
    }

    closeSpec(spec);
    *specp = spec;

    return 0;
}
