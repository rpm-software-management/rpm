/* Return list of global definitions.
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
#include <string.h>

#include <libdwarfP.h>


struct globallist
{
  Dwarf_Global global;
  struct globallist *next;
};


/* Read the whole given section.  */
int
dwarf_get_globals (dbg, globals, return_count, error)
     Dwarf_Debug dbg;
     Dwarf_Global **globals;
     Dwarf_Signed *return_count;
     Dwarf_Error *error;
{
  Dwarf_Small *readp;
  Dwarf_Small *readendp;
  struct globallist *globallist = NULL;
  unsigned int ngloballist = 0;

  if (dbg->sections[IDX_debug_pubnames].addr == NULL)
    return DW_DLV_NO_ENTRY;

  readp = (Dwarf_Small *) dbg->sections[IDX_debug_pubnames].addr;
  readendp = readp + dbg->sections[IDX_debug_pubnames].size;

  while (readp < readendp)
    {
      Dwarf_Unsigned length;
      Dwarf_Unsigned info_length;
      unsigned int length_bytes;
      unsigned int version;
      Dwarf_Unsigned offset;
      Dwarf_Global_Info global_info;

      /* Each entry starts with a header:

 	 1. A 4-byte or 12-byte length of the set of entries for this
 	 compilation unit, not including the length field itself. [...]

	 2. A 2-byte version identifier containing the value 2 for
	 DWARF Version 2.1.

	 3. A 4-byte or 8-byte offset into the .debug_info section. [...]

	 4. A 4-byte or 8-byte length containing the size in bytes of
	 the contents of the .debug_info section generated to
	 represent this compilation unit. [...]  */
      length = read_4ubyte_unaligned (dbg, readp);
      readp += 4;
      length_bytes = 4;
      if (length == 0xffffffff)
	{
	  length = read_8ubyte_unaligned (dbg, readp);
	  readp += 8;
	  length_bytes = 8;
	}

      version = read_2ubyte_unaligned (dbg, readp);
      readp += 2;
      if (unlikely (version != 2))
	{
	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      if (length_bytes == 4)
	{
	  offset = read_4ubyte_unaligned (dbg, readp);
	  readp += 4;
	  info_length = read_4ubyte_unaligned (dbg, readp);
	  readp += 4;
	}
      else
	{
	  offset = read_8ubyte_unaligned (dbg, readp);
	  readp += 8;
	  info_length = read_8ubyte_unaligned (dbg, readp);
	  readp += 8;
	}

      global_info =
	(Dwarf_Global_Info) malloc (sizeof (struct Dwarf_Global_s));
      if (global_info == NULL)
	{
	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      global_info->dbg = dbg;
      global_info->offset = offset;

      /* Following the section contains tuples of offsets and
         nul-terminated strings.  */
      while (1)
	{
	  Dwarf_Unsigned die_offset;
	  struct globallist *new_global;

	  if (length_bytes == 4)
	    die_offset = read_4ubyte_unaligned (dbg, readp);
	  else
	    die_offset = read_8ubyte_unaligned (dbg, readp);
	  readp += length_bytes;

	  if (die_offset == 0)
	    /* This closes this entry.  */
	    break;

	  new_global =
	    (struct globallist *) alloca (sizeof (struct globallist));
	  new_global->global =
	    (Dwarf_Global) malloc (sizeof (struct Dwarf_Global_s));
	  if (new_global->global == NULL)
	    {
	      __libdwarf_error (dbg, error, DW_E_NOMEM);
	      return DW_DLV_ERROR;
	    }

	  new_global->global->offset = die_offset;
	  new_global->global->name = (char *) readp;
	  new_global->global->info = global_info;

	  new_global->next = globallist;
	  globallist = new_global;
	  ++ngloballist;

	  readp = (Dwarf_Small *) rawmemchr (readp, '\0') + 1;
	}
    }

  if (ngloballist == 0)
    return DW_DLV_NO_ENTRY;

  /* Allocate the array for the result.  */
  *return_count = ngloballist;
  *globals = (Dwarf_Global *) malloc (ngloballist
				      * sizeof (struct Dwarf_Global_s));
  if (*globals == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  while (ngloballist-- > 0)
    {
      (*globals)[ngloballist] = globallist->global;
      globallist = globallist->next;
    }

  return DW_DLV_OK;
}
