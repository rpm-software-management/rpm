/* Advance in archive to next element.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <assert.h>
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Cmd
elf_next (elf)
     Elf *elf;
{
  Elf *parent;

  /* Be gratious, the specs demand it.  */
  if (elf == NULL || elf->parent == NULL)
    return ELF_C_NULL;

  /* We can be sure the parent is an archive.  */
  parent = elf->parent;
  assert (parent->kind == ELF_K_AR);

  /* Now advance the offset.  */
  parent->state.ar.offset += (sizeof (struct ar_hdr)
			      + ((parent->state.ar.elf_ar_hdr.ar_size + 1)
				 & ~1l));

  /* Get the next archive header.  */
  if (__libelf_next_arhdr (parent) != 0)
    return ELF_C_NULL;

  return elf->cmd;
}
