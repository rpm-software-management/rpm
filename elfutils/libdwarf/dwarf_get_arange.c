/* Find matching range for address.
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

#include <libdwarfP.h>


int
dwarf_get_arange (Dwarf_Arange *aranges, Dwarf_Unsigned arange_count,
		Dwarf_Addr address, Dwarf_Arange *return_arange,
		Dwarf_Error *error)
{
  Dwarf_Unsigned cnt;

  for (cnt = 0; cnt < arange_count; ++cnt)
    if (aranges[cnt]->address <= address
	&& address < aranges[cnt]->address + aranges[cnt]->length)
      {
	*return_arange = aranges[cnt];
	return DW_DLV_OK;
      }

  return DW_DLV_NO_ENTRY;
}
