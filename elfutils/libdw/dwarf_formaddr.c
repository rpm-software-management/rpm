/* Return address represented by attribute.
   Copyright (C) 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include <dwarf.h>
#include "libdwP.h"


int
dwarf_formaddr (attr, return_addr)
     Dwarf_Attribute *attr;
     Dwarf_Addr *return_addr;
{
  if (attr == NULL)
    return -1;

  if (unlikely (attr->form != DW_FORM_addr))
    {
      __libdw_seterrno (DWARF_E_NO_ADDR);
      return -1;
    }

  if (attr->cu->address_size == 8)
    *return_addr = read_8ubyte_unaligned (attr->cu->dbg, attr->valp);
  else
    *return_addr = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);

  return 0;
}
