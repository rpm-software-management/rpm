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
	if (! strncmp(line, p->token, p->len)) {
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
    char buf[BUFSIZ], *tok;

    strcpy(buf, line);
    strtok(buf, " \n\t");
    while ((tok = strtok(NULL, " \n\t"))) {
	if (! strcmp(tok, token)) {
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

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

int readLine(Spec spec, int strip)
{
    char *from, *to, *last, *s, *arch, *os;
    int match;
    char ch;
    struct ReadLevelEntry *rl;

    /* Make sure the spec file is open */
    if (!spec->file) {
	if (!(spec->file = fopen(spec->specFile, "r"))) {
	    rpmError(RPMERR_BADSPEC, "Unable to open: %s\n", spec->specFile);
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!spec->readPtr || ! *(spec->readPtr)) {
	if (!fgets(spec->readBuf, BUFSIZ, spec->file)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmError(RPMERR_UNMATCHEDIF, "Unclosed %%if");
	        return RPMERR_UNMATCHEDIF;
	    }
	    return 1;
	}
	spec->readPtr = spec->readBuf;
	spec->lineNum++;
	/*rpmMessage(RPMMESS_DEBUG, "LINE: %s", spec->readBuf);*/
    }
    
    /* Copy a single line to the line buffer */
    from = spec->readPtr;
    to = last = spec->line;
    ch = ' ';
    while (*from && ch != '\n') {
	ch = *to++ = *from++;
	if (!isspace(ch)) {
	    last = to;
	}
    }
    *to = '\0';
    spec->readPtr = from;
    
    if (strip & STRIP_COMMENTS) {
	handleComments(spec->line);
    }
    
    if (strip & STRIP_TRAILINGSPACE) {
	*last = '\0';
    }

    if (spec->readStack->reading) {
	if (expandMacros(spec, spec->macros, spec->line, sizeof(spec->line))) {
	    rpmError(RPMERR_BADSPEC, "line %d: %s", spec->lineNum, spec->line);
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
    } else if (! strncmp("%else", s, 5)) {
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, "line %d: Got a %%else with no if",
		     spec->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (! strncmp("%endif", s, 6)) {
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, "line %d: Got a %%endif with no if",
		     spec->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
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
    if (spec->file) {
	fclose(spec->file);
    }
    spec->file = NULL;
}

int noLang = 0;		/* XXX FIXME: pass as arg */

int parseSpec(Spec *specp, char *specFile, char *buildRoot,
	      int inBuildArch, char *passPhrase, char *cookie)
{
    int parsePart = PART_PREAMBLE;
    int initialPackage = 1;
    char *name, *arch, *os;
    char *saveArch;
    Package pkg;
    int x, index;
    Spec spec;
    
    /* Set up a new Spec structure with no packages. */
    spec = newSpec();

    spec->specFile = strdup(specFile);
    if (buildRoot) {
	spec->gotBuildRoot = 1;
	spec->buildRoot = strdup(buildRoot);
    }
    spec->docDir = strdup(rpmGetVar(RPMVAR_DEFAULTDOCDIR));
    spec->inBuildArchitectures = inBuildArch;
    if (passPhrase) {
	spec->passPhrase = strdup(passPhrase);
    }
    if (cookie) {
	spec->cookie = strdup(cookie);
    }

    if (rpmGetVar(RPMVAR_TIMECHECK)) {
	if (parseNum(rpmGetVar(RPMVAR_TIMECHECK), &(spec->timeCheck))) {
	    rpmError(RPMERR_BADSPEC, "Timecheck value must be an integer: %s",
		     rpmGetVar(RPMVAR_TIMECHECK));
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}
    } else {
	spec->timeCheck = 0;
    }

    /* Add some standard macros */
    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
#ifdef	DEAD
    setStandardMacros(spec, arch, os);
#endif

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
				  passPhrase, cookie)) {
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
		rpmError(RPMERR_BADSPEC, "No buildable architectures");
		return RPMERR_BADSPEC;
	    }
	    closeSpec(spec);
	    *specp = spec;
	    return 0;
	}
    }

    /* Check for description in each package and add arch and os */
    pkg = spec->packages;
    while (pkg) {
	headerGetEntry(pkg->header, RPMTAG_NAME, NULL, (void **) &name, NULL);
	if (!headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	    rpmError(RPMERR_BADSPEC, "Package has no %%description: %s", name);
	    freeSpec(spec);
	    return RPMERR_BADSPEC;
	}

	headerAddEntry(pkg->header, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
	headerAddEntry(pkg->header, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);

	pkg = pkg->next;
    }

    closeSpec(spec);
    *specp = spec;

    return 0;
}
