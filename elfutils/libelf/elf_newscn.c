/* Append new section.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"


Elf_Scn *
elf_newscn (Elf *elf)
{
  Elf_Scn *result = NULL;
  bool first = false;

  if (elf == NULL)
    return NULL;

  /* We rely on the prefix of the `elf', `elf32', and `elf64' element
     being the same.  */
  assert (offsetof (Elf, state.elf.scns_last)
	  == offsetof (Elf, state.elf32.scns_last));
  assert (offsetof (Elf, state.elf.scns_last)
	  == offsetof (Elf, state.elf64.scns_last));
  assert (offsetof (Elf, state.elf32.scns)
	  == offsetof (Elf, state.elf64.scns));

  rwlock_wrlock (elf->lock);

 again:
  if (elf->state.elf.scns_last->cnt < elf->state.elf.scns_last->max)
    {
      result = &elf->state.elf.scns_last->data[elf->state.elf.scns_last->cnt];

      if (++elf->state.elf.scns_last->cnt == 1
	  && (elf->state.elf.scns_last
	      == (elf->class == ELFCLASS32
		  || (offsetof (Elf, state.elf32.scns)
		      == offsetof (Elf, state.elf64.scns))
		  ? &elf->state.elf32.scns : &elf->state.elf64.scns)))
	/* This is zeroth section.  */
	first = true;
      else
	{
	  assert (elf->state.elf.scns_last->cnt > 1);
	  result->index = result[-1].index + 1;
	}
    }
  else
    {
      /* We must allocate a new element.  */
      Elf_ScnList *newp;

      assert (elf->state.elf.scnincr > 0);

      newp = (Elf_ScnList *) calloc (sizeof (Elf_ScnList)
				     + ((elf->state.elf.scnincr *= 2)
					* sizeof (Elf_Scn)), 1);
      if (newp == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      result = &newp->data[0];

      /* One section used.  */
      ++newp->cnt;

      /* This is the number of sections we allocated.  */
      newp->max = elf->state.elf.scnincr;

      /* Remember the index for the first section in this block.  */
      newp->data[0].index
	= 1 + elf->state.elf.scns_last->data[elf->state.elf.scns_last->max - 1].index;

      /* Enqueue the new list element.  */
      elf->state.elf.scns_last = elf->state.elf.scns_last->next = newp;
    }

  /* Create a section header for this section.  */
  if (elf->class == ELFCLASS32)
    {
      result->shdr.e32 = (Elf32_Shdr *) calloc (1, sizeof (Elf32_Shdr));
      if (result->shdr.e32 == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}
    }
  else
    {
      result->shdr.e64 = (Elf64_Shdr *) calloc (1, sizeof (Elf64_Shdr));
      if (result->shdr.e64 == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}
    }

  result->elf = elf;
  result->shdr_flags = ELF_F_DIRTY | ELF_F_MALLOCED;

  /* Initialize the data part.  */
  result->data_read = 1;
  if (unlikely (first))
    {
      /* For the first section we mark the data as already available.  */
      //result->data_list_rear = &result->data_list;
      first = false;
      goto again;
    }

  result->flags |= ELF_F_DIRTY;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
