#include "system.h"

#include <signal.h>	/* getOutputFrom() */

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>

#define	_RPMDS_INTERNAL
#include <rpmds.h>
#include <rpmfi.h>

#if HAVE_GELF_H
#include <gelf.h>
#endif

#include "debug.h"

/*@access fmagic @*/
/*@access rpmds @*/

/*@unchecked@*/
int _rpmfc_debug;

/**
 */
/*@-bounds@*/	/* LCL: internal error */
static int rpmfcExpandAppend(/*@out@*/ ARGV_t * argvp, const ARGV_t av)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *argvp, rpmGlobalMacroContext @*/
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
/*@=bounds@*/

/*@-boundswrite@*/
/** \ingroup rpmbuild
 * Return output from helper script.
 * @param dir		directory to run in (or NULL)
 * @param argv		program and arguments to run
 * @param writePtr	bytes to feed to script on stdin (or NULL)
 * @param writeBytesLeft no. of bytes to feed to script on stdin
 * @param failNonZero	is script failure an error?
 * @return		buffered stdout from script, NULL on error
 */     
/*@null@*/
static StringBuf getOutputFrom(/*@null@*/ const char * dir, ARGV_t argv,
                        const char * writePtr, int writeBytesLeft,
                        int failNonZero)
        /*@globals fileSystem, internalState@*/
        /*@modifies fileSystem, internalState@*/
{
    pid_t child, reaped;
    int toProg[2];
    int fromProg[2];
    int status;
    void *oldhandler;
    StringBuf readBuff;
    int done;

    /*@-type@*/ /* FIX: cast? */
    oldhandler = signal(SIGPIPE, SIG_IGN);
    /*@=type@*/

    toProg[0] = toProg[1] = 0;
    (void) pipe(toProg);
    fromProg[0] = fromProg[1] = 0;
    (void) pipe(fromProg);
    
    if (!(child = fork())) {
	(void) close(toProg[1]);
	(void) close(fromProg[0]);
	
	(void) dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	(void) dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */

	(void) close(toProg[0]);
	(void) close(fromProg[1]);

	if (dir) {
	    (void) chdir(dir);
	}
	
	rpmMessage(RPMMESS_DEBUG, _("\texecv(%s) pid %d\n"),
                        argv[0], (unsigned)getpid());

	unsetenv("MALLOC_CHECK_");
	(void) execvp(argv[0], (char *const *)argv);
	/* XXX this error message is probably not seen. */
	rpmError(RPMERR_EXEC, _("Couldn't exec %s: %s\n"),
		argv[0], strerror(errno));
	_exit(RPMERR_EXEC);
    }
    if (child < 0) {
	rpmError(RPMERR_FORK, _("Couldn't fork %s: %s\n"),
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
	/* XXX the select is mainly a timer since all I/O is non-blocking */
	FD_ZERO(&ibits);
	FD_ZERO(&obits);
	if (fromProg[0] >= 0) {
	    FD_SET(fromProg[0], &ibits);
	}
	if (toProg[1] >= 0) {
	    FD_SET(toProg[1], &obits);
	}
	tv.tv_sec = 1;
	tv.tv_usec = 0;
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
    /*@-type@*/ /* FIX: cast? */
    (void) signal(SIGPIPE, oldhandler);
    /*@=type@*/

    /* Collect status from prog */
    reaped = waitpid(child, &status, 0);
    rpmMessage(RPMMESS_DEBUG, _("\twaitpid(%d) rc %d status %x\n"),
        (unsigned)child, (unsigned)reaped, status);

    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmError(RPMERR_EXEC, _("%s failed\n"), argv[0]);
	return NULL;
    }
    if (writeBytesLeft) {
	rpmError(RPMERR_EXEC, _("failed to write all data to %s\n"), argv[0]);
	return NULL;
    }
    return readBuff;
}

int rpmfcExec(ARGV_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero)
{
    const char * s = NULL;
    ARGV_t xav = NULL;
    ARGV_t pav = NULL;
    int pac = 0;
    int ec = -1;
    StringBuf sb = NULL;
    const char * buf_stdin = NULL;
    int buf_stdin_len = 0;
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

/*@-branchstate@*/
    if (sb_stdoutp != NULL) {
	*sb_stdoutp = sb;
	sb = NULL;	/* XXX don't free */
    }
/*@=branchstate@*/

    ec = 0;

exit:
    sb = freeStringBuf(sb);
    xav = argvFree(xav);
    pav = _free(pav);	/* XXX popt mallocs in single blob. */
    s = _free(s);
    return ec;
}
/*@=boundswrite@*/

/**
 */
static int rpmfcSaveArg(/*@out@*/ ARGV_t * argvp, const char * key)
	/*@modifies *argvp @*/
{
    int rc = 0;

    if (argvSearch(*argvp, key, NULL) == NULL) {
	rc = argvAdd(argvp, key);
	rc = argvSort(*argvp, NULL);
    }
    return rc;
}

static char * rpmfcFileDep(/*@returned@*/ char * buf, int ix,
		/*@null@*/ rpmds this)
	/*@modifies buf @*/
{
    int_32 tagN = rpmdsTagN(this);
    char deptype = 'X';

/*@-boundswrite@*/
    buf[0] = '\0';
/*@=boundswrite@*/
    switch (tagN) {
    case RPMTAG_PROVIDENAME:
	deptype = 'P';
	break;
    case RPMTAG_REQUIRENAME:
	deptype = 'R';
	break;
    }
/*@-nullpass@*/
    if (this != NULL)
	sprintf(buf, "%08d%c %s %s 0x%08x", ix, deptype,
		rpmdsN(this), rpmdsEVR(this), rpmdsFlags(this));
/*@=nullpass@*/
    return buf;
};

/**
 * Run per-interpreter dependency helper.
 * @param fc		file classifier
 * @param deptype	'P' == Provides:, 'R' == Requires:, helper
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @return		0 on success
 */
/*@-bounds@*/
static int rpmfcHelper(rpmfc fc, unsigned char deptype, const char * nsdep)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies fc, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * fn = fc->fn[fc->ix];
    char buf[BUFSIZ];
    StringBuf sb_stdout = NULL;
    StringBuf sb_stdin;
    const char *av[2];
    rpmds * depsp, this;
    const char * N;
    const char * EVR;
    int_32 Flags, dsContext, tagN;
    ARGV_t pav;
    const char * s;
    int pac;
    int xx;
    int i;

    switch (deptype) {
    default:
	return -1;
	/*@notreached@*/ break;
    case 'P':
	if (fc->skipProv)
	    return 0;
	xx = snprintf(buf, sizeof(buf), "%%{?__%s_provides}", nsdep);
	depsp = &fc->provides;
	dsContext = RPMSENSE_FIND_PROVIDES;
	tagN = RPMTAG_PROVIDENAME;
	break;
    case 'R':
	if (fc->skipReq)
	    return 0;
	xx = snprintf(buf, sizeof(buf), "%%{?__%s_requires}", nsdep);
	depsp = &fc->requires;
	dsContext = RPMSENSE_FIND_REQUIRES;
	tagN = RPMTAG_REQUIRENAME;
	break;
    }
    buf[sizeof(buf)-1] = '\0';
    av[0] = buf;
    av[1] = NULL;

    sb_stdin = newStringBuf();
    appendLineStringBuf(sb_stdin, fn);
    sb_stdout = NULL;
    xx = rpmfcExec(av, sb_stdin, &sb_stdout, 0);
    sb_stdin = freeStringBuf(sb_stdin);

    if (xx == 0 && sb_stdout != NULL) {
	pav = NULL;
	xx = argvSplit(&pav, getStringBuf(sb_stdout), " \t\n\r");
	pac = argvCount(pav);
	if (pav)
	for (i = 0; i < pac; i++) {
	    N = pav[i];
	    EVR = "";
	    Flags = dsContext;
/*@-branchstate@*/
	    if (pav[i+1] && strchr("=<>", *pav[i+1])) {
		i++;
		for (s = pav[i]; *s; s++) {
		    switch(*s) {
		    default:
assert(*s != '\0');
			/*@switchbreak@*/ break;
		    case '=':
			Flags |= RPMSENSE_EQUAL;
			/*@switchbreak@*/ break;
		    case '<':
			Flags |= RPMSENSE_LESS;
			/*@switchbreak@*/ break;
		    case '>':
			Flags |= RPMSENSE_GREATER;
			/*@switchbreak@*/ break;
		    }
		}
		i++;
		EVR = pav[i];
assert(EVR != NULL);
	    }
/*@=branchstate@*/

	    this = rpmdsSingle(tagN, N, EVR, Flags);
	    xx = rpmdsMerge(depsp, this);

	    /* Add to file dependencies. */
	    xx = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(buf, fc->ix, this));

	    this = rpmdsFree(this);
	}

	pav = argvFree(pav);
    }
    sb_stdout = freeStringBuf(sb_stdout);

    return 0;
}
/*@=bounds@*/

/**
 */
/*@unchecked@*/ /*@observer@*/
static struct rpmfcTokens_s rpmfcTokens[] = {
  { "directory",		RPMFC_DIRECTORY|RPMFC_INCLUDE },

  { " shared object",		RPMFC_LIBRARY },
  { " executable",		RPMFC_EXECUTABLE },
  { " statically linked",	RPMFC_STATIC },
  { " not stripped",		RPMFC_NOTSTRIPPED },
  { " archive",			RPMFC_ARCHIVE },

  { "ELF 32-bit",		RPMFC_ELF32|RPMFC_INCLUDE },
  { "ELF 64-bit",		RPMFC_ELF64|RPMFC_INCLUDE },

  { " script",			RPMFC_SCRIPT },
  { " text",			RPMFC_TEXT },
  { " document",		RPMFC_DOCUMENT },

  { " compressed",		RPMFC_COMPRESSED },

  { "troff or preprocessor input",		RPMFC_MANPAGE },

  { "current ar archive",	RPMFC_STATIC|RPMFC_LIBRARY|RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { "Zip archive data",		RPMFC_COMPRESSED|RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "tar archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "cpio archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "RPM v3",			RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { " image",			RPMFC_IMAGE|RPMFC_INCLUDE },
  { " font",			RPMFC_FONT|RPMFC_INCLUDE },
  { " Font",			RPMFC_FONT|RPMFC_INCLUDE },

  { " commands",		RPMFC_SCRIPT|RPMFC_INCLUDE },
  { " script",			RPMFC_SCRIPT|RPMFC_INCLUDE },

  { "python compiled",		RPMFC_WHITE|RPMFC_INCLUDE },

  { "empty",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "HTML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "SGML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "XML",			RPMFC_WHITE|RPMFC_INCLUDE },

  { " program text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { " source",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "GLS_BINARY_LSB_FIRST",	RPMFC_WHITE|RPMFC_INCLUDE },
  { " DB ",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "ASCII English text",	RPMFC_WHITE|RPMFC_INCLUDE },
  { "ASCII text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { "ISO-8859 text",		RPMFC_WHITE|RPMFC_INCLUDE },

  { "symbolic link to",		RPMFC_SYMLINK },
  { "socket",			RPMFC_DEVICE },
  { "special",			RPMFC_DEVICE },

  { "ASCII",			RPMFC_WHITE },
  { "ISO-8859",			RPMFC_WHITE },

  { "data",			RPMFC_WHITE },

  { "application",		RPMFC_WHITE },
  { "boot",			RPMFC_WHITE },
  { "catalog",			RPMFC_WHITE },
  { "code",			RPMFC_WHITE },
  { "file",			RPMFC_WHITE },
  { "format",			RPMFC_WHITE },
  { "message",			RPMFC_WHITE },
  { "program",			RPMFC_WHITE },

  { "broken symbolic link to ",	RPMFC_WHITE|RPMFC_ERROR },
  { "can't read",		RPMFC_WHITE|RPMFC_ERROR },
  { "can't stat",		RPMFC_WHITE|RPMFC_ERROR },
  { "executable, can't read",	RPMFC_WHITE|RPMFC_ERROR },
  { "core file",		RPMFC_WHITE|RPMFC_ERROR },

  { NULL,			RPMFC_BLACK }
};

int rpmfcColoring(const char * fmstr)
{
    rpmfcToken fct;
    int fcolor = RPMFC_BLACK;

    for (fct = rpmfcTokens; fct->token != NULL; fct++) {
	if (strstr(fmstr, fct->token) == NULL)
	    continue;
	fcolor |= fct->colors;
	if (fcolor & RPMFC_INCLUDE)
	    return fcolor;
    }
    return fcolor;
}

/*@-bounds@*/
void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
{
    int fcolor;
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
		/*@switchbreak@*/ break;
	    case 'P':
assert(ix < nprovides);
		(void) rpmdsSetIx(fc->provides, ix);
		depval = rpmdsDNEVR(fc->provides);
		/*@switchbreak@*/ break;
	    case 'R':
assert(ix < nrequires);
		(void) rpmdsSetIx(fc->requires, ix);
		depval = rpmdsDNEVR(fc->requires);
		/*@switchbreak@*/ break;
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
	fc->fcolor = argiFree(fc->fcolor);
	fc->fcdictx = argiFree(fc->fcdictx);
	fc->fddictx = argiFree(fc->fddictx);
	fc->fddictn = argiFree(fc->fddictn);
	fc->cdict = argvFree(fc->cdict);
	fc->ddict = argvFree(fc->ddict);
	fc->ddictx = argiFree(fc->ddictx);

	fc->provides = rpmdsFree(fc->provides);
	fc->requires = rpmdsFree(fc->requires);

	fc->sb_java = freeStringBuf(fc->sb_java);
	fc->sb_perl = freeStringBuf(fc->sb_perl);
	fc->sb_python = freeStringBuf(fc->sb_python);

    }
    fc = _free(fc);
    return NULL;
}
/*@=bounds@*/

rpmfc rpmfcNew(void)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    return fc;
}

/**
 * Extract script dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcSCRIPT(rpmfc fc)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies fc, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * fn = fc->fn[fc->ix];
    const char * bn;
    rpmds this;
    char buf[BUFSIZ];
    FILE * fp;
    char * s, * se;
    int i;
    struct stat sb, * st = &sb;
    int xx;

    /* Only executable scripts are searched. */
    if (stat(fn, st) < 0)
	return -1;
    if (!(st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
	return 0;

    fp = fopen(fn, "r");
    if (fp == NULL || ferror(fp)) {
	if (fp) (void) fclose(fp);
	return -1;
    }

    /* Look for #! interpreter in first 10 lines. */
    for (i = 0; i < 10; i++) {

	s = fgets(buf, sizeof(buf) - 1, fp);
	if (s == NULL || ferror(fp) || feof(fp))
	    break;
	s[sizeof(buf)-1] = '\0';
	if (!(s[0] == '#' && s[1] == '!'))
	    continue;
	s += 2;

	while (*s && strchr(" \t\n\r", *s) != NULL)
	    s++;
	if (*s == '\0')
	    continue;
	if (*s != '/')
	    continue;

	for (se = s+1; *se; se++) {
	    if (strchr(" \t\n\r", *se) != NULL)
		/*@innerbreak@*/ break;
	}
	*se = '\0';
	se++;

	/* Add to package requires. */
	this = rpmdsSingle(RPMTAG_REQUIRENAME, s, "", RPMSENSE_FIND_REQUIRES);
	xx = rpmdsMerge(&fc->requires, this);

	/* Add to file requires. */
	xx = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(se, fc->ix, this));

	this = rpmdsFree(this);

	/* Set color based on interpreter name. */
	bn = basename(s);
	if (!strcmp(bn, "perl")) {
	    fc->fcolor->vals[fc->ix] |= RPMFC_PERL;
	    xx = rpmfcHelper(fc, 'P', "perl");
	    xx = rpmfcHelper(fc, 'R', "perl");
	}
	if (!strcmp(bn, "python")) {
	    fc->fcolor->vals[fc->ix] |= RPMFC_PYTHON;
	    xx = rpmfcHelper(fc, 'P', "python");
	    xx = rpmfcHelper(fc, 'R', "python");
	}

	break;
    }

    (void) fclose(fp);

    return 0;
}

/**
 * Extract Elf dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcELF(rpmfc fc)
	/*@globals fileSystem, internalState @*/
	/*@modifies fc, fileSystem, internalState @*/
{
#if HAVE_GELF_H && HAVE_LIBELF
    const char * fn = fc->fn[fc->ix];
    Elf * elf;
    Elf_Scn * scn;
    Elf_Data * data;
    GElf_Ehdr ehdr_mem, * ehdr;
    GElf_Shdr shdr_mem, * shdr;
    GElf_Verdef def_mem, * def;
    GElf_Verneed need_mem, * need;
    GElf_Dyn dyn_mem, * dyn;
    unsigned int auxoffset;
    unsigned int offset;
    int fdno;
    int cnt2;
    int cnt;
    char buf[BUFSIZ];
    const char * s;
    unsigned char deptype;
    const char * soname = NULL;
    rpmds * depsp, this;
    int_32 tagN, dsContext;
    char * t;
    int xx;
    int isElf64;

    fdno = open(fn, O_RDONLY);
    if (fdno < 0)
	return fdno;

    (void) elf_version(EV_CURRENT);

/*@-evalorder@*/
    elf = NULL;
    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || (ehdr = gelf_getehdr(elf, &ehdr_mem)) == NULL
     || !(ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC))
	goto exit;
/*@=evalorder@*/

    isElf64 = ehdr->e_ident[EI_CLASS] == ELFCLASS64;

    /*@-branchstate -uniondef @*/
    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	shdr = gelf_getshdr(scn, &shdr_mem);
	if (shdr == NULL)
	    break;

	soname = _free(soname);
	switch (shdr->sh_type) {
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case SHT_GNU_verdef:
	    deptype = 'P';
	    data = NULL;
	    if (!fc->skipProv)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		
		    def = gelf_getverdef (data, offset, &def_mem);
		    if (def == NULL)
			/*@innerbreak@*/ break;
		    auxoffset = offset + def->vd_aux;
		    for (cnt2 = def->vd_cnt; --cnt2 >= 0; ) {
			GElf_Verdaux aux_mem, * aux;

			aux = gelf_getverdaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vda_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;
			if (def->vd_flags & VER_FLG_BASE) {
			    soname = _free(soname);
			    soname = xstrdup(s);
			    auxoffset += aux->vda_next;
			    /*@innercontinue@*/ continue;
			} else if (soname != NULL) {
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

#if !defined(__alpha__)
			    if (isElf64)
				t = stpcpy(t, "(64bit)");
#endif
			    t++;

			    /* Add to package provides. */
			    this = rpmdsSingle(RPMTAG_PROVIDES,
					buf, "", RPMSENSE_FIND_PROVIDES);
			    xx = rpmdsMerge(&fc->provides, this);

			    /* Add to file dependencies. */
			    xx = rpmfcSaveArg(&fc->ddict,
					rpmfcFileDep(t, fc->ix, this));

			    this = rpmdsFree(this);
			}
			auxoffset += aux->vda_next;
		    }
		    offset += def->vd_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
	    deptype = 'R';
	    data = NULL;
	    if (!fc->skipReq)
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		    need = gelf_getverneed (data, offset, &need_mem);
		    if (need == NULL)
			/*@innerbreak@*/ break;

		    s = elf_strptr(elf, shdr->sh_link, need->vn_file);
		    if (s == NULL)
			/*@innerbreak@*/ break;
		    soname = _free(soname);
		    soname = xstrdup(s);
		    auxoffset = offset + need->vn_aux;
		    for (cnt2 = need->vn_cnt; --cnt2 >= 0; ) {
			GElf_Vernaux aux_mem, * aux;

			aux = gelf_getvernaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    /*@innerbreak@*/ break;

			s = elf_strptr(elf, shdr->sh_link, aux->vna_name);
			if (s == NULL)
			    /*@innerbreak@*/ break;
			if (soname != NULL) {
			    buf[0] = '\0';
			    t = buf;
			    t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

#if !defined(__alpha__)
			    if (isElf64)
				t = stpcpy(t, "(64bit)");
#endif
			    t++;

			    /* Add to package dependencies. */
			    this = rpmdsSingle(RPMTAG_REQUIRENAME,
					buf, "", RPMSENSE_FIND_REQUIRES);
			    xx = rpmdsMerge(&fc->requires, this);

			    /* Add to file dependencies. */
			    xx = rpmfcSaveArg(&fc->ddict,
					rpmfcFileDep(t, fc->ix, this));
			    this = rpmdsFree(this);
			}
			auxoffset += aux->vna_next;
		    }
		    offset += need->vn_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_DYNAMIC:
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
		for (cnt = 0; cnt < (shdr->sh_size / shdr->sh_entsize); ++cnt) {
		    dyn = gelf_getdyn (data, cnt, &dyn_mem);
		    if (dyn == NULL)
			/*@innerbreak@*/ break;
		    s = NULL;
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ /*@switchbreak@*/ break;
		    case DT_NEEDED:
			/* Add to package requires. */
			if (fc->skipReq)
			    /*@innercontinue@*/ continue;
			deptype = 'R';
			depsp = &fc->requires;
			tagN = RPMTAG_REQUIRENAME;
			dsContext = RPMSENSE_FIND_REQUIRES;
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			/*@switchbreak@*/ break;
		    case DT_SONAME:
			/* Add to package provides. */
			if (fc->skipProv)
			    /*@innercontinue@*/ continue;
			deptype = 'P';
			depsp = &fc->provides;
			tagN = RPMTAG_PROVIDENAME;
			dsContext = RPMSENSE_FIND_PROVIDES;
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
assert(s != NULL);
			/*@switchbreak@*/ break;
		    }
		    if (s == NULL)
			/*@innercontinue@*/ continue;

		    buf[0] = '\0';
		    t = buf;
		    t = stpcpy(t, s);

#if !defined(__alpha__)
		    if (isElf64)
			t = stpcpy(t, "()(64bit)");
#endif
		    t++;

		    /* Add to package dependencies. */
		    this = rpmdsSingle(tagN, buf, "", dsContext);
		    xx = rpmdsMerge(depsp, this);

		    /* Add to file dependencies. */
		    xx = rpmfcSaveArg(&fc->ddict,
					rpmfcFileDep(t, fc->ix, this));

		    this = rpmdsFree(this);
		}
	    }
	    /*@switchbreak@*/ break;
	}
    }
    /*@=branchstate =uniondef @*/

exit:
    soname = _free(soname);
    if (elf) (void) elf_end(elf);
    xx = close(fdno);
    return 0;
#else
    return -1;
#endif
}

typedef struct rpmfcApplyTbl_s {
    int (*func) (rpmfc fc);
    int colormask;
} * rpmfcApplyTbl;

/**
 */
/*@unchecked@*/
static struct rpmfcApplyTbl_s rpmfcApplyTable[] = {
    { rpmfcELF,		RPMFC_ELF },
    { rpmfcSCRIPT,	RPMFC_SCRIPT },
    { NULL, 0 }
};

int rpmfcApply(rpmfc fc)
{
    const char * s;
    char * se;
    rpmds this;
    const char * N;
    const char * EVR;
    int_32 Flags;
    struct stat sb, * st = &sb;
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
	rpmfcApplyTbl fcat;

	/* Files with executable bit set only. */
	if (stat(fc->fn[fc->ix], st) != 0
	 || !(st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
	    continue;
	    
	for (fcat = rpmfcApplyTable; fcat->func != NULL; fcat++) {
	    if (!(fc->fcolor->vals[fc->ix] & fcat->colormask))
		/*@innercontinue@*/ continue;
	    xx = (*fcat->func) (fc);
	}
    }

    /* Generate per-file indices into package dependencies. */
    nddict = argvCount(fc->ddict);
    previx = -1;
    for (i = 0; i < nddict; i++) {
	s = fc->ddict[i];

	/* Parse out (file#,deptype,N,EVR,Flags) */
	ix = strtol(s, &se, 10);
assert(se != NULL);
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
	    /*@switchbreak@*/ break;
	case 'P':	
	    this = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->provides, this);
	    this = rpmdsFree(this);
	    /*@switchbreak@*/ break;
	case 'R':
	    this = rpmdsSingle(RPMTAG_REQUIRENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->requires, this);
	    this = rpmdsFree(this);
	    /*@switchbreak@*/ break;
	}

assert(dix >= 0);
	val = (deptype << 24) | (dix & 0x00ffffff);
	xx = argiAdd(&fc->ddictx, -1, val);

	if (previx != ix) {
	    previx = ix;
	    xx = argiAdd(&fc->fddictx, ix, argiCount(fc->ddictx)-1);
	}
	if (fc->fddictn && fc->fddictn->vals)
	    fc->fddictn->vals[ix]++;
    }

    return 0;
}

int rpmfcClassify(rpmfc fc, ARGV_t argv)
{
    ARGV_t fcav = NULL;
    ARGV_t dav;
    const char * s, * se;
    int fcolor;
    int xx;
fmagic fm = global_fmagic;
int action = 0;
int wid = 0;	/* XXX don't prepend filename: */

    if (fc == NULL || argv == NULL)
	return 0;

    fc->nfiles = argvCount(argv);

    /* Initialize the per-file dictionary indices. */
    xx = argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    xx = argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Build (sorted) file class dictionary. */
    xx = argvAdd(&fc->cdict, "");
    xx = argvAdd(&fc->cdict, "directory");

/*@-assignexpose@*/
    fm->magicfile = default_magicfile;
/*@=assignexpose@*/
    /* XXX TODO fm->flags = ??? */

    xx = fmagicSetup(fm, fm->magicfile, action);
    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	s = argv[fc->ix];
assert(s != NULL);
	fm->obp = fm->obuf;
	*fm->obp = '\0';
	fm->nob = sizeof(fm->obuf);
	xx = fmagicProcess(fm, s, wid);
	se = fm->obuf;
        rpmMessage(RPMMESS_DEBUG, "%s: %s\n", s, se);

	xx = argvAdd(&fc->fn, s);
	xx = argvAdd(&fcav, se);

	/* Add (filtered) entry to sorted class dictionary. */
	fcolor = rpmfcColoring(se);
	xx = argiAdd(&fc->fcolor, fc->ix, fcolor);

	if (fcolor != RPMFC_WHITE && (fcolor & RPMFC_INCLUDE))
	    xx = rpmfcSaveArg(&fc->cdict, se);
    }

    /* Build per-file class index array. */
    fc->fknown = 0;
    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	se = fcav[fc->ix];
assert(se != NULL);

	dav = argvSearch(fc->cdict, se, NULL);
	if (dav) {
	    xx = argiAdd(&fc->fcdictx, fc->ix, (dav - fc->cdict));
	    fc->fknown++;
	} else {
	    xx = argiAdd(&fc->fcdictx, fc->ix, 0);
	    fc->fwhite++;
	}
    }

    fcav = argvFree(fcav);

    /* XXX TODO dump fmagic baggage. */

    return 0;
}

/**
 */
typedef struct DepMsg_s * DepMsg_t;

/**
 */
struct DepMsg_s {
/*@observer@*/ /*@null@*/
    const char * msg;
/*@observer@*/
    const char * argv[4];
    rpmTag ntag;
    rpmTag vtag;
    rpmTag ftag;
    int mask;
    int xor;
};

/**
 */
/*@unchecked@*/
static struct DepMsg_s depMsgs[] = {
  { "Provides",		{ "%{?__find_provides}", NULL, NULL, NULL },
	RPMTAG_PROVIDENAME, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS,
	0, -1 },
#ifdef	DYING
  { "PreReq",		{ NULL, NULL, NULL, NULL },
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	RPMSENSE_PREREQ, 0 },
  { "Requires(interp)",	{ NULL, "interp", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_INTERP), 0 },
#else
  { "Requires(interp)",	{ NULL, "interp", NULL, NULL },
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_INTERP), 0 },
#endif
  { "Requires(rpmlib)",	{ NULL, "rpmlib", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_RPMLIB), 0 },
  { "Requires(verify)",	{ NULL, "verify", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_VERIFY, 0 },
  { "Requires(pre)",	{ NULL, "pre", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_PRE), 0 },
  { "Requires(post)",	{ NULL, "post", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_POST), 0 },
  { "Requires(preun)",	{ NULL, "preun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_PREUN), 0 },
  { "Requires(postun)",	{ NULL, "postun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_POSTUN), 0 },
  { "Requires",		{ "%{?__find_requires}", NULL, NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,	/* XXX inherit name/version arrays */
	RPMSENSE_PREREQ, RPMSENSE_PREREQ },
  { "Conflicts",	{ "%{?__find_conflicts}", NULL, NULL, NULL },
	RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS,
	0, -1 },
  { "Obsoletes",	{ "%{?__find_obsoletes}", NULL, NULL, NULL },
	RPMTAG_OBSOLETENAME, RPMTAG_OBSOLETEVERSION, RPMTAG_OBSOLETEFLAGS,
	0, -1 },
  { NULL,		{ NULL, NULL, NULL, NULL },	0, 0, 0, 0, 0 }
};

/*@unchecked@*/
static DepMsg_t DepMsgs = depMsgs;

/**
 */
static void printDeps(Header h)
	/*@modifies h @*/
{
    DepMsg_t dm;
    rpmds ds = NULL;
    int scareMem = 0;
    const char * DNEVR;
    int_32 Flags;
    int bingo = 0;

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	if (dm->ntag != -1) {
	    ds = rpmdsFree(ds);
	    ds = rpmdsNew(h, dm->ntag, scareMem);
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
		/*@innercontinue@*/ continue;
	    if (bingo == 0) {
		rpmMessage(RPMMESS_NORMAL, "%s:", (dm->msg ? dm->msg : ""));
		bingo = 1;
	    }
	    if ((DNEVR = rpmdsDNEVR(ds)) == NULL)
		/*@innercontinue@*/ continue;	/* XXX can't happen */
	    rpmMessage(RPMMESS_NORMAL, " %s", DNEVR+2);
	}
	if (bingo)
	    rpmMessage(RPMMESS_NORMAL, "\n");
    }
    ds = rpmdsFree(ds);
}

/**
 */
static int rpmfcGenerateDependsHelper(const Spec spec, Package pkg, rpmfi fi)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
        /*@modifies fi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    StringBuf sb_stdin;
    StringBuf sb_stdout;
    DepMsg_t dm;
    int failnonzero = 0;
    int rc = 0;

    /*
     * Create file manifest buffer to deliver to dependency finder.
     */
    sb_stdin = newStringBuf();
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0)
	appendLineStringBuf(sb_stdin, rpmfiFN(fi));

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	int tag, tagflags;
	char * s;
	int xx;

	tag = (dm->ftag > 0) ? dm->ftag : dm->ntag;
	tagflags = 0;
	s = NULL;

	switch(tag) {
	case RPMTAG_PROVIDEFLAGS:
	    if (!pkg->autoProv)
		continue;
	    failnonzero = 1;
	    tagflags = RPMSENSE_FIND_PROVIDES;
	    /*@switchbreak@*/ break;
	case RPMTAG_REQUIREFLAGS:
	    if (!pkg->autoReq)
		continue;
	    failnonzero = 0;
	    tagflags = RPMSENSE_FIND_REQUIRES;
	    /*@switchbreak@*/ break;
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

	xx = rpmfcExec(dm->argv, sb_stdin, &sb_stdout, failnonzero);
	if (xx == -1)
	    continue;

	s = rpmExpand(dm->argv[0], NULL);
	rpmMessage(RPMMESS_NORMAL, _("Finding  %s: %s\n"), dm->msg,
		(s ? s : ""));
	s = _free(s);

	if (sb_stdout == NULL) {
	    rc = RPMERR_EXEC;
	    rpmError(rc, _("Failed to find %s:\n"), dm->msg);
	    break;
	}

	/* Parse dependencies into header */
	tagflags &= ~RPMSENSE_MULTILIB;

	rc = parseRCPOT(spec, pkg, getStringBuf(sb_stdout), tag, 0, tagflags);
	sb_stdout = freeStringBuf(sb_stdout);

	if (rc) {
	    rpmError(rc, _("Failed to find %s:\n"), dm->msg);
	    break;
	}
    }

    sb_stdin = freeStringBuf(sb_stdin);

    return rc;
}

int rpmfcGenerateDepends(const Spec spec, Package pkg)
{
    rpmfi fi = pkg->cpioList;
    rpmfc fc = NULL;
    rpmds ds;
    int scareMem = 0;
    ARGV_t av;
    int ac = rpmfiFC(fi);
    const void ** p;
    int c;
    int rc = 0;
    int xx;

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
/*@-noeffect@*/
	printDeps(pkg->header);
/*@=noeffect@*/
	return rc;
    }

    /* Extract absolute file paths in argv format. */
    av = xcalloc(ac+1, sizeof(*av));

    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((c = rpmfiNext(fi)) >= 0)
	av[c] = xstrdup(rpmfiFN(fi));
    av[ac] = NULL;

    fc = rpmfcNew();
    fc->skipProv = !pkg->autoProv;
    fc->skipReq = !pkg->autoReq;

    /* Copy (and delete) manually generated dependencies to dictionary. */
    if (!fc->skipProv) {
	ds = rpmdsNew(pkg->header, RPMTAG_PROVIDENAME, scareMem);
	xx = rpmdsMerge(&fc->provides, ds);
	ds = rpmdsFree(ds);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDENAME);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDEVERSION);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDEFLAGS);
    }

    if (!fc->skipReq) {
	ds = rpmdsNew(pkg->header, RPMTAG_REQUIRENAME, scareMem);
	xx = rpmdsMerge(&fc->requires, ds);
	ds = rpmdsFree(ds);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIRENAME);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIREVERSION);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIREFLAGS);
    }

    /* Build file class dictionary. */
    xx = rpmfcClassify(fc, av);

    /* Build file/package dependency dictionary. */
    xx = rpmfcApply(fc);

    /* Add per-file colors(#files) */
    p = (const void **) argiData(fc->fcolor);
    c = argiCount(fc->fcolor);
assert(ac == c);
    if (p != NULL && c > 0)
	xx = headerAddEntry(pkg->header, RPMTAG_FILECOLOR, RPM_INT32_TYPE,
			p, c);

    /* Add classes(#classes) */
    p = (const void **) argvData(fc->cdict);
    c = argvCount(fc->cdict);
    if (p != NULL && c > 0)
	xx = headerAddEntry(pkg->header, RPMTAG_CLASSDICT, RPM_STRING_ARRAY_TYPE,
			p, c);

    /* Add per-file classes(#files) */
    p = (const void **) argiData(fc->fcdictx);
    c = argiCount(fc->fcdictx);
assert(ac == c);
    if (p != NULL && c > 0)
	xx = headerAddEntry(pkg->header, RPMTAG_FILECLASS, RPM_INT32_TYPE,
			p, c);

    /* Add Provides: */
/*@-branchstate@*/
    if (fc->provides != NULL && (c = fc->provides->Count) > 0 && !fc->skipProv) {
	p = (const void **) fc->provides->N;
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->provides->EVR;
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->provides->Flags;
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			p, c);
    }

    /* Add Requires: */
    if (fc->requires != NULL && (c = fc->requires->Count && !fc->skipReq) > 0) {
	p = (const void **) fc->requires->N;
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIRENAME, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->requires->EVR;
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIREVERSION, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->requires->Flags;
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIREFLAGS, RPM_INT32_TYPE,
			p, c);
    }
/*@=branchstate@*/

    /* Add dependency dictionary(#dependencies) */
    p = (const void **) argiData(fc->ddictx);
    c = argiCount(fc->ddictx);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_DEPENDSDICT, RPM_INT32_TYPE,
			p, c);

    /* Add per-file dependency (start,number) pairs (#files) */
    p = (const void **) argiData(fc->fddictx);
    c = argiCount(fc->fddictx);
assert(ac == c);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_FILEDEPENDSX, RPM_INT32_TYPE,
			p, c);

    p = (const void **) argiData(fc->fddictn);
    c = argiCount(fc->fddictn);
assert(ac == c);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_FILEDEPENDSN, RPM_INT32_TYPE,
			p, c);

/*@-noeffect@*/
    printDeps(pkg->header);
/*@=noeffect@*/

#ifdef	NOTYET
if (fc != NULL) {
char buf[BUFSIZ];
sprintf(buf, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(buf, fc, NULL);
}
#endif

    /* Clean up. */
    fc = rpmfcFree(fc);
    av = argvFree(av);

    return rc;
}
