#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "spec.h"
#include "read.h"
#include "part.h"
#include "rpmlib.h"
#include "lib/misc.h"
#include "popt/popt.h"
#include "names.h"
#include "misc.h"
#include "config.h"

/* These have to be global to make up for stupid compilers */
    static int leaveDirs, skipDefaultAction;
    static int createDir, quietly;
    static char * dirName;
    static struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a' },
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b' },
	    { NULL, 'c', 0, &createDir, 0 },
	    { NULL, 'D', 0, &leaveDirs, 0 },
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0 },
	    { NULL, 'T', 0, &skipDefaultAction, 0 },
	    { NULL, 'q', 0, &quietly, 0 },
	    { 0, 0, 0, 0, 0 }
    };

static int doSetupMacro(Spec spec, char *line);
static int doPatchMacro(Spec spec, char *line);
static char *doPatch(Spec spec, int c, int strip, char *db,
		     int reverse, int removeEmpties);
static int isCompressed(char *file, int *compressed);
static int checkOwners(char *file);
static char *doUntar(Spec spec, int c, int quietly);

#define COMPRESSED_NOT   0
#define COMPRESSED_OTHER 1
#define COMPRESSED_BZIP2 2

int parsePrep(Spec spec)
{
    int nextPart, res, rc;
    StringBuf buf;
    char **lines, **saveLines;

    if (spec->prep) {
	rpmError(RPMERR_BADSPEC, "line %d: second %%prep", spec->lineNum);
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
	if (res) {
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
	rpmError(RPMERR_BADSPEC, "Error parsing %%setup: %s",
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
	    rpmError(RPMERR_BADSPEC, "line %d: Bad arg to %%setup %c: %s",
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
	rpmError(RPMERR_BADSPEC, "line %d: Bad %%setup option %s: %s",
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
    sprintf(buf, "cd %s", rpmGetVar(RPMVAR_BUILDDIR));
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
	appendLineStringBuf(spec->prep, "chgrp -R " ROOT_GROUP " .");
    }

    if (rpmGetVar(RPMVAR_FIXPERMS)) {
	appendStringBuf(spec->prep, "chmod -R ");
	appendStringBuf(spec->prep, rpmGetVar(RPMVAR_FIXPERMS));
	appendLineStringBuf(spec->prep, " .");
    }
    
    return 0;
}

static char *doUntar(Spec spec, int c, int quietly)
{
    static char buf[BUFSIZ];
    char file[BUFSIZ];
    char *s, *taropts;
    struct Source *sp;
    int compressed;

    s = NULL;
    sp = spec->sources;
    while (sp) {
	if ((sp->flags & RPMBUILD_ISSOURCE) && (sp->num == c)) {
	    s = sp->source;
	    break;
	}
	sp = sp->next;
    }
    if (! s) {
	rpmError(RPMERR_BADSPEC, "No source number %d", c);
	return NULL;
    }

    sprintf(file, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), s);
    taropts = (rpmIsVerbose() && !quietly ? "-xvvf" : "-xf");

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

static int doPatchMacro(Spec spec, char *line)
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E;
    char *s;
    char buf[BUFSIZ];
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
    
    strtok(buf, " \t\n");  /* remove %patch */
    while ((s = strtok(NULL, " \t\n"))) {
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
		rpmError(RPMERR_BADSPEC, "line %d: Need arg to %%patch -b: %s",
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, "line %d: Need arg to %%patch -z: %s",
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else if (!strncmp(s, "-p", 2)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (! s) {
		    rpmError(RPMERR_BADSPEC,
			     "line %d: Need arg to %%patch -p: %s",
			     spec->lineNum, spec->line);
		    return RPMERR_BADSPEC;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmError(RPMERR_BADSPEC, "line %d: Bad arg to %%patch -p: %s",
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmError(RPMERR_BADSPEC, "Too many patches!");
		return RPMERR_BADSPEC;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmError(RPMERR_BADSPEC, "line %d: Bad arg to %%patch: %s",
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    patch_index++;
	}
    }

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E);
	if (! s) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
    }

    x = 0;
    while (x < patch_index) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E);
	if (! s) {
	    return RPMERR_BADSPEC;
	}
	appendLineStringBuf(spec->prep, s);
	x++;
    }
    
    return 0;
}

static char *doPatch(Spec spec, int c, int strip, char *db,
		     int reverse, int removeEmpties)
{
    static char buf[BUFSIZ];
    char file[BUFSIZ];
    char args[BUFSIZ];
    char *s;
    struct Source *sp;
    int compressed;

    s = NULL;
    sp = spec->sources;
    while (sp) {
	if ((sp->flags & RPMBUILD_ISPATCH) && (sp->num == c)) {
	    s = sp->source;
	    break;
	}
	sp = sp->next;
    }
    if (! s) {
	rpmError(RPMERR_BADSPEC, "No patch number %d", c);
	return NULL;
    }

    sprintf(file, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), s);

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

static int checkOwners(char *file)
{
    struct stat sb;

    if (lstat(file, &sb)) {
	rpmError(RPMERR_BADSPEC, "Bad source: %s: %s", file, strerror(errno));
	return RPMERR_BADSPEC;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmError(RPMERR_BADSPEC, "Bad owner/group: %s", file);
	return RPMERR_BADSPEC;
    }

    return 0;
}

static int isCompressed(char *file, int *compressed)
{
    int fd;
    unsigned char magic[4];

    *compressed = COMPRESSED_NOT;

    if (!(fd = open(file, O_RDONLY))) {
	return 1;
    }
    if (! read(fd, magic, 4)) {
	return 1;
    }
    close(fd);

    if (((magic[0] == 0037) && (magic[1] == 0213)) ||  /* gzip */
	((magic[0] == 0037) && (magic[1] == 0236)) ||  /* old gzip */
	((magic[0] == 0037) && (magic[1] == 0036)) ||  /* pack */
	((magic[0] == 0037) && (magic[1] == 0240)) ||  /* SCO lzh */
	((magic[0] == 0037) && (magic[1] == 0235)) ||  /* compress */
	((magic[0] == 0120) && (magic[1] == 0113) &&
	 (magic[2] == 0003) && (magic[3] == 0004))     /* pkzip */
	) {
	*compressed = COMPRESSED_OTHER;
    } else if ((magic[0] == 'B') && (magic[1] == 'Z')) {
	*compressed = COMPRESSED_BZIP2;
    }

    return 0;
}
