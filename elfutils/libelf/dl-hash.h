/* Compute hash value for given string according to ELF standard.
   Copyright (C) 1995, 1996, 1997, 1998, 2002 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1995.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _DL_HASH_H
#define _DL_HASH_H	1


/* This is the hashing function specified by the ELF ABI.  In the
   first five operations no overflow is possible so we optimized it a
   bit.  */
static inline unsigned int
__attribute__ ((__pure__))
_dl_elf_hash (const char *name)
	/*@*/
{
  unsigned int hash = (unsigned int) *((const unsigned char *) name)++;
  if (*name != '\0')
    {
      hash = ((hash << 4)
	      + (unsigned int) *((const unsigned char *) name)++);
      if (*name != '\0')
	{
	  hash = ((hash << 4)
		  + (unsigned int) *((const unsigned char *) name)++);
	  if (*name != '\0')
	    {
	      hash = ((hash << 4)
		      + (unsigned int) *((const unsigned char *) name)++);
	      if (*name != '\0')
		{
		  hash = ((hash << 4)
			  + (unsigned int) *((const unsigned char *) name)++);
		  while (*name != '\0')
		    {
		      unsigned int hi;
		      hash = ((hash << 4)
			      + (unsigned int) *((const unsigned char *) name)++);
		      hi = hash & 0xf0000000;

		      /* The algorithm specified in the ELF ABI is as
			 follows:

			 if (hi != 0)
			 hash ^= hi >> 24;

			 hash &= ~hi;

			 But the following is equivalent and a lot
			 faster, especially on modern processors.  */

		      hash ^= hi;
		      hash ^= hi >> 24;
		    }
		}
	    }
	}
    }
  return hash;
}

#endif /* dl-hash.h */
