/* Compute simple checksum from permanent parts of the ELF file.
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

#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gelf.h"
#include "libelfP.h"
#include "../libebl/elf-knowledge.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


/* The SECTION_STRIP_P macro wants to call into libebl which we cannot
   do and do not have to do here.  Provide a dummy replacement.  */
#define ebl_debugscn_p(ebl, name) true


#define process_block(crc, data) \
  __libelf_crc32 (crc, data->d_buf, data->d_size)


long int
elfw2(LIBELFBITS,checksum) (Elf *elf)
{
  size_t shstrndx;
  Elf_Scn *scn;
  long int result = 0;
  char *ident;
  bool same_byte_order;

  if (elf == NULL)
    return -1l;

  /* Find the section header string table.  */
  if  (INTUSE(elf_getshstrndx) (elf, &shstrndx) < 0)
    {
      /* This can only happen if the ELF handle is not for real.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1l;
    }

  /* Determine whether the byte order of the file and that of the host
     is the same.  */
  ident = elf->state.ELFW(elf,LIBELFBITS).ehdr->e_ident;
  same_byte_order = ((ident[EI_DATA] == ELFDATA2LSB
		      && __BYTE_ORDER == __LITTLE_ENDIAN)
		     || (ident[EI_DATA] == ELFDATA2MSB
			 && __BYTE_ORDER == __BIG_ENDIAN));

  /* Iterate over all sections to find those which are not strippable.  */
  scn = NULL;
  while ((scn = INTUSE(elf_nextscn) (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;
      Elf_Data *data;

      /* Get the section header.  */
      shdr = INTUSE(gelf_getshdr) (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
	  return -1l;
	}

      if (SECTION_STRIP_P (NULL, NULL, NULL, shdr,
			   elf_strptr (elf, shstrndx, shdr->sh_name),
			   true, false))
	/* The section can be stripped.  Don't use it.  */
	continue;

      /* To compute the checksum we need to get to the data.  For
	 repeatable results we must use the external format.  The data
	 we get with 'elf'getdata' might be changed for endianess
	 reasons.  Therefore we use 'elf_rawdata' if possible.  But
	 this function can fail if the data was constructed by the
	 program.  In this case we have to use 'elf_getdata' and
	 eventually convert the data to the external format.  */
      data = INTUSE(elf_rawdata) (scn, NULL);
      if (data != NULL)
	{
	  /* The raw data is available.  */
	  result = process_block (result, data);

	  /* Maybe the user added more data.  These blocks cannot be
	     read using 'elf_rawdata'.  Simply proceed with looking
	     for more data block with 'elf_getdata'.  */
	}

      /* Iterate through the list of data blocks.  */
      while ((data = INTUSE(elf_getdata) (scn, data)) != NULL)
	/* If the file byte order is the same as the host byte order
	   process the buffer directly.  If the data is just a stream
	   of bytes which the library will not convert we can use it
	   as well.  */
	if (likely (same_byte_order) || data->d_type == ELF_T_BYTE)
	  result = process_block (result, data);
	else
	  {
	    /* Convert the data to file byte order.  */
	    if (INTUSE(elfw2(LIBELFBITS,xlatetof)) (data, data, ident[EI_DATA])
		== NULL)
	      return -1l;

	    result = process_block (result, data);

	    /* And convert it back.  */
	    if (INTUSE(elfw2(LIBELFBITS,xlatetom)) (data, data, ident[EI_DATA])
		== NULL)
	      return -1l;
	  }
    }

  return result;
}
INTDEF(elfw2(LIBELFBITS,checksum))
