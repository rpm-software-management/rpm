/* Find start of macro value.
   Copyright (C) 2000, 2002 Red Hat, Inc.
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

#include <libdwarf.h>


char *
dwarf_find_macro_value_start (char *macro_string)
{
  int with_paren = 0;

  while (*macro_string != '\0')
    {
      if (*macro_string == '(')
	with_paren = 1;
      else if (*macro_string == ')')
	/* After the closing parenthesis there must be a space.  */
	return macro_string + 2;
      else if (*macro_string == ' ' && ! with_paren)
	/* Not a function like macro and we found the space terminating
	   the name.  */
	return macro_string + 1;

      ++macro_string;
    }

  /* The macro has no value.  Return a pointer to the NUL byte.  */
  return macro_string;
}
