/*
assert.c - assert function for libelf.
Copyright (C) 1999 Michael Riepe <michael@stud.uni-hannover.de>

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
static const char rcsid[] = "@(#) Id: assert.c,v 1.1 1999/11/04 19:16:36 michael Exp ";
#endif /* lint */

#include <stdio.h>

void
__elf_assert(const char *file, unsigned line, const char *cond) {
    fprintf(stderr, "%s:%u: libelf assertion failure: %s\n",
	    file, line, cond);
    abort();
}
