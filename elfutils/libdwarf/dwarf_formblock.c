/* Return block of uninterpreted data represented by attribute.
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


int
dwarf_formblock (attr, return_block, error)
     Dwarf_Attribute attr;
     Dwarf_Block **return_block;
     Dwarf_Error *error;
{
  Dwarf_Small *attrp = attr->valp;
  Dwarf_Unsigned len;
  Dwarf_Ptr *data;
  Dwarf_Block *result;

  switch (attr->form)
    {
    case DW_FORM_block1:
      len = *attrp;
      data = (Dwarf_Ptr) (attrp + 1);
      break;

    case DW_FORM_block2:
      len = read_2ubyte_unaligned (attr->cu->dbg, attrp);
      data = (Dwarf_Ptr) (attrp + 2);
      break;

    case DW_FORM_block4:
      len = read_4ubyte_unaligned (attr->cu->dbg, attrp);
      data = (Dwarf_Ptr) (attrp + 2);
      break;

    case DW_FORM_block:
      get_uleb128 (len, attrp);
      data = (Dwarf_Ptr) attrp;
      break;

    default:
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_BLOCK);
      return DW_DLV_ERROR;
    }

  /* Allocate memory for the result.  */
  result = (Dwarf_Block *) malloc (sizeof (Dwarf_Block));
  if (result == NULL)
    {
      __libdwarf_error (attr->cu->dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  result->bl_len = len;
  result->bl_data = data;

  *return_block = result;
  return DW_DLV_OK;
}
