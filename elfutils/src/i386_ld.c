/* Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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
#include <error.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>

#include <system.h>
#include "ld.h"


/* The old callbacks.  */
static int (*old_flag_unresolved) (struct ld_state *);
static int (*old_open_outfile) (struct ld_state *, int, int);


static int
elf_i386_flag_unresolved (struct ld_state *statep)
{
  /* There are some special symbols which are not meant to be defined
     in any input file since they are created by the linker.  For x86
     this is _GLOBAL_OFFSET_TABLE_.  */
  if (statep->nunresolved_nonweak > 0)
    {
      /* Go through the list and determine the unresolved symbols.  */
      struct symbol *first;
      struct symbol *s;

      s = first = statep->unresolved->next;
      do
	{
	  if (!s->defined && !s->weak
	      && strcmp (s->name, "_GLOBAL_OFFSET_TABLE_") == 0)
	    {
	      s->defined = 1;
	      statep->need_got = true;

	      /* Since this is the only symbol we are (currently)
		 looking for stop right here.  */
	      break;
	    }

	  s = s->next;
	}
      while (s != first);
    }

  return old_flag_unresolved (statep);
}


static int
elf_i386_open_outfile (struct ld_state *statep, int klass, int data)
{
  int result;

  /* This backend only handles 32-bit object files.  */
  /* XXX For now just use the generic backend.  */
  result = old_open_outfile (statep, ELFCLASS32, ELFDATA2LSB);

  if (result == 0)
    {
      /* Set the machine type.  */
      GElf_Ehdr ehdr_mem;
      GElf_Ehdr *ehdr;

      ehdr = gelf_getehdr (statep->outelf, &ehdr_mem);
      /* It succeeded a second ago so we assume it works here, too.  */
      assert (ehdr != NULL);

      ehdr->e_machine = EM_386;

      (void) gelf_update_ehdr (statep->outelf, ehdr);
    }

  return result;
}


static void
elf_i386_relocate_section (Elf_Scn *outscn, struct scninfo *firstp,
			   const Elf32_Word *dblindirect,
			   struct ld_state *statep)
{
  struct scninfo *runp;
  Elf_Data *data;

  /* Iterate over all the input sections.  Appropriate data buffers in the
     output sections were already created.  I get them iteratively, too.  */
  runp = firstp;
  data = NULL;
  do
    {
      Elf_Data *reltgtdata;
      Elf_Data *insymdata;
      Elf_Data *inxndxdata = NULL;
      size_t maxcnt;
      size_t cnt;
      const Elf32_Word *symindirect;
      struct symbol **symref;

      /* Get the output section data buffer for this input section.  */
      data = elf_getdata (outscn, data);
      assert (data != NULL);

      /* Get the data for section in the input file this relocation
	 section is relocating.  Since these buffers are reused in the
	 output modifying these buffers has the correct result.  */
      reltgtdata
	= elf_getdata (runp->fileinfo->scninfo[runp->shdr.sh_info].scn, NULL);

      /* Get the data for the input section symbol table for this
	 relocation section.  */
      /* XXX  When do we have to use .dynsym?  */
      insymdata = runp->fileinfo->symtabdata;
      /* And the extended section index table.  */
      inxndxdata = runp->fileinfo->xndxdata;

      /* Number of relocations.  */
      maxcnt = runp->shdr.sh_size / runp->shdr.sh_entsize;

      /* Array directing local symbol table offsets to output symbol
	 table offsets.  */
      symindirect = runp->fileinfo->symindirect;

      /* References to the symbol records.  */
      symref = runp->fileinfo->symref;

      /* Iterate over all the relocations in the section.  */
      for (cnt = 0; cnt < maxcnt; ++cnt)
	{
	  GElf_Rel rel_mem;
	  GElf_Rel *rel;
	  Elf32_Word si;
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;
	  Elf32_Word xndx;

	  /* Get the relocation data itself.  x86 uses Rel
	     relocations.  In case we have to handle Rela as well the
	     whole loop probably should be duplicated.  */
	  rel = gelf_getrel (data, cnt, &rel_mem);
	  assert (rel != NULL);

	  /* Compute the symbol index in the output file.  */
	  si = symindirect[GELF_R_SYM (rel->r_info)];
	  if (si == 0)
	    {
	      /* This happens if the symbol is locally undefined or
		 superceded by some other definition.  */
	      assert (symref[GELF_R_SYM (rel->r_info)] != NULL);
	      si = symref[GELF_R_SYM (rel->r_info)]->outsymidx;
	    }
	  /* Take reordering performed to sort the symbol table into
	     account.  */
	  si = dblindirect[si];

	  /* Get the symbol table entry.  */
	  sym = gelf_getsymshndx (insymdata, inxndxdata,
				  GELF_R_SYM (rel->r_info), &sym_mem, &xndx);
	  if (sym->st_shndx != SHN_XINDEX)
	    xndx = sym->st_shndx;
	  assert (xndx < SHN_LORESERVE || xndx > SHN_HIRESERVE);

	  /* We fortunately don't have to do much.  The relocations
	     mostly get only updates of the offset.  Only is a
	     relocation referred to a section do we have to do
	     something.  In this case the reference to the sections
	     has no direct equivalent since the part the input section
	     contributes need not start at the same offset as in the
	     input file.  Therefore we have to adjust the addend which
	     in the case of Rel relocations is in the target section
	     itself.  */
	  if (GELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    {
	      Elf32_Word toadd;

	      /* We expect here on R_386_32 relocations.  */
	      assert (GELF_R_TYPE (rel->r_info) == R_386_32);

	      /* Avoid writing to the section memory if this is
		 effectively a no-op since it might save a
		 copy-on-write operation.  */
	      toadd = runp->fileinfo->scninfo[xndx].offset;
	      if (toadd != 0)
		{
#ifdef ALLOW_UNALIGNED
		  /* For architectures which allow unaligned access we
		     use it.  */
		  *((Elf32_Word *) reltgtdata->d_buf + rel->r_offset)
		    += toadd;
#else
		  Elf32_Word offset;

		  memcpy (&offset, reltgtdata->d_buf + rel->r_offset,
			  sizeof (Elf32_Word));
		  offset += toadd;
		  memcpy (reltgtdata->d_buf + rel->r_offset, &offset,
			  sizeof (Elf32_Word));
#endif
		}
	    }

	  /* Adjust the offset for the position of the input section
	     content in the output section.  */
	  rel->r_offset
	    += runp->fileinfo->scninfo[runp->shdr.sh_info].offset;

	  /* And finally adjust the index of the symbol in the output
	     symbol table.  */
	  rel->r_info = GELF_R_INFO (si, GELF_R_TYPE (rel->r_info));

	  /* Store the result.  */
	  (void) gelf_update_rel (data, cnt, rel);
	}

      runp = runp->next;
    }
  while (runp != firstp);
}


/* Each PLT entry has 16 bytes.  We need one entry as overhead for
   the code to set up the call into the runtime relocation.  */
#define PLT_ENTRY_SIZE 16

static void
elf_i386_initialize_plt (struct ld_state *statep, Elf_Scn *scn)
{
  Elf_Data *data;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;

  /* Change the entry size in the section header.  */
  shdr = gelf_getshdr (scn, &shdr_mem);
  assert (shdr != NULL);
  shdr->sh_entsize = PLT_ENTRY_SIZE;
  (void) gelf_update_shdr (scn, shdr);

  data = elf_newdata (scn);
  if (data == NULL)
    error (EXIT_FAILURE, 0, gettext ("cannot allocate PLT section: %s"),
	   elf_errmsg (-1));

  /* We need one special PLT entry (performing the jump to the runtime
     relocation routines) and one for each function we call in a DSO.  */
  data->d_size = (1 + statep->nplt) * PLT_ENTRY_SIZE;
  data->d_buf = xcalloc (1, data->d_size);
  data->d_align = 8;
  data->d_off = 0;

  statep->nplt_used = 1;
}


static void
elf_i386_initialize_pltrel (struct ld_state *statep, Elf_Scn *scn)
{
  Elf_Data *data;

  data = elf_newdata (scn);
  if (data == NULL)
    error (EXIT_FAILURE, 0, gettext ("cannot allocate PLTREL section: %s"),
	   elf_errmsg (-1));

  /* One relocation per PLT entry.  */
  data->d_size = statep->nplt * sizeof (Elf32_Rel);
  data->d_buf = xcalloc (1, data->d_size);
  data->d_type = ELF_T_REL;
  data->d_align = 4;
  data->d_off = 0;
}


static void
elf_i386_initialize_got (struct ld_state *statep, Elf_Scn *scn)
{
  Elf_Data *data;

  /* If we have no .plt we don't need the special entries we normally
     create for it.  The other contents is created later.  */
  if (statep->ngot + statep->nplt == 0)
    return;

  data = elf_newdata (scn);
  if (data == NULL)
    error (EXIT_FAILURE, 0, gettext ("cannot allocate GOT section: %s"),
	   elf_errmsg (-1));

  /* We construct the .got section in pieces.  Here we only add the data
     structures which are used by the PLT.  This includes three reserved
     entries at the beginning (the first will contain a pointer to the
     .dynamic section), and one word for each PLT entry.  */
  data->d_size = (3 + statep->ngot + statep->nplt) * sizeof (Elf32_Addr);
  data->d_buf = xcalloc (1, data->d_size);
  data->d_align = sizeof (Elf32_Addr);
  data->d_off = 0;
}


/* The first entry in an absolute procedure linkage table looks like
   this.  See the SVR4 ABI i386 supplement to see how this works.  */
static const unsigned char elf_i386_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x35,	/* pushl contents of address */
  0, 0, 0, 0,	/* replaced with address of .got + 4.  */
  0xff, 0x25,	/* jmp indirect */
  0, 0, 0, 0,	/* replaced with address of .got + 8.  */
  0, 0, 0, 0	/* pad out to 16 bytes.  */
};

/* The first entry in a PIC procedure linkage table look like this.  */
static const unsigned char elf_i386_pic_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0xb3, 4, 0, 0, 0,	/* pushl 4(%ebx) */
  0xff, 0xa3, 8, 0, 0, 0,	/* jmp *8(%ebx) */
  0, 0, 0, 0			/* pad out to 16 bytes.  */
};

/* Contents of all but the first PLT entry in executable.  */
static const unsigned char elf_i386_plt_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x25,   /* jmp indirect */
  0, 0, 0, 0,   /* replaced with address of this symbol in .got.  */
  0x68,         /* pushl immediate */
  0, 0, 0, 0,   /* replaced with offset into relocation table.  */
  0xe9,         /* jmp relative */
  0, 0, 0, 0    /* replaced with offset to start of .plt.  */
};

/* Contents of all but the first PLT entry in DSOs.  */
static const unsigned char elf_i386_pic_plt_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0xa3,	/* jmp *offset(%ebx) */
  0, 0, 0, 0,	/* replaced with offset of this symbol in .got.  */
  0x68,		/* pushl immediate */
  0, 0, 0, 0,	/* replaced with offset into relocation table.  */
  0xe9,		/* jmp relative */
  0, 0, 0, 0	/* replaced with offset to start of .plt.  */
};

static void
elf_i386_finalize_plt (struct ld_state *statep, size_t nsym, size_t nsym_dyn)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf32_Addr gotaddr;
  Elf32_Addr gotaddr_off;
  Elf_Data *data;
  Elf_Data *symdata = NULL;
  Elf_Data *dynsymdata;
  size_t cnt;
  bool build_dso = statep->file_type == dso_file_type;
#ifndef ALLOW_UNALIGNED
  Elf32_Addr val;
#endif

  if (statep->nplt + statep->ngot == 0)
    /* Nothing to be done.  */
    return;

  /* Get the address of the got section.  */
  scn = elf_getscn (statep->outelf, statep->gotscnidx);
      shdr = gelf_getshdr (scn, &shdr_mem);
  data = elf_getdata (scn, NULL);
  assert (shdr != NULL && data != NULL);
  gotaddr = shdr->sh_addr;

  /* Now create the initial values for the .got section.  The first
     word contains the address of the .dynamic section.  */
  shdr = gelf_getshdr (elf_getscn (statep->outelf, statep->dynamicscnidx),
		       &shdr_mem);
  assert (shdr != NULL);
  ((Elf32_Word *) data->d_buf)[0] = shdr->sh_addr;

  /* The second and third entry are left empty for use by the dynamic
     linker.  The following entries are pointers to the instructions
     following the initial jmp instruction in the corresponding PLT
     entry.  Since the first PLT entry is special the first used one
     has the index 1.  */
  scn = elf_getscn (statep->outelf, statep->pltscnidx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  assert (shdr != NULL);

  dynsymdata = elf_getdata (elf_getscn (statep->outelf, statep->dynsymscnidx),
			    NULL);
  assert (dynsymdata != NULL);

  if (statep->symscnidx != 0)
    {
      symdata = elf_getdata (elf_getscn (statep->outelf, statep->symscnidx),
			     NULL);
      assert (symdata != NULL);
    }

  for (cnt = 0; cnt < statep->nplt; ++cnt)
    {
      assert ((4 + cnt) * sizeof (Elf32_Word) <= data->d_size);

      /* Point the GOT entry at the PLT entry, after the initial jmp.  */
      ((Elf32_Word *) data->d_buf)[3 + cnt] = (shdr->sh_addr
					       + (1 + cnt) * PLT_ENTRY_SIZE
					       + 6);

      /* The value of the symbol is the address of the corresponding PLT
	 entry.  Store the address, also for the normal symbol table if
	 this is necessary.  */
      ((Elf32_Sym *) dynsymdata->d_buf)[nsym_dyn - statep->nplt + cnt].st_value
	= shdr->sh_addr + (1 + cnt) * PLT_ENTRY_SIZE;

      if (symdata != NULL)
	((Elf32_Sym *) symdata->d_buf)[nsym - statep->nplt + cnt].st_value
	  = shdr->sh_addr + (1 + cnt) * PLT_ENTRY_SIZE;
    }

  /* Create the .plt section.  */
  scn = elf_getscn (statep->outelf, statep->pltscnidx);
  data = elf_getdata (scn, NULL);
  assert (data != NULL);

  /* Create the first entry.  */
  assert (data->d_size >= PLT_ENTRY_SIZE);
  if (build_dso)
    /* Copy the entry.  It's complete, no relocation needed.  */
    memcpy (data->d_buf, elf_i386_pic_plt0_entry, PLT_ENTRY_SIZE);
  else
    {
      /* Copy the skeleton.  */
      memcpy (data->d_buf, elf_i386_plt0_entry, PLT_ENTRY_SIZE);

      /* And fill in the addresses.  */
#ifdef ALLOW_UNALIGNED
      *((Elf32_Addr *) ((char *) data->d_buf + 2)) = gotaddr + 4;
      *((Elf32_Addr *) ((char *) data->d_buf + 8)) = gotaddr + 8;
#else
      val = gotaddr + 4;
      memcpy ((char *) data->d_buf + 2, &val, sizeof (Elf32_Addr));
      val = gotaddr + 8;
      memcpy ((char *) data->d_buf + 8, &val, sizeof (Elf32_Addr));
#endif
    }

  /* For DSOs we need GOT offsets, otherwise the GOT address.  */
  gotaddr_off = build_dso ? 0 : gotaddr;

  /* Create the remaining entries.  */
  for (cnt = 0; cnt < statep->nplt; ++cnt)
    {
      char *addr = (char *) data->d_buf + (1 + cnt) * PLT_ENTRY_SIZE;

      /* Copy the skeleton.  */
      assert (data->d_size >= (2 + cnt) * PLT_ENTRY_SIZE);
      memcpy (addr, build_dso ? elf_i386_pic_plt_entry : elf_i386_plt_entry,
	      PLT_ENTRY_SIZE);

      /* And once more, fill in the addresses.  */
#ifdef ALLOW_UNALIGNED
      /* Address of this symbol in .got.  */
      *((Elf32_Addr *) (addr + 2)) = (gotaddr_off
				      + (3 + cnt) * sizeof (Elf32_Addr));
      /* Offset into relocation table.  */
      *((Elf32_Addr *) (addr + 7)) = cnt * sizeof (Elf32_Rel);
      /* Offset to start of .plt.  */
      *((Elf32_Addr *) (addr + 12)) = -(2 + cnt) * PLT_ENTRY_SIZE;
#else
      /* Address of this symbol in .got.  */
      val = gotaddr_off + (3 + cnt) * sizeof (Elf32_Addr);
      memcpy (addr + 2, &val, sizeof (Elf32_Addr));
      /* Offset into relocation table.  */
      val = cnt * sizeof (Elf32_Rel);
      memcpy (addr + 7, &val, sizeof (Elf32_Addr));
      /* Offset to start of .plt.  */
      val = -(2 + cnt) * PLT_ENTRY_SIZE;
      memcpy (addr + 12, &val, sizeof (Elf32_Addr));
#endif
    }

  /* Create the .rel.plt section data.  It simply means relocations
     addressing the corresponding entry in the .got section.  The
     section name is misleading.  */
  scn = elf_getscn (statep->outelf, statep->pltrelscnidx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  data = elf_getdata (scn, NULL);
  assert (shdr != NULL && data != NULL);

  /* Update the sh_link to point to the section being modified.  We
     point it here (correctly) to the .got section.  Some linkers
     (e.g., the GNU binutils linker) point to the .plt section.  This
     is wrong since the .plt section isn't modified even though the
     name .rel.plt suggests that this is correct.  */
  shdr->sh_link = statep->dynsymscnidx;
  shdr->sh_info = statep->gotscnidx;
  (void) gelf_update_shdr (scn, shdr);

  for (cnt = 0; cnt < statep->nplt; ++cnt)
    {
      GElf_Rel rel_mem;

      assert ((1 + cnt) * sizeof (Elf32_Rel) <= data->d_size);
      rel_mem.r_offset = gotaddr + (3 + cnt) * sizeof (Elf32_Addr);
      /* The symbol table entries for the functions from DSOs are at
	 the end of the symbol table.  */
      rel_mem.r_info = GELF_R_INFO (nsym_dyn - statep->nplt + cnt,
				    R_386_JMP_SLOT);
      (void) gelf_update_rel (data, cnt, &rel_mem);
    }
}


static int
elf_i386_rel_type (struct ld_state *statep __attribute__ ((__unused__)))
{
  /* ELF/i386 uses REL.  */
  return DT_REL;
}


static void
elf_i386_count_relocations (struct ld_state *statep, struct scninfo *scninfo)
{
  /* We go through the list of input sections and count those relocations
     which are not handled by the linker.  At the same time we have to
     see how many GOT entries we need and how much .bss space is needed
     for copy relocations.  */
  Elf_Data *data = elf_getdata (scninfo->scn, NULL);
  size_t maxcnt = scninfo->shdr.sh_size / scninfo->shdr.sh_entsize;
  size_t relsize = 0;
  size_t cnt;

  assert (scninfo->shdr.sh_type == SHT_REL);

  for (cnt = 0; cnt < maxcnt; ++cnt)
    {
      GElf_Rel rel_mem;
      GElf_Rel *rel;

      rel = gelf_getrel (data, cnt, &rel_mem);
      /* XXX Should we complain about failing accesses?  */
      if (rel != NULL)
	switch (GELF_R_TYPE (rel->r_info))
	  {
	  case R_386_GOT32:
	    /* This relocation is not emitted in the output file but
	       requires a GOT entry.  */
	    statep->got_size += sizeof (Elf32_Addr);
	    ++statep->nrel_got;

	    /* FALLTHROUGH */

	  case R_386_GOTOFF:
	  case R_386_GOTPC:
	    statep->need_got = true;
	    break;

	  case R_386_32:
	  case R_386_PC32:
	    /* These relocations cause text relocations in DSOs.  */
	    if (statep->file_type == dso_file_type
		/*&& in_dso_p (statep, scninfo, GELF_R_SYM (rel->r_info))*/)
	      {
		relsize += sizeof (Elf32_Addr);
		statep->dt_flags |= DF_TEXTREL;
	      }
	    break;
	  case R_386_PLT32:
	    /* We need a PLT entry.  */
	    //statep->need_plt = true;
	    break;

	  case R_386_TLS_GD:
	  case R_386_TLS_LDM:
	  case R_386_TLS_GD_32:
	  case R_386_TLS_GD_PUSH:
	  case R_386_TLS_GD_CALL:
	  case R_386_TLS_GD_POP:
	  case R_386_TLS_LDM_32:
	  case R_386_TLS_LDM_PUSH:
	  case R_386_TLS_LDM_CALL:
	  case R_386_TLS_LDM_POP:
	  case R_386_TLS_LDO_32:
	  case R_386_TLS_IE_32:
	  case R_386_TLS_LE_32:
	    /* XXX */
	    abort ();
	    break;

	  case R_386_NONE:
	    /* Nothing to be done.  */
	    break;

	  case R_386_COPY:
	  case R_386_GLOB_DAT:
	  case R_386_JMP_SLOT:
	  case R_386_TLS_DTPMOD32:
	  case R_386_TLS_DTPOFF32:
	  case R_386_TLS_TPOFF32:
	    /* These relocation should never be generated by an assembler.  */
	    abort ();


	  default:
	    /* Unknown relocation.  */
	    abort ();
	  }
    }

  scninfo->relsize = relsize;
}


#if 0
static void
elf_i386_create_relocations (struct ld_state *statep, Elf32_Word scnidx,
			     struct scninfo *first)
{
  struct scninfo *runp;
  Elf_Data *data;
  Elf_Data *symdata;
  int used;
  bool is_shared = statep->file_type != executable_file_type;

  /* Get the data for the section.  The buffer is already allocated.  */
  data = elf_getdata (elf_getscn (statep->outelf, scnidx), NULL);
  assert (data != NULL);

  symdata = elf_getdata (elf_getscn (statep->outelf, statep->symscnidx), NULL);
  assert (symdata != NULL);

  runp = first;
  do
    {
      size_t cnt;
      size_t maxcnt;
      GElf_Addr inscnoffset;
      Elf_Data *moddata;

      /* We cannot handle relocations against merge-able sections.  */
      assert ((runp->fileinfo->scninfo[runp->shdr.sh_link].shdr.sh_flags
	       & SHF_MERGE) == 0);

      /* This is the offset of the input section we are looking at in
	 the output file.  */
      inscnoffset = (statep->allsections[runp->fileinfo->scninfo[runp->shdr.sh_link].outscnndx]->addr
		     + runp->fileinfo->scninfo[runp->shdr.sh_link].offset);

      /* This is the data for the section modified by the relocations.  */
      moddata = elf_getdata (elf_getscn (runp->fileinfo->elf,
					 runp->shdr.sh_link), NULL);
      assert (moddata != NULL);

      /* Get the number of relocations from the section header.  */
      maxcnt = runp->shdr.sh_size / runp->shdr.sh_entsize;

      for (cnt = 0; cnt < maxcnt; ++cnt)
	{
	  GElf_Rel rel_mem;
	  GElf_Rel *rel;
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;
	  int symidx;
	  GElf_Addr reladdr;

	  rel = gelf_getrel (data, cnt, &rel_mem);
	  /* XXX Should we complain about failing accesses?  */
	  assert (rel != NULL);
	  if (rel == NULL)
	    continue;

	  reladdr = inscnoffset + rel->r_offset;

	  symidx = runp->symindirect[GELF_R_SYM (rel->r_info)];
	  if (symidx == 0)
	    {
	      assert (runp->symref[GELF_R_SYM (rel->r_info)] != NULL);
	      symidx = runp->symref[GELF_R_SYM (rel->r_info)]->outsymidx;
	      assert (symidx != 0);
	    }
	  symidx = statep->dblindirect[symidx];

	  sym = gelf_getsym (symdata, symidx, &sym_mem);
	  /* XXX Should we complain about failing accesses?  */
	  assert (sym != NULL);
	  if (sym == NULL)
	    continue;

	  switch (GELF_R_TYPE (rel->r_info))
	    {
	    case R_386_PC32:
	      value = sym->st_value - reladdr;
#ifdef ALLOW_UNALIGNED
	      *((uint32_t *) ((char *) data->d_buf + rel->r_offset)) += value;
#else
	      {
		uint32_t res;
		memcpy (&res, (char *) data->d_buf + rel->r_offset,
			sizeof (res));
		res += value;
		memcpy ((char *) data->d_buf + rel->r_offset, &res,
			sizeof (res));
	      }
#endif
	      break;

	    case R_386_32:
	    case R_386_GOT32:
	    case R_386_PLT32:
	    case R_386_GLOB_DAT:
	    case R_386_RELATIVE:
	    case R_386_GOTOFF:
	    case R_386_GOTPC:

	    case R_386_COPY:
	    case R_386_JMP_SLOT:
	    default:
	      /* Should not happen.  */
	      abort ();
	    }
	}
    }
  while ((runp = runp->next) != first);
}
#endif


int
elf_i386_ld_init (struct ld_state *statep)
{
  /* We have a few callbacks available.  */
  old_flag_unresolved = statep->callbacks.flag_unresolved;
  statep->callbacks.flag_unresolved = elf_i386_flag_unresolved;

  old_open_outfile = statep->callbacks.open_outfile;
  statep->callbacks.open_outfile = elf_i386_open_outfile;

  statep->callbacks.relocate_section  = elf_i386_relocate_section;

  statep->callbacks.initialize_plt = elf_i386_initialize_plt;
  statep->callbacks.initialize_pltrel = elf_i386_initialize_pltrel;

  statep->callbacks.initialize_got = elf_i386_initialize_got;

  statep->callbacks.finalize_plt = elf_i386_finalize_plt;

  statep->callbacks.rel_type = elf_i386_rel_type;

  statep->callbacks.count_relocations = elf_i386_count_relocations;

#if 0
  statep->callbacks.create_relocations = elf_i386_create_relocations;
#endif

  return 0;
}
