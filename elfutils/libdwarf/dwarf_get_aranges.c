/* Return list address ranges.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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

#include <stdlib.h>

#include <libdwarfP.h>


struct arangelist
{
  Dwarf_Arange arange;
  struct arangelist *next;
};


int
dwarf_get_aranges (dbg, aranges, return_count, error)
     Dwarf_Debug dbg;
     Dwarf_Arange **aranges;
     Dwarf_Signed *return_count;
     Dwarf_Error *error;
{
  Dwarf_Small *readp;
  Dwarf_Small *readendp;
  struct arangelist *arangelist = NULL;
  unsigned int narangelist = 0;

  if (dbg->sections[IDX_debug_aranges].addr == NULL)
    return DW_DLV_NO_ENTRY;

  readp = (Dwarf_Small *) dbg->sections[IDX_debug_aranges].addr;
  readendp = readp + dbg->sections[IDX_debug_aranges].size;

  while (readp < readendp)
    {
      Dwarf_Small *hdrstart = readp;
      Dwarf_Unsigned length;
      unsigned int length_bytes;
      unsigned int version;
      Dwarf_Unsigned offset;
      unsigned int address_size;
      unsigned int segment_size;
      Dwarf_Arange_Info arange_info;

      /* Each entry starts with a header:

	 1. A 4-byte or 12-byte length containing the length of the
	 set of entries for this compilation unit, not including the
	 length field itself. [...]

	 2. A 2-byte version identifier containing the value 2 for
	 DWARF Version 2.1.

	 3. A 4-byte or 8-byte offset into the .debug_info section. [...]

	 4. A 1-byte unsigned integer containing the size in bytes of
	 an address (or the offset portion of an address for segmented
	 addressing) on the target system.

	 5. A 1-byte unsigned integer containing the size in bytes of
	 a segment descriptor on the target system.  */
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
      if (version != 2)
	{
	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      if (length_bytes == 4)
	offset = read_4ubyte_unaligned (dbg, readp);
      else
	offset = read_8ubyte_unaligned (dbg, readp);
      readp += length_bytes;

      address_size = *readp++;
      segment_size = *readp++;

      /* Round the address to the next multiple of 2*address_size.  */
      readp += ((2 * address_size - ((readp - hdrstart) % (2 * address_size)))
		% (2 * address_size));

      arange_info =
	(Dwarf_Arange_Info) malloc (sizeof (struct Dwarf_Arange_s));
      if (arange_info == NULL)
	{
	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      arange_info->dbg = dbg;
      arange_info->offset = offset;

      while (1)
	{
	  Dwarf_Unsigned range_address;
	  Dwarf_Unsigned range_length;
	  struct arangelist *new_arange;

	  if (address_size == 4)
	    {
	      range_address = read_4ubyte_unaligned (dbg, readp);
	      readp += 4;
	      range_length = read_4ubyte_unaligned (dbg, readp);
	      readp += 4;
	    }
	  else if (likely (address_size == 8))
	    {
	      range_address = read_8ubyte_unaligned (dbg, readp);
	      readp += 8;
	      range_length = read_8ubyte_unaligned (dbg, readp);
	      readp += 8;
	    }
	  else
	    abort ();

	  /* Two zero values mark the end.  */
	  if (range_address == 0 && range_length == 0)
	    break;

	  new_arange =
	    (struct arangelist *) alloca (sizeof (struct arangelist));
	  new_arange->arange =
	    (Dwarf_Arange) malloc (sizeof (struct Dwarf_Arange_s));
	  if (new_arange->arange == NULL)
	    {
	      __libdwarf_error (dbg, error, DW_E_NOMEM);
	      return DW_DLV_ERROR;
	    }

	  new_arange->arange->address = range_address;
	  new_arange->arange->length = range_length;
	  new_arange->arange->info = arange_info;

	  new_arange->next = arangelist;
	  arangelist = new_arange;
	  ++narangelist;
	}
    }

  if (narangelist == 0)
    return DW_DLV_NO_ENTRY;

  /* Allocate the array for the result.  */
  *return_count = narangelist;
  *aranges = (Dwarf_Arange *) malloc (narangelist
				      * sizeof (struct Dwarf_Arange_s));
  if (*aranges == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  while (narangelist-- > 0)
    {
      (*aranges)[narangelist] = arangelist->arange;
      arangelist = arangelist->next;
    }

  return DW_DLV_OK;
}
