/*
32.newphdr.c - implementation of the elf32_newphdr(3) function.
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

Elf32_Phdr*
elf32_newphdr(Elf *elf, size_t count) {
    Elf32_Phdr *phdr = NULL;
    Elf32_Ehdr *ehdr;
    size_t size;

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
    else if (elf->e_class != ELFCLASS32) {
	seterr(ERROR_CLASSMISMATCH);
    }
    else if (elf->e_ehdr || _elf_cook(elf)) {
	ehdr = (Elf32_Ehdr*)elf->e_ehdr;
	if (count) {
	    size = _msize32(_elf_version, ELF_T_PHDR);
	    elf_assert(size);
	    if (!(phdr = (Elf32_Phdr*)malloc(count * size))) {
		seterr(ERROR_MEM_PHDR);
		return NULL;
	    }
	    memset(phdr, 0, count * size);
	}
	if (elf->e_free_phdr) {
	    elf_assert(elf->e_phdr);
	    free(elf->e_phdr);
	}
	elf->e_phdr = (char*)phdr;
	elf->e_free_phdr = phdr ? 1 : 0;
	elf->e_phdr_flags |= ELF_F_DIRTY;
	elf->e_phnum = ehdr->e_phnum = count;
	elf->e_ehdr_flags |= ELF_F_DIRTY;
	return phdr;
    }
    return NULL;
}
