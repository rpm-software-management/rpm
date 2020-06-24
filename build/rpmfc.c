#include "system.h"

#include <errno.h>
#include <libgen.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <magic.h>
#include <regex.h>
#ifdef HAVE_LIBELF
#include <gelf.h>
#endif

#include <rpm/header.h>
#include <rpm/argv.h>
#include <rpm/rpmfc.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>

#include "lib/rpmfi_internal.h"		/* rpmfiles stuff for now */
#include "build/rpmbuild_internal.h"

#include "debug.h"

struct matchRule {
    regex_t *path;
    regex_t *magic;
    regex_t *mime;
    ARGV_t flags;
};

typedef struct rpmfcAttr_s {
    char *name;
    struct matchRule incl;
    struct matchRule excl;
} * rpmfcAttr;

typedef struct {
    int fileIx;
    rpmds dep;
} rpmfcFileDep;

typedef struct {
    rpmfcFileDep *data;
    int size;
    int alloced;
} rpmfcFileDeps;

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE fattrHash
#define HTKEYTYPE int
#define HTDATATYPE int
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

struct rpmfc_s {
    Package pkg;
    int nfiles;		/*!< no. of files */
    int fknown;		/*!< no. of classified files */
    int fwhite;		/*!< no. of "white" files */
    int skipProv;	/*!< Don't auto-generate Provides:? */
    int skipReq;	/*!< Don't auto-generate Requires:? */
    char *buildRoot;	/*!< (Build) root dir */
    size_t brlen;	/*!< rootDir length */

    rpmfcAttr *atypes;	/*!< known file attribute types */

    char ** fn;		/*!< (no. files) file names */
    char ** ftype;	/*!< (no. files) file types */
    ARGV_t *fattrs;	/*!< (no. files) file attribute tokens */
    rpm_color_t *fcolor;/*!< (no. files) file colors */
    rpmsid *fcdictx;	/*!< (no. files) file class dictionary indices */
    ARGI_t fddictx;	/*!< (no. files) file depends dictionary start */
    ARGI_t fddictn;	/*!< (no. files) file depends dictionary no. entries */
    ARGI_t ddictx;	/*!< (no. dependencies) file->dependency mapping */
    rpmstrPool cdict;	/*!< file class dictionary */
    rpmfcFileDeps fileDeps; /*!< file dependency mapping */

    fattrHash fahash;	/*!< attr:file mapping */
    rpmstrPool pool;	/*!< general purpose string storage */
};

struct rpmfcTokens_s {
    const char * token;
    rpm_color_t colors;
};  

static int intCmp(int a, int b)
{
    return (a != b);
}

static unsigned int intId(int a)
{
    return a;
}


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
    regFree(rule->mime);
    argvFree(rule->flags);
}

static char *rpmfcAttrMacroV(const char *arg, va_list args)
{
    const char *s;
    int blen;
    char *buf = NULL, *obuf;
    char *pe;
    va_list args2;

    if (arg == NULL || rstreq(arg, ""))
	return NULL;

    va_copy(args2, args);
    blen = sizeof("%{?_") - 1;
    for (s = arg; s != NULL; s = va_arg(args, const char *)) {
	blen += sizeof("_") - 1 + strlen(s);
    }
    blen += sizeof("}") - 1;

    buf = xmalloc(blen + 1);

    pe = buf;
    pe = stpcpy(pe, "%{?_");
    for (s = arg; s != NULL; s = va_arg(args2, const char *)) {
	*pe++ = '_';
	pe = stpcpy(pe, s);
    }
    va_end(args2);
    *pe++ = '}';
    *pe = '\0';

    obuf = rpmExpand(buf, NULL);
    free(buf);

    return rstreq(obuf, "") ? _free(obuf) : obuf;
}

static char *rpmfcAttrMacro(const char *arg, ...)
{
    va_list args;
    char *s;

    va_start(args, arg);
    s = rpmfcAttrMacroV(arg, args);
    va_end(args);
    return s;
}

static regex_t *rpmfcAttrReg(const char *arg, ...)
{
    regex_t *reg = NULL;
    char *pattern;
    va_list args;

    va_start(args, arg);
    pattern = rpmfcAttrMacroV(arg, args);
    va_end(args);
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
	char *flags;

	if (prefix) {
	    flags = rpmfcAttrMacro(name, prefix, "flags", NULL);

	    (*rule)->path = rpmfcAttrReg(name, prefix, "path", NULL);
	    (*rule)->magic = rpmfcAttrReg(name, prefix, "magic", NULL);
	    (*rule)->mime = rpmfcAttrReg(name, prefix, "mime", NULL);
	} else {
	    flags = rpmfcAttrMacro(name, "flags", NULL);

	    (*rule)->path = rpmfcAttrReg(name, "path", NULL);
	    (*rule)->magic = rpmfcAttrReg(name, "magic", NULL);
	    (*rule)->mime = rpmfcAttrReg(name, "mime", NULL);
	}
	if ((*rule)->magic && (*rule)->mime) {
	    rpmlog(RPMLOG_WARNING,
		_("%s: mime and magic supplied, only mime will be used\n"),
		name);
	}


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

static rpmds rpmdsSingleNS(rpmstrPool pool,
			rpmTagVal tagN, const char *namespace,
			const char * N, const char * EVR, rpmsenseFlags Flags)
{
    rpmds ds = NULL;
    if (namespace) {
	char *NSN = rpmExpand(namespace, "(", N, ")", NULL);
	ds = rpmdsSinglePool(pool, tagN, NSN, EVR, Flags);
	free(NSN);
    } else {
	ds = rpmdsSinglePool(pool, tagN, N, EVR, Flags);
    }
    return ds;
}

#define max(x,y) ((x) > (y) ? (x) : (y))

static int getOutputFrom(ARGV_t argv,
			 const char * writePtr, size_t writeBytesLeft,
			 StringBuf sb_stdout,
			 int failNonZero, const char *buildRoot)
{
    pid_t child, reaped;
    int toProg[2] = { -1, -1 };
    int fromProg[2] = { -1, -1 };
    int status;
    int myerrno = 0;
    int ret = 1; /* assume failure */
    int doio = (writePtr || sb_stdout);

    if (doio && (pipe(toProg) < 0 || pipe(fromProg) < 0)) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for %s: %m\n"), argv[0]);
	return -1;
    }
    
    child = fork();
    if (child < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"),
		argv[0], strerror(errno));
	if (doio) {
	    close(toProg[1]);
	    close(toProg[0]);
	    close(fromProg[0]);
	    close(fromProg[1]);
	}
	return -1;
    }
    if (child == 0) {
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

    if (!doio)
	goto reap;

    close(toProg[0]);
    close(fromProg[1]);

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
	    if (sb_stdout)
		appendStringBuf(sb_stdout, buf);
	}
    }

    /* Clean up */
    if (toProg[1] >= 0)
    	close(toProg[1]);
    if (fromProg[0] >= 0)
	close(fromProg[0]);

reap:
    /* Collect status from prog */
    reaped = waitpid(child, &status, 0);
    rpmlog(RPMLOG_DEBUG, "\twaitpid(%d) rc %d status %x\n",
        (unsigned)child, (unsigned)reaped, status);

    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmlog(RPMLOG_DEBUG, _("%s failed: %x\n"), argv[0], status);
	goto exit;
    }
    if (writeBytesLeft || myerrno) {
	rpmlog(RPMLOG_ERR, _("failed to write all data to %s: %s\n"), 
		argv[0], strerror(myerrno));
	goto exit;
    }
    ret = 0;

exit:
    return ret;
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

    if (_rpmfc_debug) {
	char *cmd = argvJoin(xav, " ");
	rpmlog(RPMLOG_DEBUG, "Executing %s on %s\n", cmd, buf_stdin);
	free(cmd);
    }

    /* Read output from exec'd helper. */
    if (sb_stdoutp != NULL) {
	sb = newStringBuf();
    }
    ec = getOutputFrom(xav, buf_stdin, buf_stdin_len, sb,
		       failnonzero, buildRoot);
    if (ec) {
	sb = freeStringBuf(sb);
	goto exit;
    }

    if (sb_stdoutp != NULL) {
	*sb_stdoutp = sb;
	sb = NULL;	/* XXX don't free */
    }

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

static void rpmfcAddFileDep(rpmfcFileDeps *fileDeps, rpmds ds, int ix)
{
    if (fileDeps->size == fileDeps->alloced) {
	fileDeps->alloced <<= 2;
	fileDeps->data  = xrealloc(fileDeps->data,
	    fileDeps->alloced * sizeof(fileDeps->data[0]));
    }
    fileDeps->data[fileDeps->size].fileIx = ix;
    fileDeps->data[fileDeps->size++].dep = ds;
}

static ARGV_t runCmd(const char *cmd,
		     const char *buildRoot, const char *fn)
{
    ARGV_t output = NULL;
    ARGV_t av = NULL;
    StringBuf sb_stdout = NULL;
    StringBuf sb_stdin = newStringBuf();
    argvAdd(&av, cmd);

    appendLineStringBuf(sb_stdin, fn);
    if (rpmfcExec(av, sb_stdin, &sb_stdout, 0, buildRoot) == 0) {
	argvSplit(&output, getStringBuf(sb_stdout), "\n\r");
    }

    argvFree(av);
    freeStringBuf(sb_stdin);
    freeStringBuf(sb_stdout);

    return output;
}

static ARGV_t runCall(const char *cmd,
		     const char *buildRoot, const char *fn)
{
    ARGV_t output = NULL;

    if (_rpmfc_debug)
	rpmlog(RPMLOG_DEBUG, "Calling %s() on %s\n", cmd, fn);

    /* Hack to pass in the path as what looks like a macro argument */
    rpmPushMacroFlags(NULL, "1", NULL, fn, 1, RPMMACRO_LITERAL);
    char *exp = rpmExpand(cmd, NULL);
    rpmPopMacro(NULL, "1");
    if (*exp)
	argvSplit(&output, exp, "\n\r");
    free(exp);

    return output;
}

struct addReqProvDataFc {
    rpmfc fc;
    const char *namespace;
    regex_t *exclude;
};

static rpmRC addReqProvFc(void *cbdata, rpmTagVal tagN,
			  const char * N, const char * EVR, rpmsenseFlags Flags,
			  int index)
{
    struct addReqProvDataFc *data = cbdata;
    rpmfc fc = data->fc;
    const char *namespace = data->namespace;
    regex_t *exclude = data->exclude;

    rpmds ds = rpmdsSingleNS(fc->pool, tagN, namespace, N, EVR, Flags);
    /* Add to package and file dependencies unless filtered */
    if (regMatch(exclude, rpmdsDNEVR(ds)+2) == 0)
	rpmfcAddFileDep(&fc->fileDeps, ds, index);

    return RPMRC_OK;
}

struct exclreg_s {
    regex_t *exclude;
    regex_t *exclude_from;
    regex_t *global_exclude_from;
};

static void exclInit(const char *depname, struct exclreg_s *excl)
{
    excl->exclude = rpmfcAttrReg(depname, "exclude", NULL);
    excl->exclude_from = rpmfcAttrReg(depname, "exclude", "from", NULL);
    excl->global_exclude_from = rpmfcAttrReg("global", depname, "exclude", "from", NULL);
}

static void exclFini(struct exclreg_s *excl)
{
    regFree(excl->exclude);
    regFree(excl->exclude_from);
    regFree(excl->global_exclude_from);
    memset(excl, 0, sizeof(*excl));
}

static int rpmfcHelper(rpmfc fc, int ix, const struct exclreg_s *excl,
		       rpmsenseFlags dsContext, rpmTagVal tagN,
		       const char *namespace, const char *cmd, int callable)
{
    ARGV_t pav = NULL;
    const char * fn = fc->fn[ix];
    int pac;
    int rc = 0;

    /* If the entire path is filtered out, there's nothing more to do */
    if (regMatch(excl->exclude_from, fn+fc->brlen))
	goto exit;

    if (regMatch(excl->global_exclude_from, fn+fc->brlen))
	goto exit;

    if (callable) {
	pav = runCall(cmd, fc->buildRoot, fn);
    } else {
	pav = runCmd(cmd, fc->buildRoot, fn);
    }

    if (pav == NULL)
	goto exit;

    pac = argvCount(pav);

    struct addReqProvDataFc data;
    data.fc = fc;
    data.namespace = namespace;
    data.exclude = excl->exclude;

    for (int i = 0; i < pac; i++) {
	if (parseRCPOT(NULL, fc->pkg, pav[i], tagN, ix, dsContext, addReqProvFc, &data))
	    rc++;
    }

    argvFree(pav);

exit:
    return rc;
}

/* Only used for controlling RPMTAG_FILECLASS inclusion now */
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
		   const char *ftype, const char *fmime,
		    const char *path, int executable)
{
    const char *mtype = rule->mime ? fmime : ftype;
    regex_t *mreg = rule->mime ? rule->mime : rule->magic;

    if (!executable && hasAttr(rule->flags, "exeonly"))
	return 0;
    if (mreg && rule->path && hasAttr(rule->flags, "magic_and_path")) {
	return (regMatch(mreg, mtype) && regMatch(rule->path, path));
    } else {
	return (regMatch(mreg, mtype) || regMatch(rule->path, path));
    }
}

static void rpmfcAttributes(rpmfc fc, int ix, const char *ftype, const char *fmime, const char *fullpath)
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
	if (matches(&(*attr)->excl, ftype, fmime, path, is_executable))
	    continue;

	/* Add attributes on libmagic type & path pattern matches */
	if (matches(&(*attr)->incl, ftype, fmime, path, is_executable)) {
	    argvAddTokens(&fc->fattrs[ix], (*attr)->name);
	    #pragma omp critical(fahash)
	    fattrHashAddEntry(fc->fahash, attr-fc->atypes, ix);
	}
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
    int ndx;
    int dx;
    int fx;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    if (fc)
    for (fx = 0; fx < fc->nfiles; fx++) {
	fprintf(fp, "%3d %s", fx, fc->fn[fx]);
	if (_rpmfc_debug) {
	    rpmsid cx = fc->fcdictx[fx] + 1; /* id's are one off */
	    rpm_color_t fcolor = fc->fcolor[fx];
	    ARGV_t fattrs = fc->fattrs[fx];

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
	    rpmds ds;

	    ix = fc->ddictx->vals[dx++];
	    deptype = ((ix >> 24) & 0xff);
	    ix &= 0x00ffffff;
	    depval = NULL;
	    ds = rpmfcDependencies(fc, rpmdsDToTagN(deptype));
	    (void) rpmdsSetIx(ds, ix-1);
	    if (rpmdsNext(ds) >= 0)
		depval = rpmdsDNEVR(ds);
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
	    free(fc->ftype[i]);
	    argvFree(fc->fattrs[i]);
	}
	free(fc->fn);
	free(fc->ftype);
	free(fc->fattrs);
	free(fc->fcolor);
	free(fc->fcdictx);
	freePackage(fc->pkg);
	argiFree(fc->fddictx);
	argiFree(fc->fddictn);
	argiFree(fc->ddictx);

	for (int i = 0; i < fc->fileDeps.size; i++) {
	    rpmdsFree(fc->fileDeps.data[i].dep);
	}
	free(fc->fileDeps.data);

	fattrHashFree(fc->fahash);
	rpmstrPoolFree(fc->cdict);

	rpmstrPoolFree(fc->pool);
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
    fc->pool = rpmstrPoolCreate();
    fc->pkg = xcalloc(1, sizeof(*fc->pkg));
    fc->fileDeps.alloced = 10;
    fc->fileDeps.data = xmalloc(fc->fileDeps.alloced *
	sizeof(fc->fileDeps.data[0]));
    return fc;
}

rpmfc rpmfcNew(void)
{
    return rpmfcCreate(NULL, 0);
}

rpmds rpmfcDependencies(rpmfc fc, rpmTagVal tag)
{
    if (fc) {
	return *packageDependencies(fc->pkg, tag);
    }
    return NULL;
}

rpmds rpmfcProvides(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_PROVIDENAME);
}

rpmds rpmfcRequires(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_REQUIRENAME);
}

rpmds rpmfcRecommends(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_RECOMMENDNAME);
}

rpmds rpmfcSuggests(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_SUGGESTNAME);
}

rpmds rpmfcSupplements(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_SUPPLEMENTNAME);
}

rpmds rpmfcEnhances(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_ENHANCENAME);
}

rpmds rpmfcConflicts(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_CONFLICTNAME);
}

rpmds rpmfcObsoletes(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_OBSOLETENAME);
}

rpmds rpmfcOrderWithRequires(rpmfc fc)
{
    return rpmfcDependencies(fc, RPMTAG_ORDERNAME);
}


/* Versioned deps are less than unversioned deps */
static int cmpVerDeps(const void *a, const void *b)
{
    rpmfcFileDep *fDepA = (rpmfcFileDep *) a;
    rpmfcFileDep *fDepB = (rpmfcFileDep *) b;

    int aIsVersioned = rpmdsFlags(fDepA->dep) & RPMSENSE_SENSEMASK ? 1 : 0;
    int bIsVersioned = rpmdsFlags(fDepB->dep) & RPMSENSE_SENSEMASK ? 1 : 0;

    return bIsVersioned - aIsVersioned;
}

/* Sort by index */
static int cmpIndexDeps(const void *a, const void *b)
{
    rpmfcFileDep *fDepA = (rpmfcFileDep *) a;
    rpmfcFileDep *fDepB = (rpmfcFileDep *) b;

    return fDepA->fileIx - fDepB->fileIx;
}

/*
 * Remove unversioned deps if corresponding versioned deps exist but only
 * if the versioned dependency has the same type and the same color as the versioned.
 */
static void rpmfcNormalizeFDeps(rpmfc fc)
{
    rpmstrPool versionedDeps = rpmstrPoolCreate();
    rpmfcFileDep *normalizedFDeps = xmalloc(fc->fileDeps.size *
	sizeof(normalizedFDeps[0]));
    int ix = 0;
    char *depStr;

    /* Sort. Versioned dependencies first */
    qsort(fc->fileDeps.data, fc->fileDeps.size, sizeof(fc->fileDeps.data[0]),
	cmpVerDeps);

    for (int i = 0; i < fc->fileDeps.size; i++) {
	switch (rpmdsTagN(fc->fileDeps.data[i].dep)) {
	case RPMTAG_REQUIRENAME:
	case RPMTAG_RECOMMENDNAME:
	case RPMTAG_SUGGESTNAME:
	    rasprintf(&depStr, "%08x_%c_%s",
		fc->fcolor[fc->fileDeps.data[i].fileIx],
		rpmdsD(fc->fileDeps.data[i].dep),
		rpmdsN(fc->fileDeps.data[i].dep));

	    if (rpmdsFlags(fc->fileDeps.data[i].dep) & RPMSENSE_SENSEMASK) {
		/* preserve versioned require dependency */
		normalizedFDeps[ix++] = fc->fileDeps.data[i];
		rpmstrPoolId(versionedDeps, depStr, 1);
	    } else if (!rpmstrPoolId(versionedDeps, depStr, 0)) {
		/* preserve unversioned require dep only if versioned dep doesn't exist */
		    normalizedFDeps[ix++] =fc-> fileDeps.data[i];
	    } else {
		rpmdsFree(fc->fileDeps.data[i].dep);
	    }
	    free(depStr);
	    break;
	default:
	    /* Preserve all non-require dependencies */
	    normalizedFDeps[ix++] = fc->fileDeps.data[i];
	    break;
	}
    }
    rpmstrPoolFree(versionedDeps);

    free(fc->fileDeps.data);
    fc->fileDeps.data = normalizedFDeps;
    fc->fileDeps.size = ix;
}

struct applyDep_s {
    rpmTagVal tag;
    int type;
    const char *name;
};

static const struct applyDep_s applyDepTable[] = {
    { RPMTAG_PROVIDENAME,	RPMSENSE_FIND_PROVIDES,	"provides" },
    { RPMTAG_REQUIRENAME,	RPMSENSE_FIND_REQUIRES, "requires" },
    { RPMTAG_RECOMMENDNAME,	RPMSENSE_FIND_REQUIRES, "recommends" },
    { RPMTAG_SUGGESTNAME,	RPMSENSE_FIND_REQUIRES, "suggests" },
    { RPMTAG_SUPPLEMENTNAME,	RPMSENSE_FIND_REQUIRES, "supplements" },
    { RPMTAG_ENHANCENAME,	RPMSENSE_FIND_REQUIRES, "enhances" },
    { RPMTAG_CONFLICTNAME,	RPMSENSE_FIND_REQUIRES, "conflicts" },
    { RPMTAG_OBSOLETENAME,	RPMSENSE_FIND_REQUIRES, "obsoletes" },
    { RPMTAG_ORDERNAME,	        RPMSENSE_FIND_REQUIRES, "orderwithrequires" },
    { 0, 0, NULL },
};

static int applyAttr(rpmfc fc, int aix, const char *aname,
			const struct exclreg_s *excl,
			const struct applyDep_s *dep)
{
    int rc = 0;
    int n, *ixs;

    if (fattrHashGetEntry(fc->fahash, aix, &ixs, &n, NULL)) {
	char *mname = rstrscat(NULL, "__", aname, "_", dep->name, NULL);
	char *cmd;
	int callable = rpmMacroIsParametric(NULL, mname);

	if (callable) {
	    cmd = rstrscat(NULL, "%{", mname,
				" %{?", mname, "_opts}", "}", NULL);
	} else {
	    cmd = rpmExpand("%{?", mname, ":%{", mname, "} %{?",
				mname, "_opts}}", NULL);
	}

	if (!rstreq(cmd, "")) {
	    char *ns = rpmfcAttrMacro(aname, "namespace", NULL);
	    for (int i = 0; i < n; i++) {
		if (rpmfcHelper(fc, ixs[i], excl, dep->type, dep->tag,
				ns, cmd, callable))
		    rc = 1;
	    }
	    free(ns);
	}
	free(cmd);
	free(mname);
    }
    return rc;
}

static rpmRC rpmfcApplyInternal(rpmfc fc)
{
    rpmRC rc = RPMRC_OK;
    rpmds ds, * dsp;
    int previx;
    unsigned int val;
    int dix;
    int ix;
    const struct applyDep_s *dep;
    int skip = 0;
    struct exclreg_s excl;

    if (fc->skipProv)
	skip |= RPMSENSE_FIND_PROVIDES;
    if (fc->skipReq)
	skip |= RPMSENSE_FIND_REQUIRES;

    /* Generate package and per-file dependencies. */
    for (dep = applyDepTable; dep->tag; dep++) {
	int aix = 0;
	if (skip & dep->type)
	    continue;
	exclInit(dep->name, &excl);
	for (rpmfcAttr *attr = fc->atypes; attr && *attr; attr++, aix++) {
	    if (applyAttr(fc, aix, (*attr)->name, &excl, dep))
		rc = RPMRC_FAIL;
	}
	exclFini(&excl);
    }
    /* No more additions after this, freeze pool to minimize memory use */

    rpmfcNormalizeFDeps(fc);
    for (int i = 0; i < fc->fileDeps.size; i++) {
	ds = fc->fileDeps.data[i].dep;
	rpmdsMerge(packageDependencies(fc->pkg, rpmdsTagN(ds)), ds);
    }

    /* Sort by index */
    qsort(fc->fileDeps.data, fc->fileDeps.size,
	sizeof(fc->fileDeps.data[0]), cmpIndexDeps);

    /* Generate per-file indices into package dependencies. */
    previx = -1;
    for (int i = 0; i < fc->fileDeps.size; i++) {
	ds = fc->fileDeps.data[i].dep;
	ix = fc->fileDeps.data[i].fileIx;
	dsp = packageDependencies(fc->pkg, rpmdsTagN(ds));
	dix = rpmdsFind(*dsp, ds);
	if (dix < 0)
	    continue;

	val = (rpmdsD(ds) << 24) | (dix & 0x00ffffff);
	argiAdd(&fc->ddictx, -1, val);

	if (previx != ix) {
	    previx = ix;
	    argiAdd(&fc->fddictx, ix, argiCount(fc->ddictx)-1);
	}
	if (fc->fddictn && fc->fddictn->vals)
	    fc->fddictn->vals[ix]++;

    }
    return rc;
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

static uint32_t getElfColor(const char *fn)
{
    uint32_t color = 0;
#ifdef HAVE_LIBELF
    int fd = open(fn, O_RDONLY);
    if (fd >= 0) {
	Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
	GElf_Ehdr ehdr;
	if (elf && gelf_getehdr(elf, &ehdr)) {
	    switch (ehdr.e_ident[EI_CLASS]) {
	    case ELFCLASS64:
		color = RPMFC_ELF64;
		break;
	    case ELFCLASS32:
		color = RPMFC_ELF32;
		break;
	    }
	    elf_end(elf);
	}
	close(fd);
    }
#endif
    return color;
}

rpmRC rpmfcClassify(rpmfc fc, ARGV_t argv, rpm_mode_t * fmode)
{
    int msflags = MAGIC_CHECK | MAGIC_COMPRESS | MAGIC_NO_CHECK_TOKENS | MAGIC_ERROR;
    int mimeflags = msflags | MAGIC_MIME_TYPE;
    int nerrors = 0;
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
    fc->ftype = xcalloc(fc->nfiles, sizeof(*fc->ftype));
    fc->fattrs = xcalloc(fc->nfiles, sizeof(*fc->fattrs));
    fc->fcolor = xcalloc(fc->nfiles, sizeof(*fc->fcolor));
    fc->fcdictx = xcalloc(fc->nfiles, sizeof(*fc->fcdictx));
    fc->fahash = fattrHashCreate(fc->nfiles / 3, intId, intCmp, NULL, NULL);

    /* Initialize the per-file dictionary indices. */
    argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Build (sorted) file class dictionary. */
    fc->cdict = rpmstrPoolCreate();

    #pragma omp parallel
    {
    /* libmagic is not thread-safe, each thread needs to a private handle */
    magic_t ms = magic_open(msflags);
    magic_t mime = magic_open(mimeflags);

    if (ms == NULL || mime == NULL) {
	rpmlog(RPMLOG_ERR, _("magic_open(0x%x) failed: %s\n"),
		msflags, strerror(errno));
	#pragma omp cancel parallel
    }

    if (magic_load(ms, NULL) == -1) {
	rpmlog(RPMLOG_ERR, _("magic_load failed: %s\n"), magic_error(ms));
	#pragma omp cancel parallel
    }
    if (magic_load(mime, NULL) == -1) {
	rpmlog(RPMLOG_ERR, _("magic_load failed: %s\n"), magic_error(mime));
	#pragma omp cancel parallel
    }

    #pragma omp for reduction(+:nerrors)
    for (int ix = 0; ix < fc->nfiles; ix++) {
	const char * fmime;
	const char * ftype;
	const char * s = argv[ix];
	size_t slen = strlen(s);
	int fcolor = RPMFC_BLACK;
	rpm_mode_t mode = (fmode ? fmode[ix] : 0);
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

	    /* Silence errors from immaterial %ghosts */
	    if (ftype == NULL && errno == ENOENT)
		ftype = "";

	    if (ftype == NULL) {
		rpmlog(is_executable ? RPMLOG_ERR : RPMLOG_WARNING, 
		       _("Recognition of file \"%s\" failed: mode %06o %s\n"),
		       s, mode, magic_error(ms));
		/* only executable files are critical to dep extraction */
		if (is_executable) {
		    nerrors++;
		}
		/* unrecognized non-executables get treated as "data" */
		ftype = "data";
	    }
	}

	fmime = magic_file(mime, s);

	/* Silence errors from immaterial %ghosts */
	if (fmime == NULL && errno == ENOENT)
	    fmime = "";

	if (fmime == NULL) {
	    rpmlog(is_executable ? RPMLOG_ERR : RPMLOG_WARNING,
		   _("Recognition of file \"%s\" failed: mode %06o %s\n"),
		   s, mode, magic_error(ms));
	    /* only executable files are critical to dep extraction */
	    if (is_executable) {
		nerrors++;
	    }
	    fmime = "application/octet-stream";
	}

	rpmlog(RPMLOG_DEBUG, "%s: %s (%s)\n", s, fmime, ftype);

	/* Save the path. */
	fc->fn[ix] = xstrdup(s);

	/* Add (filtered) file coloring */
	fcolor |= rpmfcColor(ftype);

	/* Add attributes based on file type and/or path */
	rpmfcAttributes(fc, ix, ftype, fmime, s);

	if (fcolor != RPMFC_WHITE && (fcolor & RPMFC_INCLUDE))
	    fc->ftype[ix] = xstrdup(ftype);

	/* Add ELF colors */
	if (S_ISREG(mode) && is_executable)
	    fc->fcolor[ix] = getElfColor(s);
    }

    if (ms != NULL)
	magic_close(ms);
    if (mime != NULL)
	magic_close(mime);

    } /* omp parallel */

    /* Add to file class dictionary and index array */
    for (int ix = 0; ix < fc->nfiles; ix++) {
	const char *ftype = fc->ftype[ix] ? fc->ftype[ix] : "";
	/* Pool id's start from 1, for headers we want it from 0 */
	fc->fcdictx[ix] = rpmstrPoolId(fc->cdict, ftype, 1) - 1;

	if (*ftype)
	    fc->fknown++;
	else
	    fc->fwhite++;
    }

    if (nerrors == 0)
	rc = RPMRC_OK;

exit:
    /* No more additions after this, freeze pool to minimize memory use */
    rpmstrPoolFreeze(fc->cdict, 0);

    return rc;
}

typedef struct DepMsg_s * DepMsg_t;

struct DepMsg_s {
    const char * msg;
    char * const argv[4];
    rpmTagVal ntag;
    rpmTagVal vtag;
    rpmTagVal ftag;
    int mask;
    int xormask;
};

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
  { "Recommends",		{ "%{?__find_recommends}", NULL, NULL, NULL },
	RPMTAG_RECOMMENDNAME, RPMTAG_RECOMMENDVERSION, RPMTAG_RECOMMENDFLAGS,
	0, -1 },
  { "Suggests",	{ "%{?__find_suggests}", NULL, NULL, NULL },
	RPMTAG_SUGGESTNAME, RPMTAG_SUGGESTVERSION, RPMTAG_SUGGESTFLAGS,
	0, -1 },
  { "Supplements",	{ "%{?__find_supplements}", NULL, NULL, NULL },
	RPMTAG_SUPPLEMENTNAME, RPMTAG_SUPPLEMENTVERSION, RPMTAG_SUPPLEMENTFLAGS,
	0, -1 },
  { "Enhances",		{ "%{?__find_enhances}", NULL, NULL, NULL },
	RPMTAG_ENHANCENAME, RPMTAG_ENHANCEVERSION, RPMTAG_ENHANCEFLAGS,
	0, -1 },
  { "OrderWithRequires",	{ "%{?__find_orderwithrequires}", NULL, NULL, NULL },
	RPMTAG_ORDERNAME, RPMTAG_ORDERVERSION, RPMTAG_ORDERFLAGS,
	0, -1 },
  { NULL,		{ NULL, NULL, NULL, NULL },	0, 0, 0, 0, 0 }
};

static DepMsg_t DepMsgs = depMsgs;

static void printDeps(rpmfc fc)
{
    DepMsg_t dm;
    rpmds ds = NULL;
    const char * DNEVR;
    rpmsenseFlags Flags;
    int bingo = 0;

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	if (dm->ntag != -1) {
	    ds = rpmfcDependencies(fc, dm->ntag);
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
}

static rpmRC rpmfcApplyExternal(rpmfc fc)
{
    StringBuf sb_stdin = newStringBuf();
    rpmRC rc = RPMRC_OK;

    /* Create file manifest buffer to deliver to dependency finder. */
    for (int i = 0; i < fc->nfiles; i++)
	appendLineStringBuf(sb_stdin, fc->fn[i]);

    for (DepMsg_t dm = DepMsgs; dm->msg != NULL; dm++) {
	rpmTagVal tag = (dm->ftag > 0) ? dm->ftag : dm->ntag;
	rpmsenseFlags tagflags;
	char * s = NULL;
	StringBuf sb_stdout = NULL;
	int failnonzero = (tag == RPMTAG_PROVIDEFLAGS);

	switch (tag) {
	case RPMTAG_PROVIDEFLAGS:
	    if (fc->skipProv)
		continue;
	    tagflags = RPMSENSE_FIND_PROVIDES;
	    break;
	case RPMTAG_REQUIREFLAGS:
	case RPMTAG_RECOMMENDFLAGS:
	case RPMTAG_SUGGESTFLAGS:
	case RPMTAG_SUPPLEMENTFLAGS:
	case RPMTAG_ENHANCEFLAGS:
	case RPMTAG_CONFLICTFLAGS:
	case RPMTAG_OBSOLETEFLAGS:
	case RPMTAG_ORDERFLAGS:
	    if (fc->skipReq)
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
			failnonzero, fc->buildRoot) == -1)
	    continue;

	if (sb_stdout == NULL) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}

	/* Parse dependencies into header */
	rc = parseRCPOT(NULL, fc->pkg, getStringBuf(sb_stdout), dm->ntag != -1 ? dm->ntag : RPMTAG_REQUIRENAME, 0, tagflags, addReqProvPkg, NULL);
	freeStringBuf(sb_stdout);

	if (rc) {
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}
    }

    freeStringBuf(sb_stdin);

    return rc;
}

typedef const struct macroExport_s {
    const char * name;
    rpmTagVal tag;
} * macroExport;

static struct macroExport_s const macroExportList[] = {
    { "name",	RPMTAG_NAME },
    { "epoch",	RPMTAG_EPOCH },
    { "version",	RPMTAG_VERSION },
    { "release",	RPMTAG_RELEASE },
    { NULL,	0 }
};

rpmRC rpmfcApply(rpmfc fc)
{
    rpmRC rc;
    Package pkg = fc->pkg;
    macroExport me;
    for (me = macroExportList; me->name; me++) {
	char *val = headerGetAsString(pkg->header, me->tag);
	if (val) {
	    rpmPushMacro(NULL, me->name, NULL, val, RMIL_SPEC);
	    free(val);
	}
    }
    /* If new-fangled dependency generation is disabled ... */
    if (!rpmExpandNumeric("%{?_use_internal_dependency_generator}")) {
	/* ... then generate dependencies using %{__find_requires} et al. */
	rpmlog(RPMLOG_WARNING,
	    _("Deprecated external dependency generator is used!\n"));
	rc = rpmfcApplyExternal(fc);
    } else {
	/* ... otherwise generate per-file dependencies */
	rc = rpmfcApplyInternal(fc);
    }
    for (me = macroExportList; me->name; me++)
	if (headerIsEntry(pkg->header, me->tag))
	    rpmPopMacro(NULL, me->name);
    return rc;
}

rpmRC rpmfcGenerateDepends(const rpmSpec spec, Package pkg)
{
    rpmfi fi = rpmfilesIter(pkg->cpioList, RPMFI_ITER_FWD);
    rpmfc fc = NULL;
    rpm_mode_t * fmode = NULL;
    int ac = rpmfiFC(fi);
    int genConfigDeps = 0;
    rpmRC rc = RPMRC_OK;
    int idx;
    struct rpmtd_s td;

    /* Skip packages with no files. */
    if (ac <= 0)
	goto exit;

    /* Extract absolute file paths in argv format. */
    fmode = xcalloc(ac+1, sizeof(*fmode));

    fi = rpmfiInit(fi, 0);
    while ((idx = rpmfiNext(fi)) >= 0) {
	/* Does package have any %config files? */
	genConfigDeps |= (rpmfiFFlags(fi) & RPMFILE_CONFIG);
	fmode[idx] = rpmfiFMode(fi);
    }

    fc = rpmfcCreate(spec->buildRoot, 0);
    freePackage(fc->pkg);
    fc->pkg = pkg;
    fc->skipProv = !pkg->autoProv;
    fc->skipReq = !pkg->autoReq;

    if (!fc->skipProv && genConfigDeps) {
	/* Add config dependency, Provides: config(N) = EVR */
	rpmds ds = rpmdsSingleNS(fc->pool, RPMTAG_PROVIDENAME, "config",
				 rpmdsN(pkg->ds), rpmdsEVR(pkg->ds),
				 (RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	rpmdsMerge(packageDependencies(pkg, RPMTAG_PROVIDENAME), ds);
	rpmdsFree(ds);
    }
    if (!fc->skipReq && genConfigDeps) {
	rpmds ds = rpmdsSingleNS(fc->pool, RPMTAG_REQUIRENAME, "config",
				 rpmdsN(pkg->ds), rpmdsEVR(pkg->ds),
				 (RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	rpmdsMerge(packageDependencies(pkg, RPMTAG_REQUIRENAME), ds);
	rpmdsFree(ds);
    }

    /* Build file class dictionary. */
    rc = rpmfcClassify(fc, pkg->dpaths, fmode);
    if ( rc != RPMRC_OK )
	goto exit;

    /* Build file/package dependency dictionary. */
    rc = rpmfcApply(fc);
    if (rc != RPMRC_OK)
	goto exit;

    /* Add per-file colors(#files) */
    headerPutUint32(pkg->header, RPMTAG_FILECOLORS, fc->fcolor, fc->nfiles);
    
    /* Add classes(#classes) */
    for (rpmsid id = 1; id <= rpmstrPoolNumStr(fc->cdict); id++) {
	headerPutString(pkg->header, RPMTAG_CLASSDICT,
			rpmstrPoolStr(fc->cdict, id));
    }

    /* Add per-file classes(#files) */
    headerPutUint32(pkg->header, RPMTAG_FILECLASS, fc->fcdictx, fc->nfiles);

    /* Add dependency dictionary(#dependencies) */
    if (rpmtdFromArgi(&td, RPMTAG_DEPENDSDICT, fc->ddictx)) {
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);

	/* Add per-file dependency (start,number) pairs (#files) */
	if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSX, fc->fddictx)) {
	    headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
	}

	if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSN, fc->fddictn)) {
	    headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
	}
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
    printDeps(fc);

    /* Clean up. */
    if (fc)
	fc->pkg = NULL;
    free(fmode);
    rpmfcFree(fc);
    rpmfiFree(fi);

    return rc;
}
