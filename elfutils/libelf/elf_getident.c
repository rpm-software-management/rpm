/* Retrieve file identification data.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <stddef.h>

#include "libelfP.h"


char *
elf_getident (elf, ptr)
     Elf *elf;
     size_t *ptr;
{
  /* In case this is no ELF file, the handle is invalid and we return
     NULL.  */
  if (elf == NULL || elf->kind != ELF_K_ELF)
    {
      if (ptr != NULL)
	*ptr = 0;
      return NULL;
    }

  /* We already read the ELF header.  Return a pointer to it and store
     the length in *PTR.  */
  if (ptr != NULL)
    *ptr = EI_NIDENT;

  return (elf->class == ELFCLASS32
	  || (offsetof (struct Elf, state.elf32.ehdr)
	      == offsetof (struct Elf, state.elf64.ehdr))
	  ? elf->state.elf32.ehdr->e_ident : elf->state.elf64.ehdr->e_ident);
}
