/* Get information about CIE.
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
dwarf_get_cie_info (Dwarf_Cie cie, Dwarf_Unsigned *bytes_in_cie,
		Dwarf_Small *version, char **augmenter,
		Dwarf_Unsigned *code_alignment_factor,
		Dwarf_Signed *data_alignment_factor,
		Dwarf_Half *return_address_register,
		Dwarf_Ptr *initial_instructions,
		Dwarf_Unsigned *initial_instructions_length,
		Dwarf_Error *error)
{
  *bytes_in_cie = cie->length;

  *version = CIE_VERSION;

  *augmenter = cie->augmentation;

  *code_alignment_factor = cie->code_alignment_factor;
  *data_alignment_factor = cie->data_alignment_factor;

  *return_address_register = cie->return_address_register;

  *initial_instructions = cie->initial_instructions;
  *initial_instructions_length = cie->initial_instructions_length;

  return DW_DLV_OK;
}
