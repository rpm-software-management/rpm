/* Return flag represented by attribute.
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
