/* Return machine flag names.
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
#include <string.h>
#include <libeblP.h>


const char *
ebl_machine_flag_name (ebl, flags, buf, len)
     Ebl *ebl;
     Elf64_Word flags;
     char *buf;
     size_t len;
{
  const char *res;

  if (flags == 0)
    res = "";
  else
    {
      char *cp = buf;
      int first = 1;
      const char *machstr;
      size_t machstrlen;

      do
	{
	  if (! first)
	    {
	      if (cp + 1 >= buf + len)
		break;
	      *cp++ = ',';
	    }

	  machstr = ebl != NULL ? ebl->machine_flag_name (&flags) : NULL;
	  if (machstr == NULL)
	    {
	      /* No more known flag.  */
	      snprintf (cp, buf + len - cp, "%#x", flags);
	      break;
	    }

	  machstrlen = strlen (machstr) + 1;
	  if (buf + len - cp < machstrlen)
	    {
	      *((char *) mempcpy (cp, machstr, buf + len - cp - 1)) = '\0';
	      break;
	    }

	  cp = mempcpy (cp, machstr, machstrlen);

	  first = 0;
	}
      while (flags != 0);

      res = buf;
    }

  return res;
}
