/* Return the size of an object file type.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


/* These are the sizes for all the known types.  */
const size_t __libelf_type_sizes[EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] =
{
  /* We have no entry for EV_NONE siince we have to set an error.  */
  [EV_CURRENT - 1] = {
    [ELFCLASS32 - 1] = {
#define TYPE_SIZES(LIBELFBITS) \
      [ELF_T_ADDR]	= ELFW2(LIBELFBITS, FSZ_ADDR),			      \
      [ELF_T_OFF]	= ELFW2(LIBELFBITS, FSZ_OFF),			      \
      [ELF_T_BYTE]	= 1,						      \
      [ELF_T_HALF]	= ELFW2(LIBELFBITS, FSZ_HALF),			      \
      [ELF_T_WORD]	= ELFW2(LIBELFBITS, FSZ_WORD),			      \
      [ELF_T_SWORD]	= ELFW2(LIBELFBITS, FSZ_SWORD),			      \
      [ELF_T_XWORD]	= ELFW2(LIBELFBITS, FSZ_XWORD),			      \
      [ELF_T_SXWORD]	= ELFW2(LIBELFBITS, FSZ_SXWORD),		      \
      [ELF_T_EHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Ehdr)),		      \
      [ELF_T_SHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Shdr)),		      \
      [ELF_T_SYM]	= sizeof (ElfW2(LIBELFBITS, Ext_Sym)),		      \
      [ELF_T_REL]	= sizeof (ElfW2(LIBELFBITS, Ext_Rel)),		      \
      [ELF_T_RELA]	= sizeof (ElfW2(LIBELFBITS, Ext_Rela)),		      \
      [ELF_T_PHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Phdr)),		      \
      [ELF_T_DYN]	= sizeof (ElfW2(LIBELFBITS, Ext_Dyn)),		      \
      [ELF_T_VDEF]	= sizeof (ElfW2(LIBELFBITS, Ext_Verdef)),	      \
      [ELF_T_VDAUX]	= sizeof (ElfW2(LIBELFBITS, Ext_Verdaux)),	      \
      [ELF_T_VNEED]	= sizeof (ElfW2(LIBELFBITS, Ext_Verneed)),	      \
      [ELF_T_VNAUX]	= sizeof (ElfW2(LIBELFBITS, Ext_Vernaux)),	      \
      [ELF_T_NHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Nhdr)),		      \
      [ELF_T_SYMINFO]	= sizeof (ElfW2(LIBELFBITS, Ext_Syminfo)),	      \
      [ELF_T_MOVE]	= sizeof (ElfW2(LIBELFBITS, Ext_Move))
      TYPE_SIZES (32)
    },
    [ELFCLASS64 - 1] = {
      TYPE_SIZES (64)
    }
  }
};


size_t
gelf_fsize (Elf *elf, Elf_Type type, size_t count, unsigned int version)
{
  /* We do not have differences between file and memory sizes.  Better
     not since otherwise `mmap' would not work.  */
  if (elf == NULL)
    return 0;

  if (version == EV_NONE || version >= EV_NUM)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
      return 0;
    }

  if (type >= ELF_T_NUM)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_TYPE);
      return 0;
    }

#if EV_NUM != 2
  return count * __libelf_type_sizes[version - 1][elf->class - 1][type];
#else
  return count * __libelf_type_sizes[0][elf->class - 1][type];
#endif
}
INTDEF(gelf_fsize)
