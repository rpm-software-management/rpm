/* Return block of uninterpreted data represented by attribute.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <dwarf.h>
#include <stdlib.h>

#include <libdwarfP.h>


int
dwarf_formblock (Dwarf_Attribute attr, Dwarf_Block **return_block,
		Dwarf_Error *error)
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
