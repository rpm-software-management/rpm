/* Create descriptor from ELF handle for processing file.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <endian.h>
#include <gelf.h>
#include <stdlib.h>
#include <string.h>

#include <libdwarfP.h>


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


int
dwarf_elf_init (elf, access, errhand, errarg, dbg, error)
     Elf *elf;
     Dwarf_Unsigned access;
     Dwarf_Handler errhand;
     Dwarf_Ptr errarg;
     Dwarf_Debug *dbg;
     Dwarf_Error *error;
{
  int result = DW_DLV_ERROR;
  Dwarf_Debug newdbg;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  Elf_Scn *scn;

  /* XXX For now nothing but read-only support is available.  */
  if (access != DW_DLC_READ)
    abort ();

  /* Get the ELF header of the file.  We need various pieces of
     information from it.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      struct Dwarf_Debug_s tmpdbg;

      tmpdbg.dbg_errhand = errhand;
      tmpdbg.dbg_errarg = errarg;

      if (elf_kind (elf) != ELF_K_ELF)
	__libdwarf_error (&tmpdbg, error, DW_E_NOELF);
      else
	__libdwarf_error (&tmpdbg, error, DW_E_GETEHDR_ERROR);

      return result;
    }

  /* Allocate and fill the result data structure.  */
  newdbg = (Dwarf_Debug) calloc (1, sizeof (struct Dwarf_Debug_s));
  if (newdbg == NULL)
    {
      struct Dwarf_Debug_s tmpdbg;

      tmpdbg.dbg_errhand = errhand;
      tmpdbg.dbg_errarg = errarg;

      __libdwarf_error (&tmpdbg, error, DW_E_NOMEM);
    }
  else
    {
      /* We have been able to allocate the memory for the debug handle.  */
      newdbg->dbg_errhand = errhand;
      newdbg->dbg_errarg = errarg;
      newdbg->elf = elf;
      if ((BYTE_ORDER == LITTLE_ENDIAN
	   && ehdr->e_ident[EI_DATA] == ELFDATA2MSB)
	  || (BYTE_ORDER == BIG_ENDIAN
	      && ehdr->e_ident[EI_DATA] == ELFDATA2LSB))
	newdbg->other_byte_order = 1;
      newdbg->access = access;
#ifdef DWARF_DEBUG
      newdbg->memtag = DW_DLA_DEBUG;
#endif

      /* All done.  */
      *dbg = newdbg;
      result = DW_DLV_OK;

      /* Find out whether the file actually has the necessary DWARF
         sections.  */
      scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr;
	  const char *scnname;
	  int cnt;
	  Elf_Data *data;

	  /* Get the section header data.  */
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    /* This should never happen.  If it does something is
	       wrong in the libelf library.  */
	    abort ();

	  /* We recognize the DWARF section by their names.  This is
	     not very safe and stable but the best we can do.  */
	  scnname = elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name);
	  if (scnname == NULL)
	    {
	      /* The section name must be valid.  Otherwise is the ELF file
		 invalid.  */
	      __libdwarf_error (newdbg, error, DW_E_INVALID_ELF);
	      break;
	    }

	  /* Recognize the various sections.  Most names start with
             .debug_.  */
	  for (cnt = 0; cnt < ndwarf_scnnames; ++cnt)
	    if (strcmp (scnname, dwarf_scnnames[cnt]) == 0)
	      break;

	  if (cnt < ndwarf_scnnames)
	    {
	      /* Found it.  Remember where the data is.  */
	      assert (cnt < IDX_last);
	      if (unlikely (newdbg->sections[cnt].addr != NULL))
		{
		  /* A section appears twice.  That's bad.  */
		  __libdwarf_error (newdbg, error, DW_E_INVALID_DWARF);
		  break;
		}

	      /* Get the section data.  */
	      data = elf_getdata (scn, NULL);
	      if (data != NULL && data->d_size != 0)
		{
		  /* Yep, there is actually data available.  */
		  newdbg->sections[cnt].addr = data->d_buf;
		  newdbg->sections[cnt].size = data->d_size;
		}
	    }
	}

      if (scn == NULL)
	{
	  /* We looked at all the sections.  Now determine whether all
	     the sections with debugging information we need are there.

	     XXX Which sections are absolutely necessary?  Add tests
	     if necessary.  For now we require only .debug_info.  Hopefully
	     this is correct.  */
	  if (newdbg->sections[IDX_debug_info].addr == NULL)
	    {
	      __libdwarf_error (newdbg, error, DW_E_NO_DWARF);
	      result = DW_DLV_NO_ENTRY;
	    }
	  else
	    result = DW_DLV_OK;
	}

      if (result != DW_DLV_OK)
	/* Something went wrong.  */
	free (newdbg);
    }

  return result;
}
