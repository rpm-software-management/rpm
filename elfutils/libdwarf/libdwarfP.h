/* Internal definitions for libdwarf.
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

#ifndef _LIBDWARFP_H
#define _LIBDWARFP_H 1

#include <libdwarf.h>
#include <libintl.h>
#include <limits.h>

#include <dwarf_abbrev_hash.h>


/* Version of the DWARF specification we support.  */
#define DWARF_VERSION 2

/* Version of the CIE format.  */
#define CIE_VERSION 1

/* Some additional basic types.  */
typedef unsigned int Dwarf_Word;


/* Valid indeces for the section data.  */
enum
  {
    IDX_debug_info = 0,
    IDX_debug_abbrev,
    IDX_debug_aranges,
    IDX_debug_line,
    IDX_debug_frame,
    IDX_eh_frame,
    IDX_debug_loc,
    IDX_debug_pubnames,
    IDX_debug_str,
    IDX_debug_funcnames,
    IDX_debug_typenames,
    IDX_debug_varnames,
    IDX_debug_weaknames,
    IDX_debug_macinfo,
    IDX_last
  };


/* This is the structure representing the debugging state.  */
struct Dwarf_Debug_s
  {
#ifdef DWARF_DEBUG
    int memtag;
#endif

    Dwarf_Handler dbg_errhand;
    Dwarf_Ptr dbg_errarg;

    Elf *elf;
    int other_byte_order;

    Dwarf_Unsigned access;

    /* The section data.  */
    struct
    {
      Dwarf_Small *addr;
      Dwarf_Unsigned size;
    } sections[IDX_last];

    /* Compilation unit handling.  To enable efficient searching we
       keep track of the unit we already found.  */
    struct Dwarf_CU_Info_s
    {
      /* This is the information the 'dwarf_next_cu_header' function
	 is supposed to return.  */
      Dwarf_Unsigned header_length;
      Dwarf_Unsigned length;
      Dwarf_Unsigned abbrev_offset;
      Dwarf_Half version_stamp;
      Dwarf_Half address_size;

      Dwarf_Unsigned offset;		/* In .debug_info section.  */

      /* Used to distinguish between 32-bit and 64-bit DWARF.  */
      Dwarf_Half offset_size;

      /* Hash table for the abbreviations.  */
      Dwarf_Abbrev_Hash abbrev_hash;
      Dwarf_Unsigned last_abbrev_offset;

      Dwarf_Debug dbg;

/*@relnull@*/
      struct Dwarf_CU_Info_s *next;
    } *cu_list;
/*@relnull@*/
    struct Dwarf_CU_Info_s *cu_list_current;
/*@relnull@*/
    struct Dwarf_CU_Info_s *cu_list_tail;

    Dwarf_Unsigned cie_cnt;
    Dwarf_Unsigned fde_cnt;
  };
typedef struct Dwarf_CU_Info_s *Dwarf_CU_Info;


/* Memory access macros.  We have to define it here since code in the
   header needs to know the structure of Dwarf_Debug.  */
#include "memory-access.h"


/* DWARF die representation.  */
struct Dwarf_Die_s
  {
#ifdef DWARF_DEBUG
    int memtag;
#endif

    Dwarf_Small *addr;
    Dwarf_Abbrev abbrev;
    Dwarf_CU_Info cu;
  };


/* Abbreviation list.  */
struct Dwarf_Abbrev_s
  {
    Dwarf_Unsigned code;	/* The code.  */
    Dwarf_Half tag;		/* The tag.  */
    Dwarf_Small *attrp;		/* Pointer to beginning of attributes.  */
    Dwarf_Unsigned attrcnt;	/* Number of attributes.  */
    int has_children;		/* Nonzero of abbreviation has children.  */
    Dwarf_Unsigned offset;	/* Offset in the .debug_abbrev section.  */
  };


/* Attribute list.  */
struct Dwarf_Attribute_s
  {
    Dwarf_Half code;		/* DWARF attribute code.  */
    Dwarf_Half form;		/* DWARF form.  */
    Dwarf_Small *valp;
    Dwarf_CU_Info cu;
  };


/* Structure for error values.  */
struct Dwarf_Error_s
  {
    unsigned int de_error;
  };


/* Files in line information records.  */
typedef struct Dwarf_File_s
  {
    Dwarf_Debug dbg;
    unsigned int nfiles;
    struct Dwarf_Fileinfo_s
    {
      char *name;
      Dwarf_Unsigned mtime;
      Dwarf_Unsigned length;
    } info[0];
  } *Dwarf_File;
typedef struct Dwarf_Fileinfo_s Dwarf_Fileinfo;


/* Representation of a row in the line table.  */
struct Dwarf_Line_s
  {
    Dwarf_Addr addr;
    unsigned int file;
    int line;
    unsigned short int column;
    unsigned int is_stmt:1;
    unsigned int basic_block:1;
    unsigned int end_sequence:1;
    unsigned int prologue_end:1;
    unsigned int epilogue_begin:1;

    Dwarf_File files;
  };


/* Additional, shared information required for Dwarf_Global handling.  */
typedef struct Dwarf_Global_Info_s
  {
    Dwarf_Debug dbg;
    Dwarf_Unsigned offset;
  } *Dwarf_Global_Info;

/* Representation of a global name entry.  */
struct Dwarf_Global_s
  {
    Dwarf_Unsigned offset;
    char *name;
    Dwarf_Global_Info info;
  };


/* Additional, shared information required for Dwarf_Arange handling.  */
typedef struct Dwarf_Arange_Info_s
  {
    Dwarf_Debug dbg;
    Dwarf_Unsigned offset;
  } *Dwarf_Arange_Info;

/* Representation of an address range entry.  */
struct Dwarf_Arange_s
  {
    Dwarf_Addr address;
    Dwarf_Unsigned length;
    Dwarf_Arange_Info info;
  };


/* Representation of a common information entry.  */
struct Dwarf_Cie_s
  {
    Dwarf_Debug dbg;
    Dwarf_Unsigned length;
    char *augmentation;
    Dwarf_Unsigned code_alignment_factor;
    Dwarf_Signed data_alignment_factor;
    Dwarf_Small *initial_instructions;
    Dwarf_Unsigned initial_instructions_length;
    Dwarf_Small return_address_register;
    Dwarf_Unsigned offset;
    Dwarf_Signed index;
  };


/* Representation of a frame descriptor entry.  */
struct Dwarf_Fde_s
  {
    Dwarf_Cie cie;
    Dwarf_Addr initial_location;
    Dwarf_Unsigned address_range;
    Dwarf_Small *instructions;
    Dwarf_Unsigned instructions_length;
    Dwarf_Unsigned offset;
    Dwarf_Small *fde_bytes;
    Dwarf_Unsigned fde_byte_length;
  };


/* Internal error values.  */
enum
  {
    DW_E_NOERROR = 0,
    DW_E_INVALID_ACCESS,
    DW_E_NO_REGFILE,
    DW_E_IO_ERROR,
    DW_E_NOMEM,
    DW_E_NOELF,
    DW_E_GETEHDR_ERROR,
    DW_E_INVALID_ELF,
    DW_E_INVALID_DWARF,
    DW_E_NO_DWARF,
    DW_E_NO_CU,
    DW_E_1ST_NO_CU,
    DW_E_INVALID_OFFSET,
    DW_E_INVALID_REFERENCE,
    DW_E_NO_REFERENCE,
    DW_E_NO_ADDR,
    DW_E_NO_FLAG,
    DW_E_NO_CONSTANT,
    DW_E_NO_BLOCK,
    DW_E_NO_STRING,
    DW_E_WRONG_ATTR,
    DW_E_NO_DATA,
    DW_E_NO_DEBUG_LINE,
    DW_E_VERSION_ERROR,
    DW_E_INVALID_DIR_IDX,
    DW_E_INVALID_ADDR,
    DW_E_NO_ABBR,
  };


/* Handle error according to user's wishes.  */
extern void __libdwarf_error (Dwarf_Debug dbg, Dwarf_Error *err, int errval)
     internal_function
	/*@modifies *err @*/;


/* Find CU at given offset.  */
extern int __libdwarf_get_cu_at_offset (Dwarf_Debug dbg, Dwarf_Unsigned offset,
					Dwarf_CU_Info *result_cu,
					Dwarf_Error *err) internal_function
	/*@modifies dbg, *result_cu, *err @*/;

/* Find abbreviation.  */
/*@null@*/
extern Dwarf_Abbrev __libdwarf_get_abbrev (Dwarf_Debug dbg,
					   Dwarf_CU_Info cu,
					   Dwarf_Word code,
					   Dwarf_Error *err)
     internal_function
	/*@modifies cu, *err @*/;

/* Get constant type attribute value.  */
extern int __libdwarf_getconstant (Dwarf_Die die, Dwarf_Half name,
				   Dwarf_Unsigned *return_size,
				   Dwarf_Error *err) internal_function
	/*@modifies die, *return_size, *err @*/;

/* Determine length of form parameters.  */
extern int __libdwarf_form_val_len (Dwarf_Debug dbg, Dwarf_CU_Info cu,
				    Dwarf_Word form, Dwarf_Small *valp,
				    size_t *len, Dwarf_Error *err)
     internal_function
	/*@modifies *valp, *len, *err @*/;


/* gettext helper macros.  */
#define _(Str) dgettext ("libdwarf", Str)
#define N_(Str) Str

#endif	/* libdwarfP.h */
