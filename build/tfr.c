#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>


#if HAVE_GELF_H

#include <gelf.h>

#endif

#include "debug.h"

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

fprintf(stderr, "*** %s\n", fn);
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

	switch (shdr->sh_type) {
	default:
	    continue;
	    /*@switchbreak@*/ break;
	case SHT_GNU_verdef:
fprintf(stderr, "\tsection %s\n", elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name));
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
fprintf(stderr, "\t\tverdef: %s\n", elf_strptr(elf, shdr->sh_link, aux->vda_name));

			auxoffset += aux->vda_next;
		    }
		    offset += def->vd_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_GNU_verneed:
fprintf(stderr, "\tsection %s\n", elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name));
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
		offset = 0;
		for (cnt = shdr->sh_info; --cnt >= 0; ) {
		    need = gelf_getverneed (data, offset, &need_mem);
		    if (need == NULL)
			break;
		    auxoffset = offset + need->vn_aux;
		    for (cnt2 = need->vn_cnt; --cnt2 >= 0; ) {
			GElf_Vernaux aux_mem, * aux;

			aux = gelf_getvernaux (data, auxoffset, &aux_mem);
			if (aux == NULL)
			    break;
fprintf(stderr, "\t\tverneed: %s\n", elf_strptr(elf, shdr->sh_link, aux->vna_name));

			auxoffset += aux->vna_next;
		    }
		    offset += need->vn_next;
		}
	    }
	    /*@switchbreak@*/ break;
	case SHT_DYNAMIC:
fprintf(stderr, "\tsection %s\n", elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name));
	    data = NULL;
	    while ((data = elf_getdata (scn, data)) != NULL) {
		for (cnt = 0; cnt < (shdr->sh_size / shdr->sh_entsize); ++cnt) {
		    dyn = gelf_getdyn (data, cnt, &dyn_mem);
		    switch (dyn->d_tag) {
		    default:
			/*@innercontinue@*/ continue;
			/*@notreached@*/ break;
		    case DT_NEEDED:
fprintf(stderr, "\t\tneeded: %s\n", elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
			/*@switchbreak@*/ break;
		    case DT_SONAME:
fprintf(stderr, "\t\tsoname: %s\n", elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
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
