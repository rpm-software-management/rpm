/* Get next section.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Scn *
elf_nextscn (elf, scn)
     Elf *elf;
     Elf_Scn *scn;
{
  Elf_Scn *result = NULL;

  if (elf == NULL)
    return NULL;

  rwlock_rdlock (elf->lock);

  if (scn == NULL)
    {
      /* If no section handle is given return the first (not 0th) section.  */
      if (elf->class == ELFCLASS32
	   || (offsetof (Elf, state.elf32.scns)
	       == offsetof (Elf, state.elf64.scns)))
	{
	  if (elf->state.elf32.scns.cnt > 1)
	    result = &elf->state.elf32.scns.data[0];
	}
      else
	{
	  if (elf->state.elf64.scns.cnt > 1)
	    result = &elf->state.elf64.scns.data[0];
	}
    }
  else
    {
      Elf_ScnList *list = scn->list;

      if (scn + 1 < &list->data[list->cnt])
	result = scn + 1;
      else if (scn + 1 == &list->data[list->max]
	       && (list = list->next) != NULL)
	{
	  /* If there is another element in the section list it must
	     have at least one entry.  */
	  assert (list->cnt > 0);
	  result = &list->data[0];
	}
    }

  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_nextscn)
