/* Return specific DWARF attribute of a DIE.
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

#include <dwarf.h>
#include "libdwP.h"


Dwarf_Attribute *
dwarf_attr (die, search_name, result)
     Dwarf_Die *die;
     unsigned int search_name;
     Dwarf_Attribute *result;
{
  if (die == NULL)
    return NULL;

  /* Search for the attribute with the given name.  */
  result->valp = __libdw_find_attr (die, search_name, &result->code,
				    &result->form);
  /* Always fill in the CU information.  */
  result->cu = die->cu;

  return result->code == search_name ? result : NULL;
}
