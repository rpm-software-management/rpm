/* Return descriptor for sibling of die.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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

#include <dwarf.h>
#include <stdlib.h>

#include <libdwarfP.h>


int
dwarf_siblingof (Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die *return_sub,
		Dwarf_Error *error)
{
  Dwarf_Small *die_addr;
  Dwarf_CU_Info cu;
  Dwarf_Unsigned u128;
  Dwarf_Die new_die;

  if (dbg == NULL)
    return DW_DLV_ERROR;

  if (die == NULL)
    {
      Dwarf_Unsigned die_offset;

      /* We are supposed to return the DW_TAG_compile_unit die for the
	 current compile unit.  For this to succeed the user must have
	 looked for the compile unit before.  */
      if (dbg->cu_list_current == NULL)
	{
	  __libdwarf_error (dbg, error, DW_E_NO_CU);
	  return DW_DLV_ERROR;
	}

      cu = dbg->cu_list_current;

      die_offset = (cu->offset + 2 * cu->offset_size - 4 + sizeof (Dwarf_Half)
		    + cu->offset_size + 1);

      /* Check whether this is withing the debug section.  */
      if (die_offset >= dbg->sections[IDX_debug_info].size)
	return DW_DLV_NO_ENTRY;

      /* Compute the pointer.  */
      die_addr = ((Dwarf_Small *) dbg->sections[IDX_debug_info].addr
		  + die_offset);
    }
  else
    {
      unsigned int level = 0;

      /* We start from the given die.  */
      cu = die->cu;

      /* Address of the given die.  */
      die_addr = die->addr;

      /* Search for the beginning of the next die on this level.  We
         must not return the dies for children of the given die.  */
      do
	{
	  Dwarf_Abbrev abbrev;
	  Dwarf_Small *attrp;
	  Dwarf_Word attr_name;
	  Dwarf_Word attr_form;

	  /* Get abbrev code.  */
	  get_uleb128 (u128, die_addr);
	  /*  And get the abbreviation itself.  */
	  abbrev = __libdwarf_get_abbrev (dbg, cu, u128, error);
	  if (abbrev == NULL)
	    return DW_DLV_ERROR;

	  /* This is where the attributes start.  */
	  attrp = abbrev->attrp;

	  /* Does this abbreviation have children?  */
	  if (abbrev->has_children)
	    ++level;

	  while (1)
	    {
	      /* Are we still in bounds?  */
	      if (attrp >= ((Dwarf_Small *)dbg->sections[IDX_debug_abbrev].addr
			    + dbg->sections[IDX_debug_abbrev].size))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      /* Get attribute name and form.

		 XXX We don't check whether this reads beyond the end of
		 the section.  */
	      get_uleb128 (attr_name, attrp);
	      get_uleb128 (attr_form, attrp);

	      /* We can stop if we found the attribute with value zero.  */
	      if (attr_name == 0 && attr_form == 0)
		break;

	      /* See whether this is an sibling attribute which would help
		 us to skip ahead.  */
	      if (attr_name == DW_AT_sibling)
		{
		  /* Cool.  We just have to decode the parameter and we know
		     the offset.  */
		  Dwarf_Unsigned offset;

		  switch (attr_form)
		    {
		    case DW_FORM_ref1:
		      offset = *die_addr;
		      break;

		    case DW_FORM_ref2:
		      offset = read_2ubyte_unaligned (dbg, die_addr);
		      break;

		    case DW_FORM_ref4:
		      offset = read_4ubyte_unaligned (dbg, die_addr);
		      break;

		    case DW_FORM_ref8:
		      offset = read_8ubyte_unaligned (dbg, die_addr);
		      break;

		    case DW_FORM_ref_udata:
		      get_uleb128 (offset, die_addr);
		      break;

		    default:
		      /* The above are all legal forms.  Everything else is
			 an error.  */
		      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		      return DW_DLV_ERROR;
		    }

		  /* Compute the new address.  Some sanity check first,
		     though.  */
		  if (unlikely (offset > 2 * cu->offset_size - 4 + cu->length))
		    {
		      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		      return DW_DLV_ERROR;
		    }

		  die_addr =
		    ((Dwarf_Small *) dbg->sections[IDX_debug_info].addr
		     + cu->offset + offset);

		  /* Even if the abbreviation has children we have stepped
		     over them now.  */
		  if (abbrev->has_children)
		    --level;
		  break;
		}

	      /* Skip over the rest of this attribute (if there is any).  */
	      if (attr_form != 0)
		{
		  size_t len;

		  if (unlikely (__libdwarf_form_val_len (dbg, cu, attr_form,
							 die_addr, &len, error)
				!= DW_DLV_OK))
		    return DW_DLV_ERROR;

		  die_addr += len;
		}
	    }

	  /* Check that we are not yet at the end.  */
	  if (*die_addr == 0)
	    {
	      if (level == 0)
		return DW_DLV_NO_ENTRY;

	      do
		++die_addr;
	      while (--level > 0 && *die_addr == 0);
	    }
	}
      while (level > 0);
    }

  /* Are we at the end.  */
  if (die != NULL
      && die_addr >= ((Dwarf_Small *) dbg->sections[IDX_debug_info].addr
		      + cu->offset + cu->length + 2 * cu->offset_size - 4))
    return DW_DLV_NO_ENTRY;

  /* See whether there is another sibling available or whether this is
     the end.  */
  if (*die_addr == 0)
    return DW_DLV_NO_ENTRY;

  /* There is more data.  Create the data structure.  */
  new_die = (Dwarf_Die) malloc (sizeof (struct Dwarf_Die_s));
  if (new_die == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

#ifdef DWARF_DEBUG
  new_die->memtag = DW_DLA_DIE;
#endif

  /* Remember the address.  */
  new_die->addr = die_addr;

  /* And the compile unit.  */
  new_die->cu = cu;

  /* 7.5.2  Debugging Information Entry

     Each debugging information entry begins with an unsigned LEB128
     number containing the abbreviation code for the entry.  */
  get_uleb128 (u128, die_addr);

  /* Find the abbreviation.  */
  new_die->abbrev = __libdwarf_get_abbrev (dbg, cu, u128, error);
  if (new_die->abbrev == NULL)
    {
      free (new_die);
      return DW_DLV_ERROR;
    }

  /* If we are looking for the first entry this must be a compile unit.  */
  if (die == NULL && unlikely (new_die->abbrev->tag != DW_TAG_compile_unit))
    {
      free (new_die);
      __libdwarf_error (dbg, error, DW_E_1ST_NO_CU);
      return DW_DLV_ERROR;
    }

  *return_sub = new_die;
  return DW_DLV_OK;
}
