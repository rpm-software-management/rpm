#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>


#if HAVE_LIBELF_GELF_H

#include <libelf/gelf.h>

#if !defined(DT_GNU_PRELINKED)
#define	DT_GNU_PRELINKED	0x6ffffdf5
#endif
#if !defined(DT_GNU_LIBLIST)
#define	DT_GNU_LIBLIST		0x6ffffef9
#endif

#endif

#include "debug.h"

static int rpmfcELF(rpmfc fc)
{
#if HAVE_LIBELF_GELF_H && HAVE_LIBELF
    const char * fn = fc->fn[fc->ix];;
    Elf *elf = NULL;
    Elf_Scn *scn = NULL;
    Elf_Data *data = NULL;
    GElf_Ehdr ehdr;
    GElf_Shdr shdr;
    GElf_Dyn dyn;
    int bingo;
    int fdno;

size_t ndxscn[16];
const char * sn;

memset(ndxscn, 0, sizeof(ndxscn));
fprintf(stderr, "*** %s\n", fn);
    fdno = open(fn, O_RDONLY);
    if (fdno < 0)
	return fdno;

    (void) elf_version(EV_CURRENT);

    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || gelf_getehdr(elf, &ehdr) == NULL
     || !(ehdr.e_type == ET_DYN || ehdr.e_type == ET_EXEC))
	goto exit;

    bingo = 0;
    /*@-branchstate -uniondef @*/
    while (!bingo && (scn = elf_nextscn(elf, scn)) != NULL) {
	(void) gelf_getshdr(scn, &shdr);
fprintf(stderr, "\tsection %s\n", elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name));
	if (shdr.sh_type >= 0 && shdr.sh_type < 16)
	    ndxscn[shdr.sh_type] = elf_ndxscn(scn);
	switch (shdr.sh_type) {
	default:
	    continue;
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
	    /*@switchbreak@*/ break;
	case SHT_DYNAMIC:
	    while (!bingo && (data = elf_getdata (scn, data)) != NULL) {
		int maxndx = data->d_size / shdr.sh_entsize;
		int ndx;

		for (ndx = 0; ndx < maxndx; ++ndx) {
		    (void) gelf_getdyn (data, ndx, &dyn);
		    switch (dyn.d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ break;
		    case DT_NEEDED:
sn = elf_strptr(elf, ndxscn[SHT_STRTAB], dyn.d_un.d_val);
fprintf(stderr, "\t\tneeded %s\n", sn);
			/*@switchbreak@*/ break;
		    }
		}
	    }
	    /*@switchbreak@*/ break;
	}
    }
    /*@=branchstate =uniondef @*/

exit:
    if (elf) (void) elf_end(elf);
    return 0;
#else
    return -1;
#endif
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    StringBuf sb;
    ARGV_t pav;
    int pac = 0;
    ARGV_t xav;
    ARGV_t av = NULL;
    rpmfc fc;
    int ac = 0;
    const char * s;
    int ec = 1;
    int xx;
int fcolor;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    /* Find path to file(1). */
    s = rpmExpand("%{?__file}", NULL);
    if (!(s && *s))
	goto exit;

    /* Build argv, merging tokens from expansion with CLI args. */
    xx = poptParseArgvString(s, &pac, (const char ***)&pav);
    if (!(xx == 0 && pac > 0 && pav != NULL))
	goto exit;

    xav = NULL;
    xx = argvAppend(&xav, pav);
    xx = argvAppend(&xav, av);
    pav = _free(pav);	/* XXX popt mallocs in single blob. */
    s = _free(s);

    /* Read file(1) output. */
    sb = getOutputFrom(NULL, xav, NULL, 0, 1);
    xav = argvFree(xav);

    xx = argvSplit(&xav, getStringBuf(sb), "\n");
    sb = freeStringBuf(sb);

    xx = argvSort(xav, argvCmp);

    fc = NULL;
    xx = rpmfcClassify(&fc, xav);

    for (fc->ix = 0; fc->fn[fc->ix] != NULL; fc->ix++) {
	fcolor = fc->fcolor->vals[fc->ix];
	if (fcolor & RPMFC_ELF) {
	    xx = rpmfcELF(fc);
	    continue;
	}
    }
    fc = rpmfcFree(fc);

    xav = argvFree(xav);

exit:
    optCon = rpmcliFini(optCon);
    return ec;
}
