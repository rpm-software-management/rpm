/*@-bounds@*/
#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>

#if HAVE_GELF_H
#include <gelf.h>
#endif

#include "debug.h"

/*@notchecked@*/
int _rpmfc_debug;

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
    const char * fn = fc->fn[fc->ix];;
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
	if (argvSearch(fc->requires, s, NULL) == NULL) {
	    xx = argvAdd(&fc->requires, s);
	    xx = argvSort(fc->requires, NULL);
	}

	/* Add to file dependencies. */
	if (ns < (sizeof(buf) - ns - 64)) {
	    t = se + 1;
	    *t = '\0';
	    sprintf(t, "%08d%c %s", fc->ix, deptype, s);
	    if (argvSearch(fc->ddict, t, NULL) == NULL) {
		xx = argvAdd(&fc->ddict, t);
		xx = argvSort(fc->ddict, NULL);
	    }
	}
	break;
    }

    (void) fclose(fp);
    return 0;
}

#define	_permitstr(_a, _b)	(!strncmp((_a), (_b), sizeof(_b)-1))
static int rpmfcELFpermit(const char *s)
{
    if (_permitstr(s, "GLIBCPP_"))	return 1;
    if (_permitstr(s, "GLIBC_"))	return 1;
    if (_permitstr(s, "GCC_"))		return 1;
    if (_permitstr(s, "REDHAT_"))	return 1;
    return 0;
}
#undef _permitstr

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
    const char * depval;
    size_t ns;
    char * t;
    int xx;

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
			if (!rpmfcELFpermit(s)) {
			    soname = _free(soname);
			    soname = xstrdup(s);
			    auxoffset += aux->vda_next;
			    continue;
			}
			ns = strlen(s);

			t = buf;
			*t = '\0';
			sprintf(t, "%08d%c ", fc->ix, deptype);
			t += strlen(t);
			depval = t;
			t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			/* Add to package provides. */
			if (argvSearch(fc->provides, depval, NULL) == NULL) {
			    xx = argvAdd(&fc->provides, depval);
			    xx = argvSort(fc->provides, NULL);
			}

			/* Add to file dependencies. */
			if (ns < (sizeof(buf)-64)) {
			    if (argvSearch(fc->ddict, buf, NULL) == NULL) {
				xx = argvAdd(&fc->ddict, buf);
				xx = argvSort(fc->ddict, NULL);
			    }
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
			if (!rpmfcELFpermit(s)) {
			    auxoffset += aux->vna_next;
			    continue;
			}
			ns = strlen(s);

			if (soname == NULL)
			    soname = xstrdup("libc.so.6");

			t = buf;
			*t = '\0';
			sprintf(t, "%08d%c ", fc->ix, deptype);
			t += strlen(t);
			depval = t;
			t = stpcpy( stpcpy( stpcpy( stpcpy(t, soname), "("), s), ")");

			/* Add to package requires. */
			if (argvSearch(fc->requires, depval, NULL) == NULL) {
			    xx = argvAdd(&fc->requires, depval);
			    xx = argvSort(fc->requires, NULL);
			}

			/* Add to file dependencies. */
			if (ns < (sizeof(buf)-64)) {
			    if (argvSearch(fc->ddict, buf, NULL) == NULL) {
				xx = argvAdd(&fc->ddict, buf);
				xx = argvSort(fc->ddict, NULL);
			    }
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
		    t = buf;
		    *t = '\0';
		    s = NULL;
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ break;
		    case DT_NEEDED:
			/* Add to package requires. */
			deptype = 'R';
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
			if (argvSearch(fc->requires, s, NULL) == NULL) {
			    xx = argvAdd(&fc->requires, s);
			    xx = argvSort(fc->requires, NULL);
			}
			/*@switchbreak@*/ break;
		    case DT_SONAME:
			/* Add to package provides. */
			deptype = 'P';
			s = elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val);
			if (argvSearch(fc->provides, s, NULL) == NULL) {
			    xx = argvAdd(&fc->provides, s);
			    xx = argvSort(fc->provides, NULL);
			}
			/*@switchbreak@*/ break;
		    }
		    if (s == NULL)
			continue;
		    ns = strlen(s);
		    /* Add to file dependencies. */
		    if (ns < (sizeof(buf)-64)) {
			sprintf(t, "%08d%c ", fc->ix, deptype);
			t += strlen(t);
			t = stpcpy(t, s);
			if (argvSearch(fc->ddict, buf, NULL) == NULL) {
			    xx = argvAdd(&fc->ddict, buf);
			    xx = argvSort(fc->ddict, NULL);
			}
		    }
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
	se = s;
	while (*se && !(se[0] == ':' && se[1] == ' '))
	    se++;
	if (*se == '\0')
	    return -1;

	se++;
	while (*se && (*se == ' ' || *se == '\t'))
	    se++;
	if (*se == '\0')
	    return -1;

	fcolor = rpmfcColoring(se);
	if (fcolor == RPMFC_WHITE || !(fcolor & RPMFC_INCLUDE))
	    continue;

	dav = argvSearch(fc->cdict, se, NULL);
	if (dav == NULL) {
	    xx = argvAdd(&fc->cdict, se);
	    xx = argvSort(fc->cdict, NULL);
	}
    }

    /* Classify files. */
    fc->ix = 0;
    fc->fknown = 0;
    av = argv;
    while ((s = *av++) != NULL) {
	se = s;
	while (*se && !(se[0] == ':' && se[1] == ' '))
	    se++;
	if (*se == '\0')
	    return -1;

	t = stpncpy(buf, s, (se - s));
	*t = '\0';

	xx = argvAdd(&fc->fn, buf);

	se++;
	while (*se && (*se == ' ' || *se == '\t'))
	    se++;
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

#ifdef	DYING
	if (fcolor & RPMFC_ELF) {
	    xx = rpmfcELF(fc);
	} else if (fcolor & RPMFC_SCRIPT) {
	    xx = rpmfcSCRIPT(fc);
	}
#endif

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
    int fcolor;
    int nddict;
    int previx;
    unsigned int val;
    int ix;
    int i;
    int xx;

    /* Generate package and per-file dependencies. */
    for (fc->ix = 0; fc->fn[fc->ix] != NULL; fc->ix++) {
	fcolor = fc->fcolor->vals[fc->ix];
	for (fcat = rpmfcApplyTable; fcat->func != NULL; fcat++) {
	    if (!(fcolor & fcat->colormask))
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
