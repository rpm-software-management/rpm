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

static void initParts(void)
{
    struct PartRec *p = partList;

    while (p->token) {
	p->len = strlen(p->token);
	p++;
    }
}

int isPart(char *line)
{
    char c;
    struct PartRec *p = partList;

    if (p->len == 0) {
	initParts();
    }
    
    while (p->token) {
	if (! strncasecmp(line, p->token, p->len)) {
	    c = *(line + p->len);
	    if (c == '\0' || isspace(c)) {
		break;
	    }
	}
	p++;
    }

    if (p->token) {
	return p->part;
    } else {
	return PART_NONE;
    }
}

#ifdef	DYING
static int matchTok(char *token, char *line);
#endif

static int matchTok(char *token, char *line)
{
    char buf[BUFSIZ], *bp, *tok;

    /*
     * XXX The strcasecmp below is necessary so the old (rpm < 2.90) style
     * XXX os-from-uname (e.g. "Linux") is compatible with the new
     * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
     */
    strcpy(buf, line);
    for (bp = buf; (tok = strtok(bp, " \n\t")) != NULL;) {
	bp = NULL;
	if (! strcasecmp(tok, token)) {
	    return 1;
	}
    }

    return 0;
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
    struct OpenFileInfo * ofi;

    ofi = newOpenFileInfo();
    ofi->fileName = strdup(fileName);
    ofi->next = spec->fileStack;
    spec->fileStack = ofi;
}

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

int readLine(Spec spec, int strip)
{
    char *from, *to, *last, *s, *arch, *os;
    int match;
    char ch;
    struct ReadLevelEntry *rl;
    struct OpenFileInfo *ofi = spec->fileStack;

    /* Make sure the current file is open */
retry:
    if (!ofi->file) {
	if (!(ofi->file = fopen(ofi->fileName, "r"))) {
	    rpmError(RPMERR_BADSPEC, _("Unable to open: %s\n"),
		     ofi->fileName);
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = ofi->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!ofi->readPtr || ! *(ofi->readPtr)) {
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
	/*rpmMessage(RPMMESS_DEBUG, "LINE: %s", spec->readBuf);*/
    }
    
    /* Copy a single line to the line buffer */
    from = ofi->readPtr;
    to = last = spec->line;
    ch = ' ';
    while (*from && ch != '\n') {
	ch = *to++ = *from++;
	if (!isspace(ch)) {
	    last = to;
	}
    }
    *to = '\0';
    ofi->readPtr = from;
    
    if (strip & STRIP_COMMENTS) {
	handleComments(spec->line);
    }
    
    if (strip & STRIP_TRAILINGSPACE) {
	*last = '\0';
    }

    if (spec->readStack->reading) {
	if (expandMacros(spec, spec->macros, spec->line, sizeof(spec->line))) {
	    rpmError(RPMERR_BADSPEC, _("line %d: %s"), spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
    }

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);

    s = spec->line;
    SKIPSPACE(s);
    match = -1;
    if (! strncmp("%ifarch", s, 7)) {
	match = matchTok(arch, s);
    } else if (! strncmp("%ifnarch", s, 8)) {
	match = !matchTok(arch, s);
    } else if (! strncmp("%ifos", s, 5)) {
	match = matchTok(os, s);
    } else if (! strncmp("%ifnos", s, 6)) {
	match = !matchTok(os, s);
    } else if (! strncmp("%if", s, 3)) {
        match = parseExpressionBoolean(spec, s + 3);
	if (match < 0) return RPMERR_BADSPEC;
    } else if (! strncmp("%else", s, 5)) {
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
	char *fileName = s + 8, *endFileName, *p;

	if (! isspace(*fileName)) {
	    rpmError(RPMERR_BADSPEC, _("malformed %%include statement"));
	    return RPMERR_BADSPEC;
	}
	while (*fileName && isspace(*fileName)) fileName++;
	endFileName = fileName;
	while (*endFileName && !isspace(*endFileName)) endFileName++;
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
    struct OpenFileInfo *ofi;

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
    }
    addMacro(&globalMacroContext, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
    spec->inBuildArchitectures = inBuildArch;
    spec->anyarch = anyarch;
    spec->force = force;

    if (passPhrase) {
	spec->passPhrase = strdup(passPhrase);
    }
    if (cookie) {
	spec->cookie = strdup(cookie);
    }

    if (rpmGetVar(RPMVAR_TIMECHECK)) {
	if (parseNum(rpmGetVar(RPMVAR_TIMECHECK), &(spec->timeCheck))) {
	    rpmError(RPMERR_BADSPEC, _("Timecheck value must be an integer: %s"),
		     rpmGetVar(RPMVAR_TIMECHECK));
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}
    } else {
	spec->timeCheck = 0;
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
	    x = 0;
	    index = 0;
	    while (x < spec->buildArchitectureCount) {
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
		x++;
	    }
	    spec->buildArchitectureCount = index;
	    if (! index) {
		freeSpec(spec);
		rpmError(RPMERR_BADSPEC, _("No buildable architectures"));
		return RPMERR_BADSPEC;
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
