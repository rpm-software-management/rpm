/* Return string associated with given attribute.
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


int
dwarf_has_children (die)
     Dwarf_Die *die;
{
  /* Find the abbreviation entry.  */
  Dwarf_Abbrev *abbrevp = die->abbrev;
  if (abbrevp != (Dwarf_Abbrev *) -1l)
    {
      unsigned char *readp = (unsigned char *) die->addr;

      /* First we have to get the abbreviation code so that we can decode
	 the data in the DIE.  */
      unsigned int abbrev_code;
      get_uleb128 (abbrev_code, readp);

      abbrevp = __libdw_getabbrev (die->cu, abbrev_code);
      die->abbrev = abbrevp ?: (Dwarf_Abbrev *) -1l;
    }
  if (unlikely (abbrevp == (Dwarf_Abbrev *) -1l))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return 0;
    }

  return abbrevp->has_children;
}
