/* Return section index of section header string table.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <assert.h>
#include <gelf.h>
#include <stddef.h>
#include <unistd.h>

#include "libelfP.h"
#include "common.h"

#if !defined(ALLOW_UNALIGNED)
# define ALLOW_UNALIGNED	0
#endif

int
elf_getshstrndx (elf, dst)
     Elf *elf;
     size_t *dst;
{
  int result = 0;

  if (elf == NULL)
    return -1;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_rdlock (elf->lock);

  /* We rely here on the fact that the `elf' element is a common prefix
     of `elf32' and `elf64'.  */
  assert (offsetof (struct Elf, state.elf.ehdr)
	  == offsetof (struct Elf, state.elf32.ehdr));
  assert (sizeof (elf->state.elf.ehdr)
	  == sizeof (elf->state.elf32.ehdr));
  assert (offsetof (struct Elf, state.elf.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr));
  assert (sizeof (elf->state.elf.ehdr)
	  == sizeof (elf->state.elf64.ehdr));

  if (unlikely (elf->state.elf.ehdr == NULL))
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      result = -1;
    }
  else
    {
      Elf32_Word num;

      num = (elf->class == ELFCLASS32
	     ? elf->state.elf32.ehdr->e_shstrndx
	     : elf->state.elf64.ehdr->e_shstrndx);

      /* Determine whether the index is too big to fit in the ELF
	 header.  */
      if (unlikely (num == SHN_XINDEX))
	{
	  /* Yes.  Search the zeroth section header.  */
	  if (elf->class == ELFCLASS32)
	    {
	      size_t offset;

	      if (elf->state.elf32.scns.data[0].shdr.e32 != NULL)
		{
		  num = elf->state.elf32.scns.data[0].shdr.e32->sh_link;
		  goto success;
		}

	      offset = elf->state.elf32.ehdr->e_shoff;

	      if (elf->map_address != NULL
		  && elf->state.elf32.ehdr->e_ident[EI_DATA] == MY_ELFDATA
		  && (ALLOW_UNALIGNED
		      || (((size_t) ((char *) elf->map_address + offset))
			  & (__alignof__ (Elf32_Shdr) - 1)) == 0))
		/* We can directly access the memory.  */
		num = ((Elf32_Shdr *) (elf->map_address + offset))->sh_link;
	      else
		{
		  /* We avoid reading in all the section headers.  Just read
		     the first one.  */
		  Elf32_Shdr shdr_mem;

		  if (pread (elf->fildes, &shdr_mem, sizeof (Elf32_Shdr),
			     offset) != sizeof (Elf32_Shdr))
		    {
		      /* We must be able to read this ELF section header.  */
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		      result = -1;
		      goto out;
		    }

		  if (elf->state.elf32.ehdr->e_ident[EI_DATA] != MY_ELFDATA)
		    CONVERT (shdr_mem.sh_link);
		  num = shdr_mem.sh_link;
		}
	    }
	  else
	    {
	      size_t offset;

	      if (elf->state.elf64.scns.data[0].shdr.e64 != NULL)
		{
		  num = elf->state.elf64.scns.data[0].shdr.e64->sh_link;
		  goto success;
		}

	      offset = elf->state.elf64.ehdr->e_shoff;

	      if (elf->map_address != NULL
		  && elf->state.elf64.ehdr->e_ident[EI_DATA] == MY_ELFDATA
		  && (ALLOW_UNALIGNED
		      || (((size_t) ((char *) elf->map_address + offset))
			  & (__alignof__ (Elf64_Shdr) - 1)) == 0))
		/* We can directly access the memory.  */
		num = ((Elf64_Shdr *) (elf->map_address + offset))->sh_link;
	      else
		{
		  /* We avoid reading in all the section headers.  Just read
		     the first one.  */
		  Elf64_Shdr shdr_mem;

		  if (pread (elf->fildes, &shdr_mem, sizeof (Elf64_Shdr),
			     offset) != sizeof (Elf64_Shdr))
		    {
		      /* We must be able to read this ELF section header.  */
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		      result = -1;
		      goto out;
		    }

		  if (elf->state.elf64.ehdr->e_ident[EI_DATA] != MY_ELFDATA)
		    CONVERT (shdr_mem.sh_link);
		  num = shdr_mem.sh_link;
		}
	    }
	}

      /* Store the result.  */
    success:
      *dst = num;
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_getshstrndx)
