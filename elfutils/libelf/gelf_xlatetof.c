/* Convert from memory to file representation.  Generic ELF version.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Data *
gelf_xlatetof (Elf *elf, Elf_Data *dest, const Elf_Data * src,
	       unsigned int encode)
{
  if (elf == NULL)
    return NULL;

  return (elf->class == ELFCLASS32
	  ? INTUSE(elf32_xlatetof) (dest, src, encode)
	  : INTUSE(elf64_xlatetof) (dest, src, encode));
}
