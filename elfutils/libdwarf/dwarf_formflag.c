/* Return flag represented by attribute.
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

#include <libdwarfP.h>


int
dwarf_formflag (attr, return_bool, error)
     Dwarf_Attribute attr;
     Dwarf_Bool *return_bool;
     Dwarf_Error *error;
{
  if (unlikely (attr->form != DW_FORM_flag))
    {
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_FLAG);
      return DW_DLV_ERROR;
    }

  *return_bool = attr->valp != 0;

  return DW_DLV_OK;
}
