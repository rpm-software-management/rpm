/* Retrieve uninterpreted chunk of the file contents.
   Copyright (C) 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

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
    if ((size_t) pread (elf->fildes, result, size, elf->start_offset + offset)
	!= size)
      {
	/* Something went wrong.  */
	__libelf_seterrno (ELF_E_READ_ERROR);
	free (result);
      }

  return result;
}
