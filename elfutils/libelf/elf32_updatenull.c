/* Update data structures for changes.
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

#include <assert.h>
#include <endian.h>
#include <libelf.h>
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif



static int
ELFW(default_ehdr,LIBELFBITS) (Elf *elf, ElfW2(LIBELFBITS,Ehdr) *ehdr,
			       size_t shnum, int *change_bop)
	/*@modifies elf, ehdr, *change_bop @*/
{
  /* Always write the magic bytes.  */
  if (memcmp (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0)
    {
      memcpy (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG);
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  /* Always set the file class.  */
  update_if_changed (ehdr->e_ident[EI_CLASS], ELFW(ELFCLASS,LIBELFBITS),
		     elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  /* Set the data encoding if necessary.  */
  if (unlikely (ehdr->e_ident[EI_DATA] == ELFDATANONE))
    {
      ehdr->e_ident[EI_DATA] =
	BYTE_ORDER == BIG_ENDIAN ? ELFDATA2MSB : ELFDATA2LSB;
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }
  else if (unlikely (ehdr->e_ident[EI_DATA] >= ELFDATANUM))
    {
      __libelf_seterrno (ELF_E_DATA_ENCODING);
      return 1;
    }
  else
    *change_bop = ((BYTE_ORDER == LITTLE_ENDIAN
		    && ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		   || (BYTE_ORDER == BIG_ENDIAN
		       && ehdr->e_ident[EI_DATA] != ELFDATA2MSB));

  /* Unconditionally overwrite the ELF version.  */
  update_if_changed (ehdr->e_ident[EI_VERSION], EV_CURRENT,
		     elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  if (unlikely (ehdr->e_version == EV_NONE)
      || unlikely (ehdr->e_version >= EV_NUM))
    {
      __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
      return 1;
    }

  if (unlikely (shnum >= SHN_LORESERVE))
    {
      update_if_changed (ehdr->e_shnum, 0,
			 elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);
    }
  else
    update_if_changed (ehdr->e_shnum, shnum,
		       elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  if (unlikely (ehdr->e_ehsize != elf_typesize (LIBELFBITS, ELF_T_EHDR, 1)))
    {
      ehdr->e_ehsize = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  return 0;
}


off_t
internal_function
__elfw2(LIBELFBITS,updatenull) (Elf *elf, int *change_bop, size_t shnum)
{
  ElfW2(LIBELFBITS,Ehdr) *ehdr = INTUSE(elfw2(LIBELFBITS,getehdr)) (elf);
  int changed = 0;
  int ehdr_flags = 0;
  off_t size;

  /* Set the default values.  */
  if (ELFW(default_ehdr,LIBELFBITS) (elf, ehdr, shnum, change_bop) != 0)
    return -1;

  /* At least the ELF header is there.  */
  size = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);

  /* Set the program header position.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL)
    {
      /* Only executables or shared objects have a program header.  */
      if (ehdr->e_type != ET_EXEC && unlikely (ehdr->e_type != ET_DYN))
	{
	  __libelf_seterrno (ELF_E_INVALID_PHDR);
	  return -1;
	}

      if (elf->flags & ELF_F_LAYOUT)
	{
	  /* The user is supposed to fill out e_phoff.  Use it and
	     e_phnum to determine the maximum extend.  */
	  size = MAX (size,
		      ehdr->e_phoff
		      + elf_typesize (LIBELFBITS, ELF_T_PHDR, ehdr->e_phnum));
	}
      else
	{
	  update_if_changed (ehdr->e_phoff,
			     elf_typesize (LIBELFBITS, ELF_T_EHDR, 1),
			     ehdr_flags);

	  /* We need no alignment here.  */
	  size += elf_typesize (LIBELFBITS, ELF_T_PHDR, ehdr->e_phnum);
	}
    }

  if (shnum > 0)
    {
      Elf_ScnList *list;
      bool first = true;

      assert (elf->state.ELFW(elf,LIBELFBITS).scns.cnt > 0);

      if (shnum >= SHN_LORESERVE)
	{
	  /* We have to  fill in the number of sections in the header
	     of the zeroth section.  */
	  elf->state.ELFW(elf,LIBELFBITS).scns.data[0].shdr.ELFW(e,LIBELFBITS)->sh_size
	    = shnum;
	  elf->state.ELFW(elf,LIBELFBITS).scns.data[0].shdr_flags
	    |= ELF_F_DIRTY;
	}


      /* Go over all sections and find out how large they are.  */
      list = &elf->state.ELFW(elf,LIBELFBITS).scns;

      do
	{
	  int cnt;

	  for (cnt = first == true; cnt < list->cnt; ++cnt)
	    {
	      Elf_Scn *scn = &list->data[cnt];
	      ElfW2(LIBELFBITS,Shdr) *shdr = scn->shdr.ELFW(e,LIBELFBITS);
	      off_t offset = 0;
	      ElfW2(LIBELFBITS,Word) sh_entsize;
	      ElfW2(LIBELFBITS,Word) sh_align;

	      assert (shdr != NULL);
	      sh_entsize = shdr->sh_entsize;
	      sh_align = shdr->sh_addralign ? shdr->sh_addralign : 1;

	      /* Set the sh_entsize value if we can reliably detect it.  */
	      switch (shdr->sh_type)
		{
		case SHT_SYMTAB:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYM, 1);
		  break;
		case SHT_RELA:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_RELA, 1);
		  break;
		case SHT_GROUP:
		  /* Only relocatable files can contain section groups.  */
		  if (ehdr->e_type != ET_REL)
		    {
		      __libelf_seterrno (ELF_E_GROUP_NOT_REL);
		      return -1;
		    }
		  /* FALLTHROUGH */
		case SHT_HASH:
		case SHT_SYMTAB_SHNDX:
		  sh_entsize = elf_typesize (32, ELF_T_WORD, 1);
		  break;
		case SHT_DYNAMIC:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_DYN, 1);
		  break;
		case SHT_REL:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_REL, 1);
		  break;
		case SHT_DYNSYM:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYM, 1);
		  break;
		case SHT_SUNW_move:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_MOVE, 1);
		  break;
		case SHT_SUNW_syminfo:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYMINFO, 1);
		  break;
		default:
		  break;
		}

	      /* If the section header contained the wrong entry size
		 correct it and mark the header as modified.  */
	      update_if_changed (shdr->sh_entsize, sh_entsize,
				 scn->shdr_flags);

	      /* Iterate over all data blocks.  */
	      if (list->data[cnt].data_list_rear != NULL)
		{
		  Elf_Data_List *dl = &scn->data_list;

		  while (dl != NULL)
		    {
		      if (unlikely (dl->data.d.d_version == EV_NONE)
			  || unlikely (dl->data.d.d_version >= EV_NUM))
			{
			  __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
			  return -1;
			}

		      if (unlikely (! powerof2 (dl->data.d.d_align)))
			{
			  __libelf_seterrno (ELF_E_INVALID_ALIGN);
			  return -1;
			}

		      sh_align = MAX (sh_align, dl->data.d.d_align);

		      if (elf->flags & ELF_F_LAYOUT)
			{
			  /* The user specified the offset and the size.
			     All we have to do is check whether this block
			     fits in the size specified for the section.  */
			  if (unlikely (dl->data.d.d_off + dl->data.d.d_size
					> shdr->sh_size))
			    {
			      __libelf_seterrno (ELF_E_SECTION_TOO_SMALL);
			      return -1;
			    }
			}
		      else
			{
			  /* Determine the padding.  */
			  offset = ((offset + dl->data.d.d_align - 1)
				    & ~(dl->data.d.d_align - 1));

			  update_if_changed (dl->data.d.d_off, offset,
					     changed);

			  offset += dl->data.d.d_size;
			}

		      /* Next data block.  */
		      dl = dl->next;
		    }
		}

	      if (elf->flags & ELF_F_LAYOUT)
		{
		  size = MAX (size, shdr->sh_offset + shdr->sh_size);

		  /* The alignment must be a power of two.  This is a
		     requirement from the ELF specification.  Additionally
		     we test for the alignment of the section being large
		     enough for the largest alignment required by a data
		     block.  */
		  if (unlikely (! powerof2 (shdr->sh_addralign))
		      || unlikely (shdr->sh_addralign < sh_align))
		    {
		      __libelf_seterrno (ELF_E_INVALID_ALIGN);
		      return -1;
		    }
		}
	      else
		{
		  /* How much alignment do we need for this section.  */
		  update_if_changed (shdr->sh_addralign, sh_align,
				     scn->shdr_flags);

		  size = (size + sh_align - 1) & ~(sh_align - 1);
		  update_if_changed (shdr->sh_offset, size, changed);

		  /* See whether the section size is correct.  */
		  update_if_changed (shdr->sh_size, offset, changed);

		  if (shdr->sh_type != SHT_NOBITS)
		    size += offset;

		  scn->flags |= changed;
		}

	      /* Check that the section size is actually a multiple of
		 the entry size.  */
	      if (shdr->sh_entsize != 0
		  && unlikely (shdr->sh_size % shdr->sh_entsize != 0))
		{
		  __libelf_seterrno (ELF_E_INVALID_SHENTSIZE);
		  return -1;
		}
	    }

	  assert (list->next == NULL || list->cnt == list->max);

	  first = false;
	}
      while ((list = list->next) != NULL);

      /* Store section information.  */
      if (elf->flags & ELF_F_LAYOUT)
	{
	  /* The user is supposed to fill out e_phoff.  Use it and
	     e_phnum to determine the maximum extend.  */
	  size = MAX (size, (ehdr->e_shoff
			     + (elf_typesize (LIBELFBITS, ELF_T_SHDR,
					      shnum))));
	}
      else
	{
	  /* Align for section header table.

	     Yes, we use `sizeof' and not `__alignof__' since we do not
	     want to be surprised by architectures with less strict
	     alignment rules.  */
#define SHDR_ALIGN sizeof (ElfW2(LIBELFBITS,Off))
	  size = (size + SHDR_ALIGN - 1) & ~(SHDR_ALIGN - 1);

	  update_if_changed (ehdr->e_shoff, size, elf->flags);
	  update_if_changed (ehdr->e_shentsize,
			     elf_typesize (LIBELFBITS, ELF_T_SHDR, 1),
			     ehdr_flags);

	  /* Account for the section header size.  */
	  size += elf_typesize (LIBELFBITS, ELF_T_SHDR, shnum);
	}
    }

  elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ehdr_flags;

  return size;
}
