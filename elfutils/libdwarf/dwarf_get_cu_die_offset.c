/* Return offset of compile unit DIE containing the range.
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
dwarf_get_cu_die_offset (Dwarf_Arange arange, Dwarf_Off *return_offset,
		Dwarf_Error *error)
{
  Dwarf_Small *cu_header;
  unsigned int offset_size;

  /* Determine the size of the CU header.  */
  cu_header =
    ((Dwarf_Small *) arange->info->dbg->sections[IDX_debug_info].addr
     + arange->info->offset);
  if (read_4ubyte_unaligned_noncvt (cu_header) == 0xffffffff)
    offset_size = 8;
  else
    offset_size = 4;

  *return_offset = arange->info->offset + 3 * offset_size - 4 + 3;

  return DW_DLV_OK;
}
