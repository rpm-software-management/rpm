/* Return raw section content.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <stdlib.h>

#include "libelfP.h"


Elf_Data *
elf_rawdata (Elf_Scn *scn, Elf_Data *data)
{
  if (scn == NULL || scn->elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* If `data' is not NULL this means we are not addressing the initial
     data in the file.  But this also means this data is already read
     (since otherwise it is not possible to have a valid `data' pointer)
     and all the data structures are initialized as well.  In this case
     we can simply walk the list of data records.  */
  if (data != NULL
      || (scn->data_read != 0 && (scn->flags & ELF_F_FILEDATA) == 0))
    {
      /* We don't allow accessing any but the data read from the file
	 as raw.  */
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return NULL;
    }

  /* If the data for this section was not yet initialized do it now.  */
  if (scn->data_read == 0)
    {
      /* First thing we do is to read the data from the file.  There is
	 always a file (or memory region) associated with this descriptor
	 since otherwise the `data_read' flag would be set.  */
      if (__libelf_set_rawdata (scn) != 0)
	/* Something went wrong.  The error value is already set.  */
	return NULL;
    }

  /* Return the first data element in the list.  */
  return &scn->rawdata.d;
}
INTDEF(elf_rawdata)
