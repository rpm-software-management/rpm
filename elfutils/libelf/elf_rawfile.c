/* Retrieve uninterpreted file contents.
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


char *
elf_rawfile (elf, ptr)
     Elf *elf;
     size_t *ptr;
{
  if (elf == NULL)
    {
      /* No valid descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
    error_out:
      if (ptr != NULL)
	*ptr = 0;
      return NULL;
    }

  /* If the file is not mmap'ed and not previously loaded, do it now.  */
  if (elf->map_address == NULL && __libelf_readall (elf) == NULL)
    goto error_out;

  if (ptr != NULL)
    *ptr = elf->maximum_size;

  return (char *) elf->map_address + elf->start_offset;
}
