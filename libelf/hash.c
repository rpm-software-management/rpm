/*
hash.c - implementation of the elf_hash(3) function.
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

unsigned long
elf_hash(const char *name) {
    unsigned long hash = 0;
    unsigned long tmp;
    unsigned char c;

    while ((c = *name++)) {
	hash = (hash << 4) + c;
	if ((tmp = hash & 0xf0000000)) {
	    hash ^= tmp | (tmp >> 24);
	}
    }
    return hash;
}
