/* Create new, empty section data.
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

#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


Elf_Data *
elf_newdata (Elf_Scn *scn)
{
  Elf_Data_List *result = NULL;

  if (scn == NULL)
    return NULL;

  if (unlikely (scn->index == 0))
    {
      /* It is not allowed to add something to the 0th section.  */
      __libelf_seterrno (ELF_E_NOT_NUL_SECTION);
      return NULL;
    }

  if (scn->elf->class == ELFCLASS32
      || (offsetof (struct Elf, state.elf32.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr))
      ? scn->elf->state.elf32.ehdr == NULL
      : scn->elf->state.elf64.ehdr == NULL)
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      return NULL;
    }

  rwlock_wrlock (scn->elf->lock);

  if (scn->data_read && scn->data_list_rear == NULL)
    {
      /* This means the section was created by the user and this is the
	 first data.  */
      result = &scn->data_list;
      result->flags = ELF_F_DIRTY;
    }
  else
    {
      /* Create a new, empty data descriptor.  */
      result = (Elf_Data_List *) calloc (1, sizeof (Elf_Data_List));
      if (result == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      result->flags = ELF_F_DIRTY | ELF_F_MALLOCED;

      if (scn->data_list_rear == NULL)
	/* We create new data without reading/converting the data from the
	   file.  That is fine but we have to remember this.  */
	scn->data_list_rear = &scn->data_list;
    }

  /* Set the predefined values.  */
  result->data.d.d_version = __libelf_version;

  result->data.s = scn;

  /* Add to the end of the list.  */
  if (scn->data_list_rear != NULL)
    scn->data_list_rear->next = result;

  scn->data_list_rear = result;

 out:
  rwlock_unlock (scn->elf->lock);

  /* Please note that the following is thread safe and is also defined
     for RESULT == NULL since it still return NULL.  */
  return &result->data.d;
}
