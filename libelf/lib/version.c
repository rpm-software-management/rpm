/*
version.c - implementation of the elf_version(3) function.
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

unsigned
elf_version(unsigned ver) {
    unsigned tmp;

    if (ver == EV_NONE) {
	return EV_CURRENT;
    }
    if (!valid_version(ver)) {
	seterr(ERROR_UNKNOWN_VERSION);
	return EV_NONE;
    }
    tmp = _elf_version == EV_NONE ? EV_CURRENT : _elf_version;
    _elf_version = ver;
    return tmp;
}
