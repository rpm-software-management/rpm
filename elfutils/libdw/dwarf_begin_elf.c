/* Create descriptor from ELF descriptor for processing file.
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

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libdwP.h"


/* Section names.  */
static const char dwarf_scnnames[IDX_last][17] =
{
  [IDX_debug_info] = ".debug_info",
  [IDX_debug_abbrev] = ".debug_abbrev",
  [IDX_debug_aranges] = ".debug_aranges",
  [IDX_debug_line] = ".debug_line",
  [IDX_debug_frame] = ".debug_frame",
  [IDX_eh_frame] = ".eh_frame",
  [IDX_debug_loc] = ".debug_loc",
  [IDX_debug_pubnames] = ".debug_pubnames",
  [IDX_debug_str] = ".debug_str",
  [IDX_debug_funcnames] = ".debug_funcnames",
  [IDX_debug_typenames] = ".debug_typenames",
  [IDX_debug_varnames] = ".debug_varnames",
  [IDX_debug_weaknames] = ".debug_weaknames",
  [IDX_debug_macinfo] = ".debug_macinfo"
};
#define ndwarf_scnnames (sizeof (dwarf_scnnames) / sizeof (dwarf_scnnames[0]))


static void
check_section (Dwarf *result, GElf_Ehdr *ehdr, Elf_Scn *scn, bool inscngrp)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  const char *scnname;
  int cnt;

  /* Get the section header data.  */
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    /* This should never happen.  If it does something is
       wrong in the libelf library.  */
    abort ();


  /* Make sure the section is part of a section group only iff we
     really need it.  If we are looking for the global (= non-section
     group debug info) we have to ignore all the info in section
     groups.  If we are looking into a section group we cannot look at
     a section which isn't part of the section group.  */
  if (! inscngrp && (shdr->sh_flags & SHF_GROUP) != 0)
    /* Ignore the section.  */
    return;


  /* We recognize the DWARF section by their names.  This is not very
     safe and stable but the best we can do.  */
  scnname = elf_strptr (result->elf, ehdr->e_shstrndx,
				    shdr->sh_name);
  if (scnname == NULL)
    {
      /* The section name must be valid.  Otherwise is the ELF file
	 invalid.  */
      __libdwarf_seterrno (DWARF_E_INVALID_ELF);
      free (result);
      return;
    }


  /* Recognize the various sections.  Most names start with .debug_.  */
  for (cnt = 0; cnt < ndwarf_scnnames; ++cnt)
    if (strcmp (scnname, dwarf_scnnames[cnt]) == 0)
      {
	Elf_Data *data;
	/* Found it.  Remember where the data is.  */
	if (unlikely (result->sectiondata[cnt] != NULL))
	  /* A section appears twice.  That's bad.  We ignore the section.  */
	  break;

	/* Get the section data.  */
	data = elf_getdata (scn, NULL);
	if (data != NULL && data->d_size != 0)
	  /* Yep, there is actually data available.  */
	  result->sectiondata[cnt] = data;

	break;
      }
}


/* Check whether all the necessary DWARF information is available.  */
static Dwarf *
valid_p (Dwarf *result)
{
  /* We looked at all the sections.  Now determine whether all the
     sections with debugging information we need are there.

     XXX Which sections are absolutely necessary?  Add tests if
     necessary.  For now we require only .debug_info.  Hopefully this
     is correct.  */
  if (unlikely (result->sectiondata[IDX_debug_info] == NULL))
    {
      __libdwarf_seterrno (DWARF_E_NO_DWARF);
      result = NULL;
    }

  return result;
}


static Dwarf *
global_read (Dwarf *result, Elf *elf, GElf_Ehdr *ehdr, Dwarf_Cmd cmd)
{
  Elf_Scn *scn = NULL;

  while ((scn = elf_nextscn (elf, scn)) != NULL)
    check_section (result, ehdr, scn, false);

  return valid_p (result);
}


static Dwarf *
scngrp_read (Dwarf *result, Elf *elf, GElf_Ehdr *ehdr, Dwarf_Cmd cmd,
	     Elf_Scn *scngrp)
{
  Elf32_Word *scnidx;
  size_t cnt;
  /* SCNGRP is the section descriptor for a section group which might
     contain debug sections.  */
  Elf_Data *data = elf_getdata (scngrp, NULL);
  if (data == NULL)
    {
      /* We cannot read the section content.  Fail!  */
      free (result);
      return NULL;
    }

  /* The content of the section is a number of 32-bit words which
     represent section indices.  The first word is a flag word.  */
  scnidx = (Elf32_Word *) data->d_buf;
  for (cnt = 1; cnt * sizeof (Elf32_Word) <= data->d_size; ++cnt)
    {
      Elf_Scn *scn = elf_getscn (elf, scnidx[cnt]);
      if (scn == NULL)
	{
	  /* A section group refers to a non-existing section.  Should
	     never happen.  */
	  __libdwarf_seterrno (DWARF_E_INVALID_ELF);
	  free (result);
	  return NULL;
	}

      check_section (result, ehdr, scn, true);
    }

  return valid_p (result);
}


Dwarf *
dwarf_begin_elf (elf, cmd, scngrp)
     Elf *elf;
     Dwarf_Cmd cmd;
     Elf_Scn *scngrp;
{
  GElf_Ehdr *ehdr;
  GElf_Ehdr ehdr_mem;
  Dwarf *result;

  /* Get the ELF header of the file.  We need various pieces of
     information from it.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      if (elf_kind (elf) != ELF_K_ELF)
	__libdwarf_seterrno (DWARF_E_NOELF);
      else
	__libdwarf_seterrno (DWARF_E_GETEHDR_ERROR);

      return NULL;
    }


  /* Allocate the data structure.  */
  result = (Dwarf *) calloc (1, sizeof (Dwarf));
  if (result == NULL)
    {
      __libdwarf_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  /* Fill in some values.  */
  if ((BYTE_ORDER == LITTLE_ENDIAN && ehdr->e_ident[EI_DATA] == ELFDATA2MSB)
      || (BYTE_ORDER == BIG_ENDIAN && ehdr->e_ident[EI_DATA] == ELFDATA2LSB))
    result->other_byte_order = true;

  result->elf = elf;


  if (cmd == DWARF_C_READ || cmd == DWARF_C_RDWR)
    {
      /* If the caller provides a section group we get the DWARF
	 sections only from this setion group.  Otherwise we search
	 for the first section with the required name.  Further
	 sections with the name are ignored.  The DWARF specification
	 does not really say this is allowed.  */
      if (scngrp == NULL)
	return global_read (result, elf, ehdr, cmd);
      else
	return scngrp_read (result, elf, ehdr, cmd, scngrp);
    }
  else if (cmd == DWARF_C_WRITE)
    {
      __libdwarf_seterrno (DWARF_E_UNIMPL);
      free (result);
      return NULL;
    }

  __libdwarf_seterrno (DWARF_E_INVALID_CMD);
  free (result);
  return NULL;
}
