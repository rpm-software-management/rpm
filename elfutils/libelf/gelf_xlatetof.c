/* Convert from memory to file representation.  Generic ELF version.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Data *
gelf_xlatetof (Elf *elf, Elf_Data *dest, const Elf_Data * src,
	       unsigned int encode)
{
  if (elf == NULL)
    return NULL;

  return (elf->class == ELFCLASS32
	  ? INTUSE(elf32_xlatetof) (dest, src, encode)
	  : INTUSE(elf64_xlatetof) (dest, src, encode));
}
