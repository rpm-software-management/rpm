/* Get information about the function range.
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

#include "libdwarfP.h"


int
dwarf_get_fde_range (Dwarf_Fde fde, Dwarf_Addr *low_pc,
		Dwarf_Unsigned *func_length, Dwarf_Ptr *fde_bytes,
		Dwarf_Unsigned *fde_byte_length, Dwarf_Off *cie_offset,
		Dwarf_Signed *cie_index, Dwarf_Off *fde_offset,
		Dwarf_Error *error)
{
  *low_pc = fde->initial_location;
  *func_length = fde->address_range;
  *fde_bytes = fde->fde_bytes;
  *fde_byte_length = fde->fde_byte_length;
  *cie_offset = fde->cie->offset;
  *cie_index = fde->cie->index;
  *fde_offset = fde->offset;

  return DW_DLV_OK;
}
