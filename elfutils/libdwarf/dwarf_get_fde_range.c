/* Get information about the function range.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwarfP.h"


int
dwarf_get_fde_range (fde, low_pc, func_length, fde_bytes, fde_byte_length,
		     cie_offset, cie_index, fde_offset, error)
     Dwarf_Fde fde;
     Dwarf_Addr *low_pc;
     Dwarf_Unsigned *func_length;
     Dwarf_Ptr *fde_bytes;
     Dwarf_Unsigned *fde_byte_length;
     Dwarf_Off *cie_offset;
     Dwarf_Signed *cie_index;
     Dwarf_Off *fde_offset;
     Dwarf_Error *error;
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
