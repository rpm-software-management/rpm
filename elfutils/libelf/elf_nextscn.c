/* Get next section.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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


Elf_Scn *
elf_nextscn (Elf *elf, Elf_Scn *scn)
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
	    result = &elf->state.elf32.scns.data[1];
	}
      else
	{
	  if (elf->state.elf64.scns.cnt > 1)
	    result = &elf->state.elf64.scns.data[1];
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
