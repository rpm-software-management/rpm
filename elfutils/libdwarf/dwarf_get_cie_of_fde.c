/* Get CIE of FDE.
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
dwarf_get_cie_of_fde (fde, return_cie, error)
     Dwarf_Fde fde;
     Dwarf_Cie *return_cie;
     Dwarf_Error *error;
{
  *return_cie = fde->cie;
  return DW_DLV_OK;
}
