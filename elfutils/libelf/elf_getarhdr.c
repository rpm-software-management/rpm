/* Read header of next archive member.
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

#include <assert.h>
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Arhdr *
elf_getarhdr (Elf *elf)
{
  Elf *parent = elf->parent;

  /* Calling this function is not ok for any file type but archives.  */
  if (parent == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  /* Make sure we have read the archive header.  */
  if (parent->state.ar.elf_ar_hdr.ar_name == NULL
      && __libelf_next_arhdr (parent) != 0)
    /* Something went wrong.  Maybe there is no member left.  */
    return NULL;


  /* We can be sure the parent is an archive.  */
  assert (parent->kind == ELF_K_AR);

  return &parent->state.ar.elf_ar_hdr;
}
