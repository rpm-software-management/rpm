/* Return string from debug string section.
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

#include <string.h>

#include "libdwarfP.h"


int
dwarf_get_str (dbg, offset, string, returned_str_len, error)
     Dwarf_Debug dbg;
     Dwarf_Off offset;
     char **string;
     Dwarf_Signed *returned_str_len;
     Dwarf_Error *error;
{
  *string = (char *) dbg->sections[IDX_debug_str].addr;
  if (*string == NULL)
    return DW_DLV_NO_ENTRY;

  if (unlikely (offset >= dbg->sections[IDX_debug_str].size))
    return DW_DLV_NO_ENTRY;

  *string += offset;
  *returned_str_len = strlen (*string);

  return DW_DLV_OK;
}
