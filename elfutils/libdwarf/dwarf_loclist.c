/* Return location expression list.
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


struct loclist
{
  Dwarf_Small atom;
  Dwarf_Unsigned number;
  Dwarf_Unsigned number2;
  Dwarf_Unsigned offset;
  struct loclist *next;
};

struct locdesclist
{
  Dwarf_Addr lopc;
  Dwarf_Addr hipc;
  Dwarf_Half cents;
  struct loclist *s;
  struct locdesclist *next;
};


int
dwarf_loclist (Dwarf_Attribute attr, Dwarf_Locdesc **llbuf,
		Dwarf_Signed *listlen, Dwarf_Error *error)
{
  Dwarf_CU_Info cu = attr->cu;
  Dwarf_Debug dbg = cu->dbg;
  Dwarf_Unsigned offset;
  Dwarf_Unsigned offset_end;
  Dwarf_Small *locp;
  struct locdesclist *locdesclist;
  Dwarf_Locdesc *result;
  unsigned int n;

  /* Must by one of the attribute listed below.  */
  if (attr->code != DW_AT_location
      && attr->code != DW_AT_data_member_location
      && attr->code != DW_AT_vtable_elem_location
      && attr->code != DW_AT_string_length
      && attr->code != DW_AT_use_location
      && attr->code != DW_AT_return_addr)
    {
      __libdwarf_error (dbg, error, DW_E_WRONG_ATTR);
      return DW_DLV_ERROR;
    }

  /* Must have the form data4 or data8 which act as an offset.  */
  if (attr->form == DW_FORM_data4)
    offset = read_4ubyte_unaligned (dbg, attr->valp);
  else if (likely (attr->form == DW_FORM_data8))
    offset = read_8ubyte_unaligned (dbg, attr->valp);
  else
    {
      __libdwarf_error (dbg, error, DW_E_NO_DATA);
      return DW_DLV_ERROR;
    }

  /* Check whether the .debug_loc section is available.  */
  if (unlikely (dbg->sections[IDX_debug_loc].addr == NULL))
    {
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  /* This is the part of the .debug_loc section we can read.  */
  locp = (Dwarf_Small *) dbg->sections[IDX_debug_loc].addr + offset;
  offset_end = offset + dbg->sections[IDX_debug_loc].size;

  locdesclist = NULL;
  n = 0;
  while (1)
    {
      Dwarf_Addr lopc;
      Dwarf_Addr hipc;
      Dwarf_Ptr data;
      Dwarf_Unsigned len2;
      struct locdesclist *newdesc;
      Dwarf_Small *locp;
      Dwarf_Small *blkendp;
      Dwarf_Small *blkstartp;

      if (unlikely (dwarf_get_loclist_entry (dbg, offset, &hipc, &lopc, &data,
					     &len2, &offset, error)
		    != DW_DLV_OK))
	return DW_DLV_ERROR;

      /* Does this signal the end?  */
      if (lopc == 0 && hipc == 0)
	break;

      locp = data;
      blkstartp = locp;
      blkendp = locp + len2;

      /* Create a new Locdesc entry.  */
      newdesc = (struct locdesclist *) alloca (sizeof (struct locdesclist));
      newdesc->lopc = lopc;
      newdesc->hipc = hipc;
      newdesc->cents = 0;
      newdesc->s = NULL;
      newdesc->next = locdesclist;
      locdesclist = newdesc;
      ++n;

      /* Decode the opcodes.  It is possible in some situations to
         have a block of size zero.  */
      while (locp < blkendp)
	{
	  struct loclist *newloc;

	  newloc = (struct loclist *) alloca (sizeof (struct loclist));
	  newloc->number = 0;
	  newloc->number2 = 0;
	  newloc->offset = locp - blkstartp;
	  newloc->next = newdesc->s;
	  newdesc->s = newloc;
	  ++newdesc->cents;

	  newloc->atom = *locp;
	  switch (*locp++)
	    {
	    case DW_OP_addr:
	      /* Address, depends on address size of CU.  */
	      if (cu->address_size == 4)
		{
		  if (unlikely (locp + 4 > blkendp))
		    {
		      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		      return DW_DLV_ERROR;
		    }
		  newloc->number = read_4ubyte_unaligned (dbg, locp);
		  locp += 4;
		}
	      else
		{
		  if (unlikely (locp + 8 > blkendp))
		    {
		      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		      return DW_DLV_ERROR;
		    }
		  newloc->number = read_8ubyte_unaligned (dbg, locp);
		  locp += 8;
		}
	      break;

	    case DW_OP_deref:
	    case DW_OP_dup:
	    case DW_OP_drop:
	    case DW_OP_over:
	    case DW_OP_swap:
	    case DW_OP_rot:
	    case DW_OP_xderef:
	    case DW_OP_abs:
	    case DW_OP_and:
	    case DW_OP_div:
	    case DW_OP_minus:
	    case DW_OP_mod:
	    case DW_OP_mul:
	    case DW_OP_neg:
	    case DW_OP_not:
	    case DW_OP_or:
	    case DW_OP_plus:
	    case DW_OP_shl:
	    case DW_OP_shr:
	    case DW_OP_shra:
	    case DW_OP_xor:
	    case DW_OP_eq:
	    case DW_OP_ge:
	    case DW_OP_gt:
	    case DW_OP_le:
	    case DW_OP_lt:
	    case DW_OP_ne:
	    case DW_OP_lit0:
	    case DW_OP_lit1:
	    case DW_OP_lit2:
	    case DW_OP_lit3:
	    case DW_OP_lit4:
	    case DW_OP_lit5:
	    case DW_OP_lit6:
	    case DW_OP_lit7:
	    case DW_OP_lit8:
	    case DW_OP_lit9:
	    case DW_OP_lit10:
	    case DW_OP_lit11:
	    case DW_OP_lit12:
	    case DW_OP_lit13:
	    case DW_OP_lit14:
	    case DW_OP_lit15:
	    case DW_OP_lit16:
	    case DW_OP_lit17:
	    case DW_OP_lit18:
	    case DW_OP_lit19:
	    case DW_OP_lit20:
	    case DW_OP_lit21:
	    case DW_OP_lit22:
	    case DW_OP_lit23:
	    case DW_OP_lit24:
	    case DW_OP_lit25:
	    case DW_OP_lit26:
	    case DW_OP_lit27:
	    case DW_OP_lit28:
	    case DW_OP_lit29:
	    case DW_OP_lit30:
	    case DW_OP_lit31:
	    case DW_OP_reg0:
	    case DW_OP_reg1:
	    case DW_OP_reg2:
	    case DW_OP_reg3:
	    case DW_OP_reg4:
	    case DW_OP_reg5:
	    case DW_OP_reg6:
	    case DW_OP_reg7:
	    case DW_OP_reg8:
	    case DW_OP_reg9:
	    case DW_OP_reg10:
	    case DW_OP_reg11:
	    case DW_OP_reg12:
	    case DW_OP_reg13:
	    case DW_OP_reg14:
	    case DW_OP_reg15:
	    case DW_OP_reg16:
	    case DW_OP_reg17:
	    case DW_OP_reg18:
	    case DW_OP_reg19:
	    case DW_OP_reg20:
	    case DW_OP_reg21:
	    case DW_OP_reg22:
	    case DW_OP_reg23:
	    case DW_OP_reg24:
	    case DW_OP_reg25:
	    case DW_OP_reg26:
	    case DW_OP_reg27:
	    case DW_OP_reg28:
	    case DW_OP_reg29:
	    case DW_OP_reg30:
	    case DW_OP_reg31:
	    case DW_OP_nop:
	    case DW_OP_push_object_address:
	    case DW_OP_call_ref:
	      /* No operand.  */
	      break;

	    case DW_OP_const1u:
	    case DW_OP_pick:
	    case DW_OP_deref_size:
	    case DW_OP_xderef_size:
	      if (unlikely (locp >= blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = *((uint8_t *) locp)++;
	      break;

	    case DW_OP_const1s:
	      if (unlikely (locp >= blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = *((int8_t *) locp)++;
	      break;

	    case DW_OP_const2u:
	      if (unlikely (locp + 2 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_2ubyte_unaligned (dbg, locp);
	      locp += 2;
	      break;

	    case DW_OP_const2s:
	    case DW_OP_skip:
	    case DW_OP_bra:
	    case DW_OP_call2:
	      if (unlikely (locp + 2 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_2sbyte_unaligned (dbg, locp);
	      locp += 2;
	      break;

	    case DW_OP_const4u:
	      if (unlikely (locp + 4 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_4ubyte_unaligned (dbg, locp);
	      locp += 4;
	      break;

	    case DW_OP_const4s:
	    case DW_OP_call4:
	      if (unlikely (locp + 4 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_4sbyte_unaligned (dbg, locp);
	      locp += 4;
	      break;

	    case DW_OP_const8u:
	      if (unlikely (locp + 8 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_8ubyte_unaligned (dbg, locp);
	      locp += 8;
	      break;

	    case DW_OP_const8s:
	      if (unlikely (locp + 8 > blkendp))
		{
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}
	      newloc->number = read_8sbyte_unaligned (dbg, locp);
	      locp += 8;
	      break;

	    case DW_OP_constu:
	    case DW_OP_plus_uconst:
	    case DW_OP_regx:
	    case DW_OP_piece:
	      /* XXX Check size.  */
	      get_uleb128 (newloc->number, locp);
	      break;

	    case DW_OP_consts:
	    case DW_OP_breg0:
	    case DW_OP_breg1:
	    case DW_OP_breg2:
	    case DW_OP_breg3:
	    case DW_OP_breg4:
	    case DW_OP_breg5:
	    case DW_OP_breg6:
	    case DW_OP_breg7:
	    case DW_OP_breg8:
	    case DW_OP_breg9:
	    case DW_OP_breg10:
	    case DW_OP_breg11:
	    case DW_OP_breg12:
	    case DW_OP_breg13:
	    case DW_OP_breg14:
	    case DW_OP_breg15:
	    case DW_OP_breg16:
	    case DW_OP_breg17:
	    case DW_OP_breg18:
	    case DW_OP_breg19:
	    case DW_OP_breg20:
	    case DW_OP_breg21:
	    case DW_OP_breg22:
	    case DW_OP_breg23:
	    case DW_OP_breg24:
	    case DW_OP_breg25:
	    case DW_OP_breg26:
	    case DW_OP_breg27:
	    case DW_OP_breg28:
	    case DW_OP_breg29:
	    case DW_OP_breg30:
	    case DW_OP_breg31:
	    case DW_OP_fbreg:
	      /* XXX Check size.  */
	      get_sleb128 (newloc->number, locp);
	      break;

	    case DW_OP_bregx:
	      /* XXX Check size.  */
	      get_uleb128 (newloc->number, locp);
	      get_sleb128 (newloc->number2, locp);
	      break;

	    default:
	      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	      return DW_DLV_ERROR;
	    }
	}
    }

  if (unlikely (n == 0))
    {
      /* This is not allowed.

	 XXX Is it?  */
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  /* Allocate the array.  */
  result = (Dwarf_Locdesc *) malloc (n * sizeof (Dwarf_Locdesc));
  if (result == NULL)
    {
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  /* Store the result.  */
  *llbuf = result;
  *listlen = n;

  do
    {
      unsigned int cents;
      struct loclist *s;

      /* We populate the array from the back since the list is
         backwards.  */
      --n;

      result[n].ld_lopc = locdesclist->lopc;
      result[n].ld_hipc = locdesclist->hipc;
      result[n].ld_cents = cents = locdesclist->cents;
      result[n].ld_s = (Dwarf_Loc *) malloc (cents * sizeof (Dwarf_Loc));
      if (result == NULL)
	{
	  /* XXX Should be bother freeing memory?  */
	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      s = locdesclist->s;
      while (cents-- > 0)
	{
	  /* This list is also backwards.  */
	  result[n].ld_s[cents].lr_atom = s->atom;
	  result[n].ld_s[cents].lr_number = s->number;
	  result[n].ld_s[cents].lr_number2 = s->number2;
	  result[n].ld_s[cents].lr_offset = s->offset;
	  s = s->next;
	}

      locdesclist = locdesclist->next;
    }
  while (n > 0);

  /* We did it.  */
  return DW_DLV_OK;
}
