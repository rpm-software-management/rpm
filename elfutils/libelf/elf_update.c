/* Update data structures for changes and write them out.
   Copyright (C) 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1999.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libelf.h>
#include <unistd.h>
#include <sys/mman.h>

#include "libelfP.h"


static off_t
write_file (Elf *elf, off_t size, int change_bo, size_t shnum)
	/*@modifies elf @*/
{
  int class = elf->class;

  /* Adjust the size in any case.  We do this even if we use `write'.
     We cannot do this if this file is in an archive.  We also don't
     do it *now* if we are shortening the file since this woudl
     prevent programs to use the data of the file in generating the
     new file.  We truncate the file later in this case.  */
  if (elf->parent == NULL
      && (elf->maximum_size == ~((size_t) 0) || size > elf->maximum_size)
      && unlikely (ftruncate (elf->fildes, size) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      return -1;
    }

  /* Try to map the file if this isn't done yet.  */
  if (elf->map_address == NULL && elf->cmd == ELF_C_WRITE_MMAP)
    {
      elf->map_address = mmap (NULL, size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, elf->fildes, 0);
      if (unlikely (elf->map_address == MAP_FAILED))
	elf->map_address = NULL;
    }

  if (elf->map_address != NULL)
    {
      /* The file is mmaped.  */
      if ((class == ELFCLASS32
	   ? __elf32_updatemmap (elf, change_bo, shnum)
	   : __elf64_updatemmap (elf, change_bo, shnum)) != 0)
	/* Some problem while writing.  */
	size = -1;
    }
  else
    {
      /* The file is not mmaped.  */
      if ((class == ELFCLASS32
	   ? __elf32_updatefile (elf, change_bo, shnum)
	   : __elf64_updatefile (elf, change_bo, shnum)) != 0)
	/* Some problem while writing.  */
	size = -1;
    }

  if (size != -1
      && elf->parent == NULL
      && elf->maximum_size != ~((size_t) 0)
      && size < elf->maximum_size
      && unlikely (ftruncate (elf->fildes, size) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      size = -1;
    }

  if (size != -1 && elf->parent == NULL)
    elf->maximum_size = size;

  return size;
}


off_t
elf_update (Elf *elf, Elf_Cmd cmd)
{
  size_t shnum;
  off_t size;
  int change_bo = 0;

  if (cmd != ELF_C_NULL
      && cmd != ELF_C_WRITE
      && unlikely (cmd != ELF_C_WRITE_MMAP))
    {
      __libelf_seterrno (ELF_E_INVALID_CMD);
      return -1;
    }

  if (elf == NULL)
    return -1;

  if (elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_rdlock (elf->lock);

  /* Make sure we have an ELF header.  */
  if (elf->state.elf.ehdr == NULL)
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      size = -1;
      goto out;
    }

  /* Determine the number of sections.  */
  shnum = (elf->state.elf.scns_last->cnt == 0
	   ? 0
	   : 1 + elf->state.elf.scns_last->data[elf->state.elf.scns_last->cnt - 1].index);

  /* Update the ELF descriptor.  First, place the program header.  It
     will come right after the ELF header.  The count the size of all
     sections and finally place the section table.  */
  size = (elf->class == ELFCLASS32
	  ? __elf32_updatenull (elf, &change_bo, shnum)
	  : __elf64_updatenull (elf, &change_bo, shnum));
  if (likely (size != -1)
      /* See whether we actually have to write out the data.  */
      && (cmd == ELF_C_WRITE || cmd == ELF_C_WRITE_MMAP))
    {
      if (elf->cmd != ELF_C_RDWR
	  && elf->cmd != ELF_C_RDWR_MMAP
	  && elf->cmd != ELF_C_WRITE
	  && unlikely (elf->cmd != ELF_C_WRITE_MMAP))
	{
	  __libelf_seterrno (ELF_E_UPDATE_RO);
	  size = -1;
	}
      else if (unlikely (elf->fildes == -1))
	{
	  /* We closed the file already.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);
	  size = -1;
	}
      else
	size = write_file (elf, size, change_bo, shnum);
    }

 out:
  rwlock_unlock (elf->unlock);

  return size;
}
