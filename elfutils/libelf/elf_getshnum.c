/* Return number of sections in the ELF file.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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


int
elf_getshnum (Elf *elf, size_t *dst)
{
  int result = 0;
  int idx;

  if (elf == NULL)
    return -1;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_rdlock (elf->lock);

  idx = elf->state.elf.scns_last->cnt;
  if (idx != 0
      || (elf->state.elf.scns_last
	  != (elf->class == ELFCLASS32
	      || (offsetof (Elf, state.elf32.scns)
		  == offsetof (Elf, state.elf64.scns))
	      ? &elf->state.elf32.scns : &elf->state.elf64.scns)))
    /* There is at least one section.  */
    *dst = 1 + elf->state.elf.scns_last->data[idx - 1].index;
  else
    *dst = 0;

  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_getshnum)
