/* i386 specific symbolic name handling.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <libebl_i386.h>


/* Return of the backend.  */
const char *
i386_backend_name (void)
{
  return "i386";
}


/* Relocation mapping table.  */
static const char *reloc_map_table[] =
  {
    [R_386_NONE] = "R_386_NONE",
    [R_386_32] = "R_386_32",
    [R_386_PC32] = "R_386_PC32",
    [R_386_GOT32] = "R_386_GOT32",
    [R_386_PLT32] = "R_386_PLT32",
    [R_386_COPY] = "R_386_COPY",
    [R_386_GLOB_DAT] = "R_386_GLOB_DAT",
    [R_386_JMP_SLOT] = "R_386_JMP_SLOT",
    [R_386_RELATIVE] = "R_386_RELATIVE",
    [R_386_GOTOFF] = "R_386_GOTOFF",
    [R_386_GOTPC] = "R_386_GOTPC",
    [R_386_32PLT] = "R_386_32PLT",
    [R_386_TLS_GD] = "R_386_TLS_GD",
    [R_386_TLS_LDM] = "R_386_TLS_LDM",
    [R_386_16] = "R_386_16",
    [R_386_PC16] = "R_386_PC16",
    [R_386_8] = "R_386_8",
    [R_386_PC8] = "R_386_PC8",
    [R_386_TLS_GD_32] = "R_386_TLS_GD_32",
    [R_386_TLS_GD_PUSH] = "R_386_TLS_GD_PUSH",
    [R_386_TLS_GD_CALL] = "R_386_TLS_GD_CALL",
    [R_386_TLS_GD_POP] = "R_386_TLS_GD_POP",
    [R_386_TLS_LDM_32] = "R_386_TLS_LDM_32",
    [R_386_TLS_LDM_PUSH] = "R_386_TLS_LDM_PUSH",
    [R_386_TLS_LDM_CALL] = "R_386_TLS_LDM_CALL",
    [R_386_TLS_LDM_POP] = "R_386_TLS_LDM_POP",
    [R_386_TLS_LDO_32] = "R_386_TLS_LDO_32",
    [R_386_TLS_IE_32] = "R_386_TLS_IE_32",
    [R_386_TLS_LE_32] = "R_386_TLS_LE_32",
    [R_386_TLS_DTPMOD32] = "R_386_TLS_DTPMOD32",
    [R_386_TLS_DTPOFF32] = "R_386_TLS_DTPOFF32",
    [R_386_TLS_TPOFF32] = "R_386_TLS_TPOFF32"
  };


/* Determine relocation type string for x86.  */
const char *
i386_reloc_type_name (int type, char *buf, size_t len)
{
  if (type < 0 || type >= R_386_NUM)
    return NULL;

  return reloc_map_table[type];
}


/* Check for correct relocation type.  */
bool
i386_reloc_type_check (int type)
{
  return (type >= R_386_NONE && type < R_386_NUM
	  && reloc_map_table[type] != NULL) ? true : false;
}
