/* Create new ABS symbol.
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
static const AsmScn_t __libasm_abs_scn =
  {
    .data = {
      .main = {
	.scn = ASM_ABS_SCN
      }
    }
  };


AsmSym_t *
asm_newabssym (ctx, name, size, value, type, binding)
     AsmCtx_t *ctx;
     const char *name;
     GElf_Xword size;
     GElf_Addr value;
     int type;
     int binding;
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

  result->scn = (AsmScn_t *) &__libasm_abs_scn;
  result->size = size;
  result->type = type;
  result->binding = binding;
  result->symidx = 0;
  result->strent = ebl_strtabadd (ctx->symbol_strtab, name, 0);

  /* The value of an ABS symbol must not be modified.  Since there are
     no subsection and the initial offset of the section is 0 we can
     get the alignment recorded by storing it into the offset
     field.  */
  result->offset = value;

  if (unlikely (ctx->textp))
    {
      /* There is no general pseudo-op for this.  We can generate
	 symbols of type STT_FUNC but nothing else.  */
      if (type == STT_FUNC)
	fprintf (ctx->out.file, "\t.file \"%s\"\n", name);
    }
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
