/* Select specific element in archive.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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
