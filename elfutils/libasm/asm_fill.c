/* Determine fill pattern for a section.
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

#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


int
asm_fill (asmscn, bytes, len)
     AsmScn_t *asmscn;
     void *bytes;
     size_t len;
{
  struct FillPattern *pattern;
  struct FillPattern *old_pattern;

  if (asmscn == NULL)
    /* Some earlier error.  */
    return -1;

  if (bytes == NULL)
    /* Use the default pattern.  */
    pattern = (struct FillPattern *) __libasm_default_pattern;
  else
    {
      /* Allocate appropriate memory.  */
      pattern = (struct FillPattern *) xmalloc (sizeof (struct FillPattern)
						+ len);

      pattern->len = len;
      memcpy (pattern->bytes, bytes, len);
    }

  old_pattern = asmscn->pattern;
  asmscn->pattern = pattern;

  /* Free the old data structure if we have allocated it.  */
  if (old_pattern != __libasm_default_pattern)
    free (old_pattern);

  return 0;
}
