/* Interface for libdwarf.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBDWARF_H
#define _LIBDWARF_H 1

#include <libelf.h>
#include <stdint.h>

/* Basic data types.  */

/* Used for boolean values.  */
typedef int Dwarf_Bool;

/* Numeric values of different sizes.  */
typedef uint8_t Dwarf_Small;
typedef uint16_t Dwarf_Half;
typedef uint64_t Dwarf_Unsigned;
typedef int64_t Dwarf_Signed;

/* Offsets in the debugging sections.  */
typedef uint64_t Dwarf_Off;

/* Program counter value in the target object file.  */
typedef uint64_t Dwarf_Addr;

/* Address in the host process.  */
typedef void *Dwarf_Ptr;


/* Location record.  */
typedef struct
  {
    Dwarf_Small lr_atom;		/* Operation */
    Dwarf_Unsigned lr_number;		/* Operand */
    Dwarf_Unsigned lr_number2;		/* Possible second operand */
    Dwarf_Unsigned lr_offset;		/* Offset in location expression */
  } Dwarf_Loc;


/* Location description.  */
typedef struct
   {
     Dwarf_Addr ld_lopc;		/* Beginning of range */
     Dwarf_Addr ld_hipc;		/* End of range */
     Dwarf_Half ld_cents;		/* Number of location records */
     Dwarf_Loc *ld_s;			/* Array of location records */
  } Dwarf_Locdesc;


/* Error handler function.  */
typedef struct Dwarf_Error_s *Dwarf_Error;	/* Forward declaration.  */
typedef void (*Dwarf_Handler) (Dwarf_Error *, Dwarf_Ptr);

/* Descriptor for block of uninterpreted data.  */
typedef struct
  {
    Dwarf_Unsigned bl_len;
    Dwarf_Ptr bl_data;
  } Dwarf_Block;


/* Descriptor for libdwarf session.  */
typedef struct Dwarf_Debug_s *Dwarf_Debug;

/* Descriptor for DWARF DIE.  */
typedef struct Dwarf_Die_s *Dwarf_Die;

/* Descriptor for DWARF attribute list.  */
typedef struct Dwarf_Attribute_s *Dwarf_Attribute;

/* Descriptor for source lines.  */
typedef struct Dwarf_Line_s *Dwarf_Line;

/* Descriptor for global name.  */
typedef struct Dwarf_Global_s *Dwarf_Global;

/* Descriptor for address range.  */
typedef struct Dwarf_Arange_s *Dwarf_Arange;

/* Descriptor for common information entry.  */
typedef struct Dwarf_Cie_s *Dwarf_Cie;

/* Descriptor for frame descriptor entry.  */
typedef struct Dwarf_Fde_s *Dwarf_Fde;

/* Descriptor for abbreviations.  */
typedef struct Dwarf_Abbrev_s *Dwarf_Abbrev;


/* Return values.  */
enum
  {
    DW_DLV_NO_ENTRY = -1,	/* No error, but no entry.  */
    DW_DLV_OK = 0,		/* Success.  */
    DW_DLV_ERROR = 1,		/* Failure.  */
  };


/* Values for ACCESS parameter of 'dwarf_init' and 'dwarf_elf_init'.  */
enum
  {
    DW_DLC_READ = 0,		/* Read-only access.  */
    DW_DLC_WRITE = 1,		/* Write-only access.  */
    DW_DLC_RDWR = 2		/* Read-write access.  */
  };


/* Open file associates with FD for use with the other functions of
   this library.  Set the error handler and the parameter passed.  */
extern int dwarf_init (int fd, Dwarf_Unsigned access,
		       Dwarf_Handler errhand, Dwarf_Ptr errarg,
		       Dwarf_Debug *dbg, Dwarf_Error *errdesc)
	/*@modifies *dbg, *errdesc @*/;

/* Similar to `dwarf_init' but instead of a file descriptor of ELF
   descriptor is passed.  */
extern int dwarf_elf_init (Elf *elf, Dwarf_Unsigned access,
			   Dwarf_Handler errhand, Dwarf_Ptr errarg,
			   Dwarf_Debug *dbg, Dwarf_Error *errdesc)
	/*@modifies elf, *dbg, *errdesc @*/;

/* Return ELF handle.  */
extern int dwarf_get_elf_init (Dwarf_Debug dbg, Elf **elf,
			       Dwarf_Error *errdesc)
	/*@modifies *elf, *errdesc @*/;

/* Free resources allocated for debug handle.  */
extern int dwarf_finish (/*@only@*/ Dwarf_Debug dbg, Dwarf_Error *errdesc)
	/*@modifies dbg, *errdesc @*/;


/* Return information about current and find new compile unit header.  */
extern int dwarf_next_cu_header (Dwarf_Debug dbg,
				 Dwarf_Unsigned *cu_header_length,
				 Dwarf_Half *version_stamp,
				 Dwarf_Unsigned *abbrev_offset,
				 Dwarf_Half *address_size,
				 Dwarf_Unsigned *next_cu_header,
				 Dwarf_Error *errdesc)
	/*@modifies dbg, *cu_header_length, *version_stamp, *abbrev_offset,
		*address_size, *next_cu_header, *errdesc @*/;

/* Return sibling of given DIE.  */
extern int dwarf_siblingof (Dwarf_Debug dbg, Dwarf_Die die,
			    Dwarf_Die *return_sub, Dwarf_Error *errdesc)
	/*@modifies dbg, die, *return_sub, *errdesc @*/;

/* Return child of DIE.  */
extern int dwarf_child (Dwarf_Die die, Dwarf_Die *return_kid,
			Dwarf_Error *errdesc)
	/*@modifies die, *return_kid, *errdesc @*/;

/* Return DIE at given offset.  */
extern int dwarf_offdie (Dwarf_Debug dbg, Dwarf_Off offset,
			 Dwarf_Die *return_die, Dwarf_Error *errdesc)
	/*@modifies dbg, *return_die, *errdesc @*/;


/* Return tag of DIE.  */
extern int dwarf_tag (Dwarf_Die die, Dwarf_Half *tagval, Dwarf_Error *errdesc)
	/*@modifies *tagval, *errdesc @*/;

/* Return offset of DIE in .debug_info section.  */
extern int dwarf_dieoffset (Dwarf_Die die, Dwarf_Off *return_offset,
			    Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return offset of DIE in compile unit data.  */
extern int dwarf_die_CU_offset (Dwarf_Die die, Dwarf_Off *return_offset,
				Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return name attribute of DIE.  */
extern int dwarf_diename (Dwarf_Die die, char **return_name,
			  Dwarf_Error *errdesc)
	/*@modifies die, *return_name, *errdesc @*/;

/* Return list of attributes for DIE.  */
extern int dwarf_attrlist (Dwarf_Die die, Dwarf_Attribute **attrbuf,
			   Dwarf_Signed *attrcount, Dwarf_Error *errdesc)
	/*@modifies die, *attrbuf, *attrcount, *errdesc @*/;

/* Determine whether DIE has attribute specified of given type.  */
extern int dwarf_hasattr (Dwarf_Die die, Dwarf_Half attr,
			  Dwarf_Bool *return_bool, Dwarf_Error *errdesc)
	/*@modifies die, *return_bool, *errdesc @*/;

/* Return DIE attribute with specified of given type.  */
extern int dwarf_attr (Dwarf_Die die, Dwarf_Half attr,
		       Dwarf_Attribute *return_attr, Dwarf_Error *errdesc)
	/*@modifies die, *return_attr, *errdesc @*/;

/* Return low program counter value associated with die.  */
extern int dwarf_lowpc (Dwarf_Die die, Dwarf_Addr *return_lowpc,
			Dwarf_Error *errdesc)
	/*@modifies die, *return_lowpc, *errdesc @*/;

/* Return high program counter value associated with die.  */
extern int dwarf_highpc (Dwarf_Die die, Dwarf_Addr *return_lowpc,
			 Dwarf_Error *errdesc)
	/*@modifies die, *return_lowpc, *errdesc @*/;

/* Return byte size value associated with die.  */
extern int dwarf_bytesize (Dwarf_Die die, Dwarf_Unsigned *return_size,
			   Dwarf_Error *errdesc)
	/*@modifies die, *return_size, *errdesc @*/;

/* Return bit size value associated with die.  */
extern int dwarf_bitsize (Dwarf_Die die, Dwarf_Unsigned *return_size,
			  Dwarf_Error *errdesc)
	/*@modifies die, *return_size, *errdesc @*/;

/* Return bit offset value associated with die.  */
extern int dwarf_bitoffset (Dwarf_Die die, Dwarf_Unsigned *return_size,
			    Dwarf_Error *errdesc)
	/*@modifies die, *return_size, *errdesc @*/;

/* Return source language associated with die.  */
extern int dwarf_srclang (Dwarf_Die die, Dwarf_Unsigned *return_lang,
			  Dwarf_Error *errdesc)
	/*@modifies die, *return_lang, *errdesc @*/;

/* Return source language associated with die.  */
extern int dwarf_arrayorder (Dwarf_Die die, Dwarf_Unsigned *return_order,
			     Dwarf_Error *errdesc)
	/*@modifies die, *return_order, *errdesc @*/;


/* Determine whether attribute has given form.  */
extern int dwarf_hasform (Dwarf_Attribute attr, Dwarf_Half form,
			  Dwarf_Bool *return_hasform, Dwarf_Error *errdesc)
	/*@modifies *return_hasform, *errdesc @*/;

/* Return form of attribute.  */
extern int dwarf_whatform (Dwarf_Attribute attr, Dwarf_Half *return_form,
			   Dwarf_Error *errdesc)
	/*@modifies *return_form, *errdesc @*/;

/* Return code of attribute.  */
extern int dwarf_whatattr (Dwarf_Attribute attr, Dwarf_Half *return_attr,
			   Dwarf_Error *errdesc)
	/*@modifies *return_attr, *errdesc @*/;

/*  Return compile-unit relative offset of reference associated with form.  */
extern int dwarf_formref (Dwarf_Attribute attr, Dwarf_Off *return_offset,
			  Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/*  Return .debug_info section global offset of reference associated
    with form.  */
extern int dwarf_global_formref (Dwarf_Attribute attr,
				 Dwarf_Off *return_offset,
				 Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return address represented by attribute.  */
extern int dwarf_formaddr (Dwarf_Attribute attr, Dwarf_Addr *return_addr,
			   Dwarf_Error *errdesc)
	/*@modifies *return_addr, *errdesc @*/;

/* Return flag represented by attribute.  */
extern int dwarf_formflag (Dwarf_Attribute attr, Dwarf_Bool *return_bool,
			   Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;

/* Return unsigned constant represented by attribute.  */
extern int dwarf_formudata (Dwarf_Attribute attr, Dwarf_Unsigned *return_uval,
			    Dwarf_Error *errdesc)
	/*@modifies *return_uval, *errdesc @*/;

/* Return signed constant represented by attribute.  */
extern int dwarf_formsdata (Dwarf_Attribute attr, Dwarf_Signed *return_uval,
			    Dwarf_Error *errdesc)
	/*@modifies *return_uval, *errdesc @*/;

/* Return block of uninterpreted data represented by attribute.  */
extern int dwarf_formblock (Dwarf_Attribute attr, Dwarf_Block **return_block,
			    Dwarf_Error *errdesc)
	/*@modifies *return_block, *errdesc @*/;

/* Return string represented by attribute.  */
extern int dwarf_formstring (Dwarf_Attribute attr, char **return_string,
			     Dwarf_Error *errdesc)
	/*@modifies *return_string, *errdesc @*/;

/* Return location expression list.  */
extern int dwarf_loclist (Dwarf_Attribute attr, Dwarf_Locdesc **llbuf,
			  Dwarf_Signed *listlen, Dwarf_Error *errdesc)
	/*@modifies *llbuf, *listlen, *errdesc @*/;


/* Return source lines of compilation unit.  */
extern int dwarf_srclines (Dwarf_Die die, Dwarf_Line **linebuf,
			   Dwarf_Signed *linecount, Dwarf_Error *errdesc)
	/*@modifies die, *linebuf, *linecount, *errdesc @*/;

/* Return files used in compilation unit.  */
extern int dwarf_srcfiles (Dwarf_Die die, char ***srcfiles,
			   Dwarf_Signed *srcfilecount, Dwarf_Error *errdesc)
	/*@modifies die, *srcfiles, *srcfilecount, *errdesc @*/;

/* Determine whether line is the beginning of a statement.  */
extern int dwarf_linebeginstatement (Dwarf_Line line, Dwarf_Bool *return_bool,
				     Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;

/* Determine whether line is marked as ending a text sequence.  */
extern int dwarf_lineendsequence (Dwarf_Line line, Dwarf_Bool *return_bool,
				  Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;

/* Return source statement line number.  */
extern int dwarf_lineno (Dwarf_Line line, Dwarf_Unsigned *return_lineno,
			 Dwarf_Error *errdesc)
	/*@modifies *return_lineno, *errdesc @*/;

/* Return address associate with line.  */
extern int dwarf_lineaddr (Dwarf_Line line, Dwarf_Addr *return_lineaddr,
			   Dwarf_Error *errdesc)
	/*@modifies *return_lineaddr, *errdesc @*/;

/* Return column at which the statement begins.  */
extern int dwarf_lineoff (Dwarf_Line line, Dwarf_Signed *return_lineoff,
			  Dwarf_Error *errdesc)
	/*@modifies *return_lineoff, *errdesc @*/;

/* Return source file for line.  */
extern int dwarf_linesrc (Dwarf_Line line, char **return_linesrc,
			  Dwarf_Error *errdesc)
	/*@modifies *return_linesrc, *errdesc @*/;

/* Determine whether line is marked as beginning a basic block.  */
extern int dwarf_lineblock (Dwarf_Line line, Dwarf_Bool *return_bool,
			    Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;

/* Determine whether line is marked as ending the prologue.  */
extern int dwarf_lineprologueend (Dwarf_Line line, Dwarf_Bool *return_bool,
				  Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;

/* Determine whether line is marked as beginning the epilogue.  */
extern int dwarf_lineepiloguebegin (Dwarf_Line line, Dwarf_Bool *return_bool,
				    Dwarf_Error *errdesc)
	/*@modifies *return_bool, *errdesc @*/;


/* Return list of global definitions.  */
extern int dwarf_get_globals (Dwarf_Debug dbg, Dwarf_Global **globals,
			      Dwarf_Signed *return_count,
			      Dwarf_Error *errdesc)
	/*@modifies *globals, *return_count, *errdesc @*/;

/* Return name for global definition.  */
extern int dwarf_globname (Dwarf_Global global, char **return_name,
			   Dwarf_Error *errdesc)
	/*@modifies *return_name, *errdesc @*/;

/* Return DIE offset for global definition.  */
extern int dwarf_global_die_offset (Dwarf_Global global,
				    Dwarf_Off *return_offset,
				    Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return offset of header of compile unit containing the global definition. */
extern int dwarf_global_cu_offset (Dwarf_Global global,
				   Dwarf_Off *return_offset,
				   Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return name, DIE offset, and offset of the compile unit DIE for the
   global definition.  */
extern int dwarf_global_name_offsets (Dwarf_Global global, char **return_name,
				      Dwarf_Off *die_offset,
				      Dwarf_Off *cu_offset,
				      Dwarf_Error *errdesc)
	/*@modifies *return_name, *die_offset, *cu_offset, *errdesc @*/;


/* Find start of macro value.  */
extern char *dwarf_find_macro_value_start (char *macro_string)
	/*@*/;


/* Return string from debug string section.  */
extern int dwarf_get_str (Dwarf_Debug dbg, Dwarf_Off offset, char **string,
			  Dwarf_Signed *returned_str_len,
			  Dwarf_Error *errdesc)
	/*@modifies dbg, *string, *returned_str_len, *errdesc @*/;


/* Return list address ranges.  */
extern int dwarf_get_aranges (Dwarf_Debug dbg, Dwarf_Arange **aranges,
			      Dwarf_Signed *return_count,
			      Dwarf_Error *errdesc)
	/*@modifies *aranges, *return_count, *errdesc @*/;

/* Find matching range for address.  */
extern int dwarf_get_arange (Dwarf_Arange *aranges,
			     Dwarf_Unsigned arange_count, Dwarf_Addr address,
			     Dwarf_Arange *return_arange,
			     Dwarf_Error *errdesc)
	/*@modifies *return_arange, *errdesc @*/;

/* Return offset of compile unit DIE containing the range.  */
extern int dwarf_get_cu_die_offset (Dwarf_Arange arange,
				    Dwarf_Off *return_offset,
				    Dwarf_Error *errdesc)
	/*@modifies *return_offset, *errdesc @*/;

/* Return start, length, and CU DIE offset of range.  */
extern int dwarf_get_arange_info (Dwarf_Arange arange, Dwarf_Addr *start,
				  Dwarf_Unsigned *length,
				  Dwarf_Off *cu_die_offset,
				  Dwarf_Error *errdesc)
	/*@modifies *start, *length, *cu_die_offset, *errdesc @*/;


/* Frame descriptor handling.  */

/* Get frame descriptions.  GCC version using .eh_frame.  */
extern int dwarf_get_fde_list_eh (Dwarf_Debug dbg, Dwarf_Cie **cie_data,
				  Dwarf_Signed *cie_element_count,
				  Dwarf_Fde **fde_data,
				  Dwarf_Signed *fde_element_count,
				  Dwarf_Error *errdesc)
	/*@modifies dbg, *cie_data, *cie_element_count,
		*fde_data, *fde_element_count, *errdesc @*/;

/* Get CIE of FDE.  */
extern int dwarf_get_cie_of_fde (Dwarf_Fde fde, Dwarf_Cie *return_cie,
				 Dwarf_Error *errdesc)
	/*@modifies *return_cie, *errdesc @*/;

/* Get information about the function range.  */
extern int dwarf_get_fde_range (Dwarf_Fde fde, Dwarf_Addr *low_pc,
				Dwarf_Unsigned *func_length,
				Dwarf_Ptr *fde_bytes,
				Dwarf_Unsigned *fde_byte_length,
				Dwarf_Off *cie_offset, Dwarf_Signed *cie_index,
				Dwarf_Off *fde_offset, Dwarf_Error *errdesc)
	/*@modifies *low_pc, *func_length, *fde_bytes, *fde_byte_length,
		*cie_offset, *cie_index, *cie_length, *fde_offset, *errdesc @*/;

/* Get information about CIE.  */
extern int dwarf_get_cie_info (Dwarf_Cie cie, Dwarf_Unsigned *bytes_in_cie,
			       Dwarf_Small *version, char **augmenter,
			       Dwarf_Unsigned *code_alignment_factor,
			       Dwarf_Signed *data_alignment_factor,
			       Dwarf_Half *return_address_register,
			       Dwarf_Ptr *initial_instructions,
			       Dwarf_Unsigned *initial_instructions_length,
			       Dwarf_Error *errdesc)
	/*@modifies *bytes_in_cie, *version, *augmenter,
		*code_alignment_factor, *data_alignment_factor,
		*initial_instructions, *initial_instructions_length,
		*return_address_register, *errdesc @*/;

/* Get frame construction instructions of FDE.  */
extern int dwarf_get_fde_instr_bytes (Dwarf_Fde fde, Dwarf_Ptr *outinstrs,
				      Dwarf_Unsigned *outlen,
				      Dwarf_Error *errdesc)
	/*@modifies *outinstrs, *outlen, *errdesc @*/;

/* Get nth frame descriptions.  */
extern int dwarf_get_fde_n (Dwarf_Fde *fde_data, Dwarf_Unsigned fde_index,
			    Dwarf_Fde *returned_fde, Dwarf_Error *errdesc)
	/*@modifies *returned_fde, *errdesc @*/;

/* Find FDE for given address.  */
extern int dwarf_get_fde_at_pc (Dwarf_Fde *fde_data, Dwarf_Addr pc_of_interest,
				Dwarf_Fde *returned_fde, Dwarf_Addr *lopc,
				Dwarf_Addr *hipc, Dwarf_Error *errdesc)
	/*@modifies *returned_fde, *lopc, *hipc, *errdesc @*/;


/* Return location list entry.  */
extern int dwarf_get_loclist_entry (Dwarf_Debug dbg, Dwarf_Unsigned offset,
				    Dwarf_Addr *hipc_offset,
				    Dwarf_Addr *lopc_offset, Dwarf_Ptr *data,
				    Dwarf_Unsigned *entry_len,
				    Dwarf_Unsigned *next_entry,
				    Dwarf_Error *errdesc)
	/*@modifies *hipc_offset, *lopc_offset, *data,
		*entry_len, *next_entry, *errdesc @*/;


/* Get abbreviation record.  */
extern int dwarf_get_abbrev (Dwarf_Debug dbg,
			     Dwarf_Unsigned offset,
			     Dwarf_Abbrev *returned_abbrev,
			     Dwarf_Unsigned *length,
			     Dwarf_Unsigned *attr_count, Dwarf_Error *errdesc)
	/*@modifies *returned_abbrev, *length, *attr_count, *errdesc @*/;

/* Get tag of abbreviation record.  */
extern int dwarf_get_abbrev_tag (Dwarf_Abbrev abbrev, Dwarf_Half *return_tag,
				 Dwarf_Error *errdesc)
	/*@modifies *return_tag @*/;

/* Get code of abbreviation record.  */
extern int dwarf_get_abbrev_code (Dwarf_Abbrev abbrev,
				  Dwarf_Unsigned *return_code,
				  Dwarf_Error *errdesc)
	/*@modifies *return_code @*/;

/* Get children flag of abbreviation record.  */
extern int dwarf_get_abbrev_children_flag (Dwarf_Abbrev abbrev,
					   Dwarf_Signed *return_flag,
					   Dwarf_Error *errdesc)
	/*@modifies *return_flag @*/;

/* Get attribute from abbreviation record.  */
extern int dwarf_get_abbrev_entry (Dwarf_Abbrev abbrev, Dwarf_Signed idx,
				   Dwarf_Half *attr_num, Dwarf_Signed *form,
				   Dwarf_Off *offset, Dwarf_Error *errdesc)
	/*@modifies *attr_num, *form, *offset @*/;


/* Memory handling.  */

/* Values for ALLOC_TYPE parameter of 'dwarf_dealloc'.  */
enum
  {
    DW_DLA_NONE = 0,
    DW_DLA_STRING,		/* char* */
    DW_DLA_LOC,			/* Dwarf_Loc */
    DW_DLA_LOCDESC,		/* Dwarf_Locdesc */
    DW_DLA_ELLIST,		/* Dwarf_Ellist */
    DW_DLA_BOUNDS,		/* Dwarf_Bounds */
    DW_DLA_BLOCK,		/* Dwarf_Block */
    DW_DLA_DEBUG,		/* Dwarf_Debug */
    DW_DLA_DIE,			/* Dwarf_Die */
    DW_DLA_LINE,		/* Dwarf_Line */
    DW_DLA_ATTR,		/* Dwarf_Attribute */
    DW_DLA_TYPE,		/* Dwarf_Type */
    DW_DLA_SUBSCR,		/* Dwarf_Subscr */
    DW_DLA_GLOBAL,		/* Dwarf_Global */
    DW_DLA_ERROR,		/* Dwarf_Error */
    DW_DLA_LIST,		/* a list */
    DW_DLA_LINEBUF,		/* Dwarf_Line* */
    DW_DLA_ARANGE,		/* Dwarf_Arange */
    DW_DLA_ABBREV,		/* Dwarf_Abbrev */
    DW_DLA_FRAME_OP,		/* Dwarf_Frame_Op */
    DW_DLA_CIE,			/* Dwarf_Cie */
    DW_DLA_FDE,			/* Dwarf_Fde */
    DW_DLA_LOC_BLOCK,		/* Dwarf_Loc Block */
    DW_DLA_FRAME_BLOCK,		/* Dwarf_Frame Block */
    DW_DLA_FUNC,		/* Dwarf_Func */
    DW_DLA_TYPENAME,		/* Dwarf_Type */
    DW_DLA_VAR,			/* Dwarf_Var */
    DW_DLA_WEAK,		/* Dwarf_Weak */
    DW_DLA_ADDR,		/* Dwarf_Addr sized entries */
  };

/* Deallocate memory.  */
extern void dwarf_dealloc (Dwarf_Debug dbg, /*@only@*/ Dwarf_Ptr space,
			   Dwarf_Unsigned alloc_type)
	/*@modifies space @*/;


/* Determine size of address of the binary.  */
extern int dwarf_get_address_size (Dwarf_Debug dbg, Dwarf_Half *addr_size,
				   Dwarf_Error *errdesc)
	/*@modifies *addr_size @*/;


/* Return error number.  */
extern Dwarf_Unsigned dwarf_errno (Dwarf_Error errdesc)
	/*@*/;

/* Return string corresponding to error.  */
extern const char *dwarf_errmsg (Dwarf_Error errdesc)
	/*@*/;


/* Set new error handler.  */
extern Dwarf_Handler dwarf_seterrhand (Dwarf_Debug dbg, Dwarf_Handler errhand)
	/*@modifies dbg @*/;

/* Set new error handler argument.  */
extern Dwarf_Ptr dwarf_seterrarg (Dwarf_Debug dbg, Dwarf_Ptr errarg)
	/*@modifies dbg @*/;

#endif	/* libdwarf.h */
