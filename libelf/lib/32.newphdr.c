/*
32.newphdr.c - implementation of the elf{32,64}_newphdr(3) functions.
Copyright (C) 1995 - 2000 Michael Riepe <michael@stud.uni-hannover.de>

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

#ifndef lint
static const char rcsid[] = "@(#) Id: 32.newphdr.c,v 1.9 2000/03/31 12:42:32 michael Exp ";
#endif /* lint */

/*@null@*/
static char*
_elf_newphdr(Elf *elf, size_t count, unsigned cls)
	/*@globals _elf_errno @*/
	/*@modifies *elf, _elf_errno @*/
{
    char *phdr = NULL;
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
    else if (elf->e_class != cls) {
	seterr(ERROR_CLASSMISMATCH);
    }
    else if (elf->e_ehdr || _elf_cook(elf)) {
	size = _msize(cls, _elf_version, ELF_T_PHDR);
	elf_assert(size);
	if (count) {
	    if (!(phdr = (char*)malloc(count * size))) {
		seterr(ERROR_MEM_PHDR);
		return NULL;
	    }
	    memset(phdr, 0, count * size);
	}
	elf_assert(elf->e_ehdr);
	if (cls == ELFCLASS32) {
	    ((Elf32_Ehdr*)elf->e_ehdr)->e_phnum = elf->e_phnum = count;
	}
#if __LIBELF64
	else if (cls == ELFCLASS64) {
	    ((Elf64_Ehdr*)elf->e_ehdr)->e_phnum = elf->e_phnum = count;
	}
#endif /* __LIBELF64 */
	else {
	    seterr(ERROR_UNIMPLEMENTED);
	    if (phdr) {
		free(phdr);
	    }
	    return NULL;
	}
	if (elf->e_free_phdr) {
	    elf_assert(elf->e_phdr);
	    free(elf->e_phdr);
	}
	elf->e_phdr = phdr;
	elf->e_free_phdr = phdr ? 1 : 0;
	elf->e_phdr_flags |= ELF_F_DIRTY;
	elf->e_ehdr_flags |= ELF_F_DIRTY;
	return phdr;
    }
    return NULL;
}

Elf32_Phdr*
elf32_newphdr(Elf *elf, size_t count) {
    return (Elf32_Phdr*)_elf_newphdr(elf, count, ELFCLASS32);
}

#if __LIBELF64

Elf64_Phdr*
elf64_newphdr(Elf *elf, size_t count) {
    return (Elf64_Phdr*)_elf_newphdr(elf, count, ELFCLASS64);
}

unsigned long
gelf_newphdr(Elf *elf, size_t phnum) {
    if (!valid_class(elf->e_class)) {
	seterr(ERROR_UNKNOWN_CLASS);
	return 0;
    }
    if (!_elf_newphdr(elf, phnum, elf->e_class)) {
	return 0;
    }
    return 1;	/* really? */
}

#endif /* __LIBELF64 */
