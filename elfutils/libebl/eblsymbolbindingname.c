/* Return symbol binding name.
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
ebl_symbol_binding_name (ebl, binding, buf, len)
     Ebl *ebl;
     int binding;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->symbol_type_name (binding, buf, len) : NULL;
  if (res == NULL)
    {
      static const char *stb_names[STB_NUM] =
	{
	  "LOCAL", "GLOBAL", "WEAK"
	};

      /* Standard binding?  */
      if (binding < STB_NUM)
	res = stb_names[binding];
      else
	{
	  if (binding >= STB_LOPROC && binding <= STB_HIPROC)
	    snprintf (buf, len, "LOPROC+%d", binding - STB_LOPROC);
	  else if (binding >= STB_LOOS && binding <= STB_HIOS)
	    snprintf (buf, len, "LOOS+%d", binding - STB_LOOS);
	  else
	    snprintf (buf, len, gettext ("<unknown>: %d"), binding);

	  res = buf;
	}
    }

  return res;
}
