/* Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#include <fcntl.h>
#include <libelf.h>
#include <libdw.h>
#include <stdio.h>
#include <unistd.h>


static int globcnt;

static int
callback (Dwarf *dbg, Dwarf_Global *gl, void *arg)
{
  printf (" [%2d] \"%s\", die: %llu, cu: %llu\n",
	  globcnt++, gl->name, (unsigned long long int) gl->die_offset,
	  (unsigned long long int) gl->cu_offset);

#if 0
  Dwarf_Die cu_die;
  const char *cuname;
  if (dwarf_offdie (dbg, gl->cu_offset, &cu_die) == NULL
      || (cuname = dwarf_diename (&cu_die)) == NULL)
    {
      puts ("failed to get CU die");
      result = 1;
    }
  else
    {
      printf ("CU name: \"%s\"\n", cuname);
      dwarf_dealloc (dbg, cuname, DW_DLA_STRING);
    }

  const char *diename;
  if (dwarf_offdie (dbg, gl->die_offset, &die) == NULL
      || (diename = dwarf_diename (&die)) == NULL)
    {
      puts ("failed to get object die");
      result = 1;
    }
  else
    {
      printf ("object name: \"%s\"\n", diename);
      dwarf_dealloc (dbg, diename, DW_DLA_STRING);
    }
#endif
  return DWARF_CB_OK;
}


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (-1));
	  result = 1;
	  close (fd);
	  continue;
	}

      globcnt = 0;

      if (dwarf_get_pubnames (dbg, callback, NULL, 0) != 0)
	{
	  printf ("dwarf_get_pubnames didn't return zero: %s\n",
		  dwarf_errmsg (-1));
	  result = 1;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return result;
}
