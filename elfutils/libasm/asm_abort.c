/* Abort operations on the assembler context, free all resources.
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
#include <unistd.h>

#include <libasmP.h>
#include <libelf.h>


int
asm_abort (ctx)
     AsmCtx_t *ctx;
{
  if (ctx == NULL)
    /* Something went wrong earlier.  */
    return -1;

  if (likely (! ctx->textp))
    /* First free the ELF file.  We don't care about the result.  */
    (void) elf_end (ctx->out.elf);

  /* Now close the temporary file and remove it.  */
  (void) unlink (ctx->tmp_fname);

  /* Free the resources.  */
  __libasm_finictx (ctx);

  return 0;
}
