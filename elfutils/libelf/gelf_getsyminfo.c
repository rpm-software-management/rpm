/* Get additional symbol information from symbol table at the given index.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Syminfo *
gelf_getsyminfo (Elf_Data *data, int ndx, GElf_Syminfo *dst)
{
  GElf_Syminfo *result = NULL;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_SYMINFO))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* The types for 32 and 64 bit are the same.  Lucky us.  */
  assert (sizeof (GElf_Syminfo) == sizeof (Elf32_Syminfo));
  assert (sizeof (GElf_Syminfo) == sizeof (Elf64_Syminfo));

  rwlock_rdlock (((Elf_Data_Scn *) data)->s->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (unlikely ((ndx + 1) * sizeof (GElf_Syminfo) > data->d_size))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      goto out;
    }

  *dst = ((GElf_Syminfo *) data->d_buf)[ndx];

  result = dst;

 out:
  rwlock_unlock (((Elf_Data_Scn *) data)->s->elf->lock);

  return result;
}
