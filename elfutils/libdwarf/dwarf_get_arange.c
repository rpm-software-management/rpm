/* Find matching range for address.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <libdwarfP.h>


int
dwarf_get_arange (aranges, arange_count, address, return_arange, error)
     Dwarf_Arange *aranges;
     Dwarf_Unsigned arange_count;
     Dwarf_Addr address;
     Dwarf_Arange *return_arange;
     Dwarf_Error *error;
{
  Dwarf_Unsigned cnt;

  for (cnt = 0; cnt < arange_count; ++cnt)
    if (aranges[cnt]->address <= address
	&& address < aranges[cnt]->address + aranges[cnt]->length)
      {
	*return_arange = aranges[cnt];
	return DW_DLV_OK;
      }

  return DW_DLV_NO_ENTRY;
}
