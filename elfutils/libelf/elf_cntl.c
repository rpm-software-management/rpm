/* Control an ELF file desrciptor.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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
