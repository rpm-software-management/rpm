/* Retrieve uninterpreted chunk of the file contents.
   Copyright (C) 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <stdlib.h>
#include <unistd.h>

#include "libelfP.h"


char *
gelf_rawchunk (Elf *elf, GElf_Off offset, GElf_Word size)
{
  char * result;
  if (elf == NULL)
    {
      /* No valid descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (offset >= elf->maximum_size
      || offset + size >= elf->maximum_size
      || offset + size < offset)
    {
      /* Invalid request.  */
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  /* If the file is mmap'ed return an appropriate pointer.  */
  if (elf->map_address != NULL)
    return (char *) elf->map_address + elf->start_offset + offset;

  /* We allocate the memory and read the data from the file.  */
  result = (char *) malloc (size);
  if (result == NULL)
    __libelf_seterrno (ELF_E_NOMEM);
  else
    /* Read the file content.  */
    if (pread (elf->fildes, result, size, elf->start_offset + offset) != size)
      {
	/* Something went wrong.  */
	__libelf_seterrno (ELF_E_READ_ERROR);
	free (result);
      }

  return result;
}
