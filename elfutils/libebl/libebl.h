/* Interface for libebl.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.

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

#ifndef _LIBEBL_H
#define _LIBEBL_H 1

#include <gelf.h>
#include <ltdl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <elf-knowledge.h>


/* Backend handle.  */
typedef struct ebl
{
  /* Machine name.  */
  const char *name;

  /* Emulation name.  */
  const char *emulation;

  /* The libelf handle (if known).  */
  Elf *elf;

  /* Return symbol representaton of object file type.  */
  const char *(*object_type_name) (int, char *, size_t);

  /* Return symbolic representation of relocation type.  */
  const char *(*reloc_type_name) (int, char *, size_t);

  /* Check relocation type.  */
  bool (*reloc_type_check) (int);

  /* Return symbolic representation of segment type.  */
  const char *(*segment_type_name) (int, char *, size_t);

  /* Return symbolic representation of section type.  */
  const char *(*section_type_name) (int, char *, size_t);

  /* Return section name.  */
  const char *(*section_name) (int, int, char *, size_t);

  /* Return next machine flag name.  */
  const char *(*machine_flag_name) (GElf_Word *);

  /* Check whether machine flags are valid.  */
  bool (*machine_flag_check) (GElf_Word);

  /* Return symbolic representation of symbol type.  */
  const char *(*symbol_type_name) (int, char *, size_t);

  /* Return symbolic representation of symbol binding.  */
  const char *(*symbol_binding_name) (int, char *, size_t);

  /* Return symbolic representation of dynamic tag.  */
  const char *(*dynamic_tag_name) (int64_t, char *, size_t);

  /* Check dynamic tag.  */
  bool (*dynamic_tag_check) (int64_t);

  /* Combine section header flags values.  */
  GElf_Word (*sh_flags_combine) (GElf_Word, GElf_Word);

  /* Return symbolic representation of OS ABI.  */
  const char *(*osabi_name) (int, char *, size_t);

  /* Destructor for ELF backend handle.  */
  void (*destr) (struct ebl *);

  /* Internal data.  */
  lt_dlhandle dlhandle;
} Ebl;


/* Get backend handle for object associated with ELF handle.  */
extern Ebl *ebl_openbackend (Elf *elf);
/* Similar but without underlying ELF file.  */
extern Ebl *ebl_openbackend_machine (GElf_Half machine);
/* Similar but with emulation name given.  */
extern Ebl *ebl_openbackend_emulation (const char *emulation);

/* Free resources allocated for backend handle.  */
extern void ebl_closebackend (Ebl *bh);


/* Function to call the callback functions including default ELF
   handling.  */

/* Return backend name.  */
extern const char *ebl_backend_name (Ebl *ebl);

/* Return relocation type name.  */
extern const char *ebl_object_type_name (Ebl *ebl, int object,
					 char *buf, size_t len);

/* Return relocation type name.  */
extern const char *ebl_reloc_type_name (Ebl *ebl, int reloc,
					char *buf, size_t len);

/* Check relocation type.  */
extern bool ebl_reloc_type_check (Ebl *ebl, int reloc);

/* Return segment type name.  */
extern const char *ebl_segment_type_name (Ebl *ebl, int segment,
					  char *buf, size_t len);

/* Return section type name.  */
extern const char *ebl_section_type_name (Ebl *ebl, int section,
					  char *buf, size_t len);

/* Return section name.  */
extern const char *ebl_section_name (Ebl *ebl, int section, int xsection,
				     char *buf, size_t len,
				     const char *scnnames[], size_t shnum);

/* Return machine flag names.  */
extern const char *ebl_machine_flag_name (Ebl *ebl, GElf_Word flags,
					  char *buf, size_t len);

/* Check whether machine flag is valid.  */
extern bool ebl_machine_flag_check (Ebl *ebl, GElf_Word flags);

/* Return symbol type name.  */
extern const char *ebl_symbol_type_name (Ebl *ebl, int symbol,
					 char *buf, size_t len);

/* Return symbol binding name.  */
extern const char *ebl_symbol_binding_name (Ebl *ebl, int binding,
					    char *buf, size_t len);

/* Return dynamic tag name.  */
extern const char *ebl_dynamic_tag_name (Ebl *ebl, int64_t tag,
					 char *buf, size_t len);

/* Check dynamic tag.  */
extern bool ebl_dynamic_tag_check (Ebl *ebl, int64_t tag);

/* Return combined section header flags value.  */
extern GElf_Word ebl_sh_flags_combine (Ebl *ebl, GElf_Word flags1,
				       GElf_Word flags2);

/* Return symbolic representation of OS ABI.  */
extern const char *ebl_osabi_name (Ebl *ebl, int osabi, char *buf, size_t len);


/* ELF string table handling.  */
struct Ebl_Strtab;
struct Ebl_Strent;

/* Create new ELF string table object in memory.  */
extern struct Ebl_Strtab *ebl_strtabinit (bool nullstr);

/* Free resources allocated for ELF string table ST.  */
extern void ebl_strtabfree (struct Ebl_Strtab *st);

/* Add string STR (length LEN is != 0) to ELF string table ST.  */
extern struct Ebl_Strent *ebl_strtabadd (struct Ebl_Strtab *st,
					 const char *str, size_t len);

/* Finalize string table ST and store size and memory location information
   in DATA.  */
extern void ebl_strtabfinalize (struct Ebl_Strtab *st, Elf_Data *data);

/* Get offset in string table for string associated with SE.  */
extern size_t ebl_strtaboffset (struct Ebl_Strent *se);

/* Return the string associated with SE.  */
extern const char *ebl_string (struct Ebl_Strent *se);


/* ELF wide char string table handling.  */
struct Ebl_WStrtab;
struct Ebl_WStrent;

/* Create new ELF wide char string table object in memory.  */
extern struct Ebl_WStrtab *ebl_wstrtabinit (bool nullstr);

/* Free resources allocated for ELF wide char string table ST.  */
extern void ebl_wstrtabfree (struct Ebl_WStrtab *st);

/* Add string STR (length LEN is != 0) to ELF string table ST.  */
extern struct Ebl_WStrent *ebl_wstrtabadd (struct Ebl_WStrtab *st,
					   const wchar_t *str, size_t len);

/* Finalize string table ST and store size and memory location information
   in DATA.  */
extern void ebl_wstrtabfinalize (struct Ebl_WStrtab *st, Elf_Data *data);

/* Get offset in wide char string table for string associated with SE.  */
extern size_t ebl_wstrtaboffset (struct Ebl_WStrent *se);


/* Generic string table handling.  */
struct Ebl_GStrtab;
struct Ebl_GStrent;

/* Create new string table object in memory.  */
extern struct Ebl_GStrtab *ebl_gstrtabinit (unsigned int width, bool nullstr);

/* Free resources allocated for string table ST.  */
extern void ebl_gstrtabfree (struct Ebl_GStrtab *st);

/* Add string STR (length LEN is != 0) to string table ST.  */
extern struct Ebl_GStrent *ebl_gstrtabadd (struct Ebl_GStrtab *st,
					   const char *str, size_t len);

/* Finalize string table ST and store size and memory location information
   in DATA.  */
extern void ebl_gstrtabfinalize (struct Ebl_GStrtab *st, Elf_Data *data);

/* Get offset in wide char string table for string associated with SE.  */
extern size_t ebl_gstrtaboffset (struct Ebl_GStrent *se);

#endif	/* libebl.h */
