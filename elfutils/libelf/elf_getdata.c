/* Return the next data element from the section after possibly converting it.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "libelfP.h"
#include "common.h"


#if !defined(ALLOW_ALIGNED)
# define ALLOW_ALIGNED	0
#endif


#define TYPEIDX(Sh_Type) \
  (Sh_Type >= SHT_NULL && Sh_Type < SHT_NUM				      \
   ? Sh_Type								      \
   : (Sh_Type >= SHT_LOSUNW && Sh_Type <= SHT_HISUNW			      \
      ? SHT_NUM + Sh_Type - SHT_LOSUNW					      \
      : 0))

static const struct
{
  Elf_Type type;
  size_t size;
#if ALLOW_ALIGNED
# define AL(val)
#else
  size_t align;
# define AL(val), val
#endif
} shtype_map[EV_NUM - 1][ELFCLASSNUM - 1][TYPEIDX (SHT_HISUNW) + 1] =
{
  [EV_CURRENT - 1] =
  {
    [ELFCLASS32 - 1] =
    {
      /* Associate section types with libelf types, their sizes and
	 alignment.  SHT_GNU_verdef is special since the section does
	 not contain entries of only one size.  */
#define DEFINE(Bits) \
      [SHT_SYMTAB] = { ELF_T_SYM, sizeof (ElfW2(Bits,Sym))		      \
		       AL (__alignof__ (ElfW2(Bits,Sym))) },		      \
      [SHT_RELA] = { ELF_T_RELA, sizeof (ElfW2(Bits,Rela))		      \
		       AL (__alignof__ (ElfW2(Bits,Rela))) },		      \
      [SHT_HASH] = { ELF_T_WORD, sizeof (ElfW2(Bits,Word))		      \
		       AL (__alignof__ (ElfW2(Bits,Word))) },		      \
      [SHT_DYNAMIC] = { ELF_T_DYN, sizeof (ElfW2(Bits,Dyn))		      \
		       AL (__alignof__ (ElfW2(Bits,Dyn))) },		      \
      [SHT_REL] = { ELF_T_REL, sizeof (ElfW2(Bits,Rel))			      \
		       AL (__alignof__ (ElfW2(Bits,Rel))) },		      \
      [SHT_DYNSYM] = { ELF_T_SYM, sizeof (ElfW2(Bits,Sym))		      \
		       AL (__alignof__ (ElfW2(Bits,Sym))) },		      \
      [SHT_INIT_ARRAY] = { ELF_T_ADDR, sizeof (ElfW2(Bits,Addr))	      \
			   AL (__alignof__ (ElfW2(Bits,Addr))) },	      \
      [SHT_FINI_ARRAY] = { ELF_T_ADDR, sizeof (ElfW2(Bits,Addr))	      \
			   AL (__alignof__ (ElfW2(Bits,Addr))) },	      \
      [SHT_PREINIT_ARRAY] = { ELF_T_ADDR, sizeof (ElfW2(Bits,Addr))	      \
			      AL (__alignof__ (ElfW2(Bits,Addr))) },	      \
      [SHT_GROUP] = { ELF_T_WORD, sizeof (Elf32_Word)			      \
		      AL (__alignof__ (Elf32_Word)) },			      \
      [SHT_SYMTAB_SHNDX] = { ELF_T_WORD, sizeof (Elf32_Word)		      \
			     AL (__alignof__ (Elf32_Word)) },		      \
      [TYPEIDX (SHT_GNU_verdef)] = { ELF_T_VDEF, 1 AL (1) },		      \
      [TYPEIDX (SHT_GNU_verneed)] = { ELF_T_VNEED,			      \
				      sizeof (ElfW2(Bits,Verneed))	      \
				      AL (__alignof__ (ElfW2(Bits,Verneed)))},\
      [TYPEIDX (SHT_GNU_versym)] = { ELF_T_HALF, sizeof (ElfW2(Bits,Versym))  \
				     AL (__alignof__ (ElfW2(Bits,Versym))) }, \
      [TYPEIDX (SHT_SUNW_syminfo)] = { ELF_T_SYMINFO,			      \
				       sizeof (ElfW2(Bits,Syminfo))	      \
				       AL(__alignof__ (ElfW2(Bits,Syminfo)))},\
      [TYPEIDX (SHT_SUNW_move)] = { ELF_T_MOVE, sizeof (ElfW2(Bits,Move))    \
				    AL (__alignof__ (ElfW2(Bits,Move))) }
      DEFINE (32)
    },
    [ELFCLASS64 - 1] =
    {
      DEFINE (64)
    }
  }
};


/* Convert the data in the current section.  */
static void
convert_data (Elf_Scn *scn, int version, int eclass, int data,
	      size_t size, size_t type)
{
#if ALLOW_ALIGNED
  /* No need to compute the alignment requirement of the host.  */
  const size_t align = 1;
#else
# if EV_NUM != 2
  size_t align = shtype_map[__libelf_version - 1][eclass - 1][type].align;
# else
  size_t align = shtype_map[0][eclass - 1][type].align;
# endif
#endif

  if (data == MY_ELFDATA)
    {
      if (ALLOW_ALIGNED
	  || (((size_t) ((char *) scn->rawdata_base)) & (align - 1)) == 0)
	/* No need to copy, we can use the raw data.  */
	scn->data_base = scn->rawdata_base;
      else
	{
	  scn->data_base = (char *) malloc (size);
	  if (scn->data_base == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      return;
	    }

	  /* The copy will be appropriately aligned for direct access.  */
	  memcpy (scn->data_base, scn->rawdata_base, size);
	}
    }
  else
    {
      xfct_t fp;

      scn->data_base = (char *) malloc (size);
      if (scn->data_base == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  return;
	}

      /* Get the conversion function.  */
#if EV_NUM != 2
      fp = __elf_xfctstom[version - 1][__libelf_version - 1][eclass - 1][type];
#else
      fp = __elf_xfctstom[0][0][eclass - 1][type];
#endif

      fp (scn->data_base, scn->rawdata_base, size, 0);
    }

  scn->data_list.data.d.d_buf = scn->data_base;
  scn->data_list.data.d.d_size = size;
  scn->data_list.data.d.d_type = type;
  scn->data_list.data.d.d_off = scn->rawdata.d.d_off;
  scn->data_list.data.d.d_align = scn->rawdata.d.d_align;
  scn->data_list.data.d.d_version = scn->rawdata.d.d_version;

  scn->data_list.data.s = scn;
}


/* Store the information for the raw data in the `rawdata' element.  */
int
internal_function
__libelf_set_rawdata (Elf_Scn *scn)
{
  size_t offset;
  size_t size;
  size_t align;
  int type;
  Elf *elf = scn->elf;

  if (elf->class == ELFCLASS32)
    {
      Elf32_Shdr *shdr = scn->shdr.e32 ?: INTUSE(elf32_getshdr) (scn);

      if (shdr == NULL)
	/* Something went terribly wrong.  */
	return 1;

      offset = shdr->sh_offset;
      size = shdr->sh_size;
      type = shdr->sh_type;
      align = shdr->sh_addralign;
    }
  else
    {
      Elf64_Shdr *shdr = scn->shdr.e64 ?: INTUSE(elf64_getshdr) (scn);

      if (shdr == NULL)
	/* Something went terribly wrong.  */
	return 1;

      offset = shdr->sh_offset;
      size = shdr->sh_size;
      type = shdr->sh_type;
      align = shdr->sh_addralign;
    }

  /* If the section has no data (for whatever reason), leave the `d_buf'
     pointer NULL.  */
  if (size != 0 && type != SHT_NOBITS)
    {
      /* First a test whether the section is valid at all.  */
#if EV_NUM != 2
      size_t entsize = shtype_map[__libelf_version - 1][elf->class - 1][TYPEIDX (type)].size;
#else
      size_t entsize = shtype_map[0][elf->class - 1][TYPEIDX (type)].size;
#endif

      /* We assume it is an array of bytes if it is none of the structured
	 sections we know of.  */
      if (entsize == 0)
	entsize = 1;

      if (size % entsize != 0)
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  return 1;
	}

      /* We can use the mapped or loaded data if available.  */
      if (elf->map_address != NULL)
	{
	  /* First see whether the information in the section header is
	     valid and it does not ask for too much.  */
	  if (offset + size > elf->maximum_size)
	    {
	      /* Something is wrong.  */
	      __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
	      return 1;
	    }

	  scn->rawdata_base = scn->rawdata.d.d_buf
	    = (char *) elf->map_address + elf->start_offset + offset;
	}
      else if (elf->fildes != -1)
	{
	  /* We have to read the data from the file.  Allocate the needed
	     memory.  */
	  scn->rawdata_base = scn->rawdata.d.d_buf
	    = (char *) malloc (size);
	  if (scn->rawdata.d.d_buf == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      return 1;
	    }

	  if (pread (elf->fildes, scn->rawdata.d.d_buf, size,
		     elf->start_offset + offset) != size)
	    {
	      /* Cannot read the data.  */
	      free (scn->rawdata.d.d_buf);
	      scn->rawdata_base = scn->rawdata.d.d_buf = NULL;
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      return 1;
	    }
	}
      else
	{
	  /* The file descriptor is already closed, we cannot get the data
	     anymore.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);
	  return 1;
	}
    }

  scn->rawdata.d.d_size = size;
#if EV_NUM != 2
  scn->rawdata.d.d_type =
    shtype_map[__libelf_version - 1][elf->class - 1][TYPEIDX (type)].type;
#else
  scn->rawdata.d.d_type = shtype_map[0][elf->class - 1][TYPEIDX (type)].type;
#endif
  scn->rawdata.d.d_off = 0;
  scn->rawdata.d.d_align = align;
  if (elf->class == ELFCLASS32
      || (offsetof (struct Elf, state.elf32.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr)))
    scn->rawdata.d.d_version =
      elf->state.elf32.ehdr->e_ident[EI_VERSION];
  else
    scn->rawdata.d.d_version =
      elf->state.elf64.ehdr->e_ident[EI_VERSION];

  scn->rawdata.s = scn;

  scn->data_read = 1;

  /* We actually read data from the file.  At least we tried.  */
  scn->flags |= ELF_F_FILEDATA;

  return 0;
}


Elf_Data *
elf_getdata (scn, data)
     Elf_Scn *scn;
     Elf_Data *data;
{
  Elf_Data *result = NULL;
  Elf *elf;

  if (scn == NULL)
    return NULL;

  if (unlikely (scn->elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* We will need this multiple times later on.  */
  elf = scn->elf;

  rwlock_rdlock (elf->lock);

  /* If `data' is not NULL this means we are not addressing the initial
     data in the file.  But this also means this data is already read
     (since otherwise it is not possible to have a valid `data' pointer)
     and all the data structures are initialized as well.  In this case
     we can simply walk the list of data records.  */
  if (data != NULL)
    {
      Elf_Data_List *runp;

      /* It is not possible that if DATA is not NULL the first entry is
	 returned.  But this also means that there must be a first data
	 entry.  */
      if (scn->data_list_rear == NULL
	  /* The section the reference data is for must match the section
	     parameter.  */
	  || unlikely (((Elf_Data_Scn *) data)->s != scn))
	{
	  __libelf_seterrno (ELF_E_DATA_MISMATCH);
	  goto out;
	}

      /* We start searching with the first entry.  */
      runp = &scn->data_list;

      while (1)
	{
	  /* If `data' does not match any known record punt.  */
	  if (runp == NULL)
	    {
	      __libelf_seterrno (ELF_E_DATA_MISMATCH);
	      goto out;
	    }

	  if (&runp->data.d == data)
	    /* Found the entry.  */
	    break;

	  runp = runp->next;
	}

      /* Return the data for the next data record.  */
      result = runp->next ? &runp->next->data.d : NULL;
      goto out;
    }

  /* If the data for this section was not yet initialized do it now.  */
  if (scn->data_read == 0)
    {
      /* We cannot acquire a write lock while we are holding a read
         lock.  Therefore give up the read lock and then get the write
         lock.  But this means that the data could meanwhile be
         modified, therefore start the tests again.  */
      rwlock_unlock (elf->lock);
      rwlock_wrlock (elf->lock);

      /* Read the data from the file.  There is always a file (or
	 memory region) associated with this descriptor since
	 otherwise the `data_read' flag would be set.  */
      if (scn->data_read == 0 && __libelf_set_rawdata (scn) != 0)
	/* Something went wrong.  The error value is already set.  */
	goto out;
    }

  /* At this point we know the raw data is available.  But it might be
     empty in case the section has size zero (for whatever reason).
     Now create the converted data in case this is necessary.  */
  if (scn->data_list_rear == NULL)
    {
      if (scn->rawdata.d.d_buf != NULL && scn->rawdata.d.d_size > 0)
	/* Convert according to the version and the type.   */
	convert_data (scn, __libelf_version, elf->class,
		      (elf->class == ELFCLASS32
		       || (offsetof (struct Elf, state.elf32.ehdr)
			   == offsetof (struct Elf, state.elf64.ehdr))
		       ? elf->state.elf32.ehdr->e_ident[EI_DATA]
		       : elf->state.elf64.ehdr->e_ident[EI_DATA]),
			  scn->rawdata.d.d_size,
		      scn->rawdata.d.d_type);
      else
	/* This is an empty or NOBITS section.  There is no buffer but
	   the size information etc is important.  */
	scn->data_list.data.d = scn->rawdata.d;

      scn->data_list_rear = &scn->data_list;
    }

  /* If no data is present we cannot return any.  */
  if (scn->data_list_rear != NULL)
    /* Return the first data element in the list.  */
    result = &scn->data_list.data.d;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_getdata)
