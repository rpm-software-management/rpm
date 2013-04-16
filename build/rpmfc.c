#include "system.h"

#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <magic.h>
#include <regex.h>

#include <rpm/header.h>
#include <rpm/argv.h>
#include <rpm/rpmfc.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>

#include "build/rpmbuild_internal.h"

#include "debug.h"

struct matchRule {
    regex_t *path;
    regex_t *magic;
    ARGV_t flags;
};

typedef struct rpmfcAttr_s {
    char *name;
    struct matchRule incl;
    struct matchRule excl;
} * rpmfcAttr;

/**
 */
struct rpmfc_s {
    int nfiles;		/*!< no. of files */
    int fknown;		/*!< no. of classified files */
    int fwhite;		/*!< no. of "white" files */
    int ix;		/*!< current file index */
    int skipProv;	/*!< Don't auto-generate Provides:? */
    int skipReq;	/*!< Don't auto-generate Requires:? */
    char *buildRoot;	/*!< (Build) root dir */
    size_t brlen;	/*!< rootDir length */

    rpmfcAttr *atypes;	/*!< known file attribute types */

    char ** fn;		/*!< (no. files) file names */
    ARGV_t *fattrs;	/*!< (no. files) file attribute tokens */
    rpm_color_t *fcolor;/*!< (no. files) file colors */
    rpmsid *fcdictx;	/*!< (no. files) file class dictionary indices */
    ARGI_t fddictx;	/*!< (no. files) file depends dictionary start */
    ARGI_t fddictn;	/*!< (no. files) file depends dictionary no. entries */
    ARGI_t ddictx;	/*!< (no. dependencies) file->dependency mapping */
    rpmstrPool cdict;	/*!< file class dictionary */
    rpmstrPool ddict;	/*!< file depends dictionary */

    rpmds provides;	/*!< (no. provides) package provides */
    rpmds requires;	/*!< (no. requires) package requires */
};

struct rpmfcTokens_s {
    const char * token;
    rpm_color_t colors;
};  

static int regMatch(regex_t *reg, const char *val)
{
    return (reg && regexec(reg, val, 0, NULL, 0) == 0);
}

static regex_t * regFree(regex_t *reg)
{
    if (reg) {
	regfree(reg);
	free(reg);
    }
    return NULL;
}

static void ruleFree(struct matchRule *rule)
{
    regFree(rule->path);
    regFree(rule->magic);
    argvFree(rule->flags);
}

static char *rpmfcAttrMacro(const char *name,
			    const char *attr_prefix, const char *attr)
{
    char *ret;
    if (attr_prefix && attr_prefix[0] != '\0')
	ret = rpmExpand("%{?__", name, "_", attr_prefix, "_", attr, "}", NULL);
    else
	ret = rpmExpand("%{?__", name, "_", attr, "}", NULL);
    return rstreq(ret, "") ? _free(ret) : ret;
}

static regex_t *rpmfcAttrReg(const char *name,
			     const char *attr_prefix, const char *attr)
{
    regex_t *reg = NULL;
    char *pattern = rpmfcAttrMacro(name, attr_prefix, attr);
    if (pattern) {
	reg = xcalloc(1, sizeof(*reg));
	if (regcomp(reg, pattern, REG_EXTENDED) != 0) { 
	    rpmlog(RPMLOG_WARNING, _("Ignoring invalid regex %s\n"), pattern);
	    reg = _free(reg);
	}
	rfree(pattern);
    }
    return reg;
}

static rpmfcAttr rpmfcAttrNew(const char *name)
{
    rpmfcAttr attr = xcalloc(1, sizeof(*attr));
    struct matchRule *rules[] = { &attr->incl, &attr->excl, NULL };

    attr->name = xstrdup(name);
    for (struct matchRule **rule = rules; rule && *rule; rule++) {
	const char *prefix = (*rule == &attr->incl) ? NULL : "exclude";
	char *flags = rpmfcAttrMacro(name, prefix, "flags");

	(*rule)->path = rpmfcAttrReg(name, prefix, "path");
	(*rule)->magic = rpmfcAttrReg(name, prefix, "magic");
	(*rule)->flags = argvSplitString(flags, ",", ARGV_SKIPEMPTY);
	argvSort((*rule)->flags, NULL);

	free(flags);
    }

    return attr;
}

static rpmfcAttr rpmfcAttrFree(rpmfcAttr attr)
{
    if (attr) {
	ruleFree(&attr->incl);
	ruleFree(&attr->excl);
	rfree(attr->name);
	rfree(attr);
    }
    return NULL;
}

/**
 */
static int rpmfcExpandAppend(ARGV_t * argvp, ARGV_const_t av)
{
    ARGV_t argv = *argvp;
    int argc = argvCount(argv);
    int ac = argvCount(av);
    int i;

    argv = xrealloc(argv, (argc + ac + 1) * sizeof(*argv));
    for (i = 0; i < ac; i++)
	argv[argc + i] = rpmExpand(av[i], NULL);
    argv[argc + ac] = NULL;
    *argvp = argv;
    return 0;
}

static rpmds rpmdsSingleNS(rpmTagVal tagN, const char *namespace,
			const char * N, const char * EVR, rpmsenseFlags Flags)
{
    rpmds ds = NULL;
    if (namespace) {
	char *NSN = rpmExpand(namespace, "(", N, ")", NULL);
	ds = rpmdsSingle(tagN, NSN, EVR, Flags);
	free(NSN);
    } else {
	ds = rpmdsSingle(tagN, N, EVR, Flags);
    }
    return ds;
}

#define max(x,y) ((x) > (y) ? (x) : (y))

/** \ingroup rpmbuild
 * Return output from helper script.
 * @todo Use poll(2) rather than select(2), if available.
 * @param argv		program and arguments to run
 * @param writePtr	bytes to feed to script on stdin (or NULL)
 * @param writeBytesLeft no. of bytes to feed to script on stdin
 * @param failNonZero	is script failure an error?
 * @param buildRoot	buildRoot directory (or NULL)
 * @return		buffered stdout from script, NULL on error
 */     
static StringBuf getOutputFrom(ARGV_t argv,
                        const char * writePtr, size_t writeBytesLeft,
                        int failNonZero, const char *buildRoot)
{
    pid_t child, reaped;
    int toProg[2] = { -1, -1 };
    int fromProg[2] = { -1, -1 };
    int status;
    StringBuf readBuff;
    int myerrno = 0;
    int ret = 1; /* assume failure */

    if (pipe(toProg) < 0 || pipe(fromProg) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for %s: %m\n"), argv[0]);
	return NULL;
    }
    
    child = fork();
    if (child == 0) {
	/* NSPR messes with SIGPIPE, reset to default for the kids */
	signal(SIGPIPE, SIG_DFL);
	close(toProg[1]);
	close(fromProg[0]);
	
	dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	close(toProg[0]);

	dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */
	close(fromProg[1]);

	rpmlog(RPMLOG_DEBUG, "\texecv(%s) pid %d\n",
                        argv[0], (unsigned)getpid());

	if (buildRoot)
	    setenv("RPM_BUILD_ROOT", buildRoot, 1);

	unsetenv("MALLOC_CHECK_");
	execvp(argv[0], (char *const *)argv);
	rpmlog(RPMLOG_ERR, _("Couldn't exec %s: %s\n"),
		argv[0], strerror(errno));
	_exit(EXIT_FAILURE);
    }
    if (child < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"),
		argv[0], strerror(errno));
	return NULL;
    }

    close(toProg[0]);
    close(fromProg[1]);

    readBuff = newStringBuf();

    while (1) {
	fd_set ibits, obits;
	int nfd = 0;
	ssize_t iorc;
	char buf[BUFSIZ+1];

	FD_ZERO(&ibits);
	FD_ZERO(&obits);

	FD_SET(fromProg[0], &ibits);
	nfd = max(nfd, fromProg[0]);

	if (writeBytesLeft > 0) {
	    FD_SET(toProg[1], &obits);
	    nfd = max(nfd, toProg[1]);
	} else if (toProg[1] >= 0) {
	    /* Close write-side pipe to notify child on EOF */
	    close(toProg[1]);
	    toProg[1] = -1;
	}

	do {
	    iorc = select(nfd + 1, &ibits, &obits, NULL, NULL);
	} while (iorc == -1 && errno == EINTR);

	if (iorc < 0) {
	    myerrno = errno;
	    break;
	}

	/* Write data to child */
	if (writeBytesLeft > 0 && FD_ISSET(toProg[1], &obits)) {
	    size_t nb = (1024 < writeBytesLeft) ? 1024 : writeBytesLeft;
	    do {
		iorc = write(toProg[1], writePtr, nb);
	    } while (iorc == -1 && errno == EINTR);

	    if (iorc < 0) {
		myerrno = errno;
		break;
	    }
	    writeBytesLeft -= iorc;
	    writePtr += iorc;
	}
	
	/* Read when we get data back from the child */
	if (FD_ISSET(fromProg[0], &ibits)) {
	    do {
		iorc = read(fromProg[0], buf, sizeof(buf)-1);
	    } while (iorc == -1 && errno == EINTR);

	    if (iorc == 0) break; /* EOF, we're done */
	    if (iorc < 0) {
		myerrno = errno;
		break;
	    }
	    buf[iorc] = '\0';
	    appendStringBuf(readBuff, buf);
	}
    }

    /* Clean up */
    if (toProg[1] >= 0)
    	close(toProg[1]);
    if (fromProg[0] >= 0)
	close(fromProg[0]);

    /* Collect status from prog */
    reaped = waitpid(child, &status, 0);
    rpmlog(RPMLOG_DEBUG, "\twaitpid(%d) rc %d status %x\n",
        (unsigned)child, (unsigned)reaped, status);

    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmlog(RPMLOG_ERR, _("%s failed: %x\n"), argv[0], status);
	goto exit;
    }
    if (writeBytesLeft || myerrno) {
	rpmlog(RPMLOG_ERR, _("failed to write all data to %s: %s\n"), 
		argv[0], strerror(myerrno));
	goto exit;
    }
    ret = 0;

exit:
    if (ret) {
	readBuff = freeStringBuf(readBuff);
    }

    return readBuff;
}

int rpmfcExec(ARGV_const_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero, const char *buildRoot)
{
    char * s = NULL;
    ARGV_t xav = NULL;
    ARGV_t pav = NULL;
    int pac = 0;
    int ec = -1;
    StringBuf sb = NULL;
    const char * buf_stdin = NULL;
    size_t buf_stdin_len = 0;

    if (sb_stdoutp)
	*sb_stdoutp = NULL;
    if (!(av && *av))
	goto exit;

    /* Find path to executable with (possible) args. */
    s = rpmExpand(av[0], NULL);
    if (!(s && *s))
	goto exit;

    /* Parse args buried within expanded executable. */
    if (!(poptParseArgvString(s, &pac, (const char ***)&pav) == 0 && pac > 0 && pav != NULL))
	goto exit;

    /* Build argv, appending args to the executable args. */
    argvAppend(&xav, pav);
    if (av[1])
	rpmfcExpandAppend(&xav, av + 1);

    if (sb_stdin != NULL) {
	buf_stdin = getStringBuf(sb_stdin);
	buf_stdin_len = strlen(buf_stdin);
    }

    /* Read output from exec'd helper. */
    sb = getOutputFrom(xav, buf_stdin, buf_stdin_len, failnonzero, buildRoot);

    if (sb_stdoutp != NULL) {
	*sb_stdoutp = sb;
	sb = NULL;	/* XXX don't free */
    }

    ec = 0;

exit:
    freeStringBuf(sb);
    argvFree(xav);
    free(pav);	/* XXX popt mallocs in single blob. */
    free(s);
    return ec;
}

static void argvAddUniq(ARGV_t * argvp, const char * key)
{
    if (argvSearch(*argvp, key, NULL) == NULL) {
	argvAdd(argvp, key);
	argvSort(*argvp, NULL);
    }
}

#define hasAttr(_a, _n) (argvSearch((_a), (_n), NULL) != NULL)

static void rpmfcAddFileDep(rpmstrPool ddict, int ix, rpmds ds, char deptype)
{
    if (ds) {
	char *key = NULL;
	rasprintf(&key, "%08d%c %s %s 0x%08x", ix, deptype,
		  rpmdsN(ds), rpmdsEVR(ds), rpmdsFlags(ds));
	rpmstrPoolId(ddict, key, 1);
	free(key);
    }
}

static ARGV_t runCmd(const char *nsdep, const char *depname,
		     const char *buildRoot, const char *fn)
{
    ARGV_t output = NULL;
    char *buf = NULL;
    char *mname = rstrscat(NULL, "__", nsdep, "_", depname, NULL);

    rasprintf(&buf, "%%{?%s:%%{%s} %%{?%s_opts}}", mname, mname, mname);
    if (!rstreq(buf, "")) {
	ARGV_t av = NULL;
	StringBuf sb_stdout = NULL;
	StringBuf sb_stdin = newStringBuf();
	argvAdd(&av, buf);

	appendLineStringBuf(sb_stdin, fn);
	if (rpmfcExec(av, sb_stdin, &sb_stdout, 0, buildRoot) == 0) {
	    argvSplit(&output, getStringBuf(sb_stdout), " \t\n\r");
	}

	argvFree(av);
	freeStringBuf(sb_stdin);
	freeStringBuf(sb_stdout);
    }
    free(buf);
    free(mname);
    return output;
}

/**
 * Run per-interpreter dependency helper.
 * @param fc		file classifier
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @param depname	"provides" or "requires"
 * @param depsp		fc->provides or fc->requires
 * @param dsContext	RPMSENSE_FIND_PROVIDES or RPMSENSE_FIND_REQUIRES
 * @param tagN		RPMTAG_PROVIDENAME or RPMTAG_REQUIRENAME
 * @return		0
 */
static int rpmfcHelper(rpmfc fc, const char *nsdep, const char *depname,
		       rpmds *depsp, rpmsenseFlags dsContext, rpmTagVal tagN)
{
    ARGV_t pav = NULL;
    const char * fn = fc->fn[fc->ix];
    char *namespace = NULL;
    int pac;
    regex_t *exclude = NULL;
    regex_t *exclude_from = NULL;

    /* If the entire path is filtered out, there's nothing more to do */
    exclude_from = rpmfcAttrReg(depname, "exclude", "from");
    if (regMatch(exclude_from, fn+fc->brlen))
	goto exit;

    pav = runCmd(nsdep, depname, fc->buildRoot, fn);
    if (pav == NULL)
	goto exit;

    pac = argvCount(pav);
    namespace = rpmfcAttrMacro(nsdep, NULL, "namespace");
    exclude = rpmfcAttrReg(depname, NULL, "exclude");

    for (int i = 0; i < pac; i++) {
	rpmds ds = NULL;
	const char *N = pav[i];
	const char *EVR = "";
	rpmsenseFlags Flags = dsContext;
	if (pav[i+1] && strchr("=<>", *pav[i+1])) {
	    i++;
	    for (const char *s = pav[i]; *s; s++) {
		switch(*s) {
		default:
		    break;
		case '=':
		    Flags |= RPMSENSE_EQUAL;
		    break;
		case '<':
		    Flags |= RPMSENSE_LESS;
		    break;
		case '>':
		    Flags |= RPMSENSE_GREATER;
		    break;
		}
	    }
	    i++;
	    EVR = pav[i];
	}

	ds = rpmdsSingleNS(tagN, namespace, N, EVR, Flags);

	/* Add to package and file dependencies unless filtered */
	if (regMatch(exclude, rpmdsDNEVR(ds)+2) == 0) {
	    (void) rpmdsMerge(depsp, ds);
	    rpmfcAddFileDep(fc->ddict, fc->ix, ds, tagN == RPMTAG_PROVIDENAME ? 'P' : 'R');
	}

	rpmdsFree(ds);
    }

    argvFree(pav);
    regFree(exclude);
    free(namespace);

exit:
    regFree(exclude_from);
    return 0;
}

/**
 * Run per-interpreter Provides: dependency helper.
 * @param fc		file classifier
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @return		0
 */
static int rpmfcHelperProvides(rpmfc fc, const char * nsdep)
{
    if (fc->skipProv)
	return 0;

    rpmfcHelper(fc, nsdep, "provides", &fc->provides, RPMSENSE_FIND_PROVIDES, RPMTAG_PROVIDENAME);

    return 0;
}

/**
 * Run per-interpreter Requires: dependency helper.
 * @param fc		file classifier
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @return		0
 */
static int rpmfcHelperRequires(rpmfc fc, const char * nsdep)
{
    if (fc->skipReq)
	return 0;

    rpmfcHelper(fc, nsdep, "requires", &fc->requires, RPMSENSE_FIND_REQUIRES, RPMTAG_REQUIRENAME);

    return 0;
}

/* Only used for elf coloring and controlling RPMTAG_FILECLASS inclusion now */
static const struct rpmfcTokens_s rpmfcTokens[] = {
  { "directory",		RPMFC_INCLUDE },

  { "ELF 32-bit",		RPMFC_ELF32|RPMFC_INCLUDE },
  { "ELF 64-bit",		RPMFC_ELF64|RPMFC_INCLUDE },

  { "troff or preprocessor input",	RPMFC_INCLUDE },
  { "GNU Info",			RPMFC_INCLUDE },

  { "perl ",			RPMFC_INCLUDE },
  { "Perl5 module source text", RPMFC_INCLUDE },
  { "python ",			RPMFC_INCLUDE },

  { "libtool library ",         RPMFC_INCLUDE },
  { "pkgconfig ",               RPMFC_INCLUDE },

  { "Objective caml ",		RPMFC_INCLUDE },
  { "Mono/.Net assembly",       RPMFC_INCLUDE },

  { "current ar archive",	RPMFC_INCLUDE },
  { "Zip archive data",		RPMFC_INCLUDE },
  { "tar archive",		RPMFC_INCLUDE },
  { "cpio archive",		RPMFC_INCLUDE },
  { "RPM v3",			RPMFC_INCLUDE },
  { "RPM v4",			RPMFC_INCLUDE },

  { " image",			RPMFC_INCLUDE },
  { " font",			RPMFC_INCLUDE },
  { " Font",			RPMFC_INCLUDE },

  { " commands",		RPMFC_INCLUDE },
  { " script",			RPMFC_INCLUDE },

  { "empty",			RPMFC_INCLUDE },

  { "HTML",			RPMFC_INCLUDE },
  { "SGML",			RPMFC_INCLUDE },
  { "XML",			RPMFC_INCLUDE },

  { " source",			RPMFC_INCLUDE },
  { "GLS_BINARY_LSB_FIRST",	RPMFC_INCLUDE },
  { " DB ",			RPMFC_INCLUDE },

  { " text",			RPMFC_INCLUDE },

  { NULL,			RPMFC_BLACK }
};

static void argvAddTokens(ARGV_t *argv, const char *tnames)
{
    if (tnames) {
	ARGV_t tokens = NULL;
	argvSplit(&tokens, tnames, ",");
	for (ARGV_t token = tokens; token && *token; token++)
	    argvAddUniq(argv, *token);
	argvFree(tokens);
    }
}

static int matches(const struct matchRule *rule,
		   const char *ftype, const char *path, int executable)
{
    if (!executable && hasAttr(rule->flags, "exeonly"))
	return 0;
    if (rule->magic && rule->path && hasAttr(rule->flags, "magic_and_path")) {
	return (regMatch(rule->magic, ftype) && regMatch(rule->path, path));
    } else {
	return (regMatch(rule->magic, ftype) || regMatch(rule->path, path));
    }
}

static void rpmfcAttributes(rpmfc fc, const char *ftype, const char *fullpath)
{
    const char *path = fullpath + fc->brlen;
    int is_executable = 0;
    struct stat st;
    if (stat(fullpath, &st) == 0) {
	is_executable = (S_ISREG(st.st_mode)) &&
			(st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    }

    for (rpmfcAttr *attr = fc->atypes; attr && *attr; attr++) {
	/* Filter out excludes */
	if (matches(&(*attr)->excl, ftype, path, is_executable))
	    continue;

	/* Add attributes on libmagic type & path pattern matches */
	if (matches(&(*attr)->incl, ftype, path, is_executable))
	    argvAddTokens(&fc->fattrs[fc->ix], (*attr)->name);
    }
}

/* Return color for a given libmagic classification string */
static rpm_color_t rpmfcColor(const char * fmstr)
{
    rpmfcToken fct;
    rpm_color_t fcolor = RPMFC_BLACK;

    for (fct = rpmfcTokens; fct->token != NULL; fct++) {
	if (strstr(fmstr, fct->token) == NULL)
	    continue;

	fcolor |= fct->colors;
	if (fcolor & RPMFC_INCLUDE)
	    break;
    }

    return fcolor;
}

void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
{
    rpm_color_t fcolor;
    int ndx;
    int dx;
    int fx;

int nprovides;
int nrequires;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

nprovides = rpmdsCount(fc->provides);
nrequires = rpmdsCount(fc->requires);

    if (fc)
    for (fx = 0; fx < fc->nfiles; fx++) {
	rpmsid cx = fc->fcdictx[fx] + 1; /* id's are one off */
	fcolor = fc->fcolor[fx];
	ARGV_t fattrs = fc->fattrs[fx];

	fprintf(fp, "%3d %s", fx, fc->fn[fx]);
	if (fcolor != RPMFC_BLACK)
		fprintf(fp, "\t0x%x", fc->fcolor[fx]);
	else
		fprintf(fp, "\t%s", rpmstrPoolStr(fc->cdict, cx));
	if (fattrs) {
	    char *attrs = argvJoin(fattrs, ",");
	    fprintf(fp, " [%s]", attrs);
	    free(attrs);
	} else {
	    fprintf(fp, " [none]");
	}
	fprintf(fp, "\n");

	if (fc->fddictx == NULL || fc->fddictn == NULL)
	    continue;

assert(fx < fc->fddictx->nvals);
	dx = fc->fddictx->vals[fx];
assert(fx < fc->fddictn->nvals);
	ndx = fc->fddictn->vals[fx];

	while (ndx-- > 0) {
	    const char * depval;
	    unsigned char deptype;
	    unsigned ix;

	    ix = fc->ddictx->vals[dx++];
	    deptype = ((ix >> 24) & 0xff);
	    ix &= 0x00ffffff;
	    depval = NULL;
	    switch (deptype) {
	    default:
assert(depval != NULL);
		break;
	    case 'P':
		if (nprovides > 0) {
assert(ix < nprovides);
		    (void) rpmdsSetIx(fc->provides, ix-1);
		    if (rpmdsNext(fc->provides) >= 0)
			depval = rpmdsDNEVR(fc->provides);
		}
		break;
	    case 'R':
		if (nrequires > 0) {
assert(ix < nrequires);
		    (void) rpmdsSetIx(fc->requires, ix-1);
		    if (rpmdsNext(fc->requires) >= 0)
			depval = rpmdsDNEVR(fc->requires);
		}
		break;
	    }
	    if (depval)
		fprintf(fp, "\t%s\n", depval);
	}
    }
}

rpmfc rpmfcFree(rpmfc fc)
{
    if (fc) {
	for (rpmfcAttr *attr = fc->atypes; attr && *attr; attr++)
	    rpmfcAttrFree(*attr);
	free(fc->atypes);
	free(fc->buildRoot);
	for (int i = 0; i < fc->nfiles; i++) {
	    free(fc->fn[i]);
	    argvFree(fc->fattrs[i]);
	}
	free(fc->fn);
	free(fc->fattrs);
	free(fc->fcolor);
	free(fc->fcdictx);
	argiFree(fc->fddictx);
	argiFree(fc->fddictn);
	argiFree(fc->ddictx);

	rpmstrPoolFree(fc->ddict);
	rpmstrPoolFree(fc->cdict);

	rpmdsFree(fc->provides);
	rpmdsFree(fc->requires);
	memset(fc, 0, sizeof(*fc)); /* trash and burn */
	free(fc);
    }
    return NULL;
}

rpmfc rpmfcCreate(const char *buildRoot, rpmFlags flags)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    if (buildRoot) {
	fc->buildRoot = xstrdup(buildRoot);
	fc->brlen = strlen(buildRoot);
    }
    return fc;
}

rpmfc rpmfcNew(void)
{
    return rpmfcCreate(NULL, 0);
}

rpmds rpmfcProvides(rpmfc fc)
{
    return (fc != NULL ? fc->provides : NULL);
}

rpmds rpmfcRequires(rpmfc fc)
{
    return (fc != NULL ? fc->requires : NULL);
}

rpmRC rpmfcApply(rpmfc fc)
{
    const char * s;
    char * se;
    rpmds ds;
    const char * N;
    const char * EVR;
    rpmsenseFlags Flags;
    unsigned char deptype;
    int nddict;
    int previx;
    unsigned int val;
    int dix;
    int ix;

    /* Generate package and per-file dependencies. */
    for (fc->ix = 0; fc->ix < fc->nfiles && fc->fn[fc->ix] != NULL; fc->ix++) {
	for (ARGV_t fattr = fc->fattrs[fc->ix]; fattr && *fattr; fattr++) {
	    rpmfcHelperProvides(fc, *fattr);
	    rpmfcHelperRequires(fc, *fattr);
	}
    }
    /* No more additions after this, freeze pool to minimize memory use */
    rpmstrPoolFreeze(fc->ddict, 0);

    /* Generate per-file indices into package dependencies. */
    nddict = rpmstrPoolNumStr(fc->ddict);
    previx = -1;
    for (rpmsid id = 1; id <= nddict; id++) {
	s = rpmstrPoolStr(fc->ddict, id);

	/* Parse out (file#,deptype,N,EVR,Flags) */
	ix = strtol(s, &se, 10);
	if ( se == NULL ) {
		rpmlog(RPMLOG_ERR, _("Conversion of %s to long integer failed.\n"), s);
		return RPMRC_FAIL;
	}
	
	deptype = *se++;
	se++;
	N = se;
	while (*se && *se != ' ')
	    se++;
	*se++ = '\0';
	EVR = se;
	while (*se && *se != ' ')
	    se++;
	*se++ = '\0';
	Flags = strtol(se, NULL, 16);

	dix = -1;
	switch (deptype) {
	default:
	    break;
	case 'P':
	    ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->provides, ds);
	    rpmdsFree(ds);
	    break;
	case 'R':
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->requires, ds);
	    rpmdsFree(ds);
	    break;
	}

	if (dix < 0)
	    continue;

	val = (deptype << 24) | (dix & 0x00ffffff);
	argiAdd(&fc->ddictx, -1, val);

	if (previx != ix) {
	    previx = ix;
	    argiAdd(&fc->fddictx, ix, argiCount(fc->ddictx)-1);
	}
	if (fc->fddictn && fc->fddictn->vals)
	    fc->fddictn->vals[ix]++;
    }

    return RPMRC_OK;
}

static int initAttrs(rpmfc fc)
{
    ARGV_t files = NULL;
    char * attrPath = rpmExpand("%{_fileattrsdir}/*.attr", NULL);
    int nattrs = 0;

    /* Discover known attributes from pathnames + initialize them */
    if (rpmGlob(attrPath, NULL, &files) == 0) {
	nattrs = argvCount(files);
	fc->atypes = xcalloc(nattrs + 1, sizeof(*fc->atypes));
	for (int i = 0; i < nattrs; i++) {
	    char *bn = basename(files[i]);
	    bn[strlen(bn)-strlen(".attr")] = '\0';
	    fc->atypes[i] = rpmfcAttrNew(bn);
	}
	fc->atypes[nattrs] = NULL;
	argvFree(files);
    }
    free(attrPath);
    return nattrs;
}

rpmRC rpmfcClassify(rpmfc fc, ARGV_t argv, rpm_mode_t * fmode)
{
    int msflags = MAGIC_CHECK | MAGIC_COMPRESS | MAGIC_NO_CHECK_TOKENS;
    magic_t ms = NULL;
    rpmRC rc = RPMRC_FAIL;

    if (fc == NULL) {
	rpmlog(RPMLOG_ERR, _("Empty file classifier\n"));
	goto exit;
    }

    /* It is OK when we have no files to classify. */
    if (argv == NULL)
	return RPMRC_OK;

    if (initAttrs(fc) < 1) {
	rpmlog(RPMLOG_ERR, _("No file attributes configured\n"));
	goto exit;
    }

    fc->nfiles = argvCount(argv);
    fc->fn = xcalloc(fc->nfiles, sizeof(*fc->fn));
    fc->fattrs = xcalloc(fc->nfiles, sizeof(*fc->fattrs));
    fc->fcolor = xcalloc(fc->nfiles, sizeof(*fc->fcolor));
    fc->fcdictx = xcalloc(fc->nfiles, sizeof(*fc->fcdictx));

    /* Initialize the per-file dictionary indices. */
    argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Build (sorted) file class dictionary. */
    fc->cdict = rpmstrPoolCreate();
    fc->ddict = rpmstrPoolCreate();

    ms = magic_open(msflags);
    if (ms == NULL) {
	rpmlog(RPMLOG_ERR, _("magic_open(0x%x) failed: %s\n"),
		msflags, strerror(errno));
	goto exit;
    }

    if (magic_load(ms, NULL) == -1) {
	rpmlog(RPMLOG_ERR, _("magic_load failed: %s\n"), magic_error(ms));
	goto exit;
    }

    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	rpmsid ftypeId;
	const char * ftype;
	const char * s = argv[fc->ix];
	size_t slen = strlen(s);
	int fcolor = RPMFC_BLACK;
	rpm_mode_t mode = (fmode ? fmode[fc->ix] : 0);
	int is_executable = (mode & (S_IXUSR|S_IXGRP|S_IXOTH));

	switch (mode & S_IFMT) {
	case S_IFCHR:	ftype = "character special";	break;
	case S_IFBLK:	ftype = "block special";	break;
	case S_IFIFO:	ftype = "fifo (named pipe)";	break;
	case S_IFSOCK:	ftype = "socket";		break;
	case S_IFDIR:	ftype = "directory";		break;
	case S_IFLNK:
	case S_IFREG:
	default:
	    /* XXX all files with extension ".pm" are perl modules for now. */
	    if (rpmFileHasSuffix(s, ".pm"))
		ftype = "Perl5 module source text";

	    /* XXX all files with extension ".la" are libtool for now. */
	    else if (rpmFileHasSuffix(s, ".la"))
		ftype = "libtool library file";

	    /* XXX all files with extension ".pc" are pkgconfig for now. */
	    else if (rpmFileHasSuffix(s, ".pc"))
		ftype = "pkgconfig file";

	    /* XXX skip all files in /dev/ which are (or should be) %dev dummies. */
	    else if (slen >= fc->brlen+sizeof("/dev/") && rstreqn(s+fc->brlen, "/dev/", sizeof("/dev/")-1))
		ftype = "";
	    else
		ftype = magic_file(ms, s);

	    if (ftype == NULL) {
		rpmlog(is_executable ? RPMLOG_ERR : RPMLOG_WARNING, 
		       _("Recognition of file \"%s\" failed: mode %06o %s\n"),
		       s, mode, magic_error(ms));
		/* only executable files are critical to dep extraction */
		if (is_executable) {
		    goto exit;
		}
		/* unrecognized non-executables get treated as "data" */
		ftype = "data";
	    }
	}

	rpmlog(RPMLOG_DEBUG, "%s: %s\n", s, ftype);

	/* Save the path. */
	fc->fn[fc->ix] = xstrdup(s);

	/* Add (filtered) file coloring */
	fcolor |= rpmfcColor(ftype);

	/* Add attributes based on file type and/or path */
	rpmfcAttributes(fc, ftype, s);

	fc->fcolor[fc->ix] = fcolor;

	/* Add to file class dictionary and index array */
	if (fcolor != RPMFC_WHITE && (fcolor & RPMFC_INCLUDE)) {
	    ftypeId = rpmstrPoolId(fc->cdict, ftype, 1);
	    fc->fknown++;
	} else {
	    ftypeId = rpmstrPoolId(fc->cdict, "", 1);
	    fc->fwhite++;
	}
	/* Pool id's start from 1, for headers we want it from 0 */
	fc->fcdictx[fc->ix] = ftypeId - 1;
    }
    rc = RPMRC_OK;

exit:
    /* No more additions after this, freeze pool to minimize memory use */
    rpmstrPoolFreeze(fc->cdict, 0);
    if (ms != NULL)
	magic_close(ms);

    return rc;
}

/**
 */
typedef struct DepMsg_s * DepMsg_t;

/**
 */
struct DepMsg_s {
    const char * msg;
    char * const argv[4];
    rpmTagVal ntag;
    rpmTagVal vtag;
    rpmTagVal ftag;
    int mask;
    int xormask;
};

/**
 */
static struct DepMsg_s depMsgs[] = {
  { "Provides",		{ "%{?__find_provides}", NULL, NULL, NULL },
	RPMTAG_PROVIDENAME, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS,
	0, -1 },
  { "Requires(interp)",	{ NULL, "interp", NULL, NULL },
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	RPMSENSE_INTERP, 0 },
  { "Requires(rpmlib)",	{ NULL, "rpmlib", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_RPMLIB, 0 },
  { "Requires(verify)",	{ NULL, "verify", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_VERIFY, 0 },
  { "Requires(pre)",	{ NULL, "pre", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_PRE, 0 },
  { "Requires(post)",	{ NULL, "post", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_POST, 0 },
  { "Requires(preun)",	{ NULL, "preun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_PREUN, 0 },
  { "Requires(postun)",	{ NULL, "postun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_POSTUN, 0 },
  { "Requires(pretrans)",	{ NULL, "pretrans", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_PRETRANS, 0 },
  { "Requires(posttrans)",	{ NULL, "posttrans", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_POSTTRANS, 0 },
  { "Requires",		{ "%{?__find_requires}", NULL, NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,	/* XXX inherit name/version arrays */
	RPMSENSE_FIND_REQUIRES|RPMSENSE_TRIGGERIN|RPMSENSE_TRIGGERUN|RPMSENSE_TRIGGERPOSTUN|RPMSENSE_TRIGGERPREIN, 0 },
  { "Conflicts",	{ "%{?__find_conflicts}", NULL, NULL, NULL },
	RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS,
	0, -1 },
  { "Obsoletes",	{ "%{?__find_obsoletes}", NULL, NULL, NULL },
	RPMTAG_OBSOLETENAME, RPMTAG_OBSOLETEVERSION, RPMTAG_OBSOLETEFLAGS,
	0, -1 },
  { NULL,		{ NULL, NULL, NULL, NULL },	0, 0, 0, 0, 0 }
};

static DepMsg_t DepMsgs = depMsgs;

/**
 */
static void printDeps(Header h)
{
    DepMsg_t dm;
    rpmds ds = NULL;
    const char * DNEVR;
    rpmsenseFlags Flags;
    int bingo = 0;

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	if (dm->ntag != -1) {
	    rpmdsFree(ds);
	    ds = rpmdsNew(h, dm->ntag, 0);
	}
	if (dm->ftag == 0)
	    continue;

	ds = rpmdsInit(ds);
	if (ds == NULL)
	    continue;	/* XXX can't happen */

	bingo = 0;
	while (rpmdsNext(ds) >= 0) {

	    Flags = rpmdsFlags(ds);
	
	    if (!((Flags & dm->mask) ^ dm->xormask))
		continue;
	    if (bingo == 0) {
		rpmlog(RPMLOG_NOTICE, "%s:", (dm->msg ? dm->msg : ""));
		bingo = 1;
	    }
	    if ((DNEVR = rpmdsDNEVR(ds)) == NULL)
		continue;	/* XXX can't happen */
	    rpmlog(RPMLOG_NOTICE, " %s", DNEVR+2);
	}
	if (bingo)
	    rpmlog(RPMLOG_NOTICE, "\n");
    }
    rpmdsFree(ds);
}

static rpmRC rpmfcGenerateDependsHelper(const rpmSpec spec, Package pkg, rpmfi fi)
{
    StringBuf sb_stdin = newStringBuf();
    rpmRC rc = RPMRC_OK;

    /* Create file manifest buffer to deliver to dependency finder. */
    fi = rpmfiInit(fi, 0);
    while (rpmfiNext(fi) >= 0)
	appendLineStringBuf(sb_stdin, rpmfiFN(fi));

    for (DepMsg_t dm = DepMsgs; dm->msg != NULL; dm++) {
	rpmTagVal tag = (dm->ftag > 0) ? dm->ftag : dm->ntag;
	rpmsenseFlags tagflags;
	char * s = NULL;
	StringBuf sb_stdout = NULL;
	int failnonzero = (tag == RPMTAG_PROVIDEFLAGS);

	switch(tag) {
	case RPMTAG_PROVIDEFLAGS:
	    if (!pkg->autoProv)
		continue;
	    tagflags = RPMSENSE_FIND_PROVIDES;
	    break;
	case RPMTAG_REQUIREFLAGS:
	    if (!pkg->autoReq)
		continue;
	    tagflags = RPMSENSE_FIND_REQUIRES;
	    break;
	default:
	    continue;
	    break;
	}

	s = rpmExpand(dm->argv[0], NULL);
	rpmlog(RPMLOG_NOTICE, _("Finding  %s: %s\n"), dm->msg, s);
	free(s);

	if (rpmfcExec(dm->argv, sb_stdin, &sb_stdout,
			failnonzero, spec->buildRoot) == -1)
	    continue;

	if (sb_stdout == NULL) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}

	/* Parse dependencies into header */
	rc = parseRCPOT(spec, pkg, getStringBuf(sb_stdout), tag, 0, tagflags);
	freeStringBuf(sb_stdout);

	if (rc) {
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}
    }

    freeStringBuf(sb_stdin);

    return rc;
}

rpmRC rpmfcGenerateDepends(const rpmSpec spec, Package pkg)
{
    rpmfi fi = pkg->cpioList;
    rpmfc fc = NULL;
    ARGV_t av = NULL;
    rpm_mode_t * fmode = NULL;
    int ac = rpmfiFC(fi);
    int genConfigDeps = 0;
    rpmRC rc = RPMRC_OK;
    int idx;
    struct rpmtd_s td;

    /* Skip packages with no files. */
    if (ac <= 0)
	goto exit;

    /* Skip packages that have dependency generation disabled. */
    if (! (pkg->autoReq || pkg->autoProv))
	goto exit;

    /* If new-fangled dependency generation is disabled ... */
    if (!rpmExpandNumeric("%{?_use_internal_dependency_generator}")) {
	/* ... then generate dependencies using %{__find_requires} et al. */
	rc = rpmfcGenerateDependsHelper(spec, pkg, fi);
	goto exit;
    }

    /* Extract absolute file paths in argv format. */
    av = xcalloc(ac+1, sizeof(*av));
    fmode = xcalloc(ac+1, sizeof(*fmode));

    fi = rpmfiInit(fi, 0);
    while ((idx = rpmfiNext(fi)) >= 0) {
	/* Does package have any %config files? */
	genConfigDeps |= (rpmfiFFlags(fi) & RPMFILE_CONFIG);

	av[idx] = xstrdup(rpmfiFN(fi));
	fmode[idx] = rpmfiFMode(fi);
    }
    av[ac] = NULL;

    fc = rpmfcCreate(spec->buildRoot, 0);
    fc->skipProv = !pkg->autoProv;
    fc->skipReq = !pkg->autoReq;

    /* Copy (and delete) manually generated dependencies to dictionary. */
    if (!fc->skipProv) {
	rpmdsMerge(&fc->provides, pkg->provides);

	headerDel(pkg->header, RPMTAG_PROVIDENAME);
	headerDel(pkg->header, RPMTAG_PROVIDEVERSION);
	headerDel(pkg->header, RPMTAG_PROVIDEFLAGS);

	/* Add config dependency, Provides: config(N) = EVR */
	if (genConfigDeps) {
	    rpmds ds = rpmdsSingleNS(RPMTAG_PROVIDENAME, "config",
			       rpmdsN(pkg->ds), rpmdsEVR(pkg->ds),
			       (RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    rpmdsMerge(&fc->provides, ds);
	    rpmdsFree(ds);
	}
    }

    if (!fc->skipReq) {
	rpmdsMerge(&fc->requires, pkg->requires);

	headerDel(pkg->header, RPMTAG_REQUIRENAME);
	headerDel(pkg->header, RPMTAG_REQUIREVERSION);
	headerDel(pkg->header, RPMTAG_REQUIREFLAGS);

	/* Add config dependency,  Requires: config(N) = EVR */
	if (genConfigDeps) {
	    rpmds ds = rpmdsSingleNS(RPMTAG_REQUIRENAME, "config",
			       rpmdsN(pkg->ds), rpmdsEVR(pkg->ds),
			       (RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    rpmdsMerge(&fc->requires, ds);
	    rpmdsFree(ds);
	}
    }

    /* Build file class dictionary. */
    rc = rpmfcClassify(fc, av, fmode);
    if ( rc != RPMRC_OK )
	goto exit;

    /* Build file/package dependency dictionary. */
    rc = rpmfcApply(fc);
    if ( rc != RPMRC_OK )
	goto exit;

    /* Add per-file colors(#files) */
    /* XXX Make sure only primary (i.e. Elf32/Elf64) colors are added. */
    for (int i = 0; i < fc->nfiles; i++)
	fc->fcolor[i] &= 0x0f;
    headerPutUint32(pkg->header, RPMTAG_FILECOLORS, fc->fcolor, fc->nfiles);
    
    /* Add classes(#classes) */
    for (rpmsid id = 1; id <= rpmstrPoolNumStr(fc->cdict); id++) {
	headerPutString(pkg->header, RPMTAG_CLASSDICT,
			rpmstrPoolStr(fc->cdict, id));
    }

    /* Add per-file classes(#files) */
    headerPutUint32(pkg->header, RPMTAG_FILECLASS, fc->fcdictx, fc->nfiles);

    /* Add Provides: */
    if (!fc->skipProv) {
	rpmds pi = rpmdsInit(fc->provides);
	while (rpmdsNext(pi) >= 0) {
	    rpmsenseFlags flags = rpmdsFlags(pi);
	
	    headerPutString(pkg->header, RPMTAG_PROVIDENAME, rpmdsN(pi));
	    headerPutString(pkg->header, RPMTAG_PROVIDEVERSION, rpmdsEVR(pi));
	    headerPutUint32(pkg->header, RPMTAG_PROVIDEFLAGS, &flags, 1);
	}
    }

    /* Add Requires: */
    if (!fc->skipReq) {
	rpmds pi = rpmdsInit(fc->requires);
	while (rpmdsNext(pi) >= 0) {
	    rpmsenseFlags flags = rpmdsFlags(pi);
	
	    headerPutString(pkg->header, RPMTAG_REQUIRENAME, rpmdsN(pi));
	    headerPutString(pkg->header, RPMTAG_REQUIREVERSION, rpmdsEVR(pi));
	    headerPutUint32(pkg->header, RPMTAG_REQUIREFLAGS, &flags, 1);
	}
    }

    /* Add dependency dictionary(#dependencies) */
    if (rpmtdFromArgi(&td, RPMTAG_DEPENDSDICT, fc->ddictx)) {
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    /* Add per-file dependency (start,number) pairs (#files) */
    if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSX, fc->fddictx)) {
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSN, fc->fddictn)) {
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }


    if (_rpmfc_debug) {
	char *msg = NULL;
	rasprintf(&msg, "final: files %d cdict[%d] %d%% ddictx[%d]",
		  fc->nfiles, rpmstrPoolNumStr(fc->cdict),
		  ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
	rpmfcPrint(msg, fc, NULL);
	free(msg);
    }
exit:
    printDeps(pkg->header);

    /* Clean up. */
    free(fmode);
    rpmfcFree(fc);
    argvFree(av);

    return rc;
}
