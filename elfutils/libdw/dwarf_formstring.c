/* Return string associated with given attribute.
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


const char *
dwarf_formstring (attrp)
     Dwarf_Attribute *attrp;
{
  /* Ignore earlier errors.  */
  if (attrp == NULL)
    return NULL;

  /* We found it.  Now determine where the string is stored.  */
  if (attrp->form == DW_FORM_string)
    /* A simple inlined string.  */
    return (const char *) attrp->valp;

  Dwarf *dbg = attrp->cu->dbg;

  if (unlikely (attrp->form != DW_FORM_strp)
      || dbg->sectiondata[IDX_debug_str] == NULL)
    {
    invalid_error:
      __libdw_seterrno (DWARF_E_NO_STRING);
      return NULL;
    }

  uint64_t off;
  // XXX We need better boundary checks.
  if (attrp->cu->address_size == 8)
    off = read_8ubyte_unaligned (dbg, attrp->valp);
  else
    off = read_4ubyte_unaligned (dbg, attrp->valp);

  if (off  >= dbg->sectiondata[IDX_debug_str]->d_size)
    goto invalid_error;

  return (const char *) dbg->sectiondata[IDX_debug_str]->d_buf + off;
}
