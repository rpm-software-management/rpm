/* Return location list entry.
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
dwarf_get_loclist_entry (dbg, offset, hipc_offset, lopc_offset, data,
			 entry_len, next_entry, error)
     Dwarf_Debug dbg;
     Dwarf_Unsigned offset;
     Dwarf_Addr *hipc_offset;
     Dwarf_Addr *lopc_offset;
     Dwarf_Ptr *data;
     Dwarf_Unsigned *entry_len;
     Dwarf_Unsigned *next_entry;
     Dwarf_Error *error;
{
  Dwarf_Small *locp;

  /* Make sure we have room for at least two addresses.  */
  if (unlikely (offset + 2 * dbg->cu_list->address_size
		> dbg->sections[IDX_debug_loc].size))
    {
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  locp = (Dwarf_Small *) dbg->sections[IDX_debug_loc].addr + offset;

  /* Get the two values.  */
  if (dbg->cu_list->address_size == 4)
    {
      *lopc_offset = read_4ubyte_unaligned (dbg, locp);
      *hipc_offset = read_4ubyte_unaligned (dbg, locp + 4);
      locp += 8;
      offset += 8;
    }
  else
    {
      *lopc_offset = read_8ubyte_unaligned (dbg, locp);
      *hipc_offset = read_8ubyte_unaligned (dbg, locp + 8);
      locp += 16;
      offset += 16;
    }

  /* Does this signal the end?  */
  if (*lopc_offset == 0 && *hipc_offset == 0)
    return DW_DLV_OK;

  /* Make sure another 2 bytes are available for the length.  */
  if (unlikely (offset + 2 > dbg->sections[IDX_debug_loc].size))
    {
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }
  *entry_len = read_2ubyte_unaligned (dbg, locp);
  locp += 2;
  offset += 2;
  *data = locp;

  /* Now we know how long the block is.  Test whether that much
     data is available.  */
  if (unlikely (offset + *entry_len > dbg->sections[IDX_debug_loc].size))
    {
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  *next_entry = offset + *entry_len;

  return DW_DLV_OK;
}
