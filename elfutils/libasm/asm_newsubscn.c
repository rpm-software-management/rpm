/* Create new subsection section in given section.
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

#include <libasmP.h>
#include <system.h>


AsmScn_t *
asm_newsubscn (asmscn, nr)
     AsmScn_t *asmscn;
     unsigned int nr;
{
  AsmScn_t *runp;
  AsmScn_t *newp;

  /* Just return if no section is given.  The error must have been
     somewhere else.  */
  if (asmscn == NULL)
    return NULL;

  /* Determine whether there is already a subsection with this number.  */
  runp = asmscn->subsection_id == 0 ? asmscn : asmscn->data.up;
  while (1)
    {
      if (runp->subsection_id == nr)
	/* Found it.  */
	return runp;

      if (runp->subnext == NULL || runp->subnext->subsection_id > nr)
	break;

      runp = runp->subnext;
    }

  newp = (AsmScn_t *) xmalloc (sizeof (AsmScn_t));

  /* Same assembler context than the original section.  */
  newp->ctx = runp->ctx;

  /* User provided the subsectio nID.  */
  newp->subsection_id = nr;

  /* Pointer to the zeroth subsection.  */
  newp->data.up = runp->subsection_id == 0 ? runp : runp->data.up;

  /* We start at offset zero.  */
  newp->offset = 0;
  /* And generic alignment.  */
  newp->max_align = 1;

  /* No output yet.  */
  newp->content = NULL;

  /* Inherit the fill pattern from the section this one is derived from.  */
  newp->pattern = asmscn->pattern;

  /* Enqueue at the right position in the list.  */
  newp->subnext = runp->subnext;
  runp->subnext = newp;

  return newp;
}
