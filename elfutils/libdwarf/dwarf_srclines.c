/* Return source lines of compilation unit.
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

#include <dwarf.h>
#include <stdlib.h>
#include <string.h>

#include <libdwarfP.h>


struct dirlist
{
  char *dir;
  size_t len;
  struct dirlist *next;
};

struct filelist
{
  char *name;
  Dwarf_Unsigned mtime;
  Dwarf_Unsigned length;
  struct filelist *next;
};

struct linelist
{
  Dwarf_Line line;
  struct linelist *next;
};


/* Adds a new line to the matrix.  We cannot definte a function because
   we want to use alloca.  */
#define NEW_LINE(end_seq) \
  do {									      \
       /* Add the new line.  */						      \
       new_line = (struct linelist *) alloca (sizeof (struct linelist));      \
       new_line->line = (Dwarf_Line) malloc (sizeof (struct Dwarf_Line_s));   \
       if (new_line == NULL)						      \
	 {								      \
	   /* XXX Should we bother to free the memory?  */		      \
	   __libdwarf_error (dbg, error, DW_E_NOMEM);			      \
	   return DW_DLV_ERROR;						      \
	 }								      \
									      \
       /* Set the line information.  */					      \
       new_line->line->addr = address;					      \
       new_line->line->file = file;					      \
       new_line->line->line = line;					      \
       new_line->line->column = column;					      \
       new_line->line->is_stmt = is_stmt;				      \
       new_line->line->basic_block = basic_block;			      \
       new_line->line->end_sequence = end_seq;				      \
       new_line->line->prologue_end = prologue_end;			      \
       new_line->line->epilogue_begin = epilogue_begin;			      \
									      \
       new_line->next = linelist;					      \
       linelist = new_line;						      \
       ++nlinelist;							      \
  } while (0)


int
dwarf_srclines (die, linebuf, linecount, error)
     Dwarf_Die die;
     Dwarf_Line **linebuf;
     Dwarf_Signed *linecount;
     Dwarf_Error *error;
{
  Dwarf_CU_Info cu = die->cu;
  Dwarf_Debug dbg = cu->dbg;
  Dwarf_Attribute stmt_list;
  Dwarf_Attribute comp_dir_attr;
  char *comp_dir;
  Dwarf_Unsigned offset;
  Dwarf_Small *linep;
  Dwarf_Small *lineendp;
  Dwarf_Small *header_start;
  Dwarf_Unsigned header_length;
  Dwarf_File files;
  Dwarf_Line *lines;
  unsigned int unit_length;
  unsigned int version;
  unsigned int opcode_base;
  Dwarf_Small *standard_opcode_lengths;
  unsigned int minimum_instruction_length;
  unsigned int default_is_stmt;
  int line_base;
  unsigned int line_range;
  int length;
  struct dirlist comp_dir_elem;
  struct dirlist *dirlist;
  unsigned int ndirlist;
  struct dirlist **dirarray;
  struct filelist *filelist;
  unsigned int nfilelist;
  struct filelist null_file;
  Dwarf_Unsigned address;
  size_t file;
  size_t line;
  size_t column;
  int is_stmt;
  int basic_block;
  int prologue_end;
  int epilogue_begin;
  struct linelist *linelist;
  unsigned int nlinelist;
  int res;

  /* The die must be for a compilation unit.  */
  if (unlikely (die->abbrev->tag != DW_TAG_compile_unit))
    {
      __libdwarf_error (die->cu->dbg, error, DW_E_NO_CU);
      return DW_DLV_ERROR;
    }

  /* The die must have a statement list associated.  */
  res = dwarf_attr (die, DW_AT_stmt_list, &stmt_list, error);
  if (unlikely (res != DW_DLV_OK))
    return res;

  /* Get the offset into the .debug_line section.  */
  res = dwarf_formudata (stmt_list, &offset, error);
  if (unlikely (res != DW_DLV_OK))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      return res;
    }

  /* We need a .debug_line section.  */
  if (dbg->sections[IDX_debug_line].addr == NULL)
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_NO_DEBUG_LINE);
      return DW_DLV_ERROR;
    }

  linep = (Dwarf_Small *) dbg->sections[IDX_debug_line].addr + offset;
  lineendp = ((Dwarf_Small *) dbg->sections[IDX_debug_line].addr
	      + dbg->sections[IDX_debug_line].size);

  /* Test whether at least the first 4 bytes are available.  */
  if (unlikely (linep + 4 > lineendp))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  /* Get the compilation directory.  */
  res = dwarf_attr (die, DW_AT_comp_dir, &comp_dir_attr, error);
  if (unlikely (res == DW_DLV_ERROR)
      || (res == DW_DLV_OK
	  && unlikely (dwarf_formstring (comp_dir_attr, &comp_dir, error)
		       == DW_DLV_ERROR)))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }
  else if (res == DW_DLV_OK)
    dwarf_dealloc (dbg, comp_dir_attr, DW_DLA_ATTR);
  else
    comp_dir = NULL;

  /* Read the unit_length.  */
  unit_length = read_4ubyte_unaligned (dbg, linep);
  linep += 4;
  length = 4;
  if (unit_length == 0xffffffff)
    {
      if (unlikely (linep + 8 > lineendp))
	{
	  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      unit_length = read_8ubyte_unaligned (dbg, linep);
      linep += 8;
      length = 8;
    }

  /* Check whether we have enough room in the section.  */
  if (unlikely (linep + unit_length > lineendp))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }
  lineendp = linep + unit_length;

  /* The next element of the header is the version identifier.  */
  version = read_2ubyte_unaligned (dbg, linep);
  if (unlikely (version != DWARF_VERSION))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_VERSION_ERROR);
      return DW_DLV_ERROR;
    }
  linep += 2;

  /* Next comes the header length.  */
  if (length == 4)
    {
      header_length = read_4ubyte_unaligned (dbg, linep);
      linep += 4;
    }
  else
    {
      header_length = read_8ubyte_unaligned (dbg, linep);
      linep += 8;
    }
  header_start = linep;

  /* Next the minimum instruction length.  */
  minimum_instruction_length = *linep++;

  /* Then the flag determining the default value of the is_stmt
     register.  */
  default_is_stmt = *linep++;

  /* Now the line base.  */
  line_base = *((signed char *) linep)++;

  /* And the line range.  */
  line_range = *linep++;

  /* The opcode base.  */
  opcode_base = *linep++;

  /* Remember array with the standard opcode length (-1 to account for
     the opcode with value zero not being mentioned).  */
  standard_opcode_lengths = linep - 1;
  linep += opcode_base - 1;

  /* First comes the list of directories.  Add the compilation directory
     first since the index zero is used for it.  */
  comp_dir_elem.dir = comp_dir;
  comp_dir_elem.len = comp_dir ? strlen (comp_dir) : 0;
  comp_dir_elem.next = NULL;
  dirlist = &comp_dir_elem;
  ndirlist = 1;

  while (*linep != 0)
    {
      struct dirlist *new_dir = (struct dirlist *) alloca (sizeof (*new_dir));

      new_dir->dir = (char *) linep;
      new_dir->len = strlen ((char *) linep);
      new_dir->next = dirlist;
      dirlist = new_dir;
      ++ndirlist;
      linep += new_dir->len + 1;
    }
  /* Skip the final NUL byte.  */
  ++linep;

  /* Rearrange the list in array form.  */
  dirarray = (struct dirlist **) alloca (sizeof (*dirarray));
  while (ndirlist-- > 0)
    {
      dirarray[ndirlist] = dirlist;
      dirlist = dirlist->next;
    }

  comp_dir_elem.dir = comp_dir;
  comp_dir_elem.len = comp_dir ? strlen (comp_dir) : 0;
  comp_dir_elem.next = NULL;
  dirlist = &comp_dir_elem;
  ndirlist = 1;

  /* Now read the files.  */
  null_file.name = "???";
  null_file.mtime = 0;
  null_file.length = 0;
  null_file.next = NULL;
  filelist = &null_file;
  nfilelist = 1;

  while (*linep != 0)
    {
      struct filelist *new_file =
	(struct filelist *) alloca (sizeof (*new_file));
      char *fname;
      size_t fnamelen;
      Dwarf_Unsigned diridx;

      /* First comes the file name.  */
      fname = (char *) linep;
      fnamelen = strlen (fname);
      linep += fnamelen + 1;

      /* Then the index.  */
      get_uleb128 (diridx, linep);
      if (unlikely (diridx >= ndirlist))
	{
	  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
	  __libdwarf_error (dbg, error, DW_E_INVALID_DIR_IDX);
	  return DW_DLV_ERROR;
	}

      if (*fname == '/')
	/* It's an absolute path.  */
	new_file->name = strdup (fname);
      else
	{
	  new_file->name = (char *) malloc (dirarray[diridx]->len + 1
					    + fnamelen + 1);
	  if (new_file->name != NULL)
	    {
	      char *cp = new_file->name;

	      if (dirarray[diridx]->dir != NULL)
		/* This value could be NULL in case the DW_AT_comp_dir
		   was not present.  We cannot do much in this case.
		   The easiest thing is to convert the path in an
		   absolute path.  */
		cp = stpcpy (cp, dirarray[diridx]->dir);
	      *cp++ = '/';
	      strcpy (cp, fname);
	    }
	}
      if (new_file->name == NULL)
	{
	  /* XXX Should we bother to free all the memory?  */
	  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
	  __libdwarf_error (dbg, error, DW_E_NOMEM);
	  return DW_DLV_ERROR;
	}

      /* Next comes the modification time.  */
      get_uleb128 (new_file->mtime, linep);

      /* Finally the length of the file.  */
      get_uleb128 (new_file->length, linep);

      new_file->next = filelist;
      filelist = new_file;
      ++nfilelist;
    }
  ++linep;

  /* Consistency check.  */
  if (unlikely (linep != header_start + header_length))
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
      return DW_DLV_ERROR;
    }

  /* We are about to process the statement program.  Initialize the
     state machine registers (see 6.2.2 in the v2.1 specification).  */
  address = 0;
  file = 1;
  line = 1;
  column = 0;
  is_stmt = default_is_stmt;
  basic_block = 0;
  prologue_end = 0;
  epilogue_begin = 0;

  /* Process the instructions.  */
  linelist = NULL;
  nlinelist = 0;
  while (linep < lineendp)
    {
      struct linelist *new_line;
      unsigned int opcode;
      unsigned int u128;
      int s128;

      /* Read the opcode.  */
      opcode = *linep++;

      /* Is this a special opcode?  */
      if (likely (opcode >= opcode_base))
	{
	  /* Yes.  Handling this is quite easy since the opcode value
	     is computed with

	     opcode = (desired line increment - line_base)
		      + (line_range * address advance) + opcode_base
	  */
	  int line_increment = line_base + (opcode - opcode_base) % line_range;
	  unsigned int address_increment = (minimum_instruction_length
					    * ((opcode - opcode_base)
					       / line_range));

	  /* Perform the increments.  */
	  line += line_increment;
	  address += address_increment;

	  /* Add a new line with the current state machine values.  */
	  NEW_LINE (0);

	  /* Reset the flags.  */
	  basic_block = 0;
	  prologue_end = 0;
	  epilogue_begin = 0;
	}
      else if (opcode == 0)
	{
	  /* This an extended opcode.  */
	  unsigned int len;

	  /* The length.  */
	  len = *linep++;

	  /* The sub-opecode.  */
	  opcode = *linep++;

	  switch (opcode)
	    {
	    case DW_LNE_end_sequence:
	      /* Add a new line with the current state machine values.
		 The is the end of the sequence.  */
	      NEW_LINE (1);

	      /* Reset the registers.  */
	      address = 0;
	      file = 1;
	      line = 1;
	      column = 0;
	      is_stmt = default_is_stmt;
	      basic_block = 0;
	      prologue_end = 0;
	      epilogue_begin = 0;
	      break;

	    case DW_LNE_set_address:
	      if (cu->address_size == 4)
		address = read_4ubyte_unaligned (dbg, linep);
	      else
		address = read_8ubyte_unaligned (dbg, linep);
	      linep += cu->address_size;
	      break;

	    case DW_LNE_define_file:
	      {
		struct filelist *new_file;
		char *fname;
		size_t fnamelen;
		unsigned int diridx;
		Dwarf_Unsigned mtime;
		Dwarf_Unsigned length;

		fname = (char *) linep;
		fnamelen = strlen (fname);
		linep += fnamelen + 1;

		get_uleb128 (diridx, linep);
		get_uleb128 (mtime, linep);
		get_uleb128 (length, linep);

		new_file = (struct filelist *) alloca (sizeof (*new_file));
		if (fname[0] == '/')
		  new_file->name = strdup (fname);
		else
		  {

		    new_file->name = (char *) malloc (dirarray[diridx]->len + 1
						      + fnamelen + 1);
		    if (new_file->name != NULL)
		      {
			char *cp = new_file->name;

			if (dirarray[diridx]->dir != NULL)
			  /* This value could be NULL in case the
			     DW_AT_comp_dir was not present.  We
			     cannot do much in this case.  The easiest
			     thing is to convert the path in an
			     absolute path.  */
			  cp = stpcpy (cp, dirarray[diridx]->dir);
			*cp++ = '/';
			strcpy (cp, fname);
		      }
		  }
		if (new_file->name == NULL)
		  {
		    dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		    __libdwarf_error (dbg, error, DW_E_NOMEM);
		    return DW_DLV_ERROR;
		  }

		new_file->mtime = mtime;
		new_file->length = length;
		new_file->next = filelist;
		filelist = new_file;
		++nfilelist;
	      }
	      break;

	    default:
	      /* Unknown, ignore it.  */
	      linep += len - 1;
	      break;
	    }
	}
      else if (opcode <= DW_LNS_set_epilog_begin)
	{
	  /* This is a known standard opcode.  */
	  switch (opcode)
	    {
	    case DW_LNS_copy:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      /* Add a new line with the current state machine values.  */
	      NEW_LINE (0);

	      /* Reset the flags.  */
	      basic_block = 0;
	      /* XXX Whether the following two lines are necessary is
		 unclear.  I guess the current v2.1 specification has
		 a bug in that it says clearing these two registers is
		 not necessary.  */
	      prologue_end = 0;
	      epilogue_begin = 0;
	      break;

	    case DW_LNS_advance_pc:
	      /* Takes one uleb128 parameter which is added to the address.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      get_uleb128 (u128, linep);
	      address += minimum_instruction_length * u128;
	      break;

	    case DW_LNS_advance_line:
	      /* Takes one sleb128 parameter which is added to the line.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      get_sleb128 (s128, linep);
	      line += s128;
	      break;

	    case DW_LNS_set_file:
	      /* Takes one uleb128 parameter which is stored in file.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      get_uleb128 (u128, linep);
	      file = u128;
	      break;

	    case DW_LNS_set_column:
	      /* Takes one uleb128 parameter which is stored in column.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      get_uleb128 (u128, linep);
	      column = u128;
	      break;

	    case DW_LNS_negate_stmt:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      is_stmt = 1 - is_stmt;
	      break;

	    case DW_LNS_set_basic_block:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      basic_block = 1;
	      break;

	    case DW_LNS_const_add_pc:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      address += (minimum_instruction_length
			  * ((255 - opcode_base) / line_range));
	      break;

	    case DW_LNS_fixed_advance_pc:
	      /* Takes one 16 bit parameter which is added to the address.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      address += read_2ubyte_unaligned (dbg, linep);
	      linep += 2;
	      break;

	    case DW_LNS_set_prologue_end:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      prologue_end = 1;
	      break;

	    case DW_LNS_set_epilog_begin:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		{
		  /* XXX Free memory.  */
		  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
		  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
		  return DW_DLV_ERROR;
		}

	      epilogue_begin = 1;
	      break;
	    }
	}
      else
	{
	  /* This is a new opcode the generator but not we know about.
	     Read the parameters associated with it but then discard
	     everything.  */
	  int n;

	  /* Read all the parameters for this opcode.  */
	  for (n = standard_opcode_lengths[opcode]; n > 0; --n)
	    {
	      Dwarf_Unsigned u128;
	      get_uleb128 (u128, linep);
	    }

	  /* Next round, ignore this opcode.  */
	  continue;
	}
    }

  /* Put all the files in an array.  */
  files = (Dwarf_File) malloc (sizeof (struct Dwarf_File_s)
			       + nfilelist * sizeof (Dwarf_Fileinfo));
  if (files == NULL)
    {
      /* XXX Should we bother to free all the memory?  */
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  files->nfiles = nfilelist;
  while (nfilelist-- > 0)
    {
      files->info[nfilelist].name = filelist->name;
      files->info[nfilelist].mtime = filelist->mtime;
      files->info[nfilelist].length = filelist->length;
      filelist = filelist->next;
    }

  /* Remember the debugging descriptor.  */
  files->dbg = dbg;

  /* Now put the lines in an array.  */
  lines = (Dwarf_Line *) malloc (nlinelist * sizeof (Dwarf_Line));
  if (lines == NULL)
    {
      dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);
      __libdwarf_error (dbg, error, DW_E_NOMEM);
      return DW_DLV_ERROR;
    }

  *linebuf = lines;
  *linecount = nlinelist;

  while (nlinelist--)
    {
      lines[nlinelist] = linelist->line;
      linelist->line->files = files;
      linelist = linelist->next;
    }

  dwarf_dealloc (dbg, stmt_list, DW_DLA_ATTR);

  return DW_DLV_OK;
}
