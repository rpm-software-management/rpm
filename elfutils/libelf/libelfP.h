/* Internal Interfaces for libelf.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifndef _LIBELFP_H
#define _LIBELFP_H 1

#include <ar.h>
#include <gelf.h>
#include <stdint.h>

#if defined(__LCLINT__)
typedef int bool;
#endif

/* gettext helper macros.  */
#define _(Str) dgettext ("libelf", Str)


/* Helper Macros to write 32 bit and 64 bit functions.  */
#define __elfw2_(Bits, Name) __elf##Bits##_##Name
#define elfw2_(Bits, Name) elf##Bits##_##Name
#define ElfW2_(Bits, Name) Elf##Bits##_##Name
#define ELFW2_(Bits, Name) ELF##Bits##_##Name
#define ELFW_(Name, Bits) Name##Bits
#define __elfw2(Bits, Name) __elfw2_(Bits, Name)
#define elfw2(Bits, Name) elfw2_(Bits, Name)
#define ElfW2(Bits, Name) ElfW2_(Bits, Name)
#define ELFW2(Bits, Name) ELFW2_(Bits, Name)
#define ELFW(Name, Bits)  ELFW_(Name, Bits)


/* Sizes of the external types, for 32 bits objects.  */
#define ELF32_FSZ_ADDR   4
#define ELF32_FSZ_OFF    4
#define ELF32_FSZ_HALF   2
#define ELF32_FSZ_WORD   4
#define ELF32_FSZ_SWORD  4
#define ELF32_FSZ_XWORD  8
#define ELF32_FSZ_SXWORD 8

/* Same for 64 bits objects.  */
#define ELF64_FSZ_ADDR   8
#define ELF64_FSZ_OFF    8
#define ELF64_FSZ_HALF   2
#define ELF64_FSZ_WORD   4
#define ELF64_FSZ_SWORD  4
#define ELF64_FSZ_XWORD  8
#define ELF64_FSZ_SXWORD 8


/* This is an extension of the ELF_F_* enumeration.  The values here are
   not part of the library interface, they are only used internally.  */
enum
{
  ELF_F_MMAPPED = 0x40,
  ELF_F_MALLOCED = 0x80,
  ELF_F_FILEDATA = 0x100
};


/* Get definition of all the external types.  */
#include "exttypes.h"


/* Error values.  */
enum
{
  ELF_E_NOERROR = 0,
  ELF_E_UNKNOWN_VERSION,
  ELF_E_UNKNOWN_TYPE,
  ELF_E_INVALID_HANDLE,
  ELF_E_SOURCE_SIZE,
  ELF_E_DEST_SIZE,
  ELF_E_INVALID_ENCODING,
  ELF_E_NOMEM,
  ELF_E_INVALID_FILE,
  ELF_E_INVALID_OP,
  ELF_E_NO_VERSION,
  ELF_E_INVALID_CMD,
  ELF_E_RANGE,
  ELF_E_ARCHIVE_FMAG,
  ELF_E_INVALID_ARCHIVE,
  ELF_E_NO_ARCHIVE,
  ELF_E_NO_INDEX,
  ELF_E_READ_ERROR,
  ELF_E_WRITE_ERROR,
  ELF_E_INVALID_CLASS,
  ELF_E_INVALID_INDEX,
  ELF_E_INVALID_OPERAND,
  ELF_E_INVALID_SECTION,
  ELF_E_INVALID_COMMAND,
  ELF_E_WRONG_ORDER_EHDR,
  ELF_E_FD_DISABLED,
  ELF_E_FD_MISMATCH,
  ELF_E_OFFSET_RANGE,
  ELF_E_NOT_NUL_SECTION,
  ELF_E_DATA_MISMATCH,
  ELF_E_INVALID_SECTION_HEADER,
  ELF_E_INVALID_DATA,
  ELF_E_DATA_ENCODING,
  ELF_E_SECTION_TOO_SMALL,
  ELF_E_INVALID_ALIGN,
  ELF_E_INVALID_SHENTSIZE,
  ELF_E_UPDATE_RO,
  ELF_E_NOFILE,
  ELF_E_GROUP_NOT_REL,
  ELF_E_INVALID_PHDR,
  /* Keep this as the last entry.  */
  ELF_E_NUM
};


/* The visible `Elf_Data' type is not sufficent for some operations due
   to a misdesigned interface.  Extend it for internal purposes.  */
typedef struct
{
  Elf_Data d;
  Elf_Scn *s;
} Elf_Data_Scn;


/* List of `Elf_Data' descriptors.  This is what makes up the section
   contents.  */
typedef struct Elf_Data_List
{
  /* `data' *must* be the first element in the struct.  */
  Elf_Data_Scn data;
  struct Elf_Data_List *next;
  int flags;
} Elf_Data_List;


/* Descriptor for ELF section.  */
struct Elf_Scn
{
  /* We have to distinguish several different situations:

     1. the section is user created.  Therefore there is no file or memory
        region to read the data from.  Here we have two different subcases:

        a) data was not yet added (before the first `elf_newdata' call)

        b) at least one data set is available

     2. this is a section from a file/memory region.  We have to read the
        current content in one data block if we have to.  But we don't
        read the data until it is necessary.  So we have the subcases:

        a) the section in the file has size zero (for whatever reason)

        b) the data of the file is not (yet) read

        c) the data is read and available.

     In addition to this we have different data sets, the raw and the converted
     data.  This distinction only exists for the data read from the file.
     All user-added data set (all but the first when read from the file or
     all of them for user-create sections) are the same in both formats.
     We don't create the converted data before it is necessary.

     The `data_read' element signals whether data is available in the
     raw format.

     If there is data from the file/memory region or if read one data
     set is added the `rawdata_list_read' pointer in non-NULL and points
     to the last filled data set.  `raw_datalist_rear' is therefore NULL
     only if there is no data set at all.

     This so far allows to distinguish all but two cases (given that the
     `rawdata_list' and `data_list' entries are initialized to zero) is
     between not yet loaded data from the file/memory region and a section
     with zero size and type ELF_T_BYTE.   */
  Elf_Data_List data_list;	/* List of data buffers.  */
  Elf_Data_List *data_list_rear; /* Pointer to the rear of the data list. */

  Elf_Data_Scn rawdata;		/* Uninterpreted data of the section.  */

  int data_read;		/* Nonzero if the section was created by the
				   user or if the data from the file/memory
				   is read.  */

  size_t index;			/* Index of this section.  */
  struct Elf *elf;		/* The underlying ELF file.  */

  union
  {
    Elf32_Shdr *e32;		/* Pointer to 32bit section header.  */
    Elf64_Shdr *e64;		/* Pointer to 64bit section header.  */
  } shdr;

  unsigned int shdr_flags;	/* Section header modified?  */
  unsigned int flags;		/* Section changed in size?  */

  char *rawdata_base;		/* The unmodified data of the section.  */
  char *data_base;		/* The converted data of the section.  */

  struct Elf_ScnList *list;	/* Pointer the the section list element the
				   data is in.  */
};


/* List of section.  */
typedef struct Elf_ScnList
{
  unsigned int cnt;		/* Number of elements of 'data' used.  */
  unsigned int max;		/* Number of elements of 'data' allocated.  */
  struct Elf_ScnList *next;	/* Next block of sections.  */
  struct Elf_Scn data[0];	/* Section data.  */
} Elf_ScnList;


/* The ELF descriptor.  */
struct Elf
{
  /* What kind of file is underneath (ELF file, archive...).  */
  Elf_Kind kind;

  /* Command used to create this descriptor.  */
  Elf_Cmd cmd;

  /* The binary class.  */
  unsigned int class;

  /* The used file descriptor.  -1 if not available anymore.  */
  int fildes;

  /* Offset in the archive this file starts or zero.  */
  off_t start_offset;

  /* Size of the file in the archive or the entire file size, or ~0
     for an (yet) unknown size.  */
  size_t maximum_size;

  /* Address to which the file was mapped.  NULL if not mapped.  */
/*@null@*/
  void *map_address;

  /* Describes the way the memory was allocated and if the dirty bit is
     signalled it means that the whole file has to be rewritten since
     the layout changed.  */
  int flags;

  /* When created for an archive member this points to the descriptor
     for the archive. */
/*@null@*/
  Elf *parent;

  /* Lock to handle multithreaded programs.  */
  rwlock_define (,lock);

  /* Reference counting for the descriptor.  */
  int ref_count;

  struct Elf *next;             /* Used in list of archive descriptors.  */

  union
  {
    struct
    {
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */

      /* The next fields are only useful when testing for ==/!= NULL.  */
      void *ehdr;
      void *shdr;
      void *phdr;

      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
    } elf;

    struct
    {
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */

      Elf32_Ehdr *ehdr;		/* Pointer to the ELF header.  This is
				   never malloced.  */
      Elf32_Shdr *shdr;		/* Used when reading from a file.  */
      Elf32_Phdr *phdr;		/* Pointer to the program header array.  */
      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
      Elf32_Ehdr ehdr_mem;	/* Memory used for ELF header when not
				   mmaped.  */
      char __e32scnspad[sizeof (Elf64_Ehdr) - sizeof (Elf32_Ehdr)];

      /* The section array.  */
      Elf_ScnList scns;
    } elf32;

    struct
    {
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */

      Elf64_Ehdr *ehdr;		/* Pointer to the ELF header.  This is
				   never malloced.  */
      Elf64_Shdr *shdr;		/* Used when reading from a file.  */
      Elf64_Phdr *phdr;		/* Pointer to the program header array.  */
      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
      Elf64_Ehdr ehdr_mem;	/* Memory used for ELF header when not
				   mmaped.  */

      /* The section array.  */
      Elf_ScnList scns;
    } elf64;

    struct
    {
      int has_index;		/* Set when file has index.  0 means
				   undecided, > 0 means it has one.  */
      Elf_Arsym *ar_sym;	/* Symbol table returned by elf_getarsym.  */
      size_t ar_sym_num;	/* Number of entries in `ar_sym'.  */
      char *long_names;		/* If no index is available but long names
				   are used this elements points to the data.*/
      size_t long_names_len;	/* Length of the long name table.  */
      off_t offset;		/* Offset in file we are currently at.
				   elf_next() advances this to the next
				   member of the archive.  */
      Elf_Arhdr elf_ar_hdr;	/* Structure returned by 'elf_getarhdr'.  */
      struct ar_hdr ar_hdr;	/* Header read from file.  */
      char ar_name[16];		/* NUL terminated ar_name of elf_ar_hdr.  */
      char raw_name[17];	/* This is a buffer for the NUL terminated
				   named raw_name used in the elf_ar_hdr.  */
      struct Elf *children;	/* List of all descriptors for this archive. */
    } ar;
  } state;

  /* There absolutely never must be anything following the union.  */
};


/* Type of the conversion functions.  These functions will convert the
   byte order.  */
typedef void (*xfct_t) (void *, const void *, size_t, int)
	/*@*/;

/* The table with the function pointers.  */
/*@unchecked@*/
extern const xfct_t __elf_xfctstom[EV_NUM - 1][EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;
/*@unchecked@*/
extern const xfct_t __elf_xfctstof[EV_NUM - 1][EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;


/* Array with sizes of the external types indexed by ELF version, binary
   class, and type. */
/*@unchecked@*/
extern const size_t __libelf_type_sizes[EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;
/* We often have to access the size for a type in the current version.  */
#if EV_NUM != 2
# define elf_typesize(class,type,n) \
  elfw2(class,fsize) (type, n, __libelf_version)
#else
# define elf_typesize(class,type,n) \
  (__libelf_type_sizes[EV_CURRENT - 1][ELFW(ELFCLASS,class) - 1][type] * n)
#endif

/* Currently selected version of the ELF specification.  */
/*@unchecked@*/
extern unsigned int __libelf_version attribute_hidden;

/* The byte value used for filling gaps.  */
/*@unchecked@*/
extern int __libelf_fill_byte attribute_hidden;

/* Nonzero if the version was set.  */
/*@unchecked@*/
extern int __libelf_version_initialized attribute_hidden;


/* The libelf API does not have such a function but it is still useful.
   Get the memory size for the given type.

   These functions cannot be marked internal since they are aliases
   of the export elfXX_fsize functions.*/
extern size_t __elf32_msize (Elf_Type __type, size_t __count,
			     unsigned int __version)
	/*@*/;
extern size_t __elf64_msize (Elf_Type __type, size_t __count,
			     unsigned int __version)
	/*@*/;


/* Create Elf descriptor from memory image.  */
/*@null@*/
extern Elf *__libelf_read_mmaped_file (int fildes, void *map_address,
				       off_t offset, size_t maxsize,
				       Elf_Cmd cmd, /*@null@*/ Elf *parent)
     internal_function
	/*@*/;

/* Set error value.  */
extern void __libelf_seterrno (int value) internal_function
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/* Get the next archive header.  */
extern int __libelf_next_arhdr (Elf *elf) internal_function
	/*@modifies elf @*/;

/* Read all of the file associated with the descriptor.  */
/*@null@*/
extern char *__libelf_readall (Elf *elf) internal_function
	/*@modifies elf @*/;

/* Read the complete section table and convert the byte order if necessary.  */
extern int __libelf_readsections (Elf *elf) internal_function
	/*@*/;

/* Store the information for the raw data in the `rawdata_list' element.  */
extern int __libelf_set_rawdata (Elf_Scn *scn) internal_function
	/*@modifies scn @*/;


/* Helper functions for elf_update.  */
extern off_t __elf32_updatenull (Elf *elf, int *change_bop, size_t shnum)
     internal_function
	/*@modifies elf, *change_bop @*/;
extern off_t __elf64_updatenull (Elf *elf, int *change_bop, size_t shnum)
     internal_function
	/*@modifies elf, *change_bop @*/;

extern int __elf32_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function
	/*@modifies elf @*/;
extern int __elf64_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function
	/*@modifies elf @*/;
extern int __elf32_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function
	/*@modifies elf @*/;
extern int __elf64_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function
	/*@modifies elf @*/;


/* Alias for exported functions to avoid PLT entries.  */
extern Elf *__elf_begin_internal (int __fildes, Elf_Cmd __cmd, Elf *__ref)
     attribute_hidden
	/*@*/;
extern Elf32_Ehdr *__elf32_getehdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf64_Ehdr *__elf64_getehdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf32_Ehdr *__elf32_newehdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf64_Ehdr *__elf64_newehdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf32_Phdr *__elf32_getphdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf64_Phdr *__elf64_getphdr_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern Elf32_Phdr *__elf32_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden
	/*@*/;
extern Elf64_Phdr *__elf64_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden
	/*@*/;
extern int __elf_getshnum_internal (Elf *__elf, size_t *__dst)
     attribute_hidden
	/*@*/;
extern int __elf_getshstrndx_internal (Elf *__elf, size_t *__dst)
     attribute_hidden
	/*@*/;
extern Elf32_Shdr *__elf32_getshdr_internal (Elf_Scn *__scn) attribute_hidden
	/*@*/;
extern Elf64_Shdr *__elf64_getshdr_internal (Elf_Scn *__scn) attribute_hidden
	/*@*/;
extern Elf_Scn *__elf_getscn_internal (Elf *__elf, size_t __index)
     attribute_hidden
	/*@*/;
extern Elf_Scn *__elf_nextscn_internal (Elf *__elf, Elf_Scn *__scn)
     attribute_hidden
	/*@*/;
extern Elf_Data *__elf_getdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden
	/*@*/;
extern Elf_Data *__elf_rawdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden
	/*@*/;
extern char *__elf_strptr_internal (Elf *__elf, size_t __index,
				    size_t __offset) attribute_hidden
	/*@*/;
extern Elf_Data *__elf32_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden
	/*@*/;
extern Elf_Data *__elf64_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden
	/*@*/;
extern Elf_Data *__elf32_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden
	/*@*/;
extern Elf_Data *__elf64_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden
	/*@*/;
extern unsigned int __elf_version_internal (unsigned int __version)
     attribute_hidden
	/*@*/;
extern unsigned long int __elf_hash_internal (const char *__string)
       __attribute__ ((__pure__, visibility ("hidden")))
	/*@*/;
extern long int __elf32_checksum_internal (Elf *__elf) attribute_hidden
	/*@*/;
extern long int __elf64_checksum_internal (Elf *__elf) attribute_hidden
	/*@*/;


extern size_t __gelf_fsize_internal (Elf *__elf, Elf_Type __type,
				     size_t __count, unsigned int __version)
     attribute_hidden
	/*@*/;
extern GElf_Shdr *__gelf_getshdr_internal (Elf_Scn *__scn, GElf_Shdr *__dst)
     attribute_hidden
	/*@*/;
extern GElf_Sym *__gelf_getsym_internal (Elf_Data *__data, int __ndx,
					 GElf_Sym *__dst) attribute_hidden
	/*@*/;


/* We often have to update a flag iff a value changed.  Make this
   convenient.  None of the parameters must have a side effect.  */
#ifdef __GNUC__
# define update_if_changed(var, exp, flag) \
  do {									      \
    __typeof__ (var) *_var = &(var);					      \
    __typeof__ (exp) _exp = (exp);					      \
    if (*_var != _exp)							      \
      {									      \
	*_var = _exp;							      \
	(flag) |= ELF_F_DIRTY;						      \
      }									      \
  } while (0)
#else
# define update_if_changed(var, exp, flag) \
  do {									      \
    if ((var) != (exp))							      \
      {									      \
	(var) = (exp);							      \
	(flag) |= ELF_F_DIRTY;						      \
      }									      \
  } while (0)
#endif

#endif  /* libelfP.h */
