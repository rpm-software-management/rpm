/* Return segment type name.
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
ebl_segment_type_name (ebl, segment, buf, len)
     Ebl *ebl;
     int segment;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->segment_type_name (segment, buf, len) : NULL;
  if (res == NULL)
    {
      static const char *ptypes[PT_NUM] =
	{
#define PTYPE(name) [PT_##name] = #name
	  PTYPE (NULL),
	  PTYPE (LOAD),
	  PTYPE (DYNAMIC),
	  PTYPE (INTERP),
	  PTYPE (NOTE),
	  PTYPE (SHLIB),
	  PTYPE (PHDR),
	  PTYPE (TLS)
	};

      /* Is it one of the standard segment types?  */
      if (segment >= PT_NULL && segment < PT_NUM)
	res = ptypes[segment];
      else
	{
	  if (segment >= PT_LOOS && segment <= PT_HIOS)
	    snprintf (buf, len, "LOOS+%d", segment - PT_LOOS);
	  else if (segment >= PT_LOPROC && segment <= PT_HIPROC)
	    snprintf (buf, len, "LOPROC+%d", segment - PT_LOPROC);
	  else
	    snprintf (buf, len, "%s: %d", gettext ("<unknown>"), segment);

	  res = buf;
	}
    }

  return res;
}
