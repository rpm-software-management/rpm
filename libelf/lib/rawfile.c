/*
rawfile.c - implementation of the elf_rawfile(3) function.
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

char*
elf_rawfile(Elf *elf, size_t *ptr) {
    size_t tmp;

    if (!ptr) {
	ptr = &tmp;
    }
    *ptr = 0;
    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (!elf->e_readable) {
	return NULL;
    }
    else if (elf->e_size && !elf->e_rawdata) {
	elf_assert(elf->e_data);
	if (!elf->e_cooked) {
	    elf->e_rawdata = elf->e_data;
	}
	else if (!(elf->e_rawdata = _elf_read(elf, NULL, 0, elf->e_size))) {
	    return NULL;
	}
	*ptr = elf->e_size;
    }
    return elf->e_rawdata;
}
