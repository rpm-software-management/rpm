/* Get additional required symbol version information at the given offset.
   Copyright (C) 1999, 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

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

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Vernaux *
gelf_getvernaux (Elf_Data *data, int offset, GElf_Vernaux *dst)
{
  GElf_Vernaux *result;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_VNEED))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* It's easy to handle this type.  It has the same size for 32 and
     64 bit objects.  And fortunately the `ElfXXX_Vernaux' records
     also have the same size.  */
  assert (sizeof (GElf_Vernaux) == sizeof (Elf32_Verneed));
  assert (sizeof (GElf_Vernaux) == sizeof (Elf64_Verneed));
  assert (sizeof (GElf_Vernaux) == sizeof (Elf32_Vernaux));
  assert (sizeof (GElf_Vernaux) == sizeof (Elf64_Vernaux));

  rwlock_rdlock (((Elf_Data_Scn *) data)->s->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (unlikely (offset < 0)
      || unlikely (offset + sizeof (GElf_Vernaux) > data->d_size)
      || unlikely (offset % sizeof (GElf_Vernaux) != 0))
    {
      __libelf_seterrno (ELF_E_OFFSET_RANGE);
      result = NULL;
    }
  else
    result = (GElf_Vernaux *) memcpy (dst, (char *) data->d_buf + offset,
				      sizeof (GElf_Verneed));

  rwlock_unlock (((Elf_Data_Scn *) data)->s->elf->lock);

  return result;
}
