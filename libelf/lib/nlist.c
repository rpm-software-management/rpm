/*
nlist.c - implementation of the nlist(3) function.
Copyright (C) 1995, 1996 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <private.h>
#include <nlist.h>
#include <fcntl.h>

struct hash {
    const char*		name;
    unsigned long	hash;
    Elf32_Sym*		sym;
};

static int
_elf_nlist(Elf *elf, struct nlist *nl) {
    Elf_Scn *symtab = NULL;
    Elf_Scn *strtab = NULL;
    Elf_Data *symdata;
    Elf_Data *strdata;
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr;
    Elf_Scn *scn;
    Elf32_Sym *symbols;
    const char *strings;
    unsigned nsymbols;
    unsigned nstrings;
    unsigned i;
    const char *name;
    struct hash *table;
    unsigned nhash;
    unsigned long hash;
    unsigned long j;

    if (!(ehdr = elf32_getehdr(elf))) {
	return -1;
    }
    scn = NULL;
    elf_errno();
    while ((scn = elf_nextscn(elf, scn))) {
	if (!(shdr = elf32_getshdr(scn))) {
	    return -1;
	}
	if (shdr->sh_type != SHT_SYMTAB && shdr->sh_type != SHT_DYNSYM) {
	    continue;
	}
	symtab = scn;
	strtab = elf_getscn(elf, shdr->sh_link);
	if (shdr->sh_type == SHT_SYMTAB) {
	    break;
	}
    }
    if (elf_errno()) {
	return -1;
    }
    symdata = elf_getdata(symtab, NULL);
    strdata = elf_getdata(strtab, NULL);
    if (!symdata || !strdata) {
	return -1;
    }
    symbols = (Elf32_Sym*)symdata->d_buf;
    strings = (const char*)strdata->d_buf;
    nsymbols = symdata->d_size / sizeof(Elf32_Sym);
    nstrings = strdata->d_size;
    if (!symbols || !strings || !nsymbols || !nstrings) {
	return -1;
    }

    /*
     * build a simple hash table
     */
    nhash = 3 * nsymbols - 4;
    if (!(table = (struct hash*)malloc(nhash * sizeof(*table)))) {
	return -1;
    }
    for (i = 0; i < nhash; i++) {
	table[i].name = NULL;
    }
    for (i = 1; i < nsymbols; i++) {
	if (symbols[i].st_name < 0 || symbols[i].st_name >= nstrings) {
	    free(table);
	    return -1;
	}
	if (symbols[i].st_name == 0) {
	    continue;
	}
	name = strings + symbols[i].st_name;
	hash = elf_hash(name);
	for (j = hash; table[j %= nhash].name; j += 3) {
	    if (table[j].hash != hash) {
		continue;
	    }
	    if (table[j].name == name || !strcmp(table[j].name, name)) {
		break;
	    }
	}
	table[j].hash = hash;
	table[j].name = name;
	table[j].sym = &symbols[i];
    }

    /*
     * symbol lookup
     */
    for (i = 0; (name = nl[i].n_name) && *name; i++) {
	hash = elf_hash(name);
	for (j = hash; table[j %= nhash].name; j += 3) {
	    if (table[j].hash == hash && !strcmp(table[j].name, name)) {
		break;
	    }
	}
	if (table[j].name) {
	    nl[i].n_value = table[j].sym->st_value;
	    nl[i].n_scnum = table[j].sym->st_shndx;
	}
	else {
	    nl[i].n_value = 0;
	    nl[i].n_scnum = 0;
	}
	/*
	 * this needs more work
	 */
	nl[i].n_type = 0;
	nl[i].n_sclass = 0;
	nl[i].n_numaux = 0;
    }
    free(table);
    return 0;
}

int
nlist(const char *filename, struct nlist *nl) {
    int result = -1;
    unsigned oldver;
    Elf *elf;
    int fd;

    if ((oldver = elf_version(EV_CURRENT)) != EV_NONE) {
	if ((fd = open(filename, O_RDONLY)) != -1) {
	    if ((elf = elf_begin(fd, ELF_C_READ, NULL))) {
		result = _elf_nlist(elf, nl);
		elf_end(elf);
	    }
	    close(fd);
	}
	elf_version(oldver);
    }
    if (result) {
	while (nl->n_name && *nl->n_name) {
	    nl->n_value = 0;
	    nl++;
	}
    }
    return result;
}
