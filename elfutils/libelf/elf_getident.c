/* Retrieve file identification data.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <stddef.h>

#include "libelfP.h"


char *
elf_getident (Elf *elf, size_t *ptr)
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
