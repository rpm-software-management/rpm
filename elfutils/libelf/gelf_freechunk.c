/* Release uninterpreted chunk of the file contents.
   Copyright (C) 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libelf.h>
#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


void
gelf_freechunk (Elf *elf, char *ptr)
{
  if (elf == NULL)
    /* No valid descriptor.  */
    return;

  /* We do not have to do anything if the pointer returned by
     gelf_rawchunk points into the memory allocated for the ELF
     descriptor.  */
  if (ptr < (char *) elf->map_address + elf->start_offset
      || ptr >= ((char *) elf->map_address + elf->start_offset
		 + elf->maximum_size))
    free (ptr);
}
