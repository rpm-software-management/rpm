/* Create descriptor from file descriptor for processing file.
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

#include <stddef.h>
#include <sys/stat.h>

#include <libdwarfP.h>


int
dwarf_init (fd, access, errhand, errarg, dbg, error)
     int fd;
     Dwarf_Unsigned access;
     Dwarf_Handler errhand;
     Dwarf_Ptr errarg;
     Dwarf_Debug *dbg;
     Dwarf_Error *error;
{
  Elf *elf;
  Elf_Cmd cmd;
  struct Dwarf_Debug_s newdbg;
  int result = DW_DLV_ERROR;

  switch (access)
    {
    case DW_DLC_READ:
      cmd = ELF_C_READ_MMAP;
      break;
    case DW_DLC_WRITE:
      cmd = ELF_C_WRITE;
      break;
    case DW_DLC_RDWR:
      cmd = ELF_C_RDWR;
      break;
    default:
      /* These are the error values we want to use.  */
      newdbg.dbg_errhand = errhand;
      newdbg.dbg_errarg = errarg;

      __libdwarf_error (&newdbg, error, DW_E_INVALID_ACCESS);
      return DW_DLV_ERROR;
    }

  /* We have to call `elf_version' here since the user might have not
     done it or initialized libelf with a different version.  This
     would break libdwarf since we are using the ELF data structures
     in a certain way.  */
  elf_version (EV_CURRENT);

  /* Get an ELF descriptor.  */
  elf = elf_begin (fd, cmd, NULL);
  if (elf == NULL)
    {
      /* Test why the `elf_begin" call failed.  */
      struct stat64 st;

      /* These are the error values we want to use.  */
      newdbg.dbg_errhand = errhand;
      newdbg.dbg_errarg = errarg;

      if (fstat64 (fd, &st) == 0 && ! S_ISREG (st.st_mode))
	__libdwarf_error (&newdbg, error, DW_E_NO_REGFILE);
      else
	__libdwarf_error (&newdbg, error, DW_E_IO_ERROR);
    }
  else
    {
      /* Do the real work now that we have an ELF descriptor.  */
      result = dwarf_elf_init (elf, access, errhand, errarg, dbg, error);

      /* If this failed, free the resources.  */
      if (result != DW_DLV_OK)
	elf_end (elf);
    }

  return result;
}
