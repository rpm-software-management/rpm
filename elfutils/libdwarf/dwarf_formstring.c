/* Return string represented by attribute.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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
