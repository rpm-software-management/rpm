/* Add integer to a section.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <inttypes.h>
#include <string.h>

#include "libasmP.h"


int
asm_adduleb128 (asmscn, num)
     AsmScn_t *asmscn;
     uint32_t num;
{
  if (asmscn == NULL)
    return -1;

  if (asmscn->type == SHT_NOBITS && unlikely (num != 0))
    {
      __libasm_seterrno (ASM_E_TYPE);
      return -1;
    }

  if (unlikely (asmscn->ctx->textp))
    printf ("\t.uleb128\t%" PRIu32 "\n", num);
  else
    {
      char tmpbuf[(sizeof (num) * 8 + 6) / 7];
      char *dest = tmpbuf;
      uint32_t byte;
      size_t nbytes;

      while (1)
	{
	  byte = num & 0x7f;

	  num >>= 7;
	  if (num == 0)
	    /* This is the last byte.  */
	    break;

	  *dest++ = byte | 0x80;
	}

      *dest++ = byte;

      /* Number of bytes produced.  */
      nbytes = dest - tmpbuf;

      /* Make sure we have enough room.  */
      if (__libasm_ensure_section_space (asmscn, nbytes) != 0)
	return -1;

      /* Copy the bytes.  */
      if (likely (asmscn->type != SHT_NOBITS))
	memcpy (&asmscn->content->data[asmscn->content->len], tmpbuf, nbytes);

      /* Adjust the pointer in the data buffer.  */
      asmscn->content->len += nbytes;

      /* Increment the offset in the (sub)section.  */
      asmscn->offset += nbytes;
    }

  return 0;
}
