/*
32.newehdr.c - implementation of the elf32_newehdr(3) function.
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

Elf32_Ehdr*
elf32_newehdr(Elf *elf) {
    Elf32_Ehdr *ehdr;
    size_t size;

    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (elf->e_readable) {
	return elf32_getehdr(elf);
    }
    else if (!elf->e_ehdr) {
	size = _msize32(_elf_version, ELF_T_EHDR);
	elf_assert(size);
	if ((ehdr = (Elf32_Ehdr*)malloc(size))) {
	    memset(ehdr, 0, size);
	    elf->e_ehdr = (char*)ehdr;
	    elf->e_free_ehdr = 1;
	    elf->e_ehdr_flags |= ELF_F_DIRTY;
	    elf->e_kind = ELF_K_ELF;
	    elf->e_class = ELFCLASS32;
	    return ehdr;
	}
	seterr(ERROR_MEM_EHDR);
    }
    else if (elf->e_class != ELFCLASS32) {
	seterr(ERROR_CLASSMISMATCH);
    }
    else {
	elf_assert(elf->e_kind == ELF_K_ELF);
	return (Elf32_Ehdr*)elf->e_ehdr;
    }
    return NULL;
}
