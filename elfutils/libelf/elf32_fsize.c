/* Return the size of an object file type.
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

#include <libelf.h>
#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


size_t
elfw2(LIBELFBITS, fsize) (type, count, version)
     Elf_Type type;
     size_t count;
     unsigned int version;
{
  /* We do not have differences between file and memory sizes.  Better
     not since otherwise `mmap' would not work.  */
  if (unlikely (version == EV_NONE) || unlikely (version >= EV_NUM))
    {
      __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
      return 0;
    }

  if (unlikely (type >= ELF_T_NUM))
    {
      __libelf_seterrno (ELF_E_UNKNOWN_TYPE);
      return 0;
    }

#if EV_NUM != 2
  return (count
	  * __libelf_type_sizes[version - 1][ELFW(ELFCLASS,LIBELFBITS) - 1][type]);
#else
  return (count
	  * __libelf_type_sizes[0][ELFW(ELFCLASS,LIBELFBITS) - 1][type]);
#endif
}
#define local_strong_alias(n1, n2) strong_alias (n1, n2)
local_strong_alias (elfw2(LIBELFBITS, fsize), __elfw2(LIBELFBITS, msize))
