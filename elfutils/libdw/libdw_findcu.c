/* Find CU for given offset.
   Copyright (C) 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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
#include <search.h>
#include "libdwP.h"


static int
findcu_cb (const void *arg1, const void *arg2)
{
  struct Dwarf_CU *cu1 = (struct Dwarf_CU *) arg1;
  struct Dwarf_CU *cu2 = (struct Dwarf_CU *) arg2;

  /* Find out which of the two arguments is the search value.  It has
     length 0.  */
  if (cu1->length == 0)
    {
      if (cu1->start < cu2->start)
	return -1;
      if (cu1->start >= cu2->start + cu2->length)
	return 1;
    }
  else
    {
      if (cu2->start < cu1->start)
	return 1;
      if (cu2->start >= cu1->start + cu1->length)
	return -1;
    }

  return 0;
}


struct Dwarf_CU *
__libdw_findcu (dbg, start)
     Dwarf *dbg;
     Dwarf_Off start;
{
  /* Maybe we already know that CU.  */
  struct Dwarf_CU fake = { .start = start, .length = 0 };
  struct Dwarf_CU **found = tfind (&fake, &dbg->cu_tree, findcu_cb);
  if (found != NULL)
    return *found;

  if (start < dbg->next_cu_offset)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  /* No.  Then read more CUs.  */
  while (1)
    {
      Dwarf_Off oldoff = dbg->next_cu_offset;
      uint8_t address_size;
      Dwarf_Off abbrev_offset;

      if (dwarf_nextcu (dbg, oldoff, &dbg->next_cu_offset, NULL,
			&abbrev_offset, &address_size) != 0)
	/* No more entries.  */
	return NULL;

      /* Create an entry for this CU.  */
      struct Dwarf_CU *newp = libdw_alloc (dbg, struct Dwarf_CU);

      newp->dbg = dbg;
      newp->start = oldoff;
      newp->length = dbg->next_cu_offset - oldoff;
      newp->address_size = address_size;
      Dwarf_Abbrev_Hash_init (&newp->abbrev_hash, 41);
      newp->last_abbrev_offset = abbrev_offset;

      /* Add the new entry to the search tree.  */
      if (tsearch (newp, &dbg->cu_tree, findcu_cb) == NULL)
	{
	  /* Something went wrong.  Unfo the operation.  */
	  dbg->next_cu_offset = oldoff;
	  __libdw_seterrno (DWARF_E_NOMEM);
	  return NULL;
	}

      /* Is this the one we are looking for?  */
      if (start < dbg->next_cu_offset)
	// XXX Match exact offset.
	return newp;
    }
  /* NOTREACHED */
}
