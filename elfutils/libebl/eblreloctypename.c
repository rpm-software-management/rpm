/* Return relocation type name.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <stdio.h>
#include <libeblP.h>


const char *
ebl_reloc_type_name (ebl, reloc, buf, len)
     Ebl *ebl;
     int reloc;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->reloc_type_name (reloc, buf, len) : NULL;
  if (res == NULL)
    /* There are no generic relocation type names.  */
    res = "???";

  return res;
}
