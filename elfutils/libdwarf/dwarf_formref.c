/* Return compile-unit relative offset of reference associated with form.
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

#include <libdwarfP.h>


int
dwarf_formref (attr, return_offset, error)
     Dwarf_Attribute attr;
     Dwarf_Off *return_offset;
     Dwarf_Error *error;
{
  switch (attr->code)
    {
    case DW_FORM_ref1:
      *return_offset = *attr->valp;
      break;

    case DW_FORM_ref2:
      *return_offset = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_ref4:
      *return_offset = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_ref8:
      *return_offset = read_8ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_ref_udata:
      {
	Dwarf_Off off;
	Dwarf_Small *attrp = attr->valp;
	get_uleb128 (off, attrp);
	*return_offset = off;
      }
      break;

    case DW_FORM_ref_addr:
      __libdwarf_error (attr->cu->dbg, error, DW_E_INVALID_REFERENCE);
      return DW_DLV_ERROR;

    default:
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_REFERENCE);
      return DW_DLV_ERROR;
    }

  return DW_DLV_OK;
}
