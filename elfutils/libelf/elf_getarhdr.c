/* Read header of next archive member.
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
