/*
strptr.c - implementation of the elf_strptr(3) function.
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
elf_strptr(Elf *elf, size_t section, size_t offset) {
    Elf_Scn *scn;
    Elf_Data *sd;

    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (!(scn = elf_getscn(elf, section))) {
	return NULL;
    }
    if (scn->s_type != SHT_STRTAB) {
	seterr(ERROR_NOSTRTAB);
	return NULL;
    }
    if (offset >= 0 && offset < scn->s_size) {
	sd = NULL;
	while ((sd = elf_getdata(scn, sd))) {
	    if (sd->d_buf && offset >= (size_t)sd->d_off
			  && offset < (size_t)sd->d_off + sd->d_size) {
		return (char*)sd->d_buf + (offset - sd->d_off);
	    }
	}
    }
    seterr(ERROR_BADSTROFF);
    return NULL;
}
