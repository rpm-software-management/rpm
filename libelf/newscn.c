/*
newscn.c - implementation of the elf_newscn(3) function.
Copyright (C) 1995 Michael Riepe <riepe@ifwsn4.ifw.uni-hannover.de>

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

static Elf_Scn*
_buildscn(Elf *elf, size_t size) {
    Elf_Scn *scn;

    elf_assert(elf);
    elf_assert(size);
    elf_assert(_elf_scn_init.s_magic == SCN_MAGIC);
    while ((scn = (Elf_Scn*)malloc(sizeof(*scn) + size))) {
	*scn = _elf_scn_init;
	scn->s_elf = elf;
	scn->s_shdr = (char*)(scn + 1);
	memset(scn->s_shdr, 0, size);
	scn->s_scn_flags = ELF_F_DIRTY;
	scn->s_shdr_flags = ELF_F_DIRTY;
	scn->s_freeme = 1;
	if (elf->e_scn_n) {
	    scn->s_index = elf->e_scn_n->s_index + 1;
	    elf->e_scn_n->s_link = scn;
	    elf->e_scn_n = scn;
	    return scn;
	}
	elf_assert(scn->s_index == SHN_UNDEF);
	elf->e_scn_1 = elf->e_scn_n = scn;
    }
    seterr(ERROR_MEM_SCN);
    return NULL;
}

Elf_Scn*
elf_newscn(Elf *elf) {
    Elf_Scn *scn;

    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (!elf->e_ehdr && !elf->e_readable) {
	seterr(ERROR_NOEHDR);
    }
    else if (elf->e_kind != ELF_K_ELF) {
	seterr(ERROR_NOTELF);
    }
    else if (elf->e_ehdr || _elf_cook(elf)) {
	elf_assert(elf->e_ehdr);
	if (elf->e_class == ELFCLASS32) {
	    if ((scn = _buildscn(elf, sizeof(Elf32_Shdr)))) {
		((Elf32_Ehdr*)elf->e_ehdr)->e_shnum = scn->s_index + 1;
		elf->e_ehdr_flags |= ELF_F_DIRTY;
		return scn;
	    }
	}
	else if (valid_class(elf->e_class)) {
	    seterr(ERROR_UNIMPLEMENTED);
	}
	else {
	    seterr(ERROR_UNKNOWN_CLASS);
	}
    }
    return NULL;
}
