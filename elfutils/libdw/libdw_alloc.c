/* Memory handling for libdw.
   Copyright (C) 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/param.h>
#include "libdwP.h"


void *
__libdw_allocate (Dwarf *dbg, size_t minsize)
{
  size_t size = MAX (dbg->mem_default_size,
		     2 * minsize + offsetof (struct libdw_memblock, mem));
  struct libdw_memblock *newp = malloc (size);
  if (newp == NULL)
    dbg->oom_handler ();

  newp->size = newp->remaining = size - offsetof (struct libdw_memblock, mem);

  newp->prev = dbg->mem_tail;
  newp->next = NULL;
  dbg->mem_tail = newp;

  return newp->mem;
}


Dwarf_OOM
dwarf_new_oom_handler (Dwarf *dbg, Dwarf_OOM handler)
{
  Dwarf_OOM old = dbg->oom_handler;
  dbg->oom_handler = handler;
  return old;
}


void
__attribute ((noreturn, visibility ("hidden")))
__libdw_oom (void)
{
  while (1)
    error (EXIT_FAILURE, ENOMEM, "libdw");
}
