/* Control an ELF file desrciptor.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <unistd.h>

#include "libelfP.h"


int
elf_cntl (Elf *elf, Elf_Cmd cmd)
{
  int result = 0;

  if (elf == NULL)
    return -1;

  if (elf->fildes == -1)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_wrlock (elf->lock);

  switch (cmd)
    {
    case ELF_C_FDREAD:
      /* If not all of the file is in the memory read it now.  */
      if (elf->map_address == NULL && __libelf_readall (elf) == NULL)
	{
	  /* We were not able to read everything.  */
	  result = -1;
	  break;
	}
      /* FALLTHROUGH */

    case ELF_C_FDDONE:
      /* Mark the file descriptor as not usable.  */
      elf->fildes = -1;
      break;

    default:
      __libelf_seterrno (ELF_E_INVALID_CMD);
      result = -1;
      break;
    }

  rwlock_unlock (elf->lock);

  return result;
}
