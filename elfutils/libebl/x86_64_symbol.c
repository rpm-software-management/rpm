/* x86_64 specific symbolic name handling.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <stddef.h>

#include <libebl_x86_64.h>


/* Return of the backend.  */
const char *
x86_64_backend_name (void)
{
  return "x86-64";
}


/* Relocation mapping table.  */
static const char *reloc_map_table[] =
  {
    [R_X86_64_NONE] = "R_X86_64_NONE",
    [R_X86_64_64] = "R_X86_64_64",
    [R_X86_64_PC32] = "R_X86_64_PC32",
    [R_X86_64_GOT32] = "R_X86_64_GOT32",
    [R_X86_64_PLT32] = "R_X86_64_PLT32",
    [R_X86_64_COPY] = "R_X86_64_COPY",
    [R_X86_64_GLOB_DAT] = "R_X86_64_GLOB_DAT",
    [R_X86_64_JUMP_SLOT] = "R_X86_64_JUMP_SLOT",
    [R_X86_64_RELATIVE] = "R_X86_64_RELATIVE",
    [R_X86_64_GOTPCREL] = "R_X86_64_GOTPCREL",
    [R_X86_64_32] = "R_X86_64_32",
    [R_X86_64_32S] = "R_X86_64_32S",
    [R_X86_64_16] = "R_X86_64_16",
    [R_X86_64_PC16] = "R_X86_64_PC16",
    [R_X86_64_8] = "R_X86_64_8",
    [R_X86_64_PC8] = "R_X86_64_PC8",
    [R_X86_64_DTPMOD64] = "R_X86_64_DTPMOD64",
    [R_X86_64_DTPOFF64] = "R_X86_64_DTPOFF64",
    [R_X86_64_TPOFF64] = "R_X86_64_TPOFF64",
    [R_X86_64_TLSGD] = "R_X86_64_TLSGD",
    [R_X86_64_TLSLD] = "R_X86_64_TLSLD",
    [R_X86_64_DTPOFF32] = "R_X86_64_DTPOFF32",
    [R_X86_64_GOTTPOFF] = "R_X86_64_GOTTPOFF",
    [R_X86_64_TPOFF32] = "R_X86_64_TPOFF32"
  };


/* Determine relocation type string for x86-64.  */
const char *
x86_64_reloc_type_name (int type, char *buf, size_t len)
{
  if (type < 0 || type >= R_X86_64_NUM)
    return NULL;

  return reloc_map_table[type];
}


/* Check for correct relocation type.  */
bool
x86_64_reloc_type_check (int type)
{
  return (type >= R_X86_64_NONE && type < R_X86_64_NUM
	  && reloc_map_table[type] != NULL) ? true : false;
}
