/* Unaligned memory access functionality.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#ifndef _MEMORY_ACCESS_H
#define _MEMORY_ACCESS_H 1

#include <byteswap.h>
#include <stdint.h>


/* Number decoding macros.  See 7.6 Variable Length Data.  */
#define get_uleb128(var, addr) \
  do {									      \
    Dwarf_Small __b = *addr++;						      \
    var = __b & 0x7f;							      \
    if (__b & 0x80)							      \
      {									      \
	__b = *addr++;							      \
	var |= (__b & 0x7f) << 7;					      \
	if (__b & 0x80)							      \
	  {								      \
	    __b = *addr++;						      \
	    var |= (__b & 0x7f) << 14;					      \
	    if (__b & 0x80)						      \
	      {								      \
		__b = *addr++;						      \
		var |= (__b & 0x7f) << 21;				      \
		if (__b & 0x80)						      \
		  /* Other implementation set VALUE to UINT_MAX in this	      \
		     case.  So we better do this as well.  */		      \
		  var = UINT_MAX;					      \
	      }								      \
	  }								      \
      }									      \
  } while (0)

/* The signed case is a big more complicated.  */
#define get_sleb128(var, addr) \
  do {									      \
    Dwarf_Small __b = *addr++;						      \
    int32_t __res = __b & 0x7f;						      \
    if ((__b & 0x80) == 0)						      \
      {									      \
	if (__b & 0x40)							      \
	  __res |= 0xffffff80;						      \
      }									      \
    else								      \
      {									      \
	__b = *addr++;							      \
	__res |= (__b & 0x7f) << 7;					      \
	if ((__b & 0x80) == 0)						      \
	  {								      \
	    if (__b & 0x40)						      \
	      __res |= 0xffffc000;					      \
	  }								      \
	else								      \
	  {								      \
	    __b = *addr++;						      \
	    __res |= (__b & 0x7f) << 14;				      \
	    if ((__b & 0x80) == 0)					      \
	      {								      \
		if (__b & 0x40)						      \
		  __res |= 0xffe00000;					      \
	      }								      \
	    else							      \
	      {								      \
		__b = *addr++;						      \
		__res |= (__b & 0x7f) << 21;				      \
		if ((__b & 0x80) == 0)					      \
		  {							      \
		    if (__b & 0x40)					      \
		      __res |= 0xf0000000;				      \
		  }							      \
		else							      \
		  /* Other implementation set VALUE to INT_MAX in this	      \
		     case.  So we better do this as well.  */		      \
		  __res = INT_MAX;					      \
	      }								      \
	  }								      \
      }									      \
    var = __res;							      \
  } while (0)


/* We use simple memory access functions in case the hardware allows it.
   The caller has to make sure we don't have alias problems.  */
#if ALLOW_UNALIGNED

# define read_2ubyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? bswap_16 (*((uint16_t *) (Addr)))					      \
   : *((uint16_t *) (Addr)))
# define read_2sbyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? (int16_t) bswap_16 (*((int16_t *) (Addr)))				      \
   : *((int16_t *) (Addr)))

# define read_4ubyte_unaligned_noncvt(Addr) \
   *((uint32_t *) (Addr))
# define read_4ubyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? bswap_32 (*((uint32_t *) (Addr)))					      \
   : *((uint32_t *) (Addr)))
# define read_4sbyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? (int32_t) bswap_32 (*((int32_t *) (Addr)))				      \
   : *((int32_t *) (Addr)))

# define read_8ubyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? bswap_64 (*((uint64_t *) (Addr)))					      \
   : *((uint64_t *) (Addr)))
# define read_8sbyte_unaligned(Dbg, Addr) \
  ((Dbg)->other_byte_order						      \
   ? (int64_t) bswap_64 (*((int64_t *) (Addr)))				      \
   : *((int64_t *) (Addr)))

#else

# if __GNUC__

union unaligned
  {
    void *p;
    uint16_t u2;
    uint32_t u4;
    uint64_t u8;
    int16_t s2;
    int32_t s4;
    int64_t s8;
  } __attribute__ ((packed));

static inline uint16_t
read_2ubyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return bswap_16 (up->u2);
  return up->u2;
}
static inline int16_t
read_2sbyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return (int16_t) bswap_16 (up->u2);
  return up->s2;
}

static inline uint32_t
read_4ubyte_unaligned_noncvt (void *p)
	/*@*/
{
  union unaligned *up = p;
  return up->u4;
}
static inline uint32_t
read_4ubyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return bswap_32 (up->u4);
  return up->u4;
}
static inline int32_t
read_4sbyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return (int32_t) bswap_32 (up->u4);
  return up->s4;
}

static inline uint64_t
read_8ubyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return bswap_64 (up->u8);
  return up->u8;
}
static inline int64_t
read_8sbyte_unaligned (Dwarf_Debug dbg, void *p)
	/*@*/
{
  union unaligned *up = p;
  if (dbg->other_byte_order)
    return (int64_t) bswap_64 (up->u8);
  return up->s8;
}

# else
#  error "TODO"
# endif

#endif

#endif	/* memory-access.h */
