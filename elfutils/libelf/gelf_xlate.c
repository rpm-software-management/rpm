/* Transformation functions for ELF data types.
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

#include <byteswap.h>
#include <stdint.h>
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS	32
#endif


/* Well, what shall I say.  Nothing to do here.  */
#define elf_cvt_Byte(dest, src, n) \
  (__builtin_constant_p (n) && (n) == 1					      \
   ? (void) (*((char *) (dest)) = *((char *) (src)))			      \
   : Elf32_cvt_Byte (dest, src, n))
static void
(elf_cvt_Byte) (void *dest, const void *src, size_t n, int encode)
	/*@modifies dest @*/
{
  memmove (dest, src, n);
}


/* We'll optimize the definition of the conversion functions here a
   bit.  We need only functions for 16, 32, and 64 bits.  The
   functions referenced in the table will be aliases for one of these
   functions.  Which one is decided by the ELFxx_FSZ_type.  */
#define LEN2_SWAP(Src)  bswap_16 (*((uint16_t *) Src))
#define word2_t uint16_t

#define LEN4_SWAP(Src)  bswap_32 (*((uint32_t *) Src))
#define word4_t uint32_t

#define LEN8_SWAP(Src)  bswap_64 (*((uint64_t *) Src))
#define word8_t uint64_t


/* Now define the conversion functions for the basic types.  We use here
   the fact that file and memory types are the same and that we have the
   ELFxx_FSZ_* macros.

   At the same time we define inline functions which we will use to
   convert the complex types.  */
#define FUNDAMENTAL(NAME, Name, Bits) \
  INLINE2 (ELFW2(Bits,FSZ_##NAME), ElfW2(Bits,cvt_##Name), ElfW2(Bits,Name))
#define INLINE2(Bytes, FName, TName) \
  INLINE3 (Bytes, FName, TName)
#define INLINE3(Bytes, FName, TName) \
  static void FName (void *dest, const void *ptr, size_t len, int encode)     \
  {									      \
    size_t n = len / sizeof (TName);					      \
    if (dest < ptr)							      \
      while (n-- > 0)							      \
	{								      \
          *((word##Bytes##_t *) dest)++ =				      \
	    LEN##Bytes##_SWAP ((word##Bytes##_t *) ptr);		      \
	  ((word##Bytes##_t *) ptr)++;					      \
	}								      \
    else								      \
      {									      \
	word##Bytes##_t *tdest = (word##Bytes##_t *) dest + n;		      \
	const word##Bytes##_t *tptr = (const word##Bytes##_t *) ptr + n;      \
	while (n-- > 0)							      \
	  {								      \
	    --tptr;							      \
	    *--tdest = LEN##Bytes##_SWAP (tptr);			      \
	  }								      \
      }									      \
 }									      \
									      \
  static inline void FName##1 (void *dest, const void *ptr)		      \
  {									      \
    *((word##Bytes##_t *) dest) =					      \
      LEN##Bytes##_SWAP ((((word##Bytes##_t *) ptr)));			      \
  }


/* Now the tricky part: define the transformation functions for the
   complex types.  We will use the definitions of the types in
   abstract.h.  */
#define START(Bits, Name, EName) \
  static void								      \
  ElfW2 (Bits, cvt_##Name) (void *dest, const void *src, size_t len,	      \
			    int encode)					      \
  { ElfW2(Bits, Name) *tdest = (ElfW2(Bits, Name) *) dest;		      \
    ElfW2(Bits, Name) *tsrc = (ElfW2(Bits, Name) *) src;		      \
    size_t n = len / sizeof (ElfW2(Bits, Name));			      \
    for (; n > 0; ++tdest, ++tsrc, --n) {
#define END(Bits, Name) } }
#define TYPE_EXTRA(Code)
#define TYPE_XLATE(Code) Code
#define TYPE_NAME(Type, Name) TYPE_NAME2 (Type, Name)
#define TYPE_NAME2(Type, Name) Type##1 (&tdest->Name, &tsrc->Name);
#define TYPE(Name, Bits) TYPE2 (Name, Bits)
#define TYPE2(Name, Bits) TYPE3 (Name##Bits)
#define TYPE3(Name) Name (cvt_)

/* Signal that we are generating conversion functions.  */
#define GENERATE_CONVERSION

/* First generate the 32-bit conversion functions.  */
#define LIBELFBITS 32
#include "gelf_xlate.h"

/* Now generate the 64-bit conversion functions.  */
#define LIBELFBITS 64
#include "gelf_xlate.h"


/* We have a few functions which we must create by hand since the sections
   do not contain records of only one type.  */
#include "version_xlate.h"


/* Now the externally visible table with the function pointers.  */
const xfct_t __elf_xfctstom[EV_NUM - 1][EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] =
{
  [EV_CURRENT - 1] = {
    [EV_CURRENT - 1] = {
      [ELFCLASS32 - 1] = {
#define define_xfcts(Bits) \
	[ELF_T_BYTE]	= elf_cvt_Byte,					      \
	[ELF_T_ADDR]	= ElfW2(Bits, cvt_Addr),			      \
	[ELF_T_DYN]	= ElfW2(Bits, cvt_Dyn),				      \
	[ELF_T_EHDR]	= ElfW2(Bits, cvt_Ehdr),			      \
	[ELF_T_HALF]	= ElfW2(Bits, cvt_Half),			      \
	[ELF_T_OFF]	= ElfW2(Bits, cvt_Off),				      \
	[ELF_T_PHDR]	= ElfW2(Bits, cvt_Phdr),			      \
	[ELF_T_RELA]	= ElfW2(Bits, cvt_Rela),			      \
	[ELF_T_REL]	= ElfW2(Bits, cvt_Rel),				      \
	[ELF_T_SHDR]	= ElfW2(Bits, cvt_Shdr),			      \
	[ELF_T_SWORD]	= ElfW2(Bits, cvt_Sword),			      \
	[ELF_T_SYM]	= ElfW2(Bits, cvt_Sym),				      \
	[ELF_T_WORD]	= ElfW2(Bits, cvt_Word),			      \
	[ELF_T_XWORD]	= ElfW2(Bits, cvt_Xword),			      \
	[ELF_T_SXWORD]	= ElfW2(Bits, cvt_Sxword),			      \
	[ELF_T_VDEF]	= elf_cvt_Verdef,				      \
	[ELF_T_VDAUX]	= elf_cvt_Verdef,				      \
	[ELF_T_VNEED]	= elf_cvt_Verneed,				      \
	[ELF_T_VNAUX]	= elf_cvt_Verneed,				      \
	[ELF_T_NHDR]	= ElfW2(Bits, cvt_Nhdr),			      \
	[ELF_T_SYMINFO] = ElfW2(Bits, cvt_Syminfo),			      \
	[ELF_T_MOVE]	= ElfW2(Bits, cvt_Move)
        define_xfcts (32)
      },
      [ELFCLASS64 - 1] = {
	define_xfcts (64)
      }
    }
  }
};

#if !defined(__LCLINT__)
/* For now we only handle the case where the memory representation is the
   same as the file representation.  Should this change we have to define
   separate functions.  For now reuse them.  */
strong_alias (__elf_xfctstom, __elf_xfctstof)
#endif
