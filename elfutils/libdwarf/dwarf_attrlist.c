/* Return attribute list for die.
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

#include <stdlib.h>

#include <libdwarfP.h>


struct attrlist
{
  struct attrlist *next;
  Dwarf_Attribute attr;
};


int
dwarf_attrlist (die, attrbuf, attrcount, error)
     Dwarf_Die die;
     Dwarf_Attribute **attrbuf;
     Dwarf_Signed *attrcount;
     Dwarf_Error *error;
{
  Dwarf_Debug dbg = die->cu->dbg;
  Dwarf_Small *die_addr;
  Dwarf_Word u128;
  Dwarf_Abbrev abbrev;
  Dwarf_Small *attrp;
  struct attrlist *alist;
  int nattr;
  Dwarf_Attribute *result;

  /* Address of the given die.  */
  die_addr = die->addr;

  /* Get abbrev code.  */
  get_uleb128 (u128, die_addr);
  /*  And get the abbreviation itself.  */
  abbrev = __libdwarf_get_abbrev (dbg, die->cu, u128, error);
  if (abbrev == NULL)
    return DW_DLV_ERROR;

  /* This is where the attributes start.  */
  attrp = abbrev->attrp;

  /* Initialize the list.  We create one because we don't know yet how
     many attributes there will be.  */
  alist = NULL;
  nattr = 0;

  /* Go over the list of attributes.  */
  while (1)
    {
      Dwarf_Word attr_name;
      Dwarf_Word attr_form;
      Dwarf_Attribute new_attr;
      struct attrlist *new_alist;

      /* Are we still in bounds?  */
      if (unlikely (attrp
		    >= ((Dwarf_Small *) dbg->sections[IDX_debug_abbrev].addr
			+ dbg->sections[IDX_debug_abbrev].size)))
	{
	  while (alist != NULL)
	    {
	      free (alist->attr);
	      alist = alist->next;
	    }

	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      /* Get attribute name and form.

	 XXX We don't check whether this reads beyond the end of the
	 section.  */
      get_uleb128 (attr_name, attrp);
      get_uleb128 (attr_form, attrp);

      /* We can stop if we found the attribute with value zero.  */
      if (attr_name == 0 && attr_form == 0)
	break;

      /* Allocate the attribute data structure.  */
      new_attr = (Dwarf_Attribute) malloc (sizeof (struct Dwarf_Attribute_s));
      if (new_attr == NULL)
	{
	  while (alist != NULL)
	    {
	      free (alist->attr);
	      alist = alist->next;
	    }

	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      /* Fill in the values.  */
      new_attr->code = attr_name;
      new_attr->form = attr_form;
      new_attr->valp = die_addr;
      new_attr->cu = die->cu;

      /* Enqueue.  */
      new_alist = (struct attrlist *) alloca (sizeof (struct attrlist));
      new_alist->attr = new_attr;
      new_alist->next = alist;
      alist = new_alist;
      ++nattr;

      /* Skip over the rest of this attribute (if there is any).  */
      if (attr_form != 0)
	{
	  size_t len;

	  if (unlikely (__libdwarf_form_val_len (dbg, die->cu, attr_form,
						 die_addr, &len, error)
			!= DW_DLV_OK))
	    {
	      while (alist != NULL)
		{
		  free (alist->attr);
		  alist = alist->next;
		}

	      return DW_DLV_ERROR;
	    }

	  die_addr += len;
	}
    }

  if (nattr == 0)
    return DW_DLV_NO_ENTRY;

  /* Allocate the array for the result.  */
  result = (Dwarf_Attribute *) malloc (nattr * sizeof (Dwarf_Attribute));
  if (result == NULL)
    {
      while (alist != NULL)
	{
	  free (alist->attr);
	  alist = alist->next;
	}

      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  /* Store the number of attributes and the result pointer.  */
  *attrcount = nattr;
  *attrbuf = result;

  /* Put the attribute entries in the array (in the right order).  */
  do
    {
      result[--nattr] = alist->attr;
      alist = alist->next;
    }
  while (nattr > 0);

  return DW_DLV_OK;
}
