
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <popt.h>
#include <gelf.h>

#include <rpm/rpmstring.h>
#include <rpm/argv.h>

int filter_private = 0;
int soname_only = 0;

typedef struct elfInfo_s {
    Elf *elf;

    int isDSO;
    int isElf64;		/* is 64bit marker needed in dependencies */
    int isExec;			/* requires are only added to executables */
    int gotDEBUG;
    int gotHASH;
    int gotGNUHASH;
    int gotSONAME;

    ARGV_t requires;
    ARGV_t provides;
} elfInfo;

static int skipPrivate(const char *s)
{ 
    return (filter_private && rstreq(s, "GLIBC_PRIVATE"));
}

static void processVerDef(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    unsigned int offset, auxoffset;
    char *soname = NULL;

    while ((data = elf_getdata(scn, data)) != NULL) {
	offset = 0;

	for (int i = shdr->sh_info; --i >= 0; ) {
	    GElf_Verdef def_mem, *def;
	    def = gelf_getverdef (data, offset, &def_mem);
	    if (def == NULL)
		break;
	    auxoffset = offset + def->vd_aux;
	    offset += def->vd_next;

	    for (int j = def->vd_cnt; --j >= 0; ) {
		GElf_Verdaux aux_mem, * aux;
		const char *s;
		aux = gelf_getverdaux (data, auxoffset, &aux_mem);
		if (aux == NULL)
		    break;
		s = elf_strptr(ei->elf, shdr->sh_link, aux->vda_name);
		if (s == NULL)
		    break;
		if (def->vd_flags & VER_FLG_BASE) {
		    rfree(soname);
		    soname = rstrdup(s);
		    auxoffset += aux->vda_next;
		    continue;
		} else if (soname && !soname_only && !skipPrivate(s)) {
		    const char *marker = ei->isElf64 ? "(64bit)" : "";
		    char *dep = NULL;
		    rasprintf(&dep, "%s(%s)%s", soname, s, marker);
		    argvAdd(&ei->provides, dep);
		    rfree(dep);
		}
	    }
		    
	}
    }
    rfree(soname);
}

static void processVerNeed(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    char *soname = NULL;
    while ((data = elf_getdata(scn, data)) != NULL) {
	unsigned int offset = 0, auxoffset;
	for (int i = shdr->sh_info; --i >= 0; ) {
	    const char *s = NULL;
	    GElf_Verneed need_mem, *need;
	    need = gelf_getverneed (data, offset, &need_mem);
	    if (need == NULL)
		break;

	    s = elf_strptr(ei->elf, shdr->sh_link, need->vn_file);
	    if (s == NULL)
		break;
	    rfree(soname);
	    soname = rstrdup(s);
	    auxoffset = offset + need->vn_aux;

	    for (int j = need->vn_cnt; --j >= 0; ) {
		GElf_Vernaux aux_mem, * aux;
		aux = gelf_getvernaux (data, auxoffset, &aux_mem);
		if (aux == NULL)
		    break;
		s = elf_strptr(ei->elf, shdr->sh_link, aux->vna_name);
		if (s == NULL)
		    break;

		if (ei->isExec && soname && !soname_only && !skipPrivate(s)) {
		    const char *marker = ei->isElf64 ? "(64bit)" : "";
		    char *dep = NULL;
		    rasprintf(&dep, "%s(%s)%s", soname, s, marker);
		    argvAdd(&ei->requires, dep);
		    rfree(dep);
		}
		auxoffset += aux->vna_next;
	    }
	    offset += need->vn_next;
	}
    }
    rfree(soname);
}

static void processDynamic(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    while ((data = elf_getdata(scn, data)) != NULL) {
	for (int i = 0; i < (shdr->sh_size / shdr->sh_entsize); i++) {
	    ARGV_t *deptype = NULL;
	    const char *s = NULL;
	    GElf_Dyn dyn_mem, *dyn;

	    dyn = gelf_getdyn (data, i, &dyn_mem);
	    if (dyn == NULL)
		break;

	    switch (dyn->d_tag) {
	    case DT_HASH:
		ei->gotHASH = 1;
		break;
	    case DT_GNU_HASH:
		ei->gotGNUHASH = 1;
		break;
	    case DT_DEBUG:
		ei->gotDEBUG = 1;
		break;
	    case DT_SONAME:
		ei->gotSONAME = 1;
		s = elf_strptr(ei->elf, shdr->sh_link, dyn->d_un.d_val);
		deptype = &ei->provides;
		break;
	    case DT_NEEDED:
		if (ei->isExec) {
		    s = elf_strptr(ei->elf, shdr->sh_link, dyn->d_un.d_val);
		    deptype = &ei->requires;
		}
		break;
	    default:
		break;
	    }

	    if (s && deptype) {
		const char *marker = ei->isElf64 ? "()(64bit)" : "";
		char *dep = rstrscat(NULL, s, marker, NULL);
		argvAdd(deptype, dep);
		rfree(dep);
	    }
	}
    }
}

static void processSections(elfInfo *ei)
{
    Elf_Scn * scn = NULL;
    while ((scn = elf_nextscn(ei->elf, scn)) != NULL) {
	GElf_Shdr shdr_mem, *shdr;
	shdr = gelf_getshdr(scn, &shdr_mem);
	if (shdr == NULL)
	    break;

	switch (shdr->sh_type) {
	case SHT_GNU_verdef:
	    processVerDef(scn, shdr, ei);
	    break;
	case SHT_GNU_verneed:
	    processVerNeed(scn, shdr, ei);
	    break;
	case SHT_DYNAMIC:
	    processDynamic(scn, shdr, ei);
	    break;
	default:
	    break;
	}
    }
}

static int processFile(const char *fn, int dtype)
{
    int rc = 1;
    int fdno = -1;
    struct stat st;
    GElf_Ehdr *ehdr, ehdr_mem;
    elfInfo *ei = rcalloc(1, sizeof(*ei));

    fdno = open(fn, O_RDONLY);
    if (fdno < 0 || fstat(fdno, &st) < 0)
	goto exit;

    (void) elf_version(EV_CURRENT);
    ei->elf = elf_begin(fdno, ELF_C_READ, NULL);
    if (ei->elf == NULL || elf_kind(ei->elf) != ELF_K_ELF)
	goto exit;

    ehdr = gelf_getehdr(ei->elf, &ehdr_mem);
    if (ehdr == NULL)
	goto exit;

    if (ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC) {
/* on alpha, everything is 64bit but we dont want the (64bit) markers */
#if !defined(__alpha__)
    	ei->isElf64 = (ehdr->e_ident[EI_CLASS] == ELFCLASS64);
#else
	ei->isElf64 = 0;
#endif
    	ei->isDSO = (ehdr->e_type == ET_DYN);
	ei->isExec = (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));

	processSections(ei);
    }

    /*
     * For DSOs which use the .gnu_hash section and don't have a .hash
     * section, we need to ensure that we have a new enough glibc.
     */
    if (ei->isExec && ei->gotGNUHASH && !ei->gotHASH && !soname_only) {
	argvAdd(&ei->requires, "rtld(GNU_HASH)");
    }

    /* For DSO's, provide the basename of the file if DT_SONAME not found. */
    if (ei->isDSO && !ei->gotDEBUG && !ei->gotSONAME) {
	const char *marker = ei->isElf64 ? "()(64bit)" : "";
	const char *bn = strrchr(fn, '/');
	char *dep;
	bn = bn ? bn + 1 : fn;
	dep = rstrscat(NULL, bn, marker, NULL);
	argvAdd(&ei->provides, dep);
	rfree(dep);
    }

    rc = 0;
    /* dump the requested dependencies for this file */
    for (ARGV_t dep = dtype ? ei->requires : ei->provides; dep && *dep; dep++) {
	fprintf(stdout, "%s\n", *dep);
    }

exit:
    if (fdno >= 0) close(fdno);
    if (ei) {
	argvFree(ei->provides);
	argvFree(ei->requires);
    	if (ei->elf) elf_end(ei->elf);
	rfree(ei);
    }
    return rc;
}

int main(int argc, char *argv[])
{
    char fn[BUFSIZ];
    int provides = 0;
    int requires = 0;
    poptContext optCon;

    struct poptOption opts[] = {
	{ "provides", 'P', POPT_ARG_VAL, &provides, -1, NULL, NULL },
	{ "requires", 'R', POPT_ARG_VAL, &requires, -1, NULL, NULL },
	{ "filter-private", 0, POPT_ARG_VAL, &filter_private, -1, NULL, NULL },
	{ "soname-only", 0, POPT_ARG_VAL, &soname_only, -1, NULL, NULL },
	POPT_AUTOHELP 
	POPT_TABLEEND
    };

    optCon = poptGetContext(argv[0], argc, (const char **) argv, opts, 0);
    if (argc < 2 || poptGetNextOpt(optCon) == 0) {
	poptPrintUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    while (fgets(fn, sizeof(fn), stdin) != NULL) {
	fn[strlen(fn)-1] = '\0';
	(void) processFile(fn, requires);
    }

    poptFreeContext(optCon);
    return 0;
}
