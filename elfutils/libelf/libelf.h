/* Interface for libelf.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBELF_H
#define _LIBELF_H 1

#include <sys/types.h>

/* Get the ELF types.  */
#include <elf.h>


/* Known translation types.  */
typedef enum
{
  ELF_T_BYTE,                   /* unsigned char */
  ELF_T_ADDR,                   /* Elf32_Addr, Elf64_Addr, ... */
  ELF_T_DYN,                    /* Dynamic section record.  */
  ELF_T_EHDR,                   /* ELF header.  */
  ELF_T_HALF,                   /* Elf32_Half, Elf64_Half, ... */
  ELF_T_OFF,                    /* Elf32_Off, Elf64_Off, ... */
  ELF_T_PHDR,                   /* Program header.  */
  ELF_T_RELA,                   /* Relocation entry with addend.  */
  ELF_T_REL,                    /* Relocation entry.  */
  ELF_T_SHDR,                   /* Section header.  */
  ELF_T_SWORD,                  /* Elf32_Sword, Elf64_Sword, ... */
  ELF_T_SYM,                    /* Symbol record.  */
  ELF_T_WORD,                   /* Elf32_Word, Elf64_Word, ... */
  ELF_T_XWORD,                  /* Elf32_Xword, Elf64_Xword, ... */
  ELF_T_SXWORD,                 /* Elf32_Sxword, Elf64_Sxword, ... */
  ELF_T_VDEF,                   /* Elf32_Verdef, Elf64_Verdef, ... */
  ELF_T_VDAUX,                  /* Elf32_Verdaux, Elf64_Verdaux, ... */
  ELF_T_VNEED,                  /* Elf32_Verneed, Elf64_Verneed, ... */
  ELF_T_VNAUX,                  /* Elf32_Vernaux, Elf64_Vernaux, ... */
  ELF_T_NHDR,                   /* Elf32_Nhdr, Elf64_Nhdr, ... */
  ELF_T_SYMINFO,		/* Elf32_Syminfo, Elf64_Syminfo, ... */
  ELF_T_MOVE,			/* Elf32_Move, Elf64_Move, ... */
  /* Keep this the last entry.  */
  ELF_T_NUM
} Elf_Type;

/* Descriptor for data to be converted to or from memory format.  */
typedef struct
{
  void *d_buf;			/* Pointer to the actual data.  */
  Elf_Type d_type;		/* Type of this piece of data.  */
  unsigned int d_version;	/* ELF version.  */
  size_t d_size;		/* Size in bytes.  */
  off_t d_off;			/* Offset into section.  */
  size_t d_align;		/* Alignment in section.  */
} Elf_Data;


/* Commands for `...'.  */
typedef enum
{
  ELF_C_NULL,			/* Nothing, terminate, or compute only.  */
  ELF_C_READ,			/* Read .. */
  ELF_C_RDWR,			/* Read and write .. */
  ELF_C_WRITE,			/* Write .. */
  ELF_C_CLR,			/* Clear flag.  */
  ELF_C_SET,			/* Set flag.  */
  ELF_C_FDDONE,			/* Signal that file descriptor will not be
				   used anymore.  */
  ELF_C_FDREAD,			/* Read rest of data so that file descriptor
				   is not used anymore.  */
  /* The following are extensions.  */
  ELF_C_READ_MMAP,		/* Read, but mmap the file if possible.  */
  ELF_C_RDWR_MMAP,		/* Read and write, with mmap.  */
  ELF_C_WRITE_MMAP,		/* Write, with mmap.  */
  ELF_C_READ_MMAP_PRIVATE,	/* Read, but memory is writable, results are
				   not written to the file.  */
  /* Keep this the last entry.  */
  ELF_C_NUM
} Elf_Cmd;


/* Flags for the ELF structures.  */
enum
{
  ELF_F_DIRTY = 0x1,
#define ELF_F_DIRTY	ELF_F_DIRTY
  ELF_F_LAYOUT = 0x4
#define ELF_F_LAYOUT	ELF_F_LAYOUT
};


/* Identification values for recognized object files.  */
typedef enum
{
  ELF_K_NONE,			/* Unknown.  */
  ELF_K_AR,			/* Archive.  */
  ELF_K_COFF,			/* Stupid old COFF.  */
  ELF_K_ELF,			/* ELF file.  */
  /* Keep this the last entry.  */
  ELF_K_NUM
} Elf_Kind;


/* Archive member header.  */
typedef struct
{
  char *ar_name;		/* Name of archive member.  */
  time_t ar_date;		/* File date.  */
  uid_t ar_uid;			/* User ID.  */
  gid_t ar_gid;			/* Group ID.  */
  mode_t ar_mode;		/* File mode.  */
  off_t ar_size;		/* File size.  */
  char *ar_rawname;		/* Original name of archive member.  */
} Elf_Arhdr;


/* Archive symbol table entry.  */
typedef struct
{
  char *as_name;		/* Symbol name.  */
  size_t as_off;		/* Offset for this file in the archive.  */
  unsigned long int as_hash;	/* Hash value of the name.  */
} Elf_Arsym;


/* Descriptor for the ELF file.  */
typedef struct Elf Elf;

/* Descriptor for ELF file section.  */
typedef struct Elf_Scn Elf_Scn;


#ifdef __cplusplus
extern "C" {
#endif

/* Return descriptor for ELF file to work according to CMD.  */
/*@only@*/ /*@null@*/
extern Elf *elf_begin (int __fildes, Elf_Cmd __cmd,
		       /*@returned@*/ /*@null@*/ Elf *ref)
	/*@modifies ref @*/;

/* Create descriptor for memory region.  */
/*@null@*/
extern Elf *elf_memory (char *__image, size_t __size)
	/*@*/;

/* Advance archive descriptor to next element.  */
extern Elf_Cmd elf_next (Elf *elf)
	/*@modifies elf @*/;

/* Free resources allocated for ELF.  */
extern int elf_end (/*@only@*/ /*@null@*/ Elf *elf)
	/*@modifies elf @*/;

/* Update ELF descriptor and write file to disk.  */
extern off_t elf_update (Elf *elf, Elf_Cmd __cmd)
	/*@modifies elf @*/;

/* Determine what kind of file is associated with ELF.  */
extern Elf_Kind elf_kind (Elf *__elf) __attribute__ ((__pure__))
	/*@*/;

/* Get the base offset for an object file.  */
extern off_t elf_getbase (Elf *__elf)
	/*@*/;


/* Retrieve file identification data.  */
extern char *elf_getident (Elf *__elf, /*@null@*/ size_t *ptr)
	/*@modifies *ptr @*/;

/* Retrieve class-dependent object file header.  */
extern Elf32_Ehdr *elf32_getehdr (Elf *elf)
	/*@modifies elf @*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
extern Elf64_Ehdr *elf64_getehdr (Elf *elf)
	/*@modifies elf @*/;

/* Create ELF header if none exists.  */
extern Elf32_Ehdr *elf32_newehdr (Elf *elf)
	/*@modifies elf @*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
extern Elf64_Ehdr *elf64_newehdr (Elf *elf)
	/*@modifies elf @*/;

/* Retrieve class-dependent program header table.  */
extern Elf32_Phdr *elf32_getphdr (Elf *elf)
	/*@modifies elf @*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
extern Elf64_Phdr *elf64_getphdr (Elf *elf)
	/*@modifies elf @*/;

/* Create ELF program header.  */
/*@null@*/
extern Elf32_Phdr *elf32_newphdr (Elf *elf, size_t __cnt)
	/*@modifies elf @*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
/*@null@*/
extern Elf64_Phdr *elf64_newphdr (Elf *elf, size_t __cnt)
	/*@modifies elf @*/;


/* Get section at INDEX.  */
extern Elf_Scn *elf_getscn (Elf *__elf, size_t __index)
	/*@*/;

/* Get index of section.  */
extern size_t elf_ndxscn (Elf_Scn *__scn)
	/*@*/;

/* Get section with next section index.  */
extern Elf_Scn *elf_nextscn (Elf *__elf, /*@null@*/ Elf_Scn *__scn)
	/*@*/;

/* Create a new section and append it at the end of the table.  */
extern Elf_Scn *elf_newscn (Elf *elf)
	/*@modifies elf @*/;

/* Get the number of sections in the ELF file.  If the file uses more
   sections than can be represented in the e_shnum field of the ELF
   header the information from the sh_size field in the zeroth section
   header is used.  */
extern int elf_getshnum (Elf *__elf, /*@out@*/ size_t *dst)
	/*@modifies *dst @*/;


/* Get the section index of the section header string table in the ELF
   file.  If the index cannot be represented in the e_shnum field of
   the ELF header the information from the sh_link field in the zeroth
   section header is used.  */
extern int elf_getshstrndx (Elf *__elf, /*@out@*/ size_t *dst)
	/*@modifies *dst @*/;


/* Retrieve section header of ELFCLASS32 binary.  */
extern Elf32_Shdr *elf32_getshdr (Elf_Scn *scn)
	/*@modifies scn @*/;
/* Similar for ELFCLASS64.  */
extern Elf64_Shdr *elf64_getshdr (Elf_Scn *scn)
	/*@modifies scn @*/;


/* Set or clear flags for ELF file.  */
extern unsigned int elf_flagelf (Elf *elf, Elf_Cmd __cmd,
				 unsigned int __flags)
	/*@modifies elf @*/;
/* Similarly for the ELF header.  */
extern unsigned int elf_flagehdr (Elf *elf, Elf_Cmd __cmd,
				  unsigned int __flags)
	/*@modifies elf @*/;
/* Similarly for the ELF program header.  */
extern unsigned int elf_flagphdr (Elf *elf, Elf_Cmd __cmd,
				  unsigned int __flags)
	/*@modifies elf @*/;
/* Similarly for the given ELF section.  */
extern unsigned int elf_flagscn (Elf_Scn *scn, Elf_Cmd __cmd,
				 unsigned int __flags)
	/*@modifies scn @*/;
/* Similarly for the given ELF data.  */
extern unsigned int elf_flagdata (Elf_Data *__data, Elf_Cmd __cmd,
				  unsigned int __flags)
	/*@*/;
/* Similarly for the given ELF section header.  */
extern unsigned int elf_flagshdr (Elf_Scn *scn, Elf_Cmd __cmd,
				  unsigned int __flags)
	/*@modifies scn @*/;


/* Get data from section while translating from file representation
   to memory representation.  */
/*@null@*/
extern Elf_Data *elf_getdata (Elf_Scn *scn, /*@null@*/ Elf_Data *__data)
	/*@modifies scn @*/;

/* Get uninterpreted section content.  */
extern Elf_Data *elf_rawdata (Elf_Scn *scn, Elf_Data *__data)
	/*@modifies scn @*/;

/* Create new data descriptor for section SCN.  */
extern Elf_Data *elf_newdata (Elf_Scn *scn)
	/*@modifies scn @*/;


/* Return pointer to string at OFFSET in section INDEX.  */
/*@dependent@*/ /*@null@*/
extern char *elf_strptr (Elf *elf, size_t __index, size_t __offset)
	/*@modifies elf @*/;


/* Return header of archive.  */
extern Elf_Arhdr *elf_getarhdr (Elf *elf)
	/*@modifies elf @*/;

/* Select archive element at OFFSET.  */
extern size_t elf_rand (Elf *elf, size_t __offset)
	/*@modifies elf @*/;

/* Get symbol table of archhive.  */
/*@null@*/
extern Elf_Arsym *elf_getarsym (Elf *elf, /*@out@*/ size_t *ptr)
	/*@modifies elf, ptr @*/;


/* Control ELF descriptor.  */
extern int elf_cntl (Elf *elf, Elf_Cmd __cmd)
	/*@modifies elf @*/;

/* Retrieve uninterpreted file contents.  */
extern char *elf_rawfile (Elf *elf, size_t *ptr)
	/*@modifies elf, *ptr @*/;


/* Return size of array of COUNT elements of the type denoted by TYPE
   in the external representation.  The binary class is taken from ELF.
   The result is based on version VERSION of the ELF standard.  */
extern size_t elf32_fsize (Elf_Type __type, size_t __count,
			   unsigned int __version)
       __attribute__ ((__const__))
	/*@*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
extern size_t elf64_fsize (Elf_Type __type, size_t __count,
			   unsigned int __version)
       __attribute__ ((__const__))
	/*@*/;


/* Convert data structure from the representation in the file represented
   by ELF to their memory representation.  */
extern Elf_Data *elf32_xlatetom (/*@returned@*/ Elf_Data *dest,
				 const Elf_Data *__src, unsigned int __encode)
	/*@modifies dest @*/;
/* Same for 64 bit class.  */
extern Elf_Data *elf64_xlatetom (/*@returned@*/ Elf_Data *dest,
				 const Elf_Data *__src, unsigned int __encode)
	/*@modifies dest @*/;

/* Convert data structure from to the representation in memory
   represented by ELF file representation.  */
extern Elf_Data *elf32_xlatetof (/*@returned@*/ Elf_Data *dest,
				 const Elf_Data *__src, unsigned int __encode)
	/*@modifies dest @*/;
/* Same for 64 bit class.  */
extern Elf_Data *elf64_xlatetof (/*@returned@*/ Elf_Data *dest,
				 const Elf_Data *__src, unsigned int __encode)
	/*@modifies dest @*/;


/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int elf_errno (void)
	/*@*/;

/* Return error string for ERROR.  If ERROR is zero, return error string
   for most recent error or NULL is none occurred.  If ERROR is -1 the
   behaviour is similar to the last case except that not NULL but a legal
   string is returned.  */
extern const char *elf_errmsg (int __error)
	/*@*/;


/* Coordinate ELF library and application versions.  */
extern unsigned int elf_version (unsigned int __version)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/* Set fill bytes used to fill holes in data structures.  */
extern void elf_fill (int __fill)
	/*@*/;

/* Compute hash value.  */
extern unsigned long int elf_hash (const char *__string)
       __attribute__ ((__pure__))
	/*@*/;


/* Compute simple checksum from permanent parts of the ELF file.  */
extern long int elf32_checksum (Elf *elf)
	/*@modifies elf @*/;
/* Similar but this time the binary calls is ELFCLASS64.  */
extern long int elf64_checksum (Elf *elf)
	/*@modifies elf @*/;

#ifdef __cplusplus
}
#endif

#endif  /* libelf.h */
