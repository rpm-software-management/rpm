/* Create descriptor for assembling.
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gelf.h>
#include "libasmP.h"
#include <system.h>


static AsmCtx_t *
prepare_text_output (AsmCtx_t *result)
{
  return result;
}


static AsmCtx_t *
prepare_binary_output (AsmCtx_t *result, int machine, int klass, int data)
{
  GElf_Ehdr *ehdr;
  GElf_Ehdr ehdr_mem;

  /* Create the ELF descriptor for the file.  */
  result->out.elf = elf_begin (result->fd, ELF_C_WRITE_MMAP, NULL);
  if (result->out.elf == NULL)
    {
    err_libelf:
      unlink (result->tmp_fname);
      close (result->fd);
      free (result);
      __libasm_seterrno (ASM_E_LIBELF);
      return NULL;
    }

  /* Create the ELF header for the output file.  */
  if (gelf_newehdr (result->out.elf, klass) == 0)
    goto err_libelf;

  ehdr = gelf_getehdr (result->out.elf, &ehdr_mem);
  /* If this failed we are in trouble.  */
  assert (ehdr != NULL);

  /* We create an object file.  */
  ehdr->e_type = ET_REL;
  /* Set the ELF version.  */
  ehdr->e_version = EV_CURRENT;

  /* Use the machine value the user provided.  */
  ehdr->e_machine = machine;
  /* Same for the class and endianness.  */
  ehdr->e_ident[EI_CLASS] = klass;
  ehdr->e_ident[EI_DATA] = data;

  memcpy (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG);

  /* Write the ELF header information back.  */
  (void) gelf_update_ehdr (result->out.elf, ehdr);

  /* No section so far.  */
  result->section_list = NULL;

  /* Initialize the hash table.  */
  asm_symbol_tab_init (&result->symbol_tab, 67);
  /* And the string tables.  */
  result->section_strtab = ebl_strtabinit (true);
  result->symbol_strtab = ebl_strtabinit (true);

  /* We have no section groups so far.  */
  result->groups = NULL;
  result->ngroups = 0;

  return result;
}


AsmCtx_t *
asm_begin (fname, textp, machine, klass, data)
     const char *fname;
     bool textp;
     int machine;
     int klass;
     int data;
{
  size_t fname_len = strlen (fname);
  AsmCtx_t *result;


  /* First order of business: find the appropriate backend.  If it
     does not exist we don't have to look further.  */
  // XXX

  /* Create the file descriptor.  We do not generate the output file
     right away.  Instead we create a temporary file in the same
     directory which, if everything goes alright, will replace a
     possibly existing file with the given name.  */
  result = (AsmCtx_t *) xmalloc (sizeof (AsmCtx_t) + 2 * fname_len + 9);

  /* Initialize the lock.  */
  rwlock_init (result->lock);

  /* Create the name of the temporary file.  */
  result->fname = stpcpy (mempcpy (result->tmp_fname, fname, fname_len),
			  ".XXXXXX") + 1;
  memcpy (result->fname, fname, fname_len + 1);

  /* Create the temporary file.  */
  result->fd = mkstemp (result->tmp_fname);
  if (result->fd == -1)
    {
      int save_errno = errno;
      free (result);
      __libasm_seterrno (ASM_E_CANNOT_CREATE);
      errno = save_errno;
      return NULL;
    }

  /* Initialize the counter for temporary symbols.  */
  result->tempsym_count = 0;

  /* Now we differentiate between textual and binary output.   */
  result->textp = textp;
  if (textp)
    result = prepare_text_output (result);
  else
    result = prepare_binary_output (result, machine, klass, data);

  return result;
}
