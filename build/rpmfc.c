/*@-bounds@*/
#include "system.h"

#include <signal.h>	/* getOutputFrom() */

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>

#if HAVE_GELF_H
#include <gelf.h>
#endif

#include "debug.h"

/*@notchecked@*/
int _rpmfc_debug;

static int rpmfcExpandAppend(/*@out@*/ ARGV_t * argvp, const ARGV_t av)
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
static StringBuf getOutputFrom(/*@null@*/ const char * dir, char * argv[],
                        const char * writePtr, int writeBytesLeft,
                        int failNonZero)
        /*@globals fileSystem, internalState@*/
        /*@modifies fileSystem, internalState@*/
{
    int progPID;
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
    
    if (!(progPID = fork())) {
	(void) close(toProg[1]);
	(void) close(fromProg[0]);
	
	(void) dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	(void) dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */

	(void) close(toProg[0]);
	(void) close(fromProg[1]);

	if (dir) {
	    (void) chdir(dir);
	}
	
	unsetenv("MALLOC_CHECK_");
	(void) execvp(argv[0], argv);
	/* XXX this error message is probably not seen. */
	rpmError(RPMERR_EXEC, _("Couldn't exec %s: %s\n"),
		argv[0], strerror(errno));
	_exit(RPMERR_EXEC);
    }
    if (progPID < 0) {
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
    (void)waitpid(progPID, &status, 0);
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
/*@=boundswrite@*/

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

    if (sb_stdin) {
	buf_stdin = getStringBuf(sb_stdin);
	buf_stdin_len = strlen(buf_stdin);
    }

    /* Read output from exec'd helper. */
    sb = getOutputFrom(NULL, xav, buf_stdin, buf_stdin_len, failnonzero);

    if (sb_stdoutp) {
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

/**
 */
static int rpmfcSaveArg(ARGV_t * argvp, const char * key)
{
    int rc = 0;

    if (argvSearch(*argvp, key, NULL) == NULL) {
	rc = argvAdd(argvp, key);
	rc = argvSort(*argvp, NULL);
    }
    return rc;
}

/**
 */
static int rpmfcHelper(rpmfc fc, char deptype, const char * nsdep)
{
    const char * fn = fc->fn[fc->ix];
    char buf[BUFSIZ];
    StringBuf sb_stdout = NULL;
    StringBuf sb_stdin;
    const char *av[2];
    ARGV_t * depsp;
    ARGV_t pav;
    int pac;
    int xx;
    int i;
    size_t ns;
    char * t;

    switch (deptype) {
    default:
	return -1;
	break;
    case 'P':
	snprintf(buf, sizeof(buf), "%%{?__%s_provides}", nsdep);
	depsp = &fc->provides;
	break;
    case 'R':
	snprintf(buf, sizeof(buf), "%%{?__%s_requires}", nsdep);
	depsp = &fc->requires;
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
	xx = argvSplit(&pav, getStringBuf(sb_stdout), " \t\n\r");
	pac = argvCount(pav);
	if (pav)
	for (i = 0; i < pac; i++) {
	    t = buf;
	    *t = '\0';
	    t = stpcpy(t, pav[i]);
	    if (pav[i+1] && strchr("!=<>", *pav[i+1])) {
		i++;
		*t++ = ' ';
		t = stpcpy(t, pav[i]);
		if (pav[i+1]) {
		    i++;
		    *t++ = ' ';
		    t = stpcpy(t, pav[i]);
		}
	    }
	    ns = strlen(buf);

	    /* Add to package dependencies. */
	    xx = rpmfcSaveArg(depsp, buf);

	    /* Add to file dependencies. */
	    if (ns < (sizeof(buf) - ns - 64)) {
		t = buf + ns + 1;
		*t = '\0';
		sprintf(t, "%08d%c %s", fc->ix, deptype, buf);
		xx = rpmfcSaveArg(&fc->ddict, t);
	    }
	}

	pav = argvFree(pav);
    }
    sb_stdout = freeStringBuf(sb_stdout);

    return 0;
}

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

nprovides = argvCount(fc->provides);
nrequires = argvCount(fc->requires);

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
	    char deptype;
	    int ix;

	    ix = fc->ddictx->vals[dx++];
	    deptype = ((ix >> 24) & 0xff);
	    ix &= 0x00ffffff;
	    depval = NULL;
	    switch (deptype) {
	    default:
assert(depval);
		break;
	    case 'P':
assert(ix < nprovides);
		depval = fc->provides[ix];
		break;
	    case 'R':
assert(ix < nrequires);
		depval = fc->requires[ix];
		break;
	    }
	    if (depval)
		fprintf(fp, "\t%c %s\n", deptype, depval);
	}
    }
}

rpmfc rpmfcFree(rpmfc fc)
{
    if (fc) {
	fc->fn = argvFree(fc->fn);
	fc->fcdictx = argiFree(fc->fcdictx);
	fc->fcolor = argiFree(fc->fcolor);
	fc->cdict = argvFree(fc->cdict);
	fc->fddictx = argiFree(fc->fddictx);
	fc->fddictn = argiFree(fc->fddictn);
	fc->ddict = argvFree(fc->ddict);
	fc->ddictx = argiFree(fc->ddictx);
	fc->provides = argvFree(fc->provides);
	fc->requires = argvFree(fc->requires);
    }
    fc = _free(fc);
    return NULL;
}

rpmfc rpmfcNew(void)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    return fc;
}

static int rpmfcSCRIPT(rpmfc fc)
{
    const char * fn = fc->fn[fc->ix];
    const char * bn;
    char deptype;
    char buf[BUFSIZ];
    FILE * fp;
    char * s, * se;
    size_t ns;
    char * t;
    int i;
    struct stat sb, * st = &sb;;
    int xx;

    /* Only executable scripts are searched. */
    if (stat(fn, st) < 0)
	return -1;
    if (!(st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
	return 0;

    fp = fopen(fn, "r");
    if (fp == NULL || ferror(fp)) {
	if (fp) fclose(fp);
	return -1;
    }

    /* Look for #! interpreter in first 10 lines. */
    deptype = 'R';
    for (i = 0; i < 10; i++) {

	s = fgets(buf, sizeof(buf) - 1, fp);
	if (s == NULL || ferror(fp) || feof(fp))
	    break;
	s[sizeof(buf)-1] = '\0';
	ns = strlen(s);
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
		break;
	}
	*se = '\0';

	/* Add to package requires. */
	xx = rpmfcSaveArg(&fc->requires, s);

	/* Add to file requires. */
	if (ns < (sizeof(buf) - ns - 64)) {
	    t = se + 1;
	    *t = '\0';
	    sprintf(t, "%08d%c %s", fc->ix, deptype, s);
	    xx = rpmfcSaveArg(&fc->ddict, t);
	}

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

static int rpmfcELF(rpmfc fc)
{
#if HAVE_GELF_H && HAVE_LIBELF
    const char * fn = fc->fn[fc->ix];;
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
    char deptype;
    const char * soname = NULL;
    ARGV_t * depsp;
    const char * depval;
    char * t;
    int xx;
    int isElf64;

    fdno = open(fn, O_RDONLY);
    if (fdno < 0)
	return fdno;

    (void) elf_version(EV_CURRENT);

    elf = NULL;
    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || (ehdr = gelf_getehdr(elf, &ehdr_mem)) == NULL
     || !(ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC))
	goto exit;

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
	    /*@switchbreak@*/ break;
	case SHT_GNU_verdef:
	    deptype = 'P';
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		
		    def = gelf_getverdef (data, offset, &def_mem);
		    if (def == NULL)
			break;
		    auxoffset = offset + def->vd_aux;
		    for (cnt2 = def->vd_cnt; --cnt2 >= 0; ) {
			GElf_Verdaux aux_mem, * aux;

			aux = gelf_getverdaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    break;

			s = elf_strptr(elf, shdr->sh_link, aux->vda_name);
			/* XXX Ick, but what's a girl to do. */
			if (!strncmp("ld-", s, 3) || !strncmp("lib", s, 3))
			{
			    soname = _free(soname);
			    soname = xstrdup(s);
			    auxoffset += aux->vda_next;
			    continue;
			}

			t = buf;
			*t = '\0';
			sprintf(t, "%08d%c ", fc->ix, deptype);
			t += strlen(t);
			depval = t;
			t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

#if !defined(__alpha__)
			if (isElf64)
			    t = stpcpy(t, "(64bit)");
#endif

			/* Add to package provides. */
			xx = rpmfcSaveArg(&fc->provides, depval);

			/* Add to file dependencies. */
			xx = rpmfcSaveArg(&fc->ddict, buf);

			auxoffset += aux->vda_next;
		    }
		    offset += def->vd_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
	    deptype = 'R';
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		    need = gelf_getverneed (data, offset, &need_mem);
		    if (need == NULL)
			break;

		    s = elf_strptr(elf, shdr->sh_link, need->vn_file);
		    soname = _free(soname);
		    soname = xstrdup(s);
		    auxoffset = offset + need->vn_aux;
		    for (cnt2 = need->vn_cnt; --cnt2 >= 0; ) {
			GElf_Vernaux aux_mem, * aux;

			aux = gelf_getvernaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    break;

			s = elf_strptr(elf, shdr->sh_link, aux->vna_name);
assert(soname);

			t = buf;
			*t = '\0';
			sprintf(t, "%08d%c ", fc->ix, deptype);
			t += strlen(t);
			depval = t;
			t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

#if !defined(__alpha__)
			if (isElf64)
			    t = stpcpy(t, "(64bit)");
#endif

			/* Add to package requires. */
			xx = rpmfcSaveArg(&fc->requires, depval);

			/* Add to file dependencies. */
			xx = rpmfcSaveArg(&fc->ddict, buf);

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
		    s = NULL;
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ break;
		    case DT_NEEDED:
			/* Add to package requires. */
			deptype = 'R';
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
			depsp = &fc->requires;
			/*@switchbreak@*/ break;
		    case DT_SONAME:
			/* Add to package provides. */
			deptype = 'P';
			depsp = &fc->provides;
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
			/*@switchbreak@*/ break;
		    }
		    if (s == NULL)
			continue;

		    t = buf;
		    *t = '\0';
		    sprintf(t, "%08d%c ", fc->ix, deptype);
		    t += strlen(t);
		    depval = t;
		    t = stpcpy(t, s);

#if !defined(__alpha__)
		    if (isElf64)
			t = stpcpy(t, "()(64bit)");
#endif

		    /* Add to package requires. */
		    xx = rpmfcSaveArg(depsp, depval);

		    /* Add to file dependencies. */
		    xx = rpmfcSaveArg(&fc->ddict, buf);
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

int rpmfcClassify(rpmfc fc, ARGV_t argv)
{
    char buf[BUFSIZ];
    ARGV_t dav;
    ARGV_t av;
    const char * s, * se;
    char * t;
    int fcolor;
    int xx;

    if (fc == NULL || argv == NULL)
	return 0;

    fc->nfiles = argvCount(argv);

    xx = argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    xx = argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Set up the file class dictionary. */
    xx = argvAdd(&fc->cdict, "");
    xx = argvAdd(&fc->cdict, "directory");

    av = argv;
    while ((s = *av++) != NULL) {
	for (se = s; *se; se++) {
	    if (se[0] == ':' && se[1] == ' ')
		break;
	}
	if (*se == '\0')
	    return -1;

	for (se++; *se; se++) {
	    if (!(*se == ' ' || *se == '\t'))
		break;
	}
	if (*se == '\0')
	    return -1;

	fcolor = rpmfcColoring(se);
	if (fcolor == RPMFC_WHITE || !(fcolor & RPMFC_INCLUDE))
	    continue;

	xx = rpmfcSaveArg(&fc->cdict, se);
    }

    /* Classify files. */
    fc->ix = 0;
    fc->fknown = 0;
    av = argv;
    while ((s = *av++) != NULL) {
	for (se = s; *se; se++) {
	    if (se[0] == ':' && se[1] == ' ')
		break;
	}
	if (*se == '\0')
	    return -1;

	t = stpncpy(buf, s, (se - s));
	*t = '\0';

	xx = argvAdd(&fc->fn, buf);

	for (se++; *se; se++) {
	    if (!(*se == ' ' || *se == '\t'))
		break;
	}
	if (*se == '\0')
	    return -1;

	dav = argvSearch(fc->cdict, se, NULL);
	if (dav) {
	    xx = argiAdd(&fc->fcdictx, fc->ix, (dav - fc->cdict));
	    fc->fknown++;
	} else {
	    xx = argiAdd(&fc->fcdictx, fc->ix, 0);
	    fc->fwhite++;
	}

	fcolor = rpmfcColoring(se);
	xx = argiAdd(&fc->fcolor, fc->ix, fcolor);

	fc->ix++;
    }

    return 0;
}

typedef struct rpmfcApplyTbl_s {
    int (*func) (rpmfc fc);
    int colormask;
} * rpmfcApplyTbl;

static struct rpmfcApplyTbl_s rpmfcApplyTable[] = {
    { rpmfcELF,		RPMFC_ELF },
    { rpmfcSCRIPT,	RPMFC_SCRIPT },
    { NULL, 0 }
};

int rpmfcApply(rpmfc fc)
{
    const char * s;
    char * se;
    ARGV_t dav, davbase;
    rpmfcApplyTbl fcat;
    char deptype;
    int nddict;
    int previx;
    unsigned int val;
    int ix;
    int i;
    int xx;

    /* Generate package and per-file dependencies. */
    for (fc->ix = 0; fc->fn[fc->ix] != NULL; fc->ix++) {
	for (fcat = rpmfcApplyTable; fcat->func != NULL; fcat++) {
	    if (!(fc->fcolor->vals[fc->ix] & fcat->colormask))
		continue;
	    xx = (*fcat->func) (fc);
	}
    }

    /* Generate per-file indices into package dependencies. */
    nddict = argvCount(fc->ddict);
    previx = -1;
    for (i = 0; i < nddict; i++) {
	s = fc->ddict[i];
	ix = strtol(s, &se, 10);
assert(se);
	deptype = *se++;
	se++;
	
	davbase = NULL;
	switch (deptype) {
	default:
assert(davbase);
	    break;
	case 'P':	
	    davbase = fc->provides;
	    break;
	case 'R':
	    davbase = fc->requires;
	    break;
	}

	dav = argvSearch(davbase, se, NULL);
assert(dav);
	val = (deptype << 24) | ((dav - davbase) & 0x00ffffff);
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
/*@=bounds@*/
