/*
verdef_64_tom.c - copy 64-bit versioning information.
Copyright (C) 2001 Michael Riepe <michael@stud.uni-hannover.de>

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
#include <ext_types.h>
#include <byteswap.h>

#if __LIBELF64 && __LIBELF_SYMBOL_VERSIONS

#ifndef lint
static const char rcsid[] = "@(#) Id: verdef_64_tom.c,v 1.1 2001/10/07 20:03:02 michael Exp ";
#endif /* lint */

typedef Elf64_Verdaux		verdaux_mtype;
typedef Elf64_Verdef		verdef_mtype;
typedef Elf64_Vernaux		vernaux_mtype;
typedef Elf64_Verneed		verneed_mtype;
typedef Elf64_Word		align_mtype;

typedef __ext_Elf64_Verdaux	verdaux_ftype;
typedef __ext_Elf64_Verdef	verdef_ftype;
typedef __ext_Elf64_Vernaux	vernaux_ftype;
typedef __ext_Elf64_Verneed	verneed_ftype;
typedef __ext_Elf64_Word	align_ftype;

#define class_suffix		64

#undef TOFILE
#define TOFILE 0

/*
 * Include shared code
 */
#include "verdef.h"
#include "verneed.h"

#endif /* __LIBELF64 && __LIBELF_SYMBOL_VERSIONS */
