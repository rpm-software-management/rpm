/* Get attribute from abbreviation record.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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


int
dwarf_get_abbrev_entry (abbrev, idx, attr_num, form, offset, error)
     Dwarf_Abbrev abbrev;
     Dwarf_Signed idx;
     Dwarf_Half *attr_num;
     Dwarf_Signed *form;
     Dwarf_Off *offset;
     Dwarf_Error *error;
{
  Dwarf_Small *attrp;
  Dwarf_Small *start_attrp;
  Dwarf_Word attr_name;
  Dwarf_Word attr_form;

  if (idx < 0)
    return DW_DLV_NO_ENTRY;

  /* Address in memory.  */
  attrp = abbrev->attrp;

  /* Read the attributes, skip over the ones we don't want.  */
  do
    {
      start_attrp = attrp;

      get_uleb128 (attr_name, attrp);
      get_uleb128 (attr_form, attrp);

      if (attr_name == 0 || attr_form == 0)
	return DW_DLV_NO_ENTRY;
    }
  while (idx-- > 0);

  *attr_num = attr_name;
  *form = attr_form;
  *offset = (start_attrp - abbrev->attrp) + abbrev->offset;

  return DW_DLV_OK;
}
