/* Advance to next CU header.
   Copyright (C) 2002, 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libdwP.h>


int
dwarf_nextcu (dwarf, off, next_off, header_sizep, abbrev_offsetp,
	      address_sizep)
     Dwarf *dwarf;
     Dwarf_Off off;
     Dwarf_Off *next_off;
     size_t *header_sizep;
     Dwarf_Off *abbrev_offsetp;
     uint8_t *address_sizep;
{
  /* Maybe there has been an error before.  */
  if (dwarf == NULL)
    return 0;

  /* If we reached the end before don't do anything.  */
  if (off == (Dwarf_Off) -1l
      /* Make sure there is enough space in the .debug_info section
	 for at least the initial word.  We cannot test the rest since
	 we don't know yet whether this is a 64-bit object or not.  */
      || unlikely (off + 4 >= dwarf->sectiondata[IDX_debug_info]->d_size))
    {
      *next_off = (Dwarf_Off) -1l;
      return 1;
    }

  /* This points into the .debug_info section to the beginning of the
     CU entry.  */
  char *bytes = (char *) dwarf->sectiondata[IDX_debug_info]->d_buf + off;

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
  uint64_t length = read_4ubyte_unaligned (dwarf, bytes);
  bytes += 4;
  size_t offset_size = 4;
  if (length == 0xffffffffu)
    offset_size = 8;

  /* Now we know how large the header is.  Note the trick in the
     computation.  If the offset_size is 4 the '- 4' term undoes the
     '2 *'.  If offset_size is 8 this term computes the size of the
     escape value plus the 8 byte offset.  */
  if (unlikely (off + 2 * offset_size - 4 + sizeof (uint16_t)
		+ offset_size + sizeof (uint8_t)
		>= dwarf->sectiondata[IDX_debug_info]->d_size))
    {
      *next_off = -1;
      return 1;
    }

  if (length == 0xffffffffu)
    {
      /* This is a 64-bit DWARF format.  */
      length = read_8ubyte_unaligned (dwarf, bytes);
      bytes += 8;
    }

  /* Read the version stamp.  Always a 16-bit value.
     XXX Do we need the value?  */
  read_2ubyte_unaligned (dwarf, bytes);
  bytes += 2;

  /* Get offset in .debug_abbrev.  Note that the size of the entry
     depends on whether this is a 32-bit or 64-bit DWARF definition.  */
  uint64_t abbrev_offset;
  if (offset_size == 4)
    abbrev_offset = read_4ubyte_unaligned (dwarf, bytes);
  else
    abbrev_offset = read_8ubyte_unaligned (dwarf, bytes);
  if (abbrev_offsetp != NULL)
    *abbrev_offsetp = abbrev_offset;
  bytes += offset_size;

  /* The address size.  Always an 8-bit value.  */
  uint8_t address_size = *bytes++;
  if (address_sizep != NULL)
    *address_sizep = address_size;

  /* Store the header length.  */
  if (header_sizep != NULL)
    *header_sizep = (bytes
		     - ((char *) dwarf->sectiondata[IDX_debug_info]->d_buf
			+ off));

  /* See above for an explanation of the trick in this formula.  */
  *next_off = off + 2 * offset_size - 4 + length;

  return 0;
}
