/* Get information about CIE.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libdwarfP.h>


int
dwarf_get_cie_info (cie, bytes_in_cie, version, augmenter,
		    code_alignment_factor, data_alignment_factor,
		    return_address_register, initial_instructions,
		    initial_instructions_length, error)
     Dwarf_Cie cie;
     Dwarf_Unsigned *bytes_in_cie;
     Dwarf_Small *version;
     char **augmenter;
     Dwarf_Unsigned *code_alignment_factor;
     Dwarf_Signed *data_alignment_factor;
     Dwarf_Half *return_address_register;
     Dwarf_Ptr *initial_instructions;
     Dwarf_Unsigned *initial_instructions_length;
     Dwarf_Error *error;
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
