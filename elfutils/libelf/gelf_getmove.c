/* Get move structure at the given index.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


GElf_Move *
gelf_getmove (Elf_Data *data, int ndx, GElf_Move *dst)
{
  GElf_Move *result = NULL;
  Elf *elf;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_MOVE))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* The types for 32 and 64 bit are the same.  Lucky us.  */
  assert (sizeof (GElf_Move) == sizeof (Elf32_Move));
  assert (sizeof (GElf_Move) == sizeof (Elf64_Move));

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (unlikely ((ndx + 1) * sizeof (GElf_Move) > data->d_size))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      goto out;
    }

  elf = ((Elf_Data_Scn *) data)->s->elf;
  rwlock_rdlock (elf->lock);

  *dst = ((GElf_Move *) data->d_buf)[ndx];

  rwlock_unlock (elf->lock);

  result = dst;

 out:
  return result;
}
