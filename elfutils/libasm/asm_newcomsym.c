/* Create new COMMON symbol.
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


/* Object for special COMMON section.  */
static const AsmScn_t __libasm_com_scn =
  {
    .data = {
      .main = {
	.scn = ASM_COM_SCN
      }
    }
  };


AsmSym_t *
asm_newcomsym (ctx, name, size, align)
     AsmCtx_t *ctx;
     const char *name;
     GElf_Xword size;
     GElf_Addr align;
{
  AsmSym_t *result;

  if (ctx == NULL)
    /* Something went wrong before.  */
    return NULL;

  /* Common symbols are public.  Therefore the user must provide a
     name.  */
  if (name == NULL)
    {
      __libasm_seterrno (ASM_E_INVALID);
      return NULL;
    }

  rwlock_wrlock (ctx->lock);

  result = (AsmSym_t *) xmalloc (sizeof (AsmSym_t));

  result->scn = (AsmScn_t *) &__libasm_com_scn;
  result->size = size;
  /* XXX Do we have to allow a different type?  */
  result->type = STT_OBJECT;
  /* XXX Do we have to allow a different binding?  */
  result->binding = STB_GLOBAL;
  result->symidx = 0;
  result->strent = ebl_strtabadd (ctx->symbol_strtab, name, 0);

  /* The value of a COM symbol is the alignment.  Since there are no
     subsection and the initial offset of the section is 0 we can get
     the alignment recorded by storing it into the offset field.  */
  result->offset = align;

  if (unlikely (ctx->textp))
    fprintf (ctx->out.file, "\t.comm %s, %" PRIuMAX ", %" PRIuMAX "\n",
	     name, (uintmax_t) size, (uintmax_t) align);
  else
    {
      /* Put the symbol in the hash table so that we can later find it.  */
      if (asm_symbol_tab_insert (&ctx->symbol_tab, elf_hash (name), result)
	  != 0)
	{
	  /* The symbol already exists.  */
	  __libasm_seterrno (ASM_E_DUPLSYM);
	  free (result);
	  result = NULL;
	}
      else if (name != NULL && asm_emit_symbol_p (name))
	/* Only count non-private symbols.  */
	++ctx->nsymbol_tab;
    }

  rwlock_unlock (ctx->lock);

  return result;
}
