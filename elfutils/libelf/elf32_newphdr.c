/* Create new ELF program header table.
   Copyright (C) 1999, 2000, 2002 Red Hat, Inc.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Phdr) *
elfw2(LIBELFBITS,newphdr) (Elf *elf, size_t count)
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
  else if (unlikely (elf->class != ELFW(ELFCLASS,LIBELFBITS)))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      result = NULL;
      goto out;
    }

  if (unlikely (elf->state.ELFW(elf,LIBELFBITS).ehdr == NULL))
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      result = NULL;
      goto out;
    }

  /* A COUNT of zero means remove existing table.  */
  if (count == 0)
    {
      /* Free the old program header.  */
      if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL)
	{
	  if (elf->state.ELFW(elf,LIBELFBITS).phdr_flags & ELF_F_MALLOCED)
	    free (elf->state.ELFW(elf,LIBELFBITS).phdr);

	  /* Set the pointer to NULL.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr = NULL;
	  /* Set the `e_phnum' member to the new value.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum = 0;
	  /* Also set the size.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize =
	    sizeof (ElfW2(LIBELFBITS,Phdr));

	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_DIRTY;
	  elf->flags |= ELF_F_DIRTY;
	  __libelf_seterrno (ELF_E_NOERROR);
	}

      result = NULL;
    }
  else if (elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum != count)
    {
      /* Allocate a new program header with the appropriate number of
	 elements.  */
      result = (ElfW2(LIBELFBITS,Phdr) *)
	realloc (elf->state.ELFW(elf,LIBELFBITS).phdr,
		 count * sizeof (ElfW2(LIBELFBITS,Phdr)));
      if (result == NULL)
	__libelf_seterrno (ELF_E_NOMEM);
      else
	{
	  /* Now set the result.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr = result;
	  /* Clear the whole memory.  */
	  memset (result, '\0', count * sizeof (ElfW2(LIBELFBITS,Phdr)));
	  /* Set the `e_phnum' member to the new value.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum = count;
	  /* Also set the size.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize =
	    elf_typesize (LIBELFBITS, ELF_T_PHDR, 1);
	  /* Remember we allocated the array and mark the structure is
	     modified.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |=
	    ELF_F_DIRTY | ELF_F_MALLOCED;
	  /* We have to rewrite the entire file if the size of the
	     program header is changed.  */
	  elf->flags |= ELF_F_DIRTY;
	}
    }
  else
    {
      /* We have the same number of entries.  Just clear the array.  */
      assert (elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize
	      == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

      /* Mark the structure as modified.  */
      elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_DIRTY;

      result = elf->state.ELFW(elf,LIBELFBITS).phdr;
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,newphdr))
