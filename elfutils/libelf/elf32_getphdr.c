/* Get ELF program header table.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
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

#include <stdlib.h>
#include <unistd.h>

#include "libelfP.h"
#include "common.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Phdr) *
elfw2(LIBELFBITS,getphdr) (elf)
     Elf *elf;
{
  ElfW2(LIBELFBITS,Phdr) *result;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_wrlock (elf->lock);

  if (elf->class == 0)
    elf->class = ELFW(ELFCLASS,LIBELFBITS);
  else if (elf->class == ELFW(ELFCLASS,LIBELFBITS))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      result = NULL;
      goto out;
    }

  result = elf->state.ELFW(elf,LIBELFBITS).phdr;
  if (result == NULL)
    {
      /* Read the section header table.  */
      ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;
      size_t phnum;
      size_t size;

      /* Try again, maybe the data is there now.  */
      result = elf->state.ELFW(elf,LIBELFBITS).phdr;
      if (result != NULL)
	goto out;

      phnum = ehdr->e_phnum;
      size = phnum * sizeof (ElfW2(LIBELFBITS,Phdr));

      if (elf->map_address != NULL)
	{
	  /* All the data is already mapped.  Use it.  */
	  if (ehdr->e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (ehdr->e_phoff
		      & (__alignof__ (ElfW2(LIBELFBITS,Phdr)) - 1)) == 0))
	    /* Simply use the mapped data.  */
	    elf->state.ELFW(elf,LIBELFBITS).phdr = (ElfW2(LIBELFBITS,Phdr) *)
	      ((char *) elf->map_address + elf->start_offset + ehdr->e_phoff);
	  else
	    {
	      size_t cnt;
	      ElfW2(LIBELFBITS,Phdr) *notcvt;
	      ElfW2(LIBELFBITS,Phdr) *phdr;

	      /* Allocate memory for the program headers.  We know the number
		 of entries from the ELF header.  */
	      phdr = elf->state.ELFW(elf,LIBELFBITS).phdr =
		(ElfW2(LIBELFBITS,Phdr) *) malloc (size);
	      if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
		{
		  __libelf_seterrno (ELF_E_NOMEM);
		  goto out;
		}
	      elf->state.ELFW(elf,LIBELFBITS).phdr_flags |=
		ELF_F_MALLOCED | ELF_F_DIRTY;

	      /* Now copy the data and at the same time convert the
		 byte order.  */
	      if (ALLOW_UNALIGNED
		  || (ehdr->e_phoff
		      & (__alignof__ (ElfW2(LIBELFBITS,Phdr)) - 1)) == 0)
		notcvt = (ElfW2(LIBELFBITS,Phdr) *)
		  ((char *) elf->map_address
		   + elf->start_offset + ehdr->e_phoff);
	      else
		{
		  notcvt = (ElfW2(LIBELFBITS,Phdr) *) alloca (size);
		  memcpy (notcvt, ((char *) elf->map_address +
				   elf->start_offset + ehdr->e_phoff),
			  size);
		}

	      for (cnt = 0; cnt < phnum; ++cnt)
		{
		  CONVERT_TO (phdr[cnt].p_type, notcvt[cnt].p_type);
		  CONVERT_TO (phdr[cnt].p_offset, notcvt[cnt].p_offset);
		  CONVERT_TO (phdr[cnt].p_vaddr, notcvt[cnt].p_vaddr);
		  CONVERT_TO (phdr[cnt].p_paddr, notcvt[cnt].p_paddr);
		  CONVERT_TO (phdr[cnt].p_filesz, notcvt[cnt].p_filesz);
		  CONVERT_TO (phdr[cnt].p_memsz, notcvt[cnt].p_memsz);
		  CONVERT_TO (phdr[cnt].p_flags, notcvt[cnt].p_flags);
		  CONVERT_TO (phdr[cnt].p_align, notcvt[cnt].p_align);
		}
	    }
	}
      else if (likely (elf->fildes != -1))
	{
	  /* Allocate memory for the program headers.  We know the number
	     of entries from the ELF header.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr =
	    (ElfW2(LIBELFBITS,Phdr) *) malloc (size);
	  if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      goto out;
	    }
	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_MALLOCED;

	  /* Read the header.  */
	  if (pread (elf->fildes, elf->state.ELFW(elf,LIBELFBITS).phdr, size,
		     (elf->start_offset + ehdr->e_phoff)) != size)
	    {
	      /* Severe problems.  We cannot read the data.  */
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      free (elf->state.ELFW(elf,LIBELFBITS).phdr);
	      elf->state.ELFW(elf,LIBELFBITS).phdr = NULL;
	      goto out;
	    }

	  /* If the byte order of the file is not the same as the one
	     of the host convert the data now.  */
	  if (ehdr->e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      ElfW2(LIBELFBITS,Phdr) *phdr;
	      size_t cnt;

	      phdr = elf->state.ELFW(elf,LIBELFBITS).phdr;
	      for (cnt = 0; cnt < phnum; ++cnt)
		{
		  CONVERT (phdr[cnt].p_type);
		  CONVERT (phdr[cnt].p_offset);
		  CONVERT (phdr[cnt].p_vaddr);
		  CONVERT (phdr[cnt].p_paddr);
		  CONVERT (phdr[cnt].p_filesz);
		  CONVERT (phdr[cnt].p_memsz);
		  CONVERT (phdr[cnt].p_flags);
		  CONVERT (phdr[cnt].p_align);
		}
	    }
	}
      else
	{
	  /* The file descriptor was already enabled and not all data was
	     read.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);
	  goto out;
	}

      result = elf->state.ELFW(elf,LIBELFBITS).phdr;
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,getphdr))
