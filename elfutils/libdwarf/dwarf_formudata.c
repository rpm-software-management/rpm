/* Return unsigned constant represented by attribute.
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
dwarf_formudata (Dwarf_Attribute attr, Dwarf_Unsigned *return_uval,
		Dwarf_Error *error)
{
  Dwarf_Unsigned u128;

  switch (attr->form)
    {
    case DW_FORM_data1:
      *return_uval = *attr->valp;
      break;

    case DW_FORM_data2:
      *return_uval = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data4:
      *return_uval = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data8:
      *return_uval = read_8ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_sdata:
      {
	Dwarf_Small *attrp = attr->valp;
	get_sleb128 (u128, attrp);
	*return_uval = u128;
      }
      break;

    case DW_FORM_udata:
      {
	Dwarf_Small *attrp = attr->valp;
	get_uleb128 (u128, attrp);
	*return_uval = u128;
      }
      break;

    default:
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_CONSTANT);
      return DW_DLV_ERROR;
    }

  return DW_DLV_OK;
}
