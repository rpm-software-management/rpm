/*
input.c - low-level input for libelf.
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

void*
_elf_read(Elf *elf, void *buffer, size_t off, size_t len) {
    void *tmp;

    elf_assert(elf);
    elf_assert(elf->e_magic == ELF_MAGIC);
    elf_assert(off >= 0 && off + len <= elf->e_size);
    if (elf->e_disabled) {
	seterr(ERROR_FDDISABLED);
    }
    else if (len) {
	off += elf->e_base;
	if (lseek(elf->e_fd, off, 0) != (off_t)off) {
	    seterr(ERROR_IO_SEEK);
	}
	else if (!(tmp = buffer) && !(tmp = malloc(len))) {
	    seterr(ERROR_IO_2BIG);
	}
	else if (read(elf->e_fd, tmp, len) != (int)len) {
	    seterr(ERROR_IO_READ);
	    if (tmp != buffer) {
		free(tmp);
	    }
	}
	else {
	    return tmp;
	}
    }
    return NULL;
}
