/* Get frame construction instructions of FDE.
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
dwarf_get_fde_instr_bytes (fde, outinstrs, outlen, error)
     Dwarf_Fde fde;
     Dwarf_Ptr *outinstrs;
     Dwarf_Unsigned *outlen;
     Dwarf_Error *error;
{
  *outinstrs = fde->instructions;
  *outlen = fde->instructions_length;

  return DW_DLV_OK;
}
