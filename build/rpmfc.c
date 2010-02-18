#include "system.h"

#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <magic.h>

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include <rpm/argv.h>
#include <rpm/rpmfc.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "debug.h"

/**
 */
struct rpmfc_s {
    int nfiles;		/*!< no. of files */
    int fknown;		/*!< no. of classified files */
    int fwhite;		/*!< no. of "white" files */
    int ix;		/*!< current file index */
    int skipProv;	/*!< Don't auto-generate Provides:? */
    int skipReq;	/*!< Don't auto-generate Requires:? */
    int tracked;	/*!< Versioned Provides: tracking dependency added? */
    size_t brlen;	/*!< strlen(spec->buildRoot) */

    ARGV_t fn;		/*!< (no. files) file names */
    ARGV_t *fattrs;	/*!< (no. files) file attribute tokens */
    ARGI_t fcolor;	/*!< (no. files) file colors */
    ARGI_t fcdictx;	/*!< (no. files) file class dictionary indices */
    ARGI_t fddictx;	/*!< (no. files) file depends dictionary start */
    ARGI_t fddictn;	/*!< (no. files) file depends dictionary no. entries */
    ARGV_t cdict;	/*!< (no. classes) file class dictionary */
    ARGV_t ddict;	/*!< (no. dependencies) file depends dictionary */
    ARGI_t ddictx;	/*!< (no. dependencies) file->dependency mapping */

    rpmds provides;	/*!< (no. provides) package provides */
    rpmds requires;	/*!< (no. requires) package requires */
};

/**
 */
struct rpmfcTokens_s {
    const char * token;
    rpm_color_t colors;
    const char * attrs;
};  

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

/** \ingroup rpmbuild
 * Return output from helper script.
 * @todo Use poll(2) rather than select(2), if available.
 * @param dir		directory to run in (or NULL)
 * @param argv		program and arguments to run
 * @param writePtr	bytes to feed to script on stdin (or NULL)
 * @param writeBytesLeft no. of bytes to feed to script on stdin
 * @param failNonZero	is script failure an error?
 * @return		buffered stdout from script, NULL on error
 */     
static StringBuf getOutputFrom(const char * dir, ARGV_t argv,
                        const char * writePtr, size_t writeBytesLeft,
                        int failNonZero)
{
    pid_t child, reaped;
    int toProg[2];
    int fromProg[2];
    int status;
    void *oldhandler;
    StringBuf readBuff;
    int done;

    /* FIX: cast? */
    oldhandler = signal(SIGPIPE, SIG_IGN);

    toProg[0] = toProg[1] = 0;
    fromProg[0] = fromProg[1] = 0;
    if (pipe(toProg) < 0 || pipe(fromProg) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for %s: %m\n"), argv[0]);
	return NULL;
    }
    
    if (!(child = fork())) {
	(void) close(toProg[1]);
	(void) close(fromProg[0]);
	
	(void) dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	(void) dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */

	(void) close(toProg[0]);
	(void) close(fromProg[1]);

	if (dir && chdir(dir)) {
	    rpmlog(RPMLOG_ERR, _("Couldn't chdir to %s: %s\n"),
		    dir, strerror(errno));
	    _exit(EXIT_FAILURE);
	}
	
	rpmlog(RPMLOG_DEBUG, "\texecv(%s) pid %d\n",
                        argv[0], (unsigned)getpid());

	unsetenv("MALLOC_CHECK_");
	(void) execvp(argv[0], (char *const *)argv);
	/* XXX this error message is probably not seen. */
	rpmlog(RPMLOG_ERR, _("Couldn't exec %s: %s\n"),
		argv[0], strerror(errno));
	_exit(EXIT_FAILURE);
    }
    if (child < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't fork %s: %s\n"),
		argv[0], strerror(errno));
	return NULL;
    }

    (void) close(toProg[0]);
    (void) close(fromProg[1]);

    /* Do not block reading or writing from/to prog. */
    (void) fcntl(fromProg[0], F_SETFL, O_NONBLOCK);
    (void) fcntl(toProg[1], F_SETFL, O_NONBLOCK);
    
    readBuff = newStringBuf();

    do {
	fd_set ibits, obits;
	struct timeval tv;
	int nfd, nbw, nbr;
	int rc;

	done = 0;
top:
	FD_ZERO(&ibits);
	FD_ZERO(&obits);
	if (fromProg[0] >= 0) {
	    FD_SET(fromProg[0], &ibits);
	}
	if (toProg[1] >= 0) {
	    FD_SET(toProg[1], &obits);
	}
	/* XXX values set to limit spinning with perl doing ~100 forks/sec. */
	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	nfd = ((fromProg[0] > toProg[1]) ? fromProg[0] : toProg[1]);
	if ((rc = select(nfd, &ibits, &obits, NULL, &tv)) < 0) {
	    if (errno == EINTR)
		goto top;
	    break;
	}

	/* Write any data to program */
	if (toProg[1] >= 0 && FD_ISSET(toProg[1], &obits)) {
          if (writePtr && writeBytesLeft > 0) {
	    if ((nbw = write(toProg[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("getOutputFrom()");
	            exit(EXIT_FAILURE);
		}
	        nbw = 0;
	    }
	    writeBytesLeft -= nbw;
	    writePtr += nbw;
	  } else if (toProg[1] >= 0) {	/* close write fd */
	    (void) close(toProg[1]);
	    toProg[1] = -1;
	  }
	}
	
	/* Read any data from prog */
	{   char buf[BUFSIZ+1];
	    while ((nbr = read(fromProg[0], buf, sizeof(buf)-1)) > 0) {
		buf[nbr] = '\0';
		appendStringBuf(readBuff, buf);
	    }
	}

	/* terminate on (non-blocking) EOF or error */
	done = (nbr == 0 || (nbr < 0 && errno != EAGAIN));

    } while (!done);

    /* Clean up */
    if (toProg[1] >= 0)
    	(void) close(toProg[1]);
    if (fromProg[0] >= 0)
	(void) close(fromProg[0]);
    /* FIX: cast? */
    (void) signal(SIGPIPE, oldhandler);

    /* Collect status from prog */
    reaped = waitpid(child, &status, 0);
    rpmlog(RPMLOG_DEBUG, "\twaitpid(%d) rc %d status %x\n",
        (unsigned)child, (unsigned)reaped, status);

    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmlog(RPMLOG_ERR, _("%s failed\n"), argv[0]);
	return NULL;
    }
    if (writeBytesLeft) {
	rpmlog(RPMLOG_ERR, _("failed to write all data to %s\n"), argv[0]);
	return NULL;
    }
    return readBuff;
}

int rpmfcExec(ARGV_const_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero)
{
    char * s = NULL;
    ARGV_t xav = NULL;
    ARGV_t pav = NULL;
    int pac = 0;
    int ec = -1;
    StringBuf sb = NULL;
    const char * buf_stdin = NULL;
    size_t buf_stdin_len = 0;
    int xx;

    if (sb_stdoutp)
	*sb_stdoutp = NULL;
    if (!(av && *av))
	goto exit;

    /* Find path to executable with (possible) args. */
    s = rpmExpand(av[0], NULL);
    if (!(s && *s))
	goto exit;

    /* Parse args buried within expanded exacutable. */
    pac = 0;
    xx = poptParseArgvString(s, &pac, (const char ***)&pav);
    if (!(xx == 0 && pac > 0 && pav != NULL))
	goto exit;

    /* Build argv, appending args to the executable args. */
    xav = NULL;
    xx = argvAppend(&xav, pav);
    if (av[1])
	xx = rpmfcExpandAppend(&xav, av + 1);

    if (sb_stdin != NULL) {
	buf_stdin = getStringBuf(sb_stdin);
	buf_stdin_len = strlen(buf_stdin);
    }

    /* Read output from exec'd helper. */
    sb = getOutputFrom(NULL, xav, buf_stdin, buf_stdin_len, failnonzero);

    if (sb_stdoutp != NULL) {
	*sb_stdoutp = sb;
	sb = NULL;	/* XXX don't free */
    }

    ec = 0;

exit:
    sb = freeStringBuf(sb);
    xav = argvFree(xav);
    pav = _free(pav);	/* XXX popt mallocs in single blob. */
    s = _free(s);
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

static void rpmfcAddFileDep(ARGV_t * argvp, int ix, rpmds ds, char deptype)
{
    if (ds) {
	char *key = NULL;
	rasprintf(&key, "%08d%c %s %s 0x%08x", ix, deptype,
		  rpmdsN(ds), rpmdsEVR(ds), rpmdsFlags(ds));
	argvAddUniq(argvp, key);
	free(key);
    }
}

/**
 * Run per-interpreter dependency helper.
 * @param fc		file classifier
 * @param deptype	'P' == Provides:, 'R' == Requires:, helper
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @return		0 on success
 */
static int rpmfcHelper(rpmfc fc, unsigned char deptype, const char * nsdep)
{
    const char * fn = fc->fn[fc->ix];
    char *buf = NULL;
    char *mname = NULL;
    StringBuf sb_stdout = NULL;
    StringBuf sb_stdin;
    rpmds * depsp;
    rpmsenseFlags dsContext;
    rpmTag tagN;
    ARGV_t av = NULL;
    int xx;

    switch (deptype) {
    default:
	return -1;
	break;
    case 'P':
	if (fc->skipProv)
	    return 0;
	mname = rstrscat(NULL, "__", nsdep, "_provides", NULL);
	depsp = &fc->provides;
	dsContext = RPMSENSE_FIND_PROVIDES;
	tagN = RPMTAG_PROVIDENAME;
	break;
    case 'R':
	if (fc->skipReq)
	    return 0;
	mname = rstrscat(NULL, "__", nsdep, "_requires", NULL);
	depsp = &fc->requires;
	dsContext = RPMSENSE_FIND_REQUIRES;
	tagN = RPMTAG_REQUIRENAME;
	break;
    }
    rasprintf(&buf, "%%{?%s:%%{%s} %%{?%s_opts}}", mname, mname, mname);
    argvAdd(&av, buf);
    buf = rfree(buf);

    sb_stdin = newStringBuf();
    appendLineStringBuf(sb_stdin, fn);
    sb_stdout = NULL;
    xx = rpmfcExec(av, sb_stdin, &sb_stdout, 0);
    sb_stdin = freeStringBuf(sb_stdin);

    if (xx == 0 && sb_stdout != NULL) {
	ARGV_t pav = NULL;
	int pac;
	xx = argvSplit(&pav, getStringBuf(sb_stdout), " \t\n\r");
	pac = argvCount(pav);
	if (pav)
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
assert(*s != '\0');
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
assert(EVR != NULL);
	    }


	    /* Add tracking dependency for versioned Provides: */
	    if (!fc->tracked && deptype == 'P' && *EVR != '\0') {
		ds = rpmdsSingle(RPMTAG_REQUIRENAME,
			"rpmlib(VersionedDependencies)", "3.0.3-1",
			RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL));
		xx = rpmdsMerge(&fc->requires, ds);
		ds = rpmdsFree(ds);
		fc->tracked = 1;
	    }

	    ds = rpmdsSingle(tagN, N, EVR, Flags);

	    /* Add to package dependencies. */
	    xx = rpmdsMerge(depsp, ds);

	    /* Add to file dependencies. */
	    rpmfcAddFileDep(&fc->ddict, fc->ix, ds, deptype);

	    ds = rpmdsFree(ds);
	}

	pav = argvFree(pav);
    }
    sb_stdout = freeStringBuf(sb_stdout);
    free(mname);
    argvFree(av);

    return 0;
}

typedef enum depTypes_e {
    DEP_NONE = 0,
    DEP_REQ  = (1 << 0),
    DEP_PROV = (1 << 1),
} depTypes;

static int rpmfcGenDeps(rpmfc fc, const char *class, depTypes types)
{
    int rc = 0;
    if (types & DEP_PROV)
	rc += rpmfcHelper(fc, 'P', class);
    if (types & DEP_REQ)
	rc += rpmfcHelper(fc, 'R', class);
    return rc;
}

/* Handle RPMFC_INCLUDE "stop signal" as an attribute? */
static const struct rpmfcTokens_s const rpmfcTokens[] = {
  { "directory",		RPMFC_INCLUDE, "directory" },

  { " shared object",		RPMFC_BLACK, "library" },
  { " executable",		RPMFC_BLACK, "executable" },
  { " statically linked",	RPMFC_BLACK, "static" },
  { " not stripped",		RPMFC_BLACK, "notstripped" },
  { " archive",			RPMFC_BLACK, "archive" },

  { "ELF 32-bit",		RPMFC_ELF32|RPMFC_INCLUDE, "elf" },
  { "ELF 64-bit",		RPMFC_ELF64|RPMFC_INCLUDE, "elf" },

  /* "foo script text" is a file with shebang interpreter directive */
  { " script text",		RPMFC_BLACK, "script" },
  { " document",		RPMFC_BLACK, "document" },

  { " compressed",		RPMFC_BLACK, "compressed" },

  { "troff or preprocessor input",	RPMFC_INCLUDE, "man" },
  { "GNU Info",			RPMFC_INCLUDE, "info" },

  { "perl ",			RPMFC_INCLUDE, "perl" },
  { "Perl5 module source text", RPMFC_INCLUDE, "perl,module" },

  /* XXX "a /usr/bin/python -t script text executable" */
  /* XXX "python 2.3 byte-compiled" */
  { "python ",			RPMFC_INCLUDE, "python" },

  { "libtool library ",         RPMFC_INCLUDE, "libtool" },
  { "pkgconfig ",               RPMFC_INCLUDE, "pkgconfig" },

  { "Objective caml ",		RPMFC_INCLUDE, "ocaml" },

  /* XXX .NET executables and libraries.  file(1) cannot differ from win32 
   * executables unfortunately :( */
  { "Mono/.Net assembly",       RPMFC_INCLUDE, "mono" },

  { "current ar archive",	RPMFC_INCLUDE, "static,library,archive" },

  { "Zip archive data",		RPMFC_INCLUDE, "compressed,archive" },
  { "tar archive",		RPMFC_INCLUDE, "archive" },
  { "cpio archive",		RPMFC_INCLUDE, "archive" },
  { "RPM v3",			RPMFC_INCLUDE, "archive" },
  { "RPM v4",			RPMFC_INCLUDE, "archive" },

  { " image",			RPMFC_INCLUDE, "image" },
  { " font metrics",		RPMFC_WHITE|RPMFC_INCLUDE, NULL },
  { " font",			RPMFC_INCLUDE, "font" },
  { " Font",			RPMFC_INCLUDE, "font" },

  { " commands",		RPMFC_INCLUDE, "script" },
  { " script",			RPMFC_INCLUDE, "script" },

  { "empty",			RPMFC_INCLUDE, NULL },

  { "HTML",			RPMFC_INCLUDE, NULL },
  { "SGML",			RPMFC_INCLUDE, NULL },
  { "XML",			RPMFC_INCLUDE, NULL },

  { " source",			RPMFC_INCLUDE, NULL },
  { "GLS_BINARY_LSB_FIRST",	RPMFC_INCLUDE, NULL },
  { " DB ",			RPMFC_INCLUDE, NULL },

  { "symbolic link to",		RPMFC_BLACK, "symlink" },
  { "socket",			RPMFC_BLACK, "device" }, /* XXX device? */
  { "special",			RPMFC_BLACK, "device" },
  { " text",			RPMFC_INCLUDE, "text" },

  { "ASCII",			RPMFC_WHITE, NULL },
  { "ISO-8859",			RPMFC_WHITE, NULL },

  { "data",			RPMFC_WHITE, NULL },

  { "application",		RPMFC_WHITE, NULL },
  { "boot",			RPMFC_WHITE, NULL },
  { "catalog",			RPMFC_WHITE, NULL },
  { "code",			RPMFC_WHITE, NULL },
  { "file",			RPMFC_WHITE, NULL },
  { "format",			RPMFC_WHITE, NULL },
  { "message",			RPMFC_WHITE, NULL },
  { "program",			RPMFC_WHITE, NULL },

  { "broken symbolic link to ",	RPMFC_WHITE|RPMFC_ERROR, NULL },
  { "can't read",		RPMFC_WHITE|RPMFC_ERROR, NULL },
  { "can't stat",		RPMFC_WHITE|RPMFC_ERROR, NULL },
  { "executable, can't read",	RPMFC_WHITE|RPMFC_ERROR, NULL },
  { "core file",		RPMFC_WHITE|RPMFC_ERROR, NULL },

  { NULL,			RPMFC_BLACK, NULL }
};

/* Return attribute tokens + color for a given libmagic classification string */
static void rpmfcAttributes(const char * fmstr, ARGV_t *attrs, int *color)
{
    rpmfcToken fct;
    ARGV_t fattrs = NULL;
    rpm_color_t fcolor = RPMFC_BLACK;

    for (fct = rpmfcTokens; fct->token != NULL; fct++) {
	if (strstr(fmstr, fct->token) == NULL)
	    continue;

	if (fct->attrs) {
	    ARGV_t atokens = NULL;
	    argvSplit(&atokens, fct->attrs, ",");
	    for (ARGV_t token = atokens; token && *token; token++)
		argvAddUniq(&fattrs, *token);
	    argvFree(atokens);
	}

	fcolor |= fct->colors;
	if (fcolor & RPMFC_INCLUDE)
	    break;
    }

    if (attrs) *attrs = fattrs;
    if (color) *color |= fcolor;
}

void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
{
    rpm_color_t fcolor;
    int ndx;
    int cx;
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
assert(fx < fc->fcdictx->nvals);
	cx = fc->fcdictx->vals[fx];
assert(fx < fc->fcolor->nvals);
	fcolor = fc->fcolor->vals[fx];

	fprintf(fp, "%3d %s", fx, fc->fn[fx]);
	if (fcolor != RPMFC_BLACK)
		fprintf(fp, "\t0x%x", fc->fcolor->vals[fx]);
	else
		fprintf(fp, "\t%s", fc->cdict[cx]);
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
	fc->fn = argvFree(fc->fn);
	for (int i = 0; i < fc->nfiles; i++)
	    argvFree(fc->fattrs[i]);
	fc->fattrs = _free(fc->fattrs);
	fc->fcolor = argiFree(fc->fcolor);
	fc->fcdictx = argiFree(fc->fcdictx);
	fc->fddictx = argiFree(fc->fddictx);
	fc->fddictn = argiFree(fc->fddictn);
	fc->cdict = argvFree(fc->cdict);
	fc->ddict = argvFree(fc->ddict);
	fc->ddictx = argiFree(fc->ddictx);

	fc->provides = rpmdsFree(fc->provides);
	fc->requires = rpmdsFree(fc->requires);
    }
    fc = _free(fc);
    return NULL;
}

rpmfc rpmfcNew(void)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    return fc;
}

rpmds rpmfcProvides(rpmfc fc)
{
    return (fc != NULL ? fc->provides : NULL);
}

rpmds rpmfcRequires(rpmfc fc)
{
    return (fc != NULL ? fc->requires : NULL);
}

static int isExecutable(const char *fn)
{
    struct stat st;
    if (stat(fn, &st) < 0)
	return -1;
    return (S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)));
}

/**
 * Extract script interpreter dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcINTERP(rpmfc fc)
{
    int rc = 0;
    int is_executable = isExecutable(fc->fn[fc->ix]);

    if (is_executable < 0) return -1;
    /* only executables are searched, and these can only be requires */
    if (is_executable)
	rc = rpmfcGenDeps(fc, "interpreter", DEP_REQ);
    return rc;
}

/**
 * Extract language (rather loose definition...) specific dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcSCRIPT(rpmfc fc)
{
    const char * fn = fc->fn[fc->ix];
    ARGV_t fattrs = fc->fattrs[fc->ix];
    int is_executable = isExecutable(fn);
    int xx;

    if (is_executable < 0)
	return -1;

    if (hasAttr(fattrs, "perl")) {
	depTypes types = DEP_NONE;
	if (hasAttr(fattrs, "module"))
	    types |= (DEP_PROV|DEP_PROV);
	if (is_executable)
	    types |= DEP_REQ;
	xx = rpmfcGenDeps(fc, "perl", types);
    } else if (hasAttr(fattrs, "mono")) {
	depTypes types = DEP_PROV;
	if (is_executable)
	    types |= DEP_REQ;
	xx = rpmfcGenDeps(fc, "mono", types);
    /* The rest dont have clear rules wrt executability, do both req & prov */
    } else if (hasAttr(fattrs, "python")) {
	xx = rpmfcGenDeps(fc, "python", (DEP_REQ|DEP_PROV));
    } else if (hasAttr(fattrs, "ocaml")) {
	xx = rpmfcGenDeps(fc, "ocaml", (DEP_REQ|DEP_PROV));
    /* XXX libtool and pkgconfig are not scripts... */
    } else if (hasAttr(fattrs, "libtool")) {
	xx = rpmfcGenDeps(fc, "libtool", (DEP_REQ|DEP_PROV));
    } else if (hasAttr(fattrs, "pkgconfig")) {
	xx = rpmfcGenDeps(fc, "pkgconfig", (DEP_REQ|DEP_PROV));
    }

    return 0;
}

/**
 * Extract Elf dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcELF(rpmfc fc)
{
    return rpmfcGenDeps(fc, "elf", (DEP_REQ|DEP_PROV));
}

static int rpmfcMISC(rpmfc fc)
{
    struct stat st;
    int rc = -1;
    const char *what = NULL;
    const char * fn = fc->fn[fc->ix];
    ARGV_t fattrs = fc->fattrs[fc->ix];

    if (hasAttr(fattrs, "font")) {
	what = "fontconfig";
    } else if (hasAttr(fattrs, "text") && rpmFileHasSuffix(fn, ".desktop")) {
	what = "desktop";
    }

    if (what == NULL || stat(fn, &st) < 0 || !S_ISREG(st.st_mode)) {
	goto exit;
    }

    rc = rpmfcGenDeps(fc, what, (DEP_REQ|DEP_PROV));
exit:
    return rc;
}
typedef const struct rpmfcApplyTbl_s {
    int (*func) (rpmfc fc);
    const char *attrs;
} * rpmfcApplyTbl;

/**
 */
static const struct rpmfcApplyTbl_s const rpmfcApplyTable[] = {
    { rpmfcELF,		"elf" },
    { rpmfcINTERP,	"script" },
    { rpmfcSCRIPT,	"perl,python,mono,ocaml,pkgconfig,libtool" },
    { rpmfcMISC,	"font,text" },
    { NULL, 0 }
};

rpmRC rpmfcApply(rpmfc fc)
{
    rpmfcApplyTbl fcat;
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
    int i;
    int xx;

    /* Generate package and per-file dependencies. */
    for (fc->ix = 0; fc->fn[fc->ix] != NULL; fc->ix++) {

	/* XXX Insure that /usr/lib{,64}/python files are marked as python */
	/* XXX HACK: classification by path is intrinsically stupid. */
	{   const char *fn = strstr(fc->fn[fc->ix], "/usr/lib");
	    if (fn) {
		fn += sizeof("/usr/lib")-1;
		if (fn[0] == '6' && fn[1] == '4')
		    fn += 2;
		if (rstreqn(fn, "/python", sizeof("/python")-1))
		    argvAddUniq(&fc->fattrs[fc->ix], "python");
	    }
	}

	if (fc->fattrs[fc->ix] == NULL)
	    continue;

	for (fcat = rpmfcApplyTable; fcat->func != NULL; fcat++) {
	    ARGV_t applyAttrs = NULL;
	    argvSplit(&applyAttrs, fcat->attrs, ",");
	    for (ARGV_t attr = applyAttrs; attr && *attr; attr++) {
		if (!hasAttr(fc->fattrs[fc->ix], *attr))
		    continue;
		xx = (*fcat->func) (fc);
	    }
	    argvFree(applyAttrs);
	}
    }

    /* Generate per-file indices into package dependencies. */
    nddict = argvCount(fc->ddict);
    previx = -1;
    for (i = 0; i < nddict; i++) {
	s = fc->ddict[i];

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
	    ds = rpmdsFree(ds);
	    break;
	case 'R':
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->requires, ds);
	    ds = rpmdsFree(ds);
	    break;
	}

/* XXX assertion incorrect while generating -debuginfo deps. */
#if 0
assert(dix >= 0);
#else
	if (dix < 0)
	    continue;
#endif

	val = (deptype << 24) | (dix & 0x00ffffff);
	xx = argiAdd(&fc->ddictx, -1, val);

	if (previx != ix) {
	    previx = ix;
	    xx = argiAdd(&fc->fddictx, ix, argiCount(fc->ddictx)-1);
	}
	if (fc->fddictn && fc->fddictn->vals)
	    fc->fddictn->vals[ix]++;
    }

    return RPMRC_OK;
}

rpmRC rpmfcClassify(rpmfc fc, ARGV_t argv, rpm_mode_t * fmode)
{
    ARGV_t fcav = NULL;
    int xx;
    int msflags = MAGIC_CHECK | MAGIC_COMPRESS | MAGIC_NO_CHECK_TOKENS;
    magic_t ms = NULL;
    rpmRC rc = RPMRC_FAIL;

    if (fc == NULL || argv == NULL)
	return 0; /* XXX looks very wrong */

    fc->nfiles = argvCount(argv);
    fc->fattrs = xcalloc(fc->nfiles, sizeof(*fc->fattrs));

    /* Initialize the per-file dictionary indices. */
    xx = argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    xx = argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Build (sorted) file class dictionary. */
    xx = argvAdd(&fc->cdict, "");
    xx = argvAdd(&fc->cdict, "directory");

    ms = magic_open(msflags);
    if (ms == NULL) {
	rpmlog(RPMLOG_ERR, _("magic_open(0x%x) failed: %s\n"),
		msflags, strerror(errno));
	goto exit;
    }

    xx = magic_load(ms, NULL);
    if (xx == -1) {
	rpmlog(RPMLOG_ERR, _("magic_load failed: %s\n"), magic_error(ms));
	goto exit;
    }

    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
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
	case S_IFDIR:
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
	xx = argvAdd(&fc->fn, s);

	/* Save the file type string. */
	xx = argvAdd(&fcav, ftype);

	/* Add (filtered) file attribute tokens */
	rpmfcAttributes(ftype, &fc->fattrs[fc->ix], &fcolor);

	xx = argiAdd(&fc->fcolor, fc->ix, fcolor);

	if (fcolor != RPMFC_WHITE && (fcolor & RPMFC_INCLUDE))
	    argvAddUniq(&fc->cdict, ftype);
    }

    /* Build per-file class index array. */
    fc->fknown = 0;
    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	const char *se = fcav[fc->ix];
	ARGV_t dav = argvSearch(fc->cdict, se, NULL);
	if (dav) {
	    xx = argiAdd(&fc->fcdictx, fc->ix, (dav - fc->cdict));
	    fc->fknown++;
	} else {
	    xx = argiAdd(&fc->fcdictx, fc->ix, 0);
	    fc->fwhite++;
	}
    }
    rc = RPMRC_OK;

exit:
    fcav = argvFree(fcav);
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
    rpmTag ntag;
    rpmTag vtag;
    rpmTag ftag;
    int mask;
    int xor;
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
	    ds = rpmdsFree(ds);
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
	
	    if (!((Flags & dm->mask) ^ dm->xor))
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
    ds = rpmdsFree(ds);
}

/**
 */
static int rpmfcGenerateDependsHelper(const rpmSpec spec, Package pkg, rpmfi fi)
{
    StringBuf sb_stdin;
    StringBuf sb_stdout;
    DepMsg_t dm;
    int failnonzero = 0;
    int rc = RPMRC_OK;

    /*
     * Create file manifest buffer to deliver to dependency finder.
     */
    sb_stdin = newStringBuf();
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0)
	appendLineStringBuf(sb_stdin, rpmfiFN(fi));

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	rpmTag tag = (dm->ftag > 0) ? dm->ftag : dm->ntag;
	rpmsenseFlags tagflags;
	char * s = NULL;

	switch(tag) {
	case RPMTAG_PROVIDEFLAGS:
	    if (!pkg->autoProv)
		continue;
	    failnonzero = 1;
	    tagflags = RPMSENSE_FIND_PROVIDES;
	    break;
	case RPMTAG_REQUIREFLAGS:
	    if (!pkg->autoReq)
		continue;
	    failnonzero = 0;
	    tagflags = RPMSENSE_FIND_REQUIRES;
	    break;
	default:
	    continue;
	    break;
	}

	if (rpmfcExec(dm->argv, sb_stdin, &sb_stdout, failnonzero) == -1)
	    continue;

	s = rpmExpand(dm->argv[0], NULL);
	rpmlog(RPMLOG_NOTICE, _("Finding  %s: %s\n"), dm->msg, (s ? s : ""));
	free(s);

	if (sb_stdout == NULL) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}

	/* Parse dependencies into header */
	rc = parseRCPOT(spec, pkg, getStringBuf(sb_stdout), tag, 0, tagflags);
	sb_stdout = freeStringBuf(sb_stdout);

	if (rc) {
	    rpmlog(RPMLOG_ERR, _("Failed to find %s:\n"), dm->msg);
	    break;
	}
    }

    sb_stdin = freeStringBuf(sb_stdin);

    return rc;
}

rpmRC rpmfcGenerateDepends(const rpmSpec spec, Package pkg)
{
    rpmfi fi = pkg->cpioList;
    rpmfc fc = NULL;
    rpmds ds;
    ARGV_t av;
    rpm_mode_t * fmode;
    int ac = rpmfiFC(fi);
    char *buf = NULL;
    const char * N;
    const char * EVR;
    int genConfigDeps;
    int rc = RPMRC_OK;
    int xx;
    int idx;
    struct rpmtd_s td;

    /* Skip packages with no files. */
    if (ac <= 0)
	return 0;

    /* Skip packages that have dependency generation disabled. */
    if (! (pkg->autoReq || pkg->autoProv))
	return 0;

    /* If new-fangled dependency generation is disabled ... */
    if (!rpmExpandNumeric("%{?_use_internal_dependency_generator}")) {
	/* ... then generate dependencies using %{__find_requires} et al. */
	rc = rpmfcGenerateDependsHelper(spec, pkg, fi);
	printDeps(pkg->header);
	return rc;
    }

    /* Extract absolute file paths in argv format. */
    av = xcalloc(ac+1, sizeof(*av));
    fmode = xcalloc(ac+1, sizeof(*fmode));

    genConfigDeps = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((idx = rpmfiNext(fi)) >= 0) {
	rpmfileAttrs fileAttrs;

	/* Does package have any %config files? */
	fileAttrs = rpmfiFFlags(fi);
	genConfigDeps |= (fileAttrs & RPMFILE_CONFIG);

	av[idx] = xstrdup(rpmfiFN(fi));
	fmode[idx] = rpmfiFMode(fi);
    }
    av[ac] = NULL;

    fc = rpmfcNew();
    fc->skipProv = !pkg->autoProv;
    fc->skipReq = !pkg->autoReq;
    fc->tracked = 0;
    fc->brlen = (spec->buildRoot ? strlen(spec->buildRoot) : 0);

    /* Copy (and delete) manually generated dependencies to dictionary. */
    if (!fc->skipProv) {
	ds = rpmdsNew(pkg->header, RPMTAG_PROVIDENAME, 0);
	xx = rpmdsMerge(&fc->provides, ds);
	ds = rpmdsFree(ds);
	xx = headerDel(pkg->header, RPMTAG_PROVIDENAME);
	xx = headerDel(pkg->header, RPMTAG_PROVIDEVERSION);
	xx = headerDel(pkg->header, RPMTAG_PROVIDEFLAGS);

	/* Add config dependency, Provides: config(N) = EVR */
	if (genConfigDeps) {
	    N = rpmdsN(pkg->ds);
	    if (N == NULL) {
		rc = RPMRC_FAIL;
		rpmlog(RPMLOG_ERR, _("Unable to get current dependency name.\n"));
		goto exit;
	    }
	    EVR = rpmdsEVR(pkg->ds);
	    if (EVR == NULL) {
		rc = RPMRC_FAIL;
		rpmlog(RPMLOG_ERR, _("Unable to get current dependency epoch-version-release.\n"));
		goto exit;
	    }
	    rasprintf(&buf, "config(%s)", N);
	    ds = rpmdsSingle(RPMTAG_PROVIDENAME, buf, EVR,
			(RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    free(buf);
	    xx = rpmdsMerge(&fc->provides, ds);
	    ds = rpmdsFree(ds);
	}
    }

    if (!fc->skipReq) {
	ds = rpmdsNew(pkg->header, RPMTAG_REQUIRENAME, 0);
	xx = rpmdsMerge(&fc->requires, ds);
	ds = rpmdsFree(ds);
	xx = headerDel(pkg->header, RPMTAG_REQUIRENAME);
	xx = headerDel(pkg->header, RPMTAG_REQUIREVERSION);
	xx = headerDel(pkg->header, RPMTAG_REQUIREFLAGS);

	/* Add config dependency,  Requires: config(N) = EVR */
	if (genConfigDeps) {
	    N = rpmdsN(pkg->ds);
	    if (N == NULL) {
		rc = RPMRC_FAIL;
		rpmlog(RPMLOG_ERR, _("Unable to get current dependency name.\n"));
		goto exit;
	    }
	    EVR = rpmdsEVR(pkg->ds);
	    if (EVR == NULL) {
		rc = RPMRC_FAIL;
		rpmlog(RPMLOG_ERR, _("Unable to get current dependency epoch-version-release.\n"));
		goto exit;
	    }
	    rasprintf(&buf, "config(%s)", N);
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, buf, EVR,
			(RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    free(buf);
	    xx = rpmdsMerge(&fc->requires, ds);
	    ds = rpmdsFree(ds);
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
    if (rpmtdFromArgi(&td, RPMTAG_FILECOLORS, fc->fcolor)) {
	rpm_color_t *fcolor;
	if (ac != rpmtdCount(&td)) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("File count from file info doesn't match file in container.\n"));
	    goto exit;
	}
	assert(rpmtdType(&td) == RPM_INT32_TYPE);
	/* XXX Make sure only primary (i.e. Elf32/Elf64) colors are added. */
	while ((fcolor = rpmtdNextUint32(&td))) {
	    *fcolor &= 0x0f;
	}
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    /* Add classes(#classes) */
    if (rpmtdFromArgv(&td, RPMTAG_CLASSDICT, fc->cdict)) {
	if (rpmtdType(&td) != RPM_STRING_ARRAY_TYPE) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Container not of string array data type.\n"));
	    goto exit;
	}
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    /* Add per-file classes(#files) */
    if (rpmtdFromArgi(&td, RPMTAG_FILECLASS, fc->fcdictx)) {
	assert(rpmtdType(&td) == RPM_INT32_TYPE);
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    /* Add Provides: */
    if (fc->provides != NULL && rpmdsCount(fc->provides) > 0 && !fc->skipProv) {
	rpmds pi = rpmdsInit(fc->provides);
	while (rpmdsNext(pi) >= 0) {
	    const char *name = rpmdsN(pi);
	    const char *evr = rpmdsEVR(pi);
	    rpmsenseFlags flags = rpmdsFlags(pi);
	
	    headerPutString(pkg->header, RPMTAG_PROVIDENAME, name);
	    headerPutString(pkg->header, RPMTAG_PROVIDEVERSION, evr);
	    headerPutUint32(pkg->header, RPMTAG_PROVIDEFLAGS, &flags, 1);
	}
    }

    /* Add Requires: */
    if (fc->requires != NULL && rpmdsCount(fc->requires) > 0 && !fc->skipReq) {
	rpmds pi = rpmdsInit(fc->requires);
	while (rpmdsNext(pi) >= 0) {
	    const char *name = rpmdsN(pi);
	    const char *evr = rpmdsEVR(pi);
	    rpmsenseFlags flags = rpmdsFlags(pi);
	
	    headerPutString(pkg->header, RPMTAG_REQUIRENAME, name);
	    headerPutString(pkg->header, RPMTAG_REQUIREVERSION, evr);
	    headerPutUint32(pkg->header, RPMTAG_REQUIREFLAGS, &flags, 1);
	}
    }

    /* Add dependency dictionary(#dependencies) */
    if (rpmtdFromArgi(&td, RPMTAG_DEPENDSDICT, fc->ddictx)) {
	assert(rpmtdType(&td) == RPM_INT32_TYPE);
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    /* Add per-file dependency (start,number) pairs (#files) */
    if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSX, fc->fddictx)) {
	assert(rpmtdType(&td) == RPM_INT32_TYPE);
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    if (rpmtdFromArgi(&td, RPMTAG_FILEDEPENDSN, fc->fddictn)) {
	assert(rpmtdType(&td) == RPM_INT32_TYPE);
	headerPut(pkg->header, &td, HEADERPUT_DEFAULT);
    }

    printDeps(pkg->header);

if (fc != NULL && _rpmfc_debug) {
char *msg = NULL;
rasprintf(&msg, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(msg, fc, NULL);
free(msg);
}
exit:
    /* Clean up. */
    fmode = _free(fmode);
    fc = rpmfcFree(fc);
    av = argvFree(av);

    return rc;
}
