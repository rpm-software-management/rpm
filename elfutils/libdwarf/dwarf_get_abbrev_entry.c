/* Get attribute from abbreviation record.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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
dwarf_get_abbrev_entry (Dwarf_Abbrev abbrev, Dwarf_Signed idx,
		Dwarf_Half *attr_num, Dwarf_Signed *form, Dwarf_Off *offset,
		Dwarf_Error *error)
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
