/*
input.c - low-level input for libelf.
Copyright (C) 1995 - 2001 Michael Riepe <michael@stud.uni-hannover.de>

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
static const char rcsid[] = "@(#) Id: input.c,v 1.6 2001/10/15 21:38:29 michael Exp ";
#endif /* lint */

#if HAVE_MMAP
#include <sys/mman.h>
#endif /* HAVE_MMAP */

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
	if (lseek(elf->e_fd, (off_t)off, SEEK_SET) != (off_t)off) {
	    seterr(ERROR_IO_SEEK);
	}
	else if (!(tmp = buffer) && !(tmp = malloc(len))) {
	    seterr(ERROR_IO_2BIG);
	}
	else if (read(elf->e_fd, tmp, len) != (int)len) {
	    seterr(ERROR_IO_READ);
/*@-temptrans@*/
	    if (tmp != buffer) {
		free(tmp);
	    }
/*@=temptrans@*/
	}
	else {
	    return tmp;
	}
    }
    return NULL;
}

void*
_elf_mmap(Elf *elf) {
#if HAVE_MMAP
    void *tmp;

    elf_assert(elf);
    elf_assert(elf->e_magic == ELF_MAGIC);
    elf_assert(elf->e_base == 0);
    if (elf->e_disabled) {
	seterr(ERROR_FDDISABLED);
    }
    else if (elf->e_size) {
	tmp = (void*)mmap(0, elf->e_size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE, elf->e_fd, 0);
	if (tmp != (void*)-1) {
	    return tmp;
	}
    }
#endif /* HAVE_MMAP */
    return NULL;
}
