/* Return source file for line.
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

#include <string.h>

#include <libdwarfP.h>


int
dwarf_linesrc (line, return_linesrc, error)
     Dwarf_Line line;
     char **return_linesrc;
     Dwarf_Error *error;
{
  if (unlikely (line->file >= line->files->nfiles))
    {
      __libdwarf_error (line->files->dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  *return_linesrc = strdup (line->files->info[line->file].name);
  if (*return_linesrc == NULL)
    {
      __libdwarf_error (line->files->dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  return DW_DLV_OK;
}
