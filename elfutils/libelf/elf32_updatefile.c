/* Write changed data structures.
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

#include <assert.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include "libelfP.h"


#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


int
internal_function
__elfw2(LIBELFBITS,updatemmap) (Elf *elf, int change_bo, size_t shnum)
{
  ElfW2(LIBELFBITS,Ehdr) *ehdr;
  xfct_t fctp;
  char *last_position;

  /* We need the ELF header several times.  */
  ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (change_bo)
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]
#endif

	  /* Do the real work.  */
	  (*fctp) ((char *) elf->map_address + elf->start_offset, ehdr,
		   sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);
	}
      else
	memcpy (elf->map_address + elf->start_offset, ehdr,
		sizeof (ElfW2(LIBELFBITS,Ehdr)));

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;
    }

  /* Write out the program header table.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL
      && ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags)
	  & ELF_F_DIRTY))
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	      == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize)
	memset (elf->map_address + elf->start_offset + ehdr->e_ehsize,
		__libelf_fill_byte, ehdr->e_phoff - ehdr->e_ehsize);

      if (change_bo)
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]
#endif

	  /* Do the real work.  */
	  (*fctp) (elf->map_address + elf->start_offset + ehdr->e_phoff,
		   elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)), ehdr->e_phnum);
	}
      else
	memcpy (elf->map_address + elf->start_offset + ehdr->e_phoff,
		elf->state.ELFW(elf,LIBELFBITS).phdr,
		sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum);

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  last_position = ((char *) elf->map_address + elf->start_offset
		   + MAX (elf_typesize (LIBELFBITS, ELF_T_EHDR, 1),
			  ehdr->e_phoff)
		   + elf_typesize (LIBELFBITS, ELF_T_PHDR, ehdr->e_phnum));

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      ElfW2(LIBELFBITS,Shdr) *shdr_dest;
      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;

#if EV_NUM != 2
      xfct_t shdr_fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR];
#else
# undef shdr_fctp
# define shdr_fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]
#endif
      shdr_dest = (ElfW2(LIBELFBITS,Shdr) *)
	((char *) elf->map_address + elf->start_offset + ehdr->e_shoff);

      do
	{
	  int cnt;

	  for (cnt = 0; cnt < list->cnt; ++cnt)
	    {
	      ElfW2(LIBELFBITS,Shdr) *shdr;
	      char *scn_start;
	      Elf_Data_List *dl;

	      shdr = list->data[cnt].shdr.ELFW(e,LIBELFBITS);

	      scn_start = ((char *) elf->map_address
			   + elf->start_offset + shdr->sh_offset);
	      dl = &list->data[cnt].data_list;

	      if (shdr->sh_type != SHT_NOBITS
		  && list->data[cnt].data_list_rear != NULL)
		do
		  {
		    if ((list->data[cnt].flags | dl->flags
			 | elf->flags) & ELF_F_DIRTY)
		      {
			if (scn_start + dl->data.d.d_off != last_position)
			  {
			    if (scn_start + dl->data.d.d_off > last_position)
			      memset (last_position, __libelf_fill_byte,
				      scn_start + dl->data.d.d_off
				      - last_position);
			    last_position = scn_start + dl->data.d.d_off;
			  }

			if (change_bo)
			  {
			    size_t recsize;

#if EV_NUM != 2
			    fctp = __elf_xfctstom[__libelf_version - 1][dl->data.d.d_version - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
			    recsize = __libelf_type_sizes[dl->d_version - 1][ELFW(ELFCLASS,LIBELFBITS) - 1][dl->data.d.d_type];
#else
# undef fctp
			    fctp = __elf_xfctstom[0][0][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
			    recsize = __libelf_type_sizes[0][ELFW(ELFCLASS,LIBELFBITS) - 1][dl->data.d.d_type];
#endif

			    /* Make sure the data size matches the
			       record size.  */
			    assert (dl->data.d.d_size % recsize == 0);

			    /* Do the real work.  */
			    (*fctp) (last_position, dl->data.d.d_buf, recsize,
				     dl->data.d.d_size / recsize);

			    last_position += dl->data.d.d_size;
			  }
			else
			  last_position = mempcpy (last_position,
						   dl->data.d.d_buf,
						   dl->data.d.d_size);
		      }
		    else
		      last_position += dl->data.d.d_size;

		    dl->flags &= ~ELF_F_DIRTY;

		    dl = dl->next;
		  }
		while (dl != NULL);

	      /* Write the section header table entry if necessary.  */
	      if ((list->data[cnt].shdr_flags | elf->flags) & ELF_F_DIRTY)
		{
		  if (change_bo)
		    (*shdr_fctp) (shdr_dest,
				  list->data[cnt].shdr.ELFW(e,LIBELFBITS),
				  sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
		  else
		    memcpy (shdr_dest, list->data[cnt].shdr.ELFW(e,LIBELFBITS),
			    sizeof (ElfW2(LIBELFBITS,Shdr)));

		  list->data[cnt].shdr_flags  &= ~ELF_F_DIRTY;
		}
	      ++shdr_dest;

	      list->data[cnt].flags &= ~ELF_F_DIRTY;
	    }

	  assert (list->next == NULL || list->cnt == list->max);
	}
      while ((list = list->next) != NULL);


      /* Fill the gap between last section and section header table if
	 necessary.  */
      if ((elf->flags & ELF_F_DIRTY)
	  && last_position != ((char *) elf->map_address + elf->start_offset
			       + ehdr->e_shoff))
	{
	  assert ((char *) elf->map_address + elf->start_offset + ehdr->e_shoff
		  > last_position);
	  memset (last_position, __libelf_fill_byte,
		  (char *) elf->map_address + elf->start_offset + ehdr->e_shoff
		  - last_position);
	}
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  return 0;
}


/* Size of the buffer we use to generate the blocks of fill bytes.  */
#define FILLBUFSIZE	4096

/* If we have to convert the section buffer contents we have to use
   temporary buffer.  Only buffers up to MAX_TMPBUF bytes are allocated
   on the stack.  */
#define MAX_TMPBUF	32768


/* Helper function to write out fill bytes.  */
static int
fill (int fd, off_t pos, size_t len, char *fillbuf, size_t *filledp)
{
  size_t filled = *filledp;

  if (unlikely (len > filled) && filled < FILLBUFSIZE)
    {
      /* Initialize a few more bytes.  */
      memset (fillbuf + filled, __libelf_fill_byte, len - filled);
      *filledp = filled = len;
    }

  do
    {
      /* This many bytes we want to write in this round.  */
      size_t n = MIN (filled, len);

      if (unlikely (pwrite (fd, fillbuf, n, pos) != n))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      pos += n;
      len -= n;
    }
  while (len > 0);

  return 0;
}


int
internal_function
__elfw2(LIBELFBITS,updatefile) (Elf *elf, int change_bo, size_t shnum)
{
  char fillbuf[FILLBUFSIZE];
  size_t filled = 0;
  ElfW2(LIBELFBITS,Ehdr) *ehdr;
#if EV_NUM != 2
  xfct_t fctp;
#endif
  off_t last_offset;

  /* We need the ELF header several times.  */
  ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      ElfW2(LIBELFBITS,Ehdr) tmp_ehdr;
      ElfW2(LIBELFBITS,Ehdr) *out_ehdr = ehdr;

      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (change_bo)
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]
#endif

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (&tmp_ehdr, ehdr, sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);

	  /* This is the buffer we want to write.  */
	  out_ehdr = &tmp_ehdr;
	}

      /* Write out the ELF header.  */
      if (unlikely (pwrite (elf->fildes, out_ehdr,
			    sizeof (ElfW2(LIBELFBITS,Ehdr)), 0)
		    != sizeof (ElfW2(LIBELFBITS,Ehdr))))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;
    }

  /* If the type sizes should be different at some time we have to
     rewrite this code.  */
  assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	  == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

  /* Write out the program header table.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      ElfW2(LIBELFBITS,Phdr) tmp_phdr;
      ElfW2(LIBELFBITS,Phdr) *out_phdr = elf->state.ELFW(elf,LIBELFBITS).phdr;

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize
	  && unlikely (fill (elf->fildes, ehdr->e_ehsize,
			     ehdr->e_phoff - ehdr->e_ehsize, fillbuf, &filled)
		       != 0))
	return 1;

      if (change_bo)
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]
#endif

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (&tmp_phdr, elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)), ehdr->e_phnum);

	  /* This is the buffer we want to write.  */
	  out_phdr = &tmp_phdr;
	}

      /* Write out the ELF header.  */
      if (unlikely (pwrite (elf->fildes, out_phdr,
			    sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum,
			    ehdr->e_phoff)
		    != sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  last_offset = (ehdr->e_phoff
		 + sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum);

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      off_t shdr_offset;
      xfct_t shdr_fctp;
      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;
      ElfW2(LIBELFBITS,Shdr) *shdr_data;
      ElfW2(LIBELFBITS,Shdr) *shdr_data_begin;
      int shdr_flags;

      shdr_offset = elf->start_offset + ehdr->e_shoff;
#if EV_NUM != 2
      shdr_fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR];
#else
# undef shdr_fctp
# define shdr_fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]
#endif

      if (change_bo || elf->state.ELFW(elf,LIBELFBITS).shdr == NULL)
	shdr_data = (ElfW2(LIBELFBITS,Shdr) *)
	  alloca (shnum * sizeof (ElfW2(LIBELFBITS,Shdr)));
      else
	shdr_data = elf->state.ELFW(elf,LIBELFBITS).shdr;
      shdr_data_begin = shdr_data;
      shdr_flags = elf->flags;

      do
	{
	  int cnt;

	  for (cnt = 0; cnt < list->cnt; ++cnt)
	    {
	      ElfW2(LIBELFBITS,Shdr) *shdr;
	      off_t scn_start;
	      Elf_Data_List *dl;

	      shdr = list->data[cnt].shdr.ELFW(e,LIBELFBITS);

	      scn_start = elf->start_offset + shdr->sh_offset;
	      dl = &list->data[cnt].data_list;

	      if (shdr->sh_type != SHT_NOBITS
		  && list->data[cnt].data_list_rear != NULL
		  && list->data[cnt].index != 0)
		do
		  {
		    if ((list->data[cnt].flags | dl->flags
			 | elf->flags) & ELF_F_DIRTY)
		      {
			char tmpbuf[MAX_TMPBUF];
			void *buf = dl->data.d.d_buf;

			if (scn_start + dl->data.d.d_off != last_offset)
			  {
			    assert (last_offset
				    < scn_start + dl->data.d.d_off);

			    if (unlikely (fill (elf->fildes, last_offset,
						(scn_start + dl->data.d.d_off)
						- last_offset, fillbuf,
						&filled) != 0))
			      return 1;

			    last_offset = scn_start + dl->data.d.d_off;
			  }

			if (change_bo)
			  {
			    size_t recsize;

#if EV_NUM != 2
			    fctp = __elf_xfctstom[__libelf_version - 1][dl->data.d.d_version - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
			    recsize = __libelf_type_sizes[dl->d_version - 1][ELFW(ELFCLASS,LIBELFBITS) - 1][dl->data.d.d_type];
#else
			    fctp = __elf_xfctstom[0][0][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
			    recsize = __libelf_type_sizes[0][ELFW(ELFCLASS,LIBELFBITS) - 1][dl->data.d.d_type];
#endif

			    /* Make sure the data size matches the
			       record size.  */
			    assert (dl->data.d.d_size % recsize == 0);

			    buf = tmpbuf;
			    if (dl->data.d.d_size > MAX_TMPBUF)
			      {
				buf = malloc (dl->data.d.d_size);
				if (buf == NULL)
				  {
				    __libelf_seterrno (ELF_E_NOMEM);
				    return 1;
				  }
			      }

			    /* Do the real work.  */
			    (*fctp) (buf, dl->data.d.d_buf, recsize,
				     dl->data.d.d_size / recsize);
			  }

			if (unlikely (pwrite (elf->fildes, buf,
					      dl->data.d.d_size, last_offset)
				      != dl->data.d.d_size))
			  {
			    if (buf != dl->data.d.d_buf && buf != tmpbuf)
			      free (buf);

			    __libelf_seterrno (ELF_E_WRITE_ERROR);
			    return 1;
			  }

			if (buf != dl->data.d.d_buf && buf != tmpbuf)
			  free (buf);
		      }

		    last_offset += dl->data.d.d_size;

		    dl->flags &= ~ELF_F_DIRTY;

		    dl = dl->next;
		  }
		while (dl != NULL);

	      /* Collect the section header table information.  */
	      if (change_bo)
		(*shdr_fctp) (shdr_data++,
			      list->data[cnt].shdr.ELFW(e,LIBELFBITS),
			      sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
	      else if (elf->state.ELFW(elf,LIBELFBITS).shdr == NULL)
		shdr_data = mempcpy (shdr_data,
				     list->data[cnt].shdr.ELFW(e,LIBELFBITS),
				     sizeof (ElfW2(LIBELFBITS,Shdr)));

	      shdr_flags |= list->data[cnt].shdr_flags;
	      list->data[cnt].shdr_flags  &= ~ELF_F_DIRTY;
	    }
	}
      while ((list = list->next) != NULL);


      assert (shdr_data == &shdr_data_begin[shnum]);

      /* Write out the section header table.  */
      if (shdr_flags & ELF_F_DIRTY
	  && unlikely (pwrite (elf->fildes, shdr_data_begin,
			       sizeof (ElfW2(LIBELFBITS,Shdr)) * shnum,
			       shdr_offset)
		       != sizeof (ElfW2(LIBELFBITS,Shdr)) * shnum))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  return 0;
}
