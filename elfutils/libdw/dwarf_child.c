/* Return vhild of current DIE.
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
#include <string.h>

/* Some arbitrary value not conflicting with any existing code.  */
#define INVALID 0xffffe444


unsigned char *
internal_function
__libdw_find_attr (Dwarf_Die *die, unsigned int search_name,
		   unsigned int *codep, unsigned int *formp)
{
  Dwarf *dbg = die->cu->dbg;
  unsigned char *readp = (unsigned char *) die->addr;

  /* First we have to get the abbreviation code so that we can decode
     the data in the DIE.  */
  unsigned int abbrev_code;
  get_uleb128 (abbrev_code, readp);

  /* Find the abbreviation entry.  */
  Dwarf_Abbrev *abbrevp = die->abbrev;
  if (abbrevp != (Dwarf_Abbrev *) -1l)
    {
      abbrevp = __libdw_getabbrev (die->cu, abbrev_code);
      die->abbrev = abbrevp ?: (Dwarf_Abbrev *) -1l;
    }
  if (unlikely (abbrevp == (Dwarf_Abbrev *) -1l))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  /* Search the name attribute.  */
  unsigned char *const endp
    = ((unsigned char *) dbg->sectiondata[IDX_debug_abbrev]->d_buf
       + dbg->sectiondata[IDX_debug_abbrev]->d_size);

  unsigned char *attrp = abbrevp->attrp;
  while (1)
    {
      /* Are we still in bounds?  This test needs to be refined.  */
      if (unlikely (attrp + 1 >= endp))
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return NULL;
	}

      /* Get attribute name and form.

	 XXX We don't check whether this reads beyond the end of the
	 section.  */
      unsigned int attr_name;
      get_uleb128 (attr_name, attrp);
      unsigned int attr_form;
      get_uleb128 (attr_form, attrp);

      /* We can stop if we found the attribute with value zero.  */
      if (attr_name == 0 && attr_form == 0)
	break;

      /* Is this the name attribute?  */
      if (attr_name == search_name && search_name != INVALID)
	{
	  if (codep != NULL)
	    *codep = attr_name;
	  if (formp != NULL)
	    *formp = attr_form;

	  return readp;
	}

      /* Skip over the rest of this attribute (if there is any).  */
      if (attr_form != 0)
	{
	  size_t len = __libdw_form_val_len (dbg, die->cu, attr_form, readp);

	  if (unlikely (len == (size_t) -1l))
	    return NULL;

	  // XXX We need better boundary checks.
	  readp += len;
	}
    }

  // XXX Do we need other values?
  if (codep != NULL)
    *codep = INVALID;
  if (formp != NULL)
    *formp = INVALID;

  return readp;
}


Dwarf_Die *
dwarf_child (die, result)
     Dwarf_Die *die;
     Dwarf_Die *result;
{
  /* Ignore previous errors.  */
  if (die == NULL)
    return NULL;

  /* Skip past the last attribute.  */
  void *addr = NULL;

  /* If we already know there are no children do not search.  */
  if (die->abbrev == NULL || die->abbrev->has_children)
    addr = __libdw_find_attr (die, INVALID, NULL, NULL);

  /* Make sure the DIE really has children.  */
  if (die->abbrev != NULL && ! die->abbrev->has_children)
    /* There cannot be any children.  But we may have found a sibling.  */
    return NULL;

  if (addr == NULL)
    return NULL;

  /* Clear the entire DIE structure.  This signals we have not yet
     determined any of the information.  */
  memset (result, '\0', sizeof (Dwarf_Die));

  /* We have the address.  */
  result->addr = addr;

  /* Same CU as the parent.  */
  result->cu = die->cu;

  return result;
}
