/* Get frame descriptions.  GCC version using .eh_frame.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <string.h>

#include <libdwarfP.h>


struct cielist
{
  Dwarf_Cie cie;
  struct cielist *next;
};


struct fdelist
{
  Dwarf_Fde fde;
  Dwarf_Small *cie_id_ptr;
  struct fdelist *next;
};


int
dwarf_get_fde_list_eh (dbg, cie_data, cie_element_count, fde_data,
		       fde_element_count, error)
     Dwarf_Debug dbg;
     Dwarf_Cie **cie_data;
     Dwarf_Signed *cie_element_count;
     Dwarf_Fde **fde_data;
     Dwarf_Signed *fde_element_count;
     Dwarf_Error *error;
{
  Dwarf_Small *readp;
  Dwarf_Small *readendp;
  struct cielist *cielist = NULL;
  struct cielist *copy_cielist;
  unsigned int ncielist = 0;
  struct fdelist *fdelist = NULL;
  unsigned int nfdelist = 0;

  if (dbg->sections[IDX_eh_frame].addr == NULL)
    return DW_DLV_NO_ENTRY;

  readp = (Dwarf_Small *) dbg->sections[IDX_eh_frame].addr;
  readendp = readp + dbg->sections[IDX_eh_frame].size;

  while (readp < readendp)
    {
      /* Each CIE contains the following:

	 1. CIE_length (initial length)

	 A constant that gives the number of bytes of the CIE
	 structure, not including the length field, itself [...].

	 2. CIE_id

	 A constant that is used to distinguish CIEs from FDEs.

	 3. version (ubyte) [...]

	 4. augmentation (array of ubyte)

	 A null-terminated string that identifies the augmentation to
	 this CIE or to the FDEs that use it.

	 5. code_alignment_factor (unsigned LEB128)

	 A constant that is factored out of all advance location
	 instructions (see below).

	 6. data_alignment_factor (signed LEB128)

	 A constant that is factored out of all offset instructions
	 [...].

	 7. return_address_register (ubyte)

	 A constant that indicates which column in the rule table
	 represents the return address of the function.

	 8. initial_instructions (array of ubyte) [...] */
      Dwarf_Small *fde_cie_start;
      Dwarf_Small *readstartp;
      Dwarf_Small *cie_id_ptr;
      Dwarf_Unsigned length;
      unsigned int address_size;
      Dwarf_Unsigned start_offset;
      Dwarf_Unsigned cie_id;

      /* Remember where this entry started.  */
      fde_cie_start = readp;
      start_offset = (readp
		      - (Dwarf_Small *) dbg->sections[IDX_eh_frame].addr);

      length = read_4ubyte_unaligned (dbg, readp);
      address_size = 4;
      readp += 4;
      if (length == 0xffffffff)
	{
	  length = read_8ubyte_unaligned (dbg, readp);
	  readp += 8;
	  address_size = 8;
	}
      readstartp = readp;

      /* Requirement from the DWARF specification.  */
      if (unlikely (length % address_size != 0))
	{
	  /* XXX Free resources.  */
	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      /* No more entries.  */
      if (length == 0)
	break;

      cie_id_ptr = readp;
      if (address_size == 4)
	{
	  cie_id = read_4sbyte_unaligned (dbg, readp);
	  readp += 4;
	}
      else
	{
	  cie_id = read_8sbyte_unaligned (dbg, readp);
	  readp += 8;
	}

      /* Now we can distinguish between CIEs and FDEs.  gcc uses 0 to
	 signal the record is a CIE.  */
      if (cie_id == 0)
	{
	  char *augmentation;
	  Dwarf_Unsigned code_alignment_factor;
	  Dwarf_Signed data_alignment_factor;
	  Dwarf_Small *initial_instructions;
	  Dwarf_Small return_address_register;
	  struct cielist *new_cie;

	  if (unlikely (*readp++ != CIE_VERSION))
	    {
	      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	      return DW_DLV_ERROR;
	    }

	  augmentation = (char *) readp;
	  readp += strlen (augmentation) + 1;

	  if (strcmp (augmentation, "") == 0)
	    {
	      get_uleb128 (code_alignment_factor, readp);
	      get_sleb128 (data_alignment_factor, readp);
	      return_address_register = *readp++;
	      initial_instructions = readp;
	    }
	  else if (strcmp (augmentation, "eh") == 0)
	    {
	      /* GCC exception handling.  It has an extra field next
		 which is the address of a exception table.  We ignore
		 this value since it's only used at runtime by the
		 exception handling.  */
	      readp += address_size;

	      /* Now the standard fields.  */
	      get_uleb128 (code_alignment_factor, readp);
	      get_sleb128 (data_alignment_factor, readp);
	      return_address_register = *readp++;
	      initial_instructions = readp;
	    }
	  else
	    {
	      /* We don't know this augmentation.  Skip the rest.  The
		 specification says that nothing after the augmentation
		 string is usable.  */
	      code_alignment_factor = 0;
	      data_alignment_factor = 0;
	      return_address_register = 0;
	      initial_instructions = NULL;
	    }

	  /* Go to the next record.  */
	  readp = readstartp + length;

	  /* Create the new CIE record.  */
	  new_cie = (struct cielist *) alloca (sizeof (struct cielist));
	  new_cie->cie = (Dwarf_Cie) malloc (sizeof (struct Dwarf_Cie_s));
	  if (new_cie->cie == NULL)
	    {
	      __libdwarf_error (dbg, error, DW_E_NOMEM);
	      return DW_DLV_ERROR;
	    }

	  new_cie->cie->dbg = dbg;
	  new_cie->cie->length = length;
	  new_cie->cie->augmentation = augmentation;
	  new_cie->cie->code_alignment_factor = code_alignment_factor;
	  new_cie->cie->data_alignment_factor = data_alignment_factor;
	  new_cie->cie->return_address_register = return_address_register;
	  new_cie->cie->initial_instructions = initial_instructions;
	  new_cie->cie->initial_instructions_length =
	    readp - initial_instructions;

	  new_cie->cie->offset = start_offset;
	  new_cie->cie->index = ncielist;
	  new_cie->next = cielist;
	  cielist = new_cie;
	  ++ncielist;
	}
      else
	{
	  Dwarf_Addr initial_location;
	  Dwarf_Unsigned address_range;
	  Dwarf_Small *instructions;
	  struct fdelist *new_fde;
	  struct cielist *cie;

	  if (address_size == 4)
	    {
	      initial_location = read_4ubyte_unaligned (dbg, readp);
	      readp += 4;
	      address_range = read_4ubyte_unaligned (dbg, readp);
	      readp += 4;
	    }
	  else
	    {
	      initial_location = read_8ubyte_unaligned (dbg, readp);
	      readp += 8;
	      address_range = read_8ubyte_unaligned (dbg, readp);
	      readp += 8;
	    }

	  instructions = readp;

	  /* Go to the next record.  */
	  readp = readstartp + length;

	  /* Create the new FDE record.  */
	  new_fde = (struct fdelist *) alloca (sizeof (struct fdelist));
	  new_fde->fde = (Dwarf_Fde) malloc (sizeof (struct Dwarf_Fde_s));
	  if (new_fde->fde == NULL)
	    {
	      __libdwarf_error (dbg, error, DW_E_NOMEM);
	      return DW_DLV_ERROR;
	    }

	  new_fde->fde->initial_location = initial_location;
	  new_fde->fde->address_range = address_range;
	  new_fde->fde->instructions = instructions;
	  new_fde->fde->instructions_length = readp - instructions;
	  new_fde->fde->fde_bytes = fde_cie_start;
	  new_fde->fde->fde_byte_length = readstartp + length - fde_cie_start;
	  new_fde->fde->cie = NULL;
	  new_fde->cie_id_ptr = cie_id_ptr;

	  for (cie = cielist; cie != NULL; cie = cie->next)
	    /* This test takes the non-standard way of using the CIE ID
	       in the GNU .eh_frame sectio into account.  Instead of being
	       a direct offset in the section it is a offset from the
	       location of the FDE'S CIE ID value itself to the CIE entry.  */
	    if (cie->cie->offset
		== (size_t) (cie_id_ptr - cie_id
			     - (Dwarf_Small *) dbg->sections[IDX_eh_frame].addr))
	      {
		new_fde->fde->cie = cie->cie;
		break;
	      }

	  new_fde->fde->offset = cie_id;
	  new_fde->next = fdelist;
	  fdelist = new_fde;
	  ++nfdelist;
	}
    }

  /* There must always be at least one CIE.  */
  if (unlikely (ncielist == 0))
    {
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  /* Create the lists.  */
  *cie_data = (Dwarf_Cie *) malloc (ncielist * sizeof (struct Dwarf_Cie_s));
  if (nfdelist > 0)
    *fde_data = (Dwarf_Fde *) malloc (nfdelist * sizeof (struct Dwarf_Fde_s));
  else
    *fde_data = NULL;
  if ((nfdelist > 0 && *fde_data == NULL) || *cie_data == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  /* Remember the counts.  */
  dbg->fde_cnt = nfdelist;
  dbg->cie_cnt = ncielist;

  /* Add all the CIEs.  */
  copy_cielist = cielist;
  *cie_element_count = ncielist;
  while (ncielist-- > 0)
    {
      (*cie_data)[ncielist] = cielist->cie;
      cielist = cielist->next;
    }

  /* Add all the FDEs.  */
  *fde_element_count = nfdelist;
  while (nfdelist-- > 0)
    {
      (*fde_data)[nfdelist] = fdelist->fde;

      if (fdelist->fde->cie == NULL)
	{
	  /* We have not yet found the CIE.  Search now that we know
             about all of them.  */
	  cielist = copy_cielist;
	  do
	    {
	      if (cielist->cie->offset
		  == (size_t) (fdelist->cie_id_ptr - fdelist->fde->offset
			       - (Dwarf_Small *) dbg->sections[IDX_eh_frame].addr))
		{
		  fdelist->fde->cie = cielist->cie;
		  break;
		}
	      cielist = cielist->next;
	    }
	  while (cielist != NULL);

	  if (cielist == NULL)
	    {
	      /* There is no matching CIE.  This is bad.  */
	      /* XXX Free everything.  */
	      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	      return DW_DLV_ERROR;
	    }
	}

      fdelist = fdelist->next;
    }

  return DW_DLV_OK;
}
