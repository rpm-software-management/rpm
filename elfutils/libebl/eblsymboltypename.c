/* Return symbol type name.
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
ebl_symbol_type_name (ebl, symbol, buf, len)
     Ebl *ebl;
     int symbol;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->symbol_type_name (symbol, buf, len) : NULL;
  if (res == NULL)
    {
      static const char *stt_names[STT_NUM] =
	{
	  [STT_NOTYPE] = "NOTYPE",
	  [STT_OBJECT] = "OBJECT",
	  [STT_FUNC] = "FUNC",
	  [STT_SECTION] = "SECTION",
	  [STT_FILE] = "FILE",
	  [STT_COMMON] = "COMMON",
	  [STT_TLS] = "TLS"
	};

      /* Standard type?  */
      if (symbol < STT_NUM)
	res = stt_names[symbol];
      else
	{
	  if (symbol >= STT_LOPROC && symbol <= STT_HIPROC)
	    snprintf (buf, len, "LOPROC+%d", symbol - STT_LOPROC);
	  else if (symbol >= STT_LOOS && symbol <= STT_HIOS)
	    snprintf (buf, len, "LOOS+%d", symbol - STT_LOOS);
	  else
	    snprintf (buf, len, gettext ("<unknown>: %d"), symbol);

	  res = buf;
	}
    }

  return res;
}
