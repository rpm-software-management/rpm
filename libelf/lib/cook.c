/*
cook.c - read and translate ELF files.
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

const Elf_Scn _elf_scn_init = INIT_SCN;
const Scn_Data _elf_data_init = INIT_DATA;

const Elf_Type _elf_scn_types[SHT_NUM] = {
    /* SHT_NULL */	ELF_T_BYTE,
    /* SHT_PROGBITS */	ELF_T_BYTE,
    /* SHT_SYMTAB */	ELF_T_SYM,
    /* SHT_STRTAB */	ELF_T_BYTE,
    /* SHT_RELA */	ELF_T_RELA,
    /* SHT_HASH */	ELF_T_WORD,
    /* SHT_DYNAMIC */	ELF_T_DYN,
    /* SHT_NOTE */	ELF_T_BYTE,
    /* SHT_NOBITS */	ELF_T_BYTE,
    /* SHT_REL */	ELF_T_REL,
    /* SHT_SHLIB */	ELF_T_BYTE,
    /* SHT_DYNSYM */	ELF_T_SYM
};

#define truncerr(t) ((t)==ELF_T_EHDR?ERROR_TRUNC_EHDR:	\
		    ((t)==ELF_T_PHDR?ERROR_TRUNC_PHDR:	\
		    ERROR_INTERNAL))
#define memerr(t)   ((t)==ELF_T_EHDR?ERROR_MEM_EHDR:	\
		    ((t)==ELF_T_PHDR?ERROR_MEM_PHDR:	\
		    ERROR_INTERNAL))

static char*
_elf32_item(Elf *elf, Elf_Type type, unsigned n, size_t off, int *flag) {
    Elf_Data src, dst;

    *flag = 0;
    elf_assert(n);
    elf_assert(valid_type(type));
    if (off < 0 || off > elf->e_size) {
	seterr(ERROR_OUTSIDE);
	return NULL;
    }

    src.d_type = type;
    src.d_version = elf->e_version;
    src.d_size = n * _fsize32(src.d_version, type);
    elf_assert(src.d_size);
    if (off + src.d_size > elf->e_size) {
	seterr(truncerr(type));
	return NULL;
    }

    dst.d_version = _elf_version;
    dst.d_size = n * _msize32(dst.d_version, type);
    elf_assert(dst.d_size);

    elf_assert(elf->e_data);
    if (elf->e_rawdata != elf->e_data && dst.d_size <= src.d_size) {
	dst.d_buf = elf->e_data + off;
    }
    else if (!(dst.d_buf = malloc(dst.d_size))) {
	seterr(memerr(type));
	return NULL;
    }
    else {
	*flag = 1;
    }

    if (elf->e_rawdata) {
	src.d_buf = elf->e_rawdata + off;
    }
    else {
	src.d_buf = elf->e_data + off;
    }

    if (elf32_xlatetom(&dst, &src, elf->e_encoding)) {
	if (!*flag) {
	    elf->e_cooked = 1;
	}
	return (char*)dst.d_buf;
    }

    if (*flag) {
	free(dst.d_buf);
	*flag = 0;
    }
    return NULL;
}

#undef truncerr
#undef memerr

static int
_elf32_cook(Elf *elf) {
    Elf_Scn *scn;
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr;
    Scn_Data *sd;
    unsigned i;
    int flag;

    elf->e_ehdr = _elf32_item(elf, ELF_T_EHDR, 1, 0, &flag);
    if (!(ehdr = (Elf32_Ehdr*)elf->e_ehdr)) {
	return 0;
    }
    if (flag) {
	elf->e_free_ehdr = 1;
    }
    if (ehdr->e_phnum && ehdr->e_phoff) {
	elf->e_phdr = _elf32_item(elf, ELF_T_PHDR, ehdr->e_phnum, ehdr->e_phoff, &flag);
	if (!elf->e_phdr) {
	    return 0;
	}
	if (flag) {
	    elf->e_free_phdr = 1;
	}
	elf->e_phnum = ehdr->e_phnum;
    }
    if (ehdr->e_shnum && ehdr->e_shoff) {
	Elf_Data src, dst;
	struct tmp {
	    Elf_Scn	scn;
	    Scn_Data	data;
	} *head;

	src.d_type = ELF_T_SHDR;
	src.d_version = elf->e_version;
	src.d_size = _fsize32(src.d_version, ELF_T_SHDR);
	elf_assert(src.d_size);
	dst.d_version = EV_CURRENT;

	if (ehdr->e_shoff < 0 || ehdr->e_shoff > elf->e_size) {
	    seterr(ERROR_OUTSIDE);
	    return 0;
	}
	if (ehdr->e_shoff + ehdr->e_shnum * src.d_size > elf->e_size) {
	    seterr(ERROR_TRUNC_SHDR);
	    return 0;
	}

	if (!(head = (struct tmp*)malloc(ehdr->e_shnum * sizeof(*head)))) {
	    seterr(ERROR_MEM_SCN);
	    return 0;
	}
	for (scn = NULL, i = ehdr->e_shnum; i-- > 0; ) {
	    head[i].scn = _elf_scn_init;
	    head[i].data = _elf_data_init;
	    head[i].scn.s_link = scn;
	    if (!scn) {
		elf->e_scn_n = &head[i].scn;
	    }
	    scn = &head[i].scn;
	    sd = &head[i].data;

	    if (elf->e_rawdata) {
		src.d_buf = elf->e_rawdata + ehdr->e_shoff + i * src.d_size;
	    }
	    else {
		src.d_buf = elf->e_data + ehdr->e_shoff + i * src.d_size;
	    }
	    dst.d_buf = shdr = &scn->s_shdr32;
	    dst.d_size = sizeof(Elf32_Shdr);
	    if (!(elf32_xlatetom(&dst, &src, elf->e_encoding))) {
		elf->e_scn_n = NULL;
		free(head);
		return 0;
	    }
	    elf_assert(dst.d_size == sizeof(Elf32_Shdr));
	    elf_assert(dst.d_type == ELF_T_SHDR);

	    scn->s_elf = elf;
	    scn->s_index = i;
	    scn->s_data_1 = sd;
	    scn->s_data_n = sd;
	    scn->s_type = shdr->sh_type;
	    scn->s_size = shdr->sh_size;
	    scn->s_offset = shdr->sh_offset;

	    sd->sd_scn = scn;
	    if (valid_scntype(shdr->sh_type)) {
		sd->sd_data.d_type = _elf_scn_types[shdr->sh_type];
	    }
	    else {
		sd->sd_data.d_type = ELF_T_BYTE;
	    }
	    sd->sd_data.d_size = shdr->sh_size;
	    sd->sd_data.d_align = shdr->sh_addralign;
	    sd->sd_data.d_version = _elf_version;
	}
	elf_assert(scn == &head[0].scn);
	elf->e_scn_1 = &head[0].scn;
	head[0].scn.s_freeme = 1;
    }
    return 1;
}

int
_elf_cook(Elf *elf) {
    elf_assert(_elf_scn_init.s_magic == SCN_MAGIC);
    elf_assert(_elf_data_init.sd_magic == DATA_MAGIC);
    elf_assert(elf);
    elf_assert(elf->e_magic == ELF_MAGIC);
    elf_assert(elf->e_kind == ELF_K_ELF);
    elf_assert(!elf->e_ehdr);
    if (!valid_version(elf->e_version)) {
	seterr(ERROR_UNKNOWN_VERSION);
    }
    else if (!valid_encoding(elf->e_encoding)) {
	seterr(ERROR_UNKNOWN_ENCODING);
    }
    else if (elf->e_class == ELFCLASS32) {
	return _elf32_cook(elf);
    }
    else if (valid_class(elf->e_class)) {
	seterr(ERROR_UNIMPLEMENTED);
    }
    else {
	seterr(ERROR_UNKNOWN_CLASS);
    }
    return 0;
}
