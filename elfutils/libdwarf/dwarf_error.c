/* Handle error.
   Copyright (C) 2000, 2002 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>

#include <libdwarfP.h>
#include <system.h>


void
internal_function
__libdwarf_error (Dwarf_Debug dbg, Dwarf_Error *error, int errval)
{
  /* Allocate memory for the error structure given to the user.  */
  Dwarf_Error errmem = (Dwarf_Error) xmalloc (sizeof (*error));

  errmem->de_error = errval;

  /* DBG must never be NULL.  */
  assert (dbg != NULL);

  if (error != NULL)
    /* If the user provides an ERROR parameter we have to use it.  */
    *error = errmem;
  else if (likely (dbg->dbg_errhand != NULL))
    /* Use the handler the user provided if possible.  */
    dbg->dbg_errhand (error, dbg->dbg_errarg);
  else
    {
      assert (! "error and dbg->dbg_errhand == NULL");
      abort ();
    }
}
