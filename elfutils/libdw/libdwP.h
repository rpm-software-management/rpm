/* Internal definitions for libdwarf.
   Copyright (C) 2002 Red Hat, Inc.
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

#ifndef _LIBDWP_H
#define _LIBDWP_H 1

#include <stdbool.h>

#include <libdw.h>


/* Valid indeces for the section data.  */
enum
  {
    IDX_debug_info = 0,
    IDX_debug_abbrev,
    IDX_debug_aranges,
    IDX_debug_line,
    IDX_debug_frame,
    IDX_eh_frame,
    IDX_debug_loc,
    IDX_debug_pubnames,
    IDX_debug_str,
    IDX_debug_funcnames,
    IDX_debug_typenames,
    IDX_debug_varnames,
    IDX_debug_weaknames,
    IDX_debug_macinfo,
    IDX_last
  };


/* Error values.  */
enum
{
  DWARF_E_NOERROR = 0,
  DWARF_E_INVALID_ACCESS,
  DWARF_E_NO_REGFILE,
  DWARF_E_IO_ERROR,
  DWARF_E_INVALID_ELF,
  DWARF_E_NO_DWARF,
  DWARF_E_NOELF,
  DWARF_E_GETEHDR_ERROR,
  DWARF_E_NOMEM,
  DWARF_E_UNIMPL,
  DWARF_E_INVALIDCMD,
};


/* This is the structure representing the debugging state.  */
struct Dwarf
{
  /* The underlying ELF file.  */
  Elf *elf;

  /* The section data.  */
  Elf_Data *sectiondata[IDX_last];

  /* True if the file has a byte order different from the host.  */
  bool other_byte_order;
};




/* Set error value.  */
extern void __libdwarf_seterrno (int value) internal_function;


#endif	/* libdwP.h */
