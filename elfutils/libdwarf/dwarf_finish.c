/* Free resources allocated for debug handle.
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

#include <stdlib.h>

#include <libdwarfP.h>


int
dwarf_finish (dbg, error)
     Dwarf_Debug dbg;
     Dwarf_Error *error;
{
  if (dbg == NULL)
    return DW_DLV_ERROR;

#ifdef DWARF_DEBUG
  if (dbg->memtag != DW_DLA_DEBUG)
    return DW_DLV_ERROR;
#endif

  free (dbg);

  return DW_DLV_OK;
}
