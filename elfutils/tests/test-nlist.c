/* Copyright (C) 2000, 2002 Red Hat, Inc.
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

#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>


int var = 1;

int bss;


int
foo (int a)
{
  return a;
}

int
main (int argc, char *argv[])
{
  struct nlist nl[6] =
  {
    [0] = { .n_name = "var" },
    [1] = { .n_name = "bss" },
    [2] = { .n_name = "main" },
    [3] = { .n_name = "foo" },
    [4] = { .n_name = "not-there" },
    [5] = { .n_name = NULL },
  };
  int cnt;
  int result = 0;

  if (nlist (".libs/test-nlist", nl) != 0
      && nlist ("./test-nlist", nl) != 0)
    {
      puts ("nlist failed");
      exit (1);
    }

  for (cnt = 0; nl[cnt].n_name != NULL; ++cnt)
    {
      if (argc > 1)
	/* For debugging.  */
	printf ("nl[%d].n_name = \"%s\"\n"
		"nl[%d].n_value = %ld\n"
		"nl[%d].n_scnum = %d\n"
		"nl[%d].n_type = %u\n"
		"nl[%d].n_sclass = %d\n"
		"nl[%d].n_numaux = %d\n\n",
		cnt, nl[cnt].n_name,
		cnt, nl[cnt].n_value,
		cnt, nl[cnt].n_scnum,
		cnt, nl[cnt].n_type,
		cnt, nl[cnt].n_sclass,
		cnt, nl[cnt].n_numaux);

      if ((cnt != 4 && nl[cnt].n_value == 0 && nl[cnt].n_scnum == 0
	   && nl[cnt].n_type == 0 && nl[cnt].n_sclass == 0
	   && nl[cnt].n_numaux == 0)
	  || (cnt == 4 && (nl[cnt].n_value != 0 || nl[cnt].n_scnum != 0
			   || nl[cnt].n_type != 0 || nl[cnt].n_sclass != 0
			   || nl[cnt].n_numaux != 0)))
	result = 1;
    }

  return foo (result);
}
