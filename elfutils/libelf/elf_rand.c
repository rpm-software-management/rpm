/* Select specific element in archive.
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

#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


size_t
elf_rand (Elf *elf, size_t offset)
{
  /* Be gratious, the specs demand it.  */
  if (elf == NULL || elf->kind != ELF_K_AR)
    return 0;

  /* Save the old offset and set the offset.  */
  elf->state.ar.offset = elf->start_offset + offset;

  /* Get the next archive header.  */
  if (__libelf_next_arhdr (elf) != 0)
    {
      /* Mark the archive header as unusable.  */
      elf->state.ar.elf_ar_hdr.ar_name = NULL;
      return 0;
    }

  return offset;
}
