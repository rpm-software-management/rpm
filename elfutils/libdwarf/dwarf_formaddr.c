/* Return address represented by attribute.
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
dwarf_formaddr (attr, return_addr, error)
     Dwarf_Attribute attr;
     Dwarf_Addr *return_addr;
     Dwarf_Error *error;
{
  if (unlikely (attr->form != DW_FORM_addr))
    {
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_ADDR);
      return DW_DLV_ERROR;
    }

  if (attr->cu->address_size == 4)
    *return_addr = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);
  else
    *return_addr = read_8ubyte_unaligned (attr->cu->dbg, attr->valp);

  return DW_DLV_OK;
}
