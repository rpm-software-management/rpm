/* Return string pointer from string section.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


char *
elf_strptr (Elf *elf, size_t index, size_t offset)
{
  char *result = NULL;
  Elf_ScnList *runp;
  Elf_Scn *strscn;

  if (elf == NULL)
    return NULL;

  if (elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_rdlock (elf->lock);

  /* Find the section in the list.  */
  runp = (elf->class == ELFCLASS32
	  || (offsetof (struct Elf, state.elf32.scns)
	      == offsetof (struct Elf, state.elf64.scns))
	  ? &elf->state.elf32.scns : &elf->state.elf64.scns);
  while (1)
    {
      if (index < runp->max)
	{
	  if (index < runp->cnt)
	    strscn = &runp->data[index];
	  else
	    {
	      __libelf_seterrno (ELF_E_INVALID_INDEX);
	      goto out;
	    }
	  break;
	}

      index -= runp->max;

      runp = runp->next;
      if (runp == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}
    }

  if (elf->class == ELFCLASS32)
    {
      if (unlikely (strscn->shdr.e32->sh_type != SHT_STRTAB))
	{
	  /* This is no string section.  */
	  __libelf_seterrno (ELF_E_INVALID_SECTION);
	  goto out;
	}

      if (unlikely (offset >= strscn->shdr.e32->sh_size))
	{
	  /* The given offset is too big, it is beyond this section.  */
	  __libelf_seterrno (ELF_E_OFFSET_RANGE);
	  goto out;
	}
    }
  else
    {
      if (unlikely (strscn->shdr.e64->sh_type != SHT_STRTAB))
	{
	  /* This is no string section.  */
	  __libelf_seterrno (ELF_E_INVALID_SECTION);
	  goto out;
	}

      if (unlikely (offset >= strscn->shdr.e64->sh_size))
	{
	  /* The given offset is too big, it is beyond this section.  */
	  __libelf_seterrno (ELF_E_OFFSET_RANGE);
	  goto out;
	}
    }

  if (strscn->rawdata_base == NULL
      /* Read the section data.  */
      && __libelf_set_rawdata (strscn) != 0)
    goto out;

  result = &strscn->rawdata_base[offset];

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_strptr)
