#include "system.h"

#include "rpmbuild.h"

#include "popt/popt.h"

/* These have to be global to make up for stupid compilers */
    static int leaveDirs, skipDefaultAction;
    static int createDir, quietly;
    static char * dirName;
    static struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a',	NULL, NULL},
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b',	NULL, NULL},
	    { NULL, 'c', 0, &createDir, 0,		NULL, NULL},
	    { NULL, 'D', 0, &leaveDirs, 0,		NULL, NULL},
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0,	NULL, NULL},
	    { NULL, 'T', 0, &skipDefaultAction, 0,	NULL, NULL},
	    { NULL, 'q', 0, &quietly, 0,		NULL, NULL},
	    { 0, 0, 0, 0, 0,	NULL, NULL}
    };

#ifdef DYING
static int doSetupMacro(Spec spec, char *line);
static int doPatchMacro(Spec spec, char *line);
static char *doPatch(Spec spec, int c, int strip, char *db,
		     int reverse, int removeEmpties);
static int checkOwners(char *file);
static char *doUntar(Spec spec, int c, int quietly);
#endif

static int checkOwners(char *file)
{
    struct stat sb;

    if (lstat(file, &sb)) {
	rpmError(RPMERR_BADSPEC, _("Bad source: %s: %s"), file, strerror(errno));
	return RPMERR_BADSPEC;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmError(RPMERR_BADSPEC, _("Bad owner/group: %s"), file);
	return RPMERR_BADSPEC;
    }

    return 0;
}

static char *doPatch(Spec spec, int c, int strip, char *db,
		     int reverse, int removeEmpties)
{
    static char buf[BUFSIZ];
    char file[BUFSIZ];
    char args[BUFSIZ];
    struct Source *sp;
    int compressed;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No patch number %d"), c);
	return NULL;
    }

    strcpy(file, "%{_sourcedir}/");
    expandMacros(spec, spec->macros, file, sizeof(file));
    strcat(file, sp->source);

    args[0] = '\0';
    if (db) {
#if HAVE_OLDPATCH_21 == 0
	strcat(args, "-b ");
#endif
	strcat(args, "--suffix ");
	strcat(args, db);
    }
    if (reverse) {
	strcat(args, " -R");
    }
    if (removeEmpties) {
	strcat(args, " -E");
    }

    if (isCompressed(file, &compressed)) {
	return NULL;
    }
    if (checkOwners(file)) {
	return NULL;
    }
    if (compressed) {
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"%s -dc %s | patch -p%d %s -s\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		c,
		(compressed == COMPRESSED_BZIP2) ?
		rpmGetVar(RPMVAR_BZIP2BIN) : rpmGetVar(RPMVAR_GZIPBIN),
		file, strip, args);
    } else {
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"patch -p%d %s -s < %s", c, strip, args, file);
    }

    return buf;
}

static char *doUntar(Spec spec, int c, int quietly)
{
    static char buf[BUFSIZ];
    char file[BUFSIZ];
    char *taropts;
    struct Source *sp;
    int compressed;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmError(RPMERR_BADSPEC, _("No source number %d"), c);
	return NULL;
    }

    strcpy(file, "%{_sourcedir}/");
    expandMacros(spec, spec->macros, file, sizeof(file));
    strcat(file, sp->source);

    taropts = ((rpmIsVerbose() && !quietly) ? "-xvvf" : "-xf");

    if (isCompressed(file, &compressed)) {
	return NULL;
    }
    if (checkOwners(file)) {
	return NULL;
    }
    if (compressed) {
	sprintf(buf,
		"%s -dc %s | tar %s -\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		(compressed == COMPRESSED_BZIP2) ?
		rpmGetVar(RPMVAR_BZIP2BIN) : rpmGetVar(RPMVAR_GZIPBIN),
		file, taropts);
    } else {
	sprintf(buf, "tar %s %s", taropts, file);
    }

    return buf;
}

static int doSetupMacro(Spec spec, char *line)
{
    char *version, *name;
    char buf[BUFSIZ];
    StringBuf before;
    StringBuf after;
    poptContext optCon;
    int argc;
    char ** argv;
    int arg;
    char * optArg;
    char * chptr;
    int rc;
    int num;

    leaveDirs = skipDefaultAction = 0;
    createDir = quietly = 0;
    dirName = NULL;

    if ((rc = poptParseArgvString(line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, _("Error parsing %%setup: %s"),
			poptStrerror(rc));
	return RPMERR_BADSPEC;
    }

    before = newStringBuf();
    after = newStringBuf();

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	if (parseNum(optArg, &num)) {
	    rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%setup %c: %s"),
		     spec->lineNum, num, optArg);
	    free(argv);
	    freeStringBuf(before);
	    freeStringBuf(after);
	    poptFreeContext(optCon);
	    return RPMERR_BADSPEC;
	}

	chptr = doUntar(spec, num, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}

	if (arg == 'a')
	    appendLineStringBuf(after, chptr);
	else
	    appendLineStringBuf(before, chptr);
    }

    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, _("line %d: Bad %%setup option %s: %s"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	free(argv);
	freeStringBuf(before);
	freeStringBuf(after);
	poptFreeContext(optCon);
	return RPMERR_BADSPEC;
    }

    if (dirName) {
	spec->buildSubdir = strdup(dirName);
    } else {
	headerGetEntry(spec->packages->header, RPMTAG_VERSION, NULL,
		 (void *) &version, NULL);
	headerGetEntry(spec->packages->header, RPMTAG_NAME, NULL,
		 (void *) &name, NULL);
	sprintf(buf, "%s-%s", name, version);
	spec->buildSubdir = strdup(buf);
    }
    
    free(argv);
    poptFreeContext(optCon);

    /* cd to the build dir */
    strcpy(buf, "cd %{_builddir}");
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    appendLineStringBuf(spec->prep, buf);
    
    /* delete any old sources */
    if (!leaveDirs) {
	sprintf(buf, "rm -rf %s", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	sprintf(buf, MKDIR_P " %s\ncd %s",
		spec->buildSubdir, spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	chptr = doUntar(spec, 0, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd %s", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    if (createDir && !skipDefaultAction) {
	chptr = doUntar(spec, 0, quietly);
	if (!chptr) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    freeStringBuf(after);

    /* clean up permissions etc */
    if (!geteuid()) {
	appendLineStringBuf(spec->prep, "chown -R root .");
#ifdef	__LCLINT__
#define	ROOT_GROUP	"root"
#endif
	appendLineStringBuf(spec->prep, "chgrp -R " ROOT_GROUP " .");
    }

    if (rpmGetVar(RPMVAR_FIXPERMS)) {
	appendStringBuf(spec->prep, "chmod -R ");
	appendStringBuf(spec->prep, rpmGetVar(RPMVAR_FIXPERMS));
	appendLineStringBuf(spec->prep, " .");
    }
    
    return 0;
}

static int doPatchMacro(Spec spec, char *line)
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E;
    char *s;
    char buf[BUFSIZ], *bp;
    int patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    opt_P = opt_p = opt_R = opt_E = 0;
    opt_b = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
    for (bp = buf; (s = strtok(bp, " \t\n")) != NULL;) {
	if (bp) {	/* remove 1st token (%patch) */
		bp = NULL;
		continue;
	}
	if (!strcmp(s, "-P")) {
	    opt_P = 1;
	} else if (!strcmp(s, "-R")) {
	    opt_R = 1;
	} else if (!strcmp(s, "-E")) {
	    opt_E = 1;
	} else if (!strcmp(s, "-b")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, _("line %d: Need arg to %%patch -b: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, _("line %d: Need arg to %%patch -z: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strncmp(s, "-p", 2)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (s == NULL) {
		    rpmError(RPMERR_BADSPEC,
			     _("line %d: Need arg to %%patch -p: %s"),
			     spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%patch -p: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmError(RPMERR_BADSPEC, _("Too many patches!"));
		return RPMERR_BADSPEC;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmError(RPMERR_BADSPEC, _("line %d: Bad arg to %%patch: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    patch_index++;
	}
    }

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E);
	if (s == NULL) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
    }

    for (x = 0; x < patch_index; x++) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E);
	if (s == NULL) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
    }
    
    return 0;
}

int parsePrep(Spec spec, int force)
{
    int nextPart, res, rc;
    StringBuf buf;
    char **lines, **saveLines;

    if (spec->prep != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: second %%prep"), spec->lineNum);
	return RPMERR_BADSPEC;
    }

    spec->prep = newStringBuf();

    /* There are no options to %prep */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	return PART_NONE;
    }
    if (rc) {
	return rc;
    }
    
    buf = newStringBuf();
    
    while (! (nextPart = isPart(spec->line))) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	appendStringBuf(buf, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc) {
	    return rc;
	}
    }

    lines = splitString(getStringBuf(buf), strlen(getStringBuf(buf)), '\n');
    saveLines = lines;
    while (*lines) {
	res = 0;
	if (! strncmp(*lines, "%setup", 6)) {
	    res = doSetupMacro(spec, *lines);
	} else if (! strncmp(*lines, "%patch", 6)) {
	    res = doPatchMacro(spec, *lines);
	} else {
	    appendLineStringBuf(spec->prep, *lines);
	}
	if (res && !force) {
	    freeSplitString(saveLines);
	    freeStringBuf(buf);
	    return res;
	}
	lines++;
    }

    freeSplitString(saveLines);
    freeStringBuf(buf);

    return nextPart;
}
