/* Find start of macro value.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libdwarf.h>


char *
dwarf_find_macro_value_start (macro_string)
     char *macro_string;
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
