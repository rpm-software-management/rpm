/* Copyright (C) 1999, 2000, 2001, 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#include <assert.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  Elf *elf;
  int fd;
  Elf_Scn *section;

  if (elf_version (EV_CURRENT) == EV_NONE)
    {
      fprintf (stderr, "library fd of date\n");
      exit (1);
    }

  char name[] = "test.XXXXXX";
  fd = mkstemp (name);
  if (fd < 0)
    {
      fprintf (stderr, "Failed to open fdput file: %s\n", name);
      exit (1);
    }
  unlink (name);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "Failed to elf_begin fdput file: %s\n", name);
      exit (1);
    }

  section = elf_newscn (elf);
  section = elf_nextscn (elf, section);
  assert (section == NULL);

  elf_end (elf);
  close (fd);

  return 0;
}
