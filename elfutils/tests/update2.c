/* Test program for elf_update function.
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

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main (int argc, char *argv[])
{
  const char *fname = "xxx";
  int fd;
  Elf *elf;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  int i;

  fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd == -1)
    {
      printf ("cannot open `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  elf_version (EV_CURRENT);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Create an ELF header.  */
  ehdr = elf32_newehdr (elf);
  if (ehdr == NULL)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Print the ELF header values.  */
  if (argc > 1)
    {
      for (i = 0; i < EI_NIDENT; ++i)
	printf (" %02x", ehdr->e_ident[i]);
      printf ("\
\ntype = %hu\nmachine = %hu\nversion = %u\nentry = %u\nphoff = %u\n"
	      "shoff = %u\nflags = %u\nehsize = %hu\nphentsize = %hu\n"
	      "phnum = %hu\nshentsize = %hu\nshnum = %hu\nshstrndx = %hu\n",
	      ehdr->e_type, ehdr->e_machine, ehdr->e_version, ehdr->e_entry,
	      ehdr->e_phoff, ehdr->e_shoff, ehdr->e_flags, ehdr->e_ehsize,
	      ehdr->e_phentsize, ehdr->e_phnum, ehdr->e_shentsize,
	      ehdr->e_shnum, ehdr->e_shstrndx);
    }

  ehdr->e_ident[0] = 42;
  ehdr->e_ident[4] = 1;
  ehdr->e_ident[5] = 1;
  ehdr->e_ident[6] = 2;
  ehdr->e_type = ET_EXEC;
  ehdr->e_version = 1;
  ehdr->e_ehsize = 1;
  elf_flagehdr (elf, ELF_C_SET, ELF_F_DIRTY);

  /* Create the program header.  */
  phdr = elf32_newphdr (elf, 1);
  if (phdr == NULL)
    {
      printf ("cannot create program header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  phdr[0].p_type = PT_PHDR;
  elf_flagphdr (elf, ELF_C_SET, ELF_F_DIRTY);

  /* Let the library compute the internal structure information.  */
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  ehdr = elf32_getehdr (elf);

  phdr[0].p_offset = ehdr->e_phoff;
  phdr[0].p_offset = ehdr->e_phoff;
  phdr[0].p_vaddr = ehdr->e_phoff;
  phdr[0].p_paddr = ehdr->e_phoff;
  phdr[0].p_flags = PF_R | PF_X;
  phdr[0].p_filesz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_memsz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_align = sizeof (Elf32_Word);

  /* Write out the file.  */
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Print the ELF header values.  */
  if (argc > 1)
    {
      for (i = 0; i < EI_NIDENT; ++i)
	printf (" %02x", ehdr->e_ident[i]);
      printf ("\
\ntype = %hu\nmachine = %hu\nversion = %u\nentry = %u\nphoff = %u\n"
	      "shoff = %u\nflags = %u\nehsize = %hu\nphentsize = %hu\n"
	      "phnum = %hu\nshentsize = %hu\nshnum = %hu\nshstrndx = %hu\n",
	      ehdr->e_type, ehdr->e_machine, ehdr->e_version, ehdr->e_entry,
	      ehdr->e_phoff, ehdr->e_shoff, ehdr->e_flags, ehdr->e_ehsize,
	      ehdr->e_phentsize, ehdr->e_phnum, ehdr->e_shentsize,
	      ehdr->e_shnum, ehdr->e_shstrndx);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  return 0;
}
