/* Return sibling of given DIE.
   Copyright (C) 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include "libdwP.h"
#include <dwarf.h>
#include <string.h>


Dwarf_Die *
dwarf_siblingof (die, result)
     Dwarf_Die *die;
     Dwarf_Die *result;
{
  /* Ignore previous errors.  */
  if (die == NULL)
    return NULL;

  unsigned int level = 0;
  /* The address we are looking at.  */
  unsigned char *addr = die->addr;

  /* Copy of the current DIE.  */
  Dwarf_Die this_die = *die;

  /* Search for the beginning of the next die on this level.  We
     must not return the dies for children of the given die.  */
  do
    {
      /* Get the abbreviation code.  */
      unsigned int abbrev_code;
      get_uleb128 (abbrev_code, addr);

      if (this_die.abbrev == NULL)
	{
	  this_die.abbrev = __libdw_getabbrev (die->cu, abbrev_code);
	  if (this_die.abbrev == NULL)
	    this_die.abbrev = (Dwarf_Abbrev *) -1l;
	}
      if (unlikely (this_die.abbrev == (Dwarf_Abbrev *) -1l))
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return NULL;
	}

      /* Does this abbreviation have children?  */
      if (this_die.abbrev->has_children)
	++level;

      /* Find the end of the DIE or the sibling attribute.  */
      unsigned int code;
      addr = __libdw_find_attr (&this_die, DW_AT_sibling, &code, NULL);
      if (code == DW_AT_sibling)
	{
	  Dwarf_Off offset;
	  Dwarf_Attribute sibattr;

	  sibattr.valp = addr;
	  sibattr.cu = die->cu;
	  if (dwarf_formref (&sibattr, &offset) != 0)
	    /* Something went wrong.  */
	    return NULL;

	  /* Compute the next address.  */
	  addr
	    = ((unsigned char *) die->cu->dbg->sectiondata[IDX_debug_info]->d_buf
	       + die->cu->start + offset);

	  /* Even if the abbreviation has children we have stepped
	     over them now.  */
	  if (this_die.abbrev->has_children)
	    --level;
	}

      /* Check that we are not yet at the end.  */
      while (*addr == '\0')
	{
	  if (level-- == 0)
	    /* No more sibling at all.  */
	    return DWARF_END_DIE;

	  ++addr;
	}

      /* Initialize the 'current DIE'.  */
      this_die.addr = addr;
      this_die.abbrev = NULL;
    }
  while (level > 0);

  /* Clear the entire DIE structure.  This signals we have not yet
     determined any of the information.  */
  memset (result, '\0', sizeof (Dwarf_Die));

  /* We have the address.  */
  result->addr = addr;

  /* Same CU as the parent.  */
  result->cu = die->cu;

  return result;
}
