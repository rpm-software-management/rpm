/* Return string represented by attribute.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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
#include <string.h>

#include <libdwarfP.h>


int
dwarf_formstring (attr, return_string, error)
     Dwarf_Attribute attr;
     char **return_string;
     Dwarf_Error *error;
{
  char *result;

  if (attr->form == DW_FORM_string)
    result = (char *) attr->valp;
  else if (likely (attr->form == DW_FORM_strp))
    {
      Dwarf_Unsigned offset;

      offset = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);

      result = (char *) attr->cu->dbg->sections[IDX_debug_str].addr + offset;
    }
  else
    {
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_STRING);
      return DW_DLV_ERROR;
    }

  *return_string = result;
  return DW_DLV_OK;
}
