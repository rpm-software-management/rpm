/* Discard section not used at runtime from object files.
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

#include <argp.h>
#include <assert.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/param.h>

#include <elf-knowledge.h>
#include <libebl.h>
#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the parameters which have no short form.  */
#define OPT_REMOVE_COMMENT	0x100


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 'o', "FILE", 0, N_("Place stripped output into FILE") },
  { "preserve-dates", 'p', NULL, 0, N_("Copy modified/access timestamps to the output") },
  { "remove-comment", OPT_REMOVE_COMMENT, NULL, 0, N_("Remove .comment section") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("Discard symbols from object files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname);

/* Handle one ELF file.  */
static int handle_elf (int fd, Elf *elf, const char *prefix,
		       const char *fname, mode_t mode);

#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR: %s"),		      \
	 fname, elf_errmsg (-1))


/* Name of the output file.  */
static const char *output_fname;

/* If nonzero output files shall have same date as the input file.  */
static int preserve_dates;

/* If nonzero .comment sections will be removed.  */
static int remove_comment;


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* Make memory leak detection possible.  */
  mtrace ();

  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  bindtextdomain (PACKAGE, LOCALEDIR);

  /* Initialize the message catalog.  */
  textdomain (PACKAGE);

  /* Parse and process arguments.  */
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out");
  else
    {
      /* If we have seen the `-o' option there must be exactly one
	 input file.  */
      if (output_fname != NULL && remaining + 1 < argc)
	error (EXIT_FAILURE, 0,
	       gettext ("Only one input file allowed together with `-o'"));

      /* Process all the remaining files.  */
      do
	result |= process_file (argv[remaining]);
      while (++remaining < argc);
    }

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "strip (Red Hat %s) %s\n", PACKAGE, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2001");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'o':
      output_fname = arg;
      break;

    case 'p':
      preserve_dates = 1;
      break;

    case OPT_REMOVE_COMMENT:
      remove_comment = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
more_help (int key, const char *text, void *input)
{
  switch (key)
    {
    case ARGP_KEY_HELP_EXTRA:
      /* We print some extra information.  */
      return xstrdup (gettext ("\
Report bugs to <drepper@redhat.com>.\n"));
    default:
      break;
    }
  return (char *) text;
}


static int
process_file (const char *fname)
{
  /* Open the file and determine the type.  */
  int fd;
  Elf *elf;
  struct stat64 st;

  /* If we have to preserve the modify and access timestamps get them
     now.  */
  if (stat64 (fname, &st) != 0)
    {
      error (0, errno, gettext ("cannot stat input file \"%s\""), fname);
      return 1;
    }

  /* Open the file.  */
  fd = open (fname, O_RDWR);
  if (fd == -1)
    {
      error (0, errno, gettext ("while opening \"%s\""), fname);
      return 1;
    }

  /* Now get the ELF descriptor.  */
  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf != NULL)
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  int result = handle_elf (fd, elf, NULL, fname,
				   st.st_mode & ACCESSPERMS);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (preserve_dates)
	    {
	      struct utimbuf ut;

	      ut.actime = st.st_atime;
	      ut.modtime = st.st_mtime;

	      if (utime (output_fname ?: fname, &ut) != 0)
		{
		  error (0, errno, gettext ("\
cannot set access and modification date of \"%s\""),
			 output_fname ?: fname);
		  result = 1;
		}
	    }

	  return result;
	}
      else
	/* XXX Shall we handle archives?  The originial GNU strip does
	   but it seems awfully complicated for the gains.  */
	;//return handle_ar (fd, elf, NULL, fname);

      /* We cannot handle this type.  Close the descriptor anyway.  */
      if (elf_end (elf) != 0)
	INTERNAL_ERROR (fname);
    }

  error (0, 0, gettext ("%s: File format not recognized"), fname);

  return 1;
}


static int
handle_elf (int fd, Elf *elf, const char *prefix, const char *fname,
	    mode_t mode)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len];
  char *cp = fullname;
  Elf *newelf;
  int result = 0;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  uint32_t shstrndx;
  size_t shnum;
  GElf_Ehdr newehdr_mem;
  GElf_Ehdr *newehdr;
  struct shdr_info
  {
    Elf_Scn *scn;
    GElf_Shdr shdr;
    Elf_Data *data;
    const char *name;
    GElf_Section idx;		/* Index in new file.  */
    struct Ebl_Strent *se;
  } *shdr_info;
  struct Ebl_Strtab *shst;
  Elf_Scn *scn;
  int changes;
  size_t cnt;
  size_t idx;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  /* If we are not replacing the input file open a new file here.  */
  if (output_fname != NULL)
    {
      fd = open (output_fname, O_RDWR | O_CREAT, mode);
      if (fd == -1)
	{
	  error (0, errno, gettext ("cannot open `%s'"), output_fname);
	  return 1;
	}
    }

  /* Get the information from the old file.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fname);

  /* Get the section header string table index.  */
  if (elf_getshstrndx (elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* We now create a new ELF descriptor for the same file.  We
     construct it almost exactly in the same way with some information
     dropped.  */
  newelf = elf_begin (fd, ELF_C_WRITE_MMAP, NULL);
  if (gelf_newehdr (newelf, gelf_getclass (elf)) == 0
      || (ehdr->e_type != ET_REL && gelf_newphdr (newelf, ehdr->e_phnum) == 0))
    {
      error (0, 0, gettext ("cannot create new file `%s': %s"),
	     output_fname, elf_errmsg (-1));
      goto fail;
    }

  /* Copy over the old program header if needed.  */
  if (ehdr->e_type != ET_REL)
    for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr;

	phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	if (phdr == NULL
	    || gelf_update_phdr (newelf, cnt, phdr) == 0)
	  INTERNAL_ERROR (fname);
      }

  /* Number of sections.  */
  if (elf_getshnum (elf, &shnum) < 0)
    {
      error (0, 0, gettext ("cannot determine number of sections: %s"),
	     elf_errmsg (-1));
      goto fail;
    }

  /* Storage for section information.  We leave room for one more
     entry since we unconditionally create a section header string
     table.  Maybe some weird tool created an ELF file without one.  */
  shdr_info = (struct shdr_info *) alloca ((shnum + 1)
					   * sizeof (struct shdr_info));
  memset (shdr_info, '\0', (shnum + 1) * sizeof (struct shdr_info));

  /* Prepare section information data structure.  */
  scn = NULL;
  cnt = 1;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      shdr_info[cnt].scn = scn;

      /* Get the header.  */
      if (gelf_getshdr (scn, &shdr_info[cnt].shdr) == NULL)
	INTERNAL_ERROR (fname);

      /* Get the name of the section.  */
      shdr_info[cnt].name = elf_strptr (elf, shstrndx,
					shdr_info[cnt].shdr.sh_name);
      if (shdr_info[cnt].name == NULL)
	{
	  error (0, 0, gettext ("illformed file `%s'"), fname);
	  goto fail_close;
	}

      /* Mark them as present but not yet investigated.  */
      shdr_info[cnt].idx = 1;

      /* Increment the counter.  */
      ++cnt;
    }

  /* Now determine which sections can go away.  The general rule is that
     all sections which are not used at runtime are stripped out.  But
     there are a few exceptions:

     - special sections named ".comment" and ".note" are kept
     - OS or architecture specific sections are kept since we might not
       know how to handle them
     - section groups must be observed.  If any section of a section
       is not removed the entire group is not removed
     - if a section is referred to from a section which is not removed
       in the sh_link or sh_info element it cannot be removed either
  */
  for (cnt = 1; cnt < shnum; ++cnt)
#if 1
    /* Check whether the section can be removed.  */
    if (SECTION_STRIP_P (&shdr_info[cnt].shdr, shdr_info[cnt].name,
			 remove_comment))
      /* For now assume this section will be removed.  */
      shdr_info[cnt].idx = 0;
#else
    if ((shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) == 0)
      {
	/* We are never removing the .note section(s).  */
	if (shdr_info[cnt].shdr.sh_type == SHT_NOTE)
	  continue;

	/* We are removing the .comment section(s) only if explicitly
           told so.  */
	if (shdr_info[cnt].shdr.sh_type == SHT_PROGBITS
	    && ! remove_comment
	    && strcmp (shdr_info[cnt].name, ".comment") == 0)
	  continue;

  	/* Don't remove non-standard sections.
	   XXX Once we handle the non-standard sections this can be
	   extended .  */
	if (shdr_info[cnt].shdr.sh_type >= SHT_NUM)
	  continue;

	/* For now assume this section will be removed.  */
	shdr_info[cnt].idx = 0;
      }
#endif

  /* Mark the SHT_NULL section as handled.  */
  shdr_info[0].idx = 2;

  /* Handle exceptions: section groups and cross-references.  We might
     have to repeat this a few times since the resetting of the flag
     might propagate.  */
  do
    {
      changes = 0;

      for (cnt = 1; cnt < shnum; ++cnt)
	if (shdr_info[cnt].idx == 1)
	  {
	    /* The content of symbol tables we don't remove must not
	       reference any section which we do remove.  Otherwise
	       we cannot remove the section.  */
	    if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		|| shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
	      {
		size_t elsize;
		size_t inner;

		/* Make sure the data is loaded.  */
		if (shdr_info[cnt].data == NULL)
		  {
		    shdr_info[cnt].data =
		      elf_getdata (shdr_info[cnt].scn, NULL);
		    if (shdr_info[cnt].data == NULL)
		      INTERNAL_ERROR (fname);
		  }

		/* Go through all symbols and make sure the section they
		   reference is not removed.  */
		elsize = gelf_fsize (elf, ELF_T_SYM, 1, ehdr->e_version);

		for (inner = 0;
		     inner < shdr_info[cnt].data->d_size / elsize;
		     ++inner)
		  {
		    GElf_Section scnidx;
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (shdr_info[cnt].data, inner,
						 &sym_mem);
		    if (sym == NULL)
		      INTERNAL_ERROR (fname);

		    scnidx = sym->st_shndx;
		    if (scnidx == SHN_UNDEF || scnidx >= shnum)
		      /* This is no section index, leave it alone.  */
		      continue;

		    if (shdr_info[scnidx].idx == 0)
		      {
			/* Mark this section and all before it which are
			   unmarked as used.  */
			shdr_info[scnidx].idx = 1;
			while (scnidx-- > 1)
			  if (shdr_info[scnidx].idx == 0)
			    shdr_info[scnidx].idx = 1;
			changes = 1;
		      }

		    /* XXX We have to decide who to handle the extended
		       section index table.  Maybe libelf does it itself.  */
		  }
	      }

	    /* Cross referencing happens:
	       - for the cases the ELF specification says.  That are
		 + SHT_DYNAMIC in sh_link to string table
		 + SHT_HASH in sh_link to symbol table
		 + SHT_REL and SHT_RELA in sh_link to symbol table
		 + SHT_SYMTAB and SHT_DYNSYM in sh_link to string table
		 + SHT_GROUP in sh_link to symbol table
		 + SHT_SYMTAB_SHNDX in sh_link to symbol table
		 Other (OS or architecture-specific) sections might as
		 well use this field so we process it unconditionally.
	       - references inside section groups
	       - specially marked references in sh_info if the SHF_INFO_LINK
		 flag is set
	    */

	    if (shdr_info[shdr_info[cnt].shdr.sh_link].idx != 0)
	      {
		size_t inner = shdr_info[cnt].shdr.sh_link;

		shdr_info[inner].idx = 1;
		while (inner-- > 1)
		  if (shdr_info[inner].idx == 0)
		    shdr_info[inner].idx = 1;

		changes = 1;
	      }

	    if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	      {
		size_t inner;
		Elf32_Word *grpref;

		if (shdr_info[cnt].data == NULL)
		  {
		    shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn,
						       NULL);
		    if (shdr_info[cnt].data == NULL)
		      INTERNAL_ERROR (fname);
		  }

		grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
		/* The first word of the section is a flag which we can
		   ignore here.  */
		for (inner = 1;
		     inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		     ++inner)
		  if (shdr_info[grpref[inner]].idx == 0)
		    {
		      size_t inner2 = grpref[inner];

		      shdr_info[inner2].idx = 1;
#if 0
		      /* XXX This isn't correct.  Is it possible to
			 leave out sections in the middle?  If not we
			 have to add all sections before the one we
			 just added as well.  But "before" means in
			 the address space and not in the section
			 header table (which is what the current code
			 does).  */
		      while (inner2-- > 1)
			if (shdr_info[inner2].idx == 0)
			  shdr_info[inner2].idx = 1;
#endif
		      changes = 1;
		    }
	      }

	    /* Handle references through sh_info.  */
	    if (SH_INFO_LINK_P (&shdr_info[cnt].shdr)
		&& shdr_info[shdr_info[cnt].shdr.sh_info].idx == 0)
	      {
		size_t inner = shdr_info[cnt].shdr.sh_info;

		shdr_info[inner].idx = 1;
		while (inner-- > 1)
		  if (shdr_info[inner].idx == 0)
		    shdr_info[inner].idx = 1;

		changes = 1;
	      }

	    /* Mark the section as investigated.  */
	    shdr_info[cnt].idx = 2;
	  }
	else if (shdr_info[cnt].idx == 0
		 && shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	  {
	    /* If any member of the group isn't removed all of the group
	       stays.  */
	    size_t inner;
	    size_t maxidx = 0;
	    Elf32_Word *grpref;
	    int newval = -1;

	    if (shdr_info[cnt].data == NULL)
	      {
		shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
		if (shdr_info[cnt].data == NULL)
		  INTERNAL_ERROR (fname);
	      }

	    grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	    for (inner = 1;
		 inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		 ++inner)
	      if (shdr_info[grpref[inner]].idx > 0)
		{
		  /* The whole group must stay.  Except for debugging
		     sections and the relocations for those sections.  */
		  for (inner = 1;
		       inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		       ++inner)
		    {
		      shdr_info[grpref[inner]].idx = 1;
		      maxidx = MAX (maxidx, grpref[inner]);
		    }

		  /* XXX This is wrong.  We must check for file offsets
		     and not section header table indeces.  */
		  while (maxidx-- > 1)
		    if (shdr_info[maxidx].idx == 0)
		      shdr_info[maxidx].idx = 1;

		  newval = 2;
		  changes = 1;
		  break;
		}

	    /* Mark section as used or verified to be not used.  */
	    shdr_info[cnt].idx = newval;
	  }
    }
  while (changes);

  /* Mark the section header string table as unused, we will create
     a new one.  */
  shdr_info[shstrndx].idx = 0;

  /* We need a string table for the section headers.  */
  shst = ebl_strtabinit (true);
  if (shst == NULL)
    error (EXIT_FAILURE, errno, gettext ("while preparing output for `%s'"),
	   output_fname ?: fname);

  /* Assign new section numbers.  */
  for (cnt = idx = 1; cnt < shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	shdr_info[cnt].idx = idx;

	/* Add this name to the section header string table.  */
	shdr_info[cnt].se = ebl_strtabadd (shst, shdr_info[cnt].name, 0);
      }

  /* Test whether we are doing anything at all.  */
  if (cnt == idx)
    /* Nope, all removable sections are already gone.  */
    goto fail_close;

  /* Add the section header string table section name.  */
  shdr_info[cnt].se = ebl_strtabadd (shst, ".shstrtab", 10);
  shdr_info[cnt].idx = idx;

  /* Create the section header.  */
  shdr_info[cnt].shdr.sh_type = SHT_STRTAB;
  shdr_info[cnt].shdr.sh_flags = 0;
  shdr_info[cnt].shdr.sh_addr = 0;
  shdr_info[cnt].shdr.sh_link = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_info = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_entsize = 1;
  /* We have to initialize these fields because they might contain
     values which later get rejected by the `gelf_update_shdr'
     function.  The values we are using don't matter, they must only
     be representable.  */
  shdr_info[cnt].shdr.sh_offset = 0;
  shdr_info[cnt].shdr.sh_size = 0;
  shdr_info[cnt].shdr.sh_addralign = 0;

  /* Create the section.  */
  for (cnt = 1; cnt <= shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	/* Create a new section.  */
	scn = elf_newscn (newelf);
	if (scn == NULL)
	  error (EXIT_FAILURE, 0, gettext ("while generating output file: %s"),
		 elf_errmsg (-1));

	assert (elf_ndxscn (scn) == shdr_info[cnt].idx);
      }

  /* Finalize the string table and fill in the correct indices in the
     section headers.  */
  assert (scn == elf_getscn (newelf, idx));
  ebl_strtabfinalize (shst, elf_newdata (scn));

  /* Update the section information.  */
  for (cnt = 1; cnt <= shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	Elf_Data *newdata;

	scn = elf_getscn (newelf, shdr_info[cnt].idx);
	assert (scn != NULL);

	/* Update the name.  */
	shdr_info[cnt].shdr.sh_name = ebl_strtaboffset (shdr_info[cnt].se);

	/* Update the section header from the input file.  Some fields
	   might be section indeces which now have to be adjusted.  */
	if (shdr_info[cnt].shdr.sh_link != 0)
	  shdr_info[cnt].shdr.sh_link =
	    shdr_info[shdr_info[cnt].shdr.sh_link].idx;

	if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	  {
	    size_t inner;
	    Elf32_Word *grpref;

	    assert (shdr_info[cnt].data != NULL);

	    grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	    for (inner = 0;
		 inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		 ++inner)
	      grpref[inner] = shdr_info[grpref[inner]].idx;
	  }

	/* Handle the SHT_REL, SHT_RELA, and SHF_INFO_LINK flag.  */
	if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
	  shdr_info[cnt].shdr.sh_info =
	    shdr_info[shdr_info[cnt].shdr.sh_info].idx;

	/* Set the section header in the new file.  */
	if (gelf_update_shdr (scn, &shdr_info[cnt].shdr) == 0)
	  /* There cannot be any overflows.  */
	  INTERNAL_ERROR (fname);

	/* Get the data from the old file if necessary.  We already
           created the data for the section header string table.  */
	if (cnt < shnum)
	  {
	    if (shdr_info[cnt].data == NULL)
	      {
		shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
		if (shdr_info[cnt].data == NULL)
		  INTERNAL_ERROR (fname);
	      }

	    /* Set the data.  This is done by copying from the old file.  */
	    newdata = elf_newdata (scn);
	    if (newdata == NULL)
	      INTERNAL_ERROR (fname);

	    /* Copy the structure.  */
	    *newdata = *shdr_info[cnt].data;

	    /* We have to adjust symtol tables.  The st_shndx member might
	       have to be updated.  */
	    if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		|| shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
	      {
		size_t elsize;
		size_t inner;

		elsize = gelf_fsize (elf, ELF_T_SYM, 1, ehdr->e_version);

		for (inner = 0;
		     inner < shdr_info[cnt].data->d_size / elsize;
		     ++inner)
		  {
		    GElf_Section sec;
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (shdr_info[cnt].data, inner,
						 &sym_mem);
		    if (sym == NULL)
		      INTERNAL_ERROR (fname);

		    if (sym->st_shndx == SHN_UNDEF
			|| sym->st_shndx >= shnum)
		      /* This is no section index, leave it alone.  */
		      continue;

		    sec = shdr_info[sym->st_shndx].idx;
		    assert (sec != 0);
		    if (sec != sym->st_shndx)
		      {
			sym->st_shndx = sec;
			if (gelf_update_sym (shdr_info[cnt].data, inner, sym)
			    == 0)
			  INTERNAL_ERROR (fname);
		      }

		    /* XXX We have to update the extended index section
		       table as well once we decide how to handle it.  Maybe
		       libelf will do it itself.  */
		  }
	      }
	  }
      }

  /* Finally finish the ELF header.  Fill in the fields not handled by
     libelf from the old file.  */
  newehdr = gelf_getehdr (newelf, &newehdr_mem);
  if (newehdr == NULL)
    INTERNAL_ERROR (fname);

  memcpy (newehdr->e_ident, ehdr->e_ident, EI_NIDENT);
  newehdr->e_type = ehdr->e_type;
  newehdr->e_machine = ehdr->e_machine;
  newehdr->e_version = ehdr->e_version;
  newehdr->e_entry = ehdr->e_entry;
  newehdr->e_flags = ehdr->e_flags;

  /* The new section header string table index.  */
  if (likely (idx < SHN_HIRESERVE) && likely (idx != SHN_XINDEX))
    newehdr->e_shstrndx = idx;
  else
    {
      /* The index does not fit in the ELF header field.  */
      shdr_info[0].scn = elf_getscn (elf, 0);

      if (gelf_getshdr (shdr_info[0].scn, &shdr_info[0].shdr) == NULL)
	INTERNAL_ERROR (fname);

      shdr_info[0].shdr.sh_link = idx;
      (void) gelf_update_shdr (shdr_info[0].scn, &shdr_info[0].shdr);

      newehdr->e_shstrndx = SHN_XINDEX;
    }

  if (gelf_update_ehdr (newelf, newehdr) == 0)
    {
      error (0, 0, gettext ("%s: error while creating ELF header: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* We have everything from the old file.  */
  if (elf_cntl (elf, ELF_C_FDDONE) != 0)
    {
      error (0, 0, gettext ("%s: error while reading the file: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* Finally write the file.  */
  if (elf_update (newelf, ELF_C_WRITE) == -1)
    {
      error (0, 0, gettext ("while writing `%s': %s"),
	     fname, elf_errmsg (-1));
      result = 1;
    }

 fail_close:
  /* That was it.  Close the descriptor.  */
  if (elf_end (newelf) != 0)
    {
      error (0, 0, gettext ("error while finishing `%s': %s"), fname,
	     elf_errmsg (-1));
      result = 1;
    }

 fail:
  /* Close the file descriptor if we created a new file.  */
  if (output_fname != NULL)
    close (fd);

  return result;
}
