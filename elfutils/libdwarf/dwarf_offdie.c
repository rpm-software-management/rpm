/* Return die at given offset.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <dwarf.h>
#include <stdlib.h>

#include <libdwarfP.h>


/* XXX This function will have to be optimized.  The search is too linear
   to be performed too often -> O(n²).  */
static Dwarf_CU_Info
find_cu (Dwarf_Debug dbg, Dwarf_Off offset, Dwarf_Error *error)
{
  Dwarf_CU_Info cu;
  Dwarf_Word cu_offset;

  /* Search in the CUs already known.  */
  for (cu = dbg->cu_list; cu != NULL; cu = cu->next)
    if (cu->offset <= offset
	&& cu->offset + 2 * cu->offset_size - 4 + cu->length > offset)
      return cu;

  /* The CU is not yet loaded.  Do this now.  */
  if (dbg->cu_list_tail == NULL)
    cu_offset = 0;
  else
    cu_offset = (dbg->cu_list_tail->offset
		 + 2 * dbg->cu_list_tail->offset_size - 4
		 + dbg->cu_list_tail->length);

  while (1)
    {
      /* Get next CU and add it to the end of the list.  */
      if (__libdwarf_get_cu_at_offset (dbg, cu_offset, &cu, error)
	  != DW_DLV_OK)
	return NULL;

      /* Offset of next CU.  */
      cu_offset += 2 * cu->offset_size - 4 + cu->length;

      /* If this the CU we are looking for?  */
      if (offset < cu_offset)
	return cu;
    }
}


int
dwarf_offdie (dbg, offset, return_die, error)
     Dwarf_Debug dbg;
     Dwarf_Off offset;
     Dwarf_Die *return_die;
     Dwarf_Error *error;
{
  Dwarf_CU_Info cu;
  Dwarf_Die new_die;
  Dwarf_Small *die_addr;
  Dwarf_Word u128;

  if (offset >= dbg->sections[IDX_debug_info].size)
    {
      /* Completely out of bounds.  */
      __libdwarf_error (dbg, error, DW_E_INVALID_OFFSET);
      return DW_DLV_ERROR;
    }

  /* Find the compile unit this address belongs to.  */
  cu = find_cu (dbg, offset, error);
  if (cu == NULL)
    return DW_DLV_ERROR;

  /* Creata a new die.  */
  new_die = (Dwarf_Die) malloc (sizeof (struct Dwarf_Die_s));
  if (new_die == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

#ifdef DWARF_DEBUG
  new_die->memtag = DW_DLA_DIE;
#endif

  /* Remember the address.  */
  die_addr = (Dwarf_Small *) dbg->sections[IDX_debug_info].addr + offset;
  new_die->addr = die_addr;

  /* And the compile unit.  */
  new_die->cu = cu;

  /* 7.5.2  Debugging Information Entry

     Each debugging information entry begins with an unsigned LEB128
     number containing the abbreviation code for the entry.  */
  get_uleb128 (u128, die_addr);

  /* Find the abbreviation.  */
  new_die->abbrev = __libdwarf_get_abbrev (dbg, cu, u128, error);
  if (new_die->abbrev == NULL)
    {
      free (new_die);
      return DW_DLV_ERROR;
    }

  *return_die = new_die;
  return DW_DLV_OK;
}
