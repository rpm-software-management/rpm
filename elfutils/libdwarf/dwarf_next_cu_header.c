/* Get offset of next compilation unit.
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

#include <assert.h>
#include <stdlib.h>

#include <libdwarfP.h>


/* A good initial size for the abbreviation hashing table.  */
#define DEFAULT_ABBREV_HASH_SIZE	257


int
internal_function
__libdwarf_get_cu_at_offset (Dwarf_Debug dbg, Dwarf_Unsigned offset,
			     Dwarf_CU_Info *result_cu, Dwarf_Error *error)
{
  Dwarf_CU_Info cu;
  Dwarf_Small *cu_bytes;
  Dwarf_Unsigned length;
  Dwarf_Half offset_size;

  /* Make sure there is enough space in the .debug_info section for at
     least the initial word.  We cannot test the rest since we don't
     know yet whether this is a 64-bit object or not.  */
  if (unlikely (offset + 4 >= dbg->sections[IDX_debug_info].size))
    {
      dbg->cu_list_current = NULL;
      return DW_DLV_NO_ENTRY;
    }

  /* This points into the .debug_info section to the beginning of the
     CU entry.  */
  cu_bytes = (Dwarf_Small *) dbg->sections[IDX_debug_info].addr + offset;

  /* The format of the CU header is described in dwarf2p1 7.5.1:

     1.  A 4-byte or 12-byte unsigned integer representing the length
	 of the .debug_info contribution for that compilation unit, not
	 including the length field itself. In the 32-bit DWARF format,
	 this is a 4-byte unsigned integer (which must be less than
	 0xffffff00); in the 64-bit DWARF format, this consists of the
	 4-byte value 0xffffffff followed by an 8-byte unsigned integer
	 that gives the actual length (see Section 7.4).

      2. A 2-byte unsigned integer representing the version of the
	 DWARF information for that compilation unit. For DWARF Version
	 2.1, the value in this field is 2.

      3. A 4-byte or 8-byte unsigned offset into the .debug_abbrev
	 section. This offset associates the compilation unit with a
	 particular set of debugging information entry abbreviations. In
	 the 32-bit DWARF format, this is a 4-byte unsigned length; in
	 the 64-bit DWARF format, this is an 8-byte unsigned length (see
	 Section 7.4).

      4. A 1-byte unsigned integer representing the size in bytes of
	 an address on the target architecture. If the system uses
	 segmented addressing, this value represents the size of the
	 offset portion of an address.  */
  length = read_4ubyte_unaligned (dbg, cu_bytes);
  cu_bytes += 4;
  offset_size = 4;
  if (length == 0xffffffff)
    offset_size = 8;

  /* Now we know how large the header is.  Note the trick in the
     computation.  If the offset_size is 4 the '- 4' term undoes the
     '2 *'.  If offset_size is 8 this term computes the size of the
     escape value plus the 8 byte offset.  */
  if (unlikely (offset + 2 * offset_size - 4 + sizeof (Dwarf_Half)
		+ offset_size + sizeof (Dwarf_Small)
		>= dbg->sections[IDX_debug_info].size))
    {
      dbg->cu_list_current = NULL;
      return DW_DLV_NO_ENTRY;
    }

  if (length == 0xffffffff)
    {
      /* This is a 64-bit DWARF format.  */
      length = read_8ubyte_unaligned (dbg, cu_bytes);
      cu_bytes += 8;
    }

  /* We know we have enough room in the .debug_section.  Allocate the
     result data structure.  */
  cu = (Dwarf_CU_Info) malloc (sizeof (struct Dwarf_CU_Info_s));
  if (cu == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  /* Store the values in the data structure.  */
  cu->offset = offset;
  cu->offset_size = offset_size;
  cu->length = length;

  /* Store the version stamp.  Always a 16-bit value.  */
  cu->version_stamp = read_2ubyte_unaligned (dbg, cu_bytes);
  cu_bytes += 2;

  /* Get offset in .debug_abbrev.  Note that the size of the entry
     depends on whether this is a 32-bit or 64-bit DWARF definition.  */
  if (offset_size == 4)
    cu->abbrev_offset = read_4ubyte_unaligned (dbg, cu_bytes);
  else
    cu->abbrev_offset = read_8ubyte_unaligned (dbg, cu_bytes);
  cu_bytes += offset_size;
  cu->last_abbrev_offset = cu->abbrev_offset;

  /* The address size.  Always an 8-bit value.  */
  cu->address_size = *cu_bytes++;

  /* Store the header length.  */
  cu->header_length = (cu_bytes
		       - ((Dwarf_Small *) dbg->sections[IDX_debug_info].addr
			  + offset));

  /* Initilize a few more members.  */
  if (unlikely (Dwarf_Abbrev_Hash_init (&cu->abbrev_hash,
					DEFAULT_ABBREV_HASH_SIZE) != 0))
    {
      free (cu);
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  /* Remember the debugging handle.  */
  cu->dbg = dbg;

  /* There is no other entry yet.  */
  cu->next = NULL;

  /* Enqueue the new entry.  */
  if (dbg->cu_list == NULL)
    dbg->cu_list = dbg->cu_list_tail = cu;
  else
    {
      dbg->cu_list_tail->next = cu;
      dbg->cu_list_tail = cu;
    }
  *result_cu = cu;

  return DW_DLV_OK;
}


int
dwarf_next_cu_header (dbg, cu_header_length, version_stamp, abbrev_offset,
		      address_size, next_cu_header, error)
     Dwarf_Debug dbg;
     Dwarf_Unsigned *cu_header_length;
     Dwarf_Half *version_stamp;
     Dwarf_Unsigned *abbrev_offset;
     Dwarf_Half *address_size;
     Dwarf_Unsigned *next_cu_header;
     Dwarf_Error *error;
{
  Dwarf_Unsigned offset_next_cu;
  Dwarf_CU_Info cu;

  if (dbg == NULL)
    return DW_DLV_ERROR;

  /* Determine offset of next CU header.  If we don't have a current
     CU this is the first call and we start right at the beginning.  */
  if (dbg->cu_list_current == NULL)
    {
      offset_next_cu = 0;
      cu = dbg->cu_list;
    }
  else
    {
      /* We can compute the offset from the information we read from
	 the last CU header.  */
      cu = dbg->cu_list_current;

      offset_next_cu = cu->offset + 2 * cu->offset_size - 4 + cu->length;

      /* See whether the next entry entry is available.  */
      cu = cu->next;
    }

  /* If the entry is not yet available get it.  */
  if (cu == NULL)
    {
      int res = __libdwarf_get_cu_at_offset (dbg, offset_next_cu, &cu, error);

      /* If it still does not exist, fail.  Note that this can mean an
	 error or that we reached the end.  */
      if (res != DW_DLV_OK)
	return res;

      dbg->cu_list_current = cu;
      assert (cu != NULL);
    }

  /* See get_cu_at_offset for an explanation of the trick in this
     formula.  */
  *next_cu_header = offset_next_cu + 2 * cu->offset_size - 4 + cu->length;

  /* Extract the information and put it where the user wants it.  */
  if (cu_header_length != NULL)
    *cu_header_length = cu->header_length;

  if (version_stamp != NULL)
    *version_stamp = cu->version_stamp;

  if (abbrev_offset != NULL)
    *abbrev_offset = cu->abbrev_offset;

  if (address_size != NULL)
    *address_size = cu->address_size;

  return DW_DLV_OK;
}
