/* Return string from debug string section.
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
