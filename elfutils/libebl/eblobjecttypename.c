/* Return object file type name.
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
ebl_object_type_name (ebl, object, buf, len)
     Ebl *ebl;
     int object;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->object_type_name (object, buf, len) : NULL;
  if (res == NULL)
    {
      /* Handle OS-specific section names.  */
      if (object >= ET_LOOS && object <= ET_HIOS)
	snprintf (buf, len, "LOOS+%x", object - ET_LOOS);
      /* Handle processor-specific section names.  */
      else if (object >= ET_LOPROC && object <= ET_HIPROC)
	snprintf (buf, len, "LOPROC+%x", object - ET_LOPROC);
      else
	snprintf (buf, len, "%s: %d", gettext ("<unknown>"), object);

      res = buf;
    }

  return res;
}
