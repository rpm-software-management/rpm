/* SH specific relocation handling.
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

#include <libebl_sh.h>


/* Return of the backend.  */
const char *
sh_backend_name (void)
{
  return "sh";
}


/* Determine relocation type string for SH.  */
const char *
sh_reloc_type_name (int type, char *buf, size_t len)
{
  static const char *map_table1[] =
  {
    [R_SH_NONE] = "R_SH_NONE",
    [R_SH_DIR32] = "R_SH_DIR32",
    [R_SH_REL32] = "R_SH_REL32",
    [R_SH_DIR8WPN] = "R_SH_DIR8WPN",
    [R_SH_IND12W] = "R_SH_IND12W",
    [R_SH_DIR8WPL] = "R_SH_DIR8WPL",
    [R_SH_DIR8WPZ] = "R_SH_DIR8WPZ",
    [R_SH_DIR8BP] = "R_SH_DIR8BP",
    [R_SH_DIR8W] = "R_SH_DIR8W",
    [R_SH_DIR8L] = "R_SH_DIR8L",
    [R_SH_SWITCH16] = "R_SH_SWITCH16",
    [R_SH_SWITCH32] = "R_SH_SWITCH32",
    [R_SH_USES] = "R_SH_USES",
    [R_SH_COUNT] = "R_SH_COUNT",
    [R_SH_ALIGN] = "R_SH_ALIGN",
    [R_SH_CODE] = "R_SH_CODE",
    [R_SH_DATA] = "R_SH_DATA",
    [R_SH_LABEL] = "R_SH_LABEL",
    [R_SH_SWITCH8] = "R_SH_SWITCH8",
    [R_SH_GNU_VTINHERIT] ="R_SH_GNU_VTINHERIT",
    [R_SH_GNU_VTENTRY] = "R_SH_GNU_VTENTRY"
  };
  static const char *map_table2[] =
  {
    [R_SH_GOT32 - R_SH_GOT32] = "R_SH_GOT32",
    [R_SH_PLT32 - R_SH_GOT32] = "R_SH_PLT32",
    [R_SH_COPY - R_SH_GOT32] = "R_SH_COPY",
    [R_SH_GLOB_DAT - R_SH_GOT32] = "R_SH_GLOB_DAT",
    [R_SH_JMP_SLOT - R_SH_GOT32] = "R_SH_JMP_SLOT",
    [R_SH_RELATIVE - R_SH_GOT32] = "R_SH_RELATIVE",
    [R_SH_GOTOFF - R_SH_GOT32] = "R_SH_GOTOFF",
    [R_SH_GOTPC - R_SH_GOT32] = "R_SH_GOTPC"
  };

  if (type >= 0 && type < sizeof (map_table1) / sizeof (map_table1[0]))
    return map_table1[type];

  if ((type - R_SH_GOT32) >= 0
      && (type - R_SH_GOT32) < sizeof (map_table2) / sizeof (map_table2[0]))
    return map_table2[type - R_SH_GOT32];

  return NULL;
}
