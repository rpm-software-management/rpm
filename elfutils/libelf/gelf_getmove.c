/* Get move structure at the given index.
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
