/* Get abbreviation record.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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

#include <dwarf.h>
#include <stdlib.h>

#include <libdwarfP.h>


int
dwarf_get_abbrev (dbg, offset, return_abbrev, length, attr_count, error)
     Dwarf_Debug dbg;
     Dwarf_Unsigned offset;
     Dwarf_Abbrev *return_abbrev;
     Dwarf_Unsigned *length;
     Dwarf_Unsigned *attr_count;
     Dwarf_Error *error;
{
  Dwarf_Abbrev ent;
  Dwarf_Small *abbrevp;
  Dwarf_Small *start_abbrevp;

  /* Address in memory.  */
  abbrevp = (Dwarf_Small *) dbg->sections[IDX_debug_abbrev].addr + offset;

  /* Remember where we started.  */
  start_abbrevp = abbrevp;

  if (*abbrevp != '\0')
    {
      /* 7.5.3 Abbreviations Tables

	 [...] Each declaration begins with an unsigned LEB128 number
	 representing the abbreviation code itself.  [...]  The
	 abbreviation code is followed by another unsigned LEB128
	 number that encodes the entry's tag.  [...]

	 [...] Following the tag encoding is a 1-byte value that
	 determines whether a debugging information entry using this
	 abbreviation has child entries or not. [...]

	 [...] Finally, the child encoding is followed by a series of
	 attribute specifications. Each attribute specification
	 consists of two parts. The first part is an unsigned LEB128
	 number representing the attribute's name. The second part is
	 an unsigned LEB128 number representing the attribute s form.  */
      Dwarf_Word abbrev_code;
      Dwarf_Word abbrev_tag;
      Dwarf_Word attr_name;
      Dwarf_Word attr_form;
      Dwarf_Unsigned attrcnt;

      /* XXX We have no tests for crossing the section boundary here.
	 We should compare with dbg->sections[IDX_debug_abbrev].size.  */
      get_uleb128 (abbrev_code, abbrevp);
      get_uleb128 (abbrev_tag, abbrevp);

      /* Get memory for the result.  */
      ent = (Dwarf_Abbrev) malloc (sizeof (struct Dwarf_Abbrev_s));
      if (ent == NULL)
	{
	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      ent->code = abbrev_code;
      ent->tag = abbrev_tag;
      ent->has_children = *abbrevp++ == DW_CHILDREN_yes;
      ent->attrp = abbrevp;
      ent->offset = offset;

      /* Skip over all the attributes.  */
      attrcnt = 0;
      do
	{
	  get_uleb128 (attr_name, abbrevp);
	  get_uleb128 (attr_form, abbrevp);
	}
      while (attr_name != 0 && attr_form != 0 && ++attrcnt);

      /* Number of attributes.  */
      *attr_count = ent->attrcnt = attrcnt;

      /* Store the actual abbreviation record.  */
      *return_abbrev = ent;
    }
  else
    /* Read over the NUL byte.  */
    ++abbrevp;

  /* Length of the entry.  */
  *length = abbrevp - start_abbrevp;

  /* If we come here we haven't found anything.  */
  return DW_DLV_OK;
}
