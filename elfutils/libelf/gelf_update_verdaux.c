/* Update additional symbol version definition information.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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


int
gelf_update_verdaux (data, offset, src)
     Elf_Data *data;
     int offset;
     GElf_Verdaux *src;
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;

  if (data == NULL)
    return 0;

  /* The types for 32 and 64 bit are the same.  Lucky us.  */
  assert (sizeof (GElf_Verdaux) == sizeof (Elf32_Verdaux));
  assert (sizeof (GElf_Verdaux) == sizeof (Elf64_Verdaux));

  /* Check whether we have to resize the data buffer.  */
  if (unlikely (offset < 0)
      || unlikely ((offset + sizeof (GElf_Verdaux)) > data_scn->d.d_size))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      return 0;
    }

  if (unlikely (data_scn->d.d_type != ELF_T_VDEF))
    {
      /* The type of the data better should match.  */
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return 0;
    }

  rwlock_wrlock (data_scn->s->elf->lock);

  memcpy ((char *) data_scn->d.d_buf + offset, src, sizeof (GElf_Verdaux));

  /* Mark the section as modified.  */
  data_scn->s->flags |= ELF_F_DIRTY;

  rwlock_unlock (data_scn->s->elf->lock);

  return 1;
}
