#include "system.h"

#include <format>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <popt.h>
#include <gelf.h>

#include <rpm/rpmstring.h>

int soname_only = 0;
int fake_soname = 1;
int filter_soname = 1;
int require_interp = 0;
int multifile = 0;

struct elfInfo {
    Elf *elf;

    int isDSO;
    int isExec;			/* requires are only added to executables */
    int gotDEBUG;
    int gotHASH;
    int gotGNUHASH;
    std::string soname;
    std::string interp;
    std::string marker;		/* elf class marker */

    std::vector<std::string> requires_;
    std::vector<std::string> provides;
};

/*
 * Rough soname sanity filtering: all sane soname's dependencies need to
 * contain ".so", and normal linkable libraries start with "lib",
 * everything else is an exception of some sort. The most notable
 * and common exception is the dynamic linker itself, which we allow
 * here, the rest can use --no-filter-soname.
 */
static bool skipSoname(const std::string & soname)
{
    /* Filter out empty and all-whitespace sonames */
    if (soname.empty())
	return true;

    if (soname.find_first_not_of(" \t\n\r\f\v") == std::string::npos)
	return true;

    if (filter_soname) {
	if (soname.find(".so") == std::string::npos)
	    return true;

	const auto keep = { "ld.", "ld-", "ld64.", "ld64-", "lib" };
	for (auto & prefix : keep) {
	    if (soname.starts_with(prefix))
		return false;
	}
	return true;
    }

    return false;
}

static int genRequires(elfInfo *ei)
{
    return !(ei->interp.empty() == false && ei->isExec == 0);
}

static std::string mkmarker(GElf_Ehdr *ehdr)
{
    std::string marker;
    if (ehdr->e_ident[EI_CLASS] == ELFCLASS64) {
	switch (ehdr->e_machine) {
	case EM_ALPHA:
	case EM_FAKE_ALPHA:
	    /* alpha doesn't traditionally have 64bit markers */
	    break;
	default:
	    marker = "(64bit)";
	    break;
	}
    }
    return marker;
}

static void addDep(std::vector<std::string> & deps, const std::string & dep)
{
    deps.push_back(dep);
}

static void addSoDep(std::vector<std::string> & deps,
		     const std::string & soname,
		     const std::string & ver, const std::string & marker)
{
    if (skipSoname(soname))
	return;

    if (ver.empty() && marker.empty()) {
	addDep(deps, soname);
    } else {
	auto dep = std::format("{}({}){}", soname, ver, marker);
	addDep(deps, dep);
    }
}

static void processVerDef(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    unsigned int offset, auxoffset;
    std::string soname;

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
		    soname = s;
		    auxoffset += aux->vda_next;
		    continue;
		} else if (!soname_only) {
		    addSoDep(ei->provides, soname, s, ei->marker);
		}
	    }
		    
	}
    }
}

static void processVerNeed(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    std::string soname;
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
	    soname = s;
	    auxoffset = offset + need->vn_aux;

	    for (int j = need->vn_cnt; --j >= 0; ) {
		GElf_Vernaux aux_mem, * aux;
		aux = gelf_getvernaux (data, auxoffset, &aux_mem);
		if (aux == NULL)
		    break;
		s = elf_strptr(ei->elf, shdr->sh_link, aux->vna_name);
		if (s == NULL)
		    break;

		if (genRequires(ei) && !soname_only) {
		    addSoDep(ei->requires_, soname, s, ei->marker);
		}
		auxoffset += aux->vna_next;
	    }
	    offset += need->vn_next;
	}
    }
}

static void processDynamic(Elf_Scn *scn, GElf_Shdr *shdr, elfInfo *ei)
{
    Elf_Data *data = NULL;
    if (shdr->sh_entsize == 0)
	return;
    while ((data = elf_getdata(scn, data)) != NULL) {
	for (unsigned i = 0; i < (shdr->sh_size / shdr->sh_entsize); i++) {
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
		s = elf_strptr(ei->elf, shdr->sh_link, dyn->d_un.d_val);
		if (s)
		    ei->soname = s;
		break;
	    case DT_NEEDED:
		if (genRequires(ei)) {
		    s = elf_strptr(ei->elf, shdr->sh_link, dyn->d_un.d_val);
		    if (s)
			addSoDep(ei->requires_, s, "", ei->marker);
		}
		break;
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

static void processProgHeaders(elfInfo *ei, GElf_Ehdr *ehdr)
{
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
	GElf_Phdr mem;
	GElf_Phdr *phdr = gelf_getphdr(ei->elf, i, &mem);

	if (phdr && phdr->p_type == PT_INTERP) {
	    size_t maxsize;
	    char * filedata = elf_rawfile(ei->elf, &maxsize);

	    if (filedata && phdr->p_offset < maxsize) {
		ei->interp = filedata + phdr->p_offset;
		break;
	    }
	}
    }
}

static int processFile(const char *fn, int dtype)
{
    int rc = 1;
    int fdno;
    struct stat st;
    GElf_Ehdr *ehdr, ehdr_mem;
    elfInfo *ei = new elfInfo {};
    auto const & dep = dtype ? ei->requires_ : ei->provides;

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
	ei->marker = mkmarker(ehdr);
    	ei->isDSO = (ehdr->e_type == ET_DYN);
	ei->isExec = (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));

	processProgHeaders(ei, ehdr);
	processSections(ei);
    }

    /*
     * For DSOs which use the .gnu_hash section and don't have a .hash
     * section, we need to ensure that we have a new enough glibc.
     */
    if (genRequires(ei) && ei->gotGNUHASH && !ei->gotHASH && !soname_only) {
	addDep(ei->requires_, "rtld(GNU_HASH)");
    }

    /*
     * For DSOs, add DT_SONAME as provide. If its missing, we can fake
     * it from the basename if requested. The bizarre looking DT_DEBUG
     * check is used to avoid adding basename provides for PIE executables.
     */
    if (ei->isDSO && !ei->gotDEBUG) {
	if (ei->soname.empty() && fake_soname) {
	    const char *bn = strrchr(fn, '/');
	    ei->soname = bn ? bn + 1 : fn;
	}
	if (ei->soname.empty() == false)
	    addSoDep(ei->provides, ei->soname, "", ei->marker);
    }

    /* If requested and present, add dep for interpreter (ie dynamic linker) */
    if (ei->interp.empty() == false && require_interp)
	addDep(ei->requires_, ei->interp);

    rc = 0;
    /* dump the requested dependencies for this file */
    if (dep.empty() == false) {
	if (multifile)
	    fprintf(stdout, ";%s\n", fn);
	for (auto const & d : dep)
	    fprintf(stdout, "%s\n", d.c_str());
    }

exit:
    if (fdno >= 0) close(fdno);
    if (ei) {
    	if (ei->elf) elf_end(ei->elf);
	delete ei;
    }
    return rc;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    int provides = 0;
    int requires_ = 0;
    poptContext optCon;

    struct poptOption opts[] = {
	{ "provides", 'P', POPT_ARG_VAL, &provides, -1, NULL, NULL },
	{ "requires", 'R', POPT_ARG_VAL, &requires_, -1, NULL, NULL },
	{ "soname-only", 0, POPT_ARG_VAL, &soname_only, -1, NULL, NULL },
	{ "no-fake-soname", 0, POPT_ARG_VAL, &fake_soname, 0, NULL, NULL },
	{ "no-filter-soname", 0, POPT_ARG_VAL, &filter_soname, 0, NULL, NULL },
	{ "require-interp", 0, POPT_ARG_VAL, &require_interp, -1, NULL, NULL },
	{ "multifile", 'm', POPT_ARG_VAL, &multifile, -1, NULL, NULL },
	POPT_AUTOHELP 
	POPT_TABLEEND
    };

    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = poptGetContext(argv[0], argc, (const char **) argv, opts, 0);
    if (argc < 2 || poptGetNextOpt(optCon) == 0) {
	poptPrintUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    /* Normally our data comes from stdin, but permit args too */
    if (poptPeekArg(optCon)) {
	const char *fn;
	while ((fn = poptGetArg(optCon)) != NULL) {
	    if (processFile(fn, requires_))
		rc = EXIT_FAILURE;
	}
    } else {
	char fn[BUFSIZ];
	while (fgets(fn, sizeof(fn), stdin) != NULL) {
	    fn[strlen(fn)-1] = '\0';
	    if (processFile(fn, requires_))
		rc = EXIT_FAILURE;
	}
    }

    poptFreeContext(optCon);
    return rc;
}
