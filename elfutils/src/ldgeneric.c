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
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <system.h>
#include "ld.h"


/* Prototypes for local functions.  */
static const char **ld_generic_lib_extensions (struct ld_state *)
     __attribute__ ((__const__));
static int ld_generic_is_debugscn_p (const char *name,
				     struct ld_state *statep);
static int ld_generic_file_close (struct usedfiles *fileinfo,
				  struct ld_state *statep);
static int ld_generic_file_process (int fd, struct usedfiles *fileinfo,
				    struct ld_state *statep,
				    struct usedfiles **nextp);
static void ld_generic_generate_sections (struct ld_state *statep);
static void ld_generic_create_sections (struct ld_state *statep);
static int ld_generic_flag_unresolved (struct ld_state *statep);
static int ld_generic_open_outfile (struct ld_state *statep, int class,
				    int data);
static int ld_generic_create_outfile (struct ld_state *statep);
static void ld_generic_relocate_section (Elf_Scn *outscn,
					 struct scninfo *firstp,
					 const Elf32_Word *dblindirect,
					 struct ld_state *statep);
static int ld_generic_finalize (struct ld_state *statep);
static bool ld_generic_special_section_number_p (struct ld_state *statep,
						 size_t number);
static bool ld_generic_section_type_p (struct ld_state *statep,
				       GElf_Word type);
static GElf_Xword ld_generic_dynamic_section_flags (struct ld_state *statep);
static void ld_generic_initialize_plt (struct ld_state *statep, Elf_Scn *scn);
static void ld_generic_initialize_pltrel (struct ld_state *statep,
					  Elf_Scn *scn);
static void ld_generic_initialize_got (struct ld_state *statep, Elf_Scn *scn);
static void ld_generic_finalize_plt (struct ld_state *statep, size_t nsym,
				     size_t nsym_dyn);
static int ld_generic_rel_type (struct ld_state *statep);
static void ld_generic_count_relocations (struct ld_state *statep,
					  struct scninfo *scninfo);
static void ld_generic_create_relocations (struct ld_state *statep,
					   Elf32_Word scnidx,
					   struct scninfo *scninfo);

static int file_process2 (struct usedfiles *fileinfo, struct ld_state *statep);
static void mark_section_used (struct scninfo *scninfo, Elf32_Word shndx,
			       struct ld_state *statep,
			       struct scninfo **grpscnp);


/* Check whether file associated with FD is a DSO.  */
static bool
is_dso_p (int fd)
{
  /* We have to read the 'e_type' field.  It has the same size (16
     bits) in 32- and 64-bit ELF.  */
  GElf_Half e_type;

  return (pread (fd, &e_type, sizeof (e_type), offsetof (GElf_Ehdr, e_type))
	  == sizeof (e_type)
	  && e_type == ET_DYN);
}


/* Print the complete name of a file, including the archive it is
   contained in.  */
static int
print_file_name (FILE *s, struct usedfiles *fileinfo, int first_level,
		 int newline)
{
  int npar = 0;

  if (fileinfo->archive_file != NULL)
    {
      npar = print_file_name (s, fileinfo->archive_file, 0, 0) + 1;
      fputc_unlocked ('(', s);
      fputs_unlocked (fileinfo->rfname, s);

      if (first_level)
	while (npar-- > 0)
	  fputc_unlocked (')', s);
    }
  else
    fputs_unlocked (fileinfo->rfname, s);

  if (first_level && newline)
    fputc_unlocked ('\n', s);

  return npar;
}


/* Function to determine whether an object will be dynamically linked.  */
static bool
dynamically_linked_p (struct ld_state *statep)
{
  return (statep->file_type == dso_file_type || statep->nplt > 0
	  || statep->ngot > 0);
}


/* Initialize state object.  This callback function is called after the
   parameters are parsed but before any file is searched for.  */
int
ld_prepare_state (struct ld_state *statep, const char *emulation)
{
  size_t emulation_len;
  char *fname;
  lt_dlhandle h;
  char *initname;
  int (*initfct) (struct ld_state *);

  /* When generating DSO we normally allow undefined symbols.  */
  statep->nodefs = true;

  /* To be able to detect problems we add a .comment section entry by
     default.  */
  statep->add_ld_comment = true;

  /* XXX We probably should find a better place for this.  The index
     of the first user-defined version is 2.  */
  statep->nextveridx = 2;

  /* Pick an not too small number for the initial size of the tables.  */
  ld_symbol_tab_init (&statep->symbol_tab, 1027);
  ld_section_tab_init (&statep->section_tab, 67);
  ld_version_str_tab_init (&statep->version_str_tab, 67);

  /* Initialize the section header string table.  */
  statep->shstrtab = ebl_strtabinit (true);
  if (statep->shstrtab == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot create string table"));

  /* Initialize the callbacks.  These are the defaults, the appropriate
     backend can later install its own callbacks.  */
  statep->callbacks.lib_extensions = ld_generic_lib_extensions;
  statep->callbacks.is_debugscn_p = ld_generic_is_debugscn_p;
  statep->callbacks.file_process = ld_generic_file_process;
  statep->callbacks.file_close = ld_generic_file_close;
  statep->callbacks.generate_sections = ld_generic_generate_sections;
  statep->callbacks.create_sections = ld_generic_create_sections;
  statep->callbacks.flag_unresolved = ld_generic_flag_unresolved;
  statep->callbacks.open_outfile = ld_generic_open_outfile;
  statep->callbacks.create_outfile = ld_generic_create_outfile;
  statep->callbacks.relocate_section = ld_generic_relocate_section;
  statep->callbacks.finalize = ld_generic_finalize;
  statep->callbacks.special_section_number_p =
    ld_generic_special_section_number_p;
  statep->callbacks.section_type_p = ld_generic_section_type_p;
  statep->callbacks.dynamic_section_flags = ld_generic_dynamic_section_flags;
  statep->callbacks.initialize_plt = ld_generic_initialize_plt;
  statep->callbacks.initialize_pltrel = ld_generic_initialize_pltrel;
  statep->callbacks.initialize_got = ld_generic_initialize_got;
  statep->callbacks.finalize_plt = ld_generic_finalize_plt;
  statep->callbacks.rel_type = ld_generic_rel_type;
  statep->callbacks.count_relocations = ld_generic_count_relocations;
  statep->callbacks.create_relocations = ld_generic_create_relocations;

  /* Find the ld backend library.  Use EBL to determine the name if
     the user hasn't provided one on the command line.  */
  if (emulation == NULL)
    {
      emulation = ebl_backend_name (statep->ebl);
      assert (emulation != NULL);
    }
  emulation_len = strlen (emulation);

  /* Construct the file name.  */
  fname = (char *) alloca (sizeof "libld_" + emulation_len);
  memcpy (stpcpy (fname, "libld_"), emulation, emulation_len + 1);

  /* Get the module loaded.  */
  if (lt_dlinit () != 0)
    error (EXIT_FAILURE, 0, gettext ("initialization of libltdl failed"));

  /* Make sure we can find our modules.  */
  /* XXX Use the correct path when done.  */
  lt_dladdsearchdir (OBJDIR "/.libs");

  /* Try loading.  */
  h = lt_dlopenext (fname);
  if (h == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot load ld backend library '%s': %s"),
	   fname, lt_dlerror ());

  /* Find the initializer.  It must be present.  */
  initname = (char *) alloca (emulation_len + sizeof "_ld_init");
  strcpy (mempcpy (initname, emulation, emulation_len), "_ld_init");
  initfct = (int (*) (struct ld_state *)) lt_dlsym (h, initname);

  if (initfct == NULL)
    error (EXIT_FAILURE, 0, gettext ("\
cannot find init function in ld backend library '%s': %s"),
	   fname, lt_dlerror ());

  /* Store the handle.  */
  statep->ldlib = h;

  /* Call the init function.  */
  return initfct (statep);
}


static int
check_for_duplicate2 (struct usedfiles *newp, struct usedfiles *list)
{
  struct usedfiles *first;

  if (list == NULL)
    return 0;

  list = first = list->next;
  do
    {
      /* When searching the needed list we might come across entries
	 for files which are not yet opened.  Stop then, there is
	 nothing more to test.  */
      if (list->status == not_opened)
	break;

      if (unlikely (list->ino == newp->ino)
	  && unlikely (list->dev == newp->dev))
	{
	  close (newp->fd);
	  newp->fd = -1;
	  if (newp->file_type == relocatable_file_type)
	    error (0, 0, gettext ("%s listed more than once as input"),
		   newp->rfname);

	  return 1;
	}
      list = list->next;
    }
  while (list != first);

  return 0;
}


static int
check_for_duplicate (struct usedfiles *newp, struct ld_state *statep)
{
  struct stat st;

  if (unlikely (fstat (newp->fd, &st) < 0))
    {
      close (newp->fd);
      return errno;
    }

  newp->dev = st.st_dev;
  newp->ino = st.st_ino;

  return (check_for_duplicate2 (newp, statep->relfiles)
	  || check_for_duplicate2 (newp, statep->dsofiles)
	  || check_for_duplicate2 (newp, statep->needed));
}


/* Find a file along the path described in the state.  */
static int
open_along_path2 (struct usedfiles *fileinfo, struct pathelement *path,
		  struct ld_state *statep)
{
  const char *fname = fileinfo->fname;
  size_t fnamelen = strlen (fname);
  int err = ENOENT;
  struct pathelement *firstp = path;

  if (path == NULL)
    /* Cannot find anything since we have no path.  */
    return ENOENT;

  do
    {
      if (likely (path->exist >= 0))
	{
	  /* Create the file name.  */
	  char *rfname = NULL;
	  size_t dirlen = strlen (path->pname);
	  int fd = -1;

	  if (fileinfo->file_type == archive_file_type)
	    {
	      const char **exts = (statep->statically
				   ? (const char *[2]) { ".a", NULL }
				   : LIB_EXTENSION (statep));

	      /* We have to create the actual file name.  We prepend "lib"
		 and add one of the extensions the platform has.  */
	      while (*exts != NULL)
		{
		  size_t extlen = strlen (*exts);
		  rfname = (char *) alloca (dirlen + 5 + fnamelen + extlen);
		  memcpy (mempcpy (stpcpy (mempcpy (rfname, path->pname,
						    dirlen),
					   "/lib"),
				   fname, fnamelen),
			  *exts, extlen + 1);

		  fd = open (rfname, O_RDONLY);
		  if (likely (fd != -1) || errno != ENOENT)
		    {
		      err = fd == -1 ? errno : 0;
		      break;
		    }

		  /* Next extension.  */
		  ++exts;
		}
	    }
	  else
	    {
	      assert (fileinfo->file_type == dso_file_type
		      || fileinfo->file_type == dso_needed_file_type);

	      rfname = (char *) alloca (dirlen + 1 + fnamelen + 1);
	      memcpy (stpcpy (mempcpy (rfname, path->pname, dirlen), "/"),
		      fname, fnamelen + 1);

	      fd = open (rfname, O_RDONLY);
	      if (unlikely (fd == -1))
		err = errno;
	    }

	  if (likely (fd != -1))
	    {
	      /* We found the file.  This also means the directory
		 exists.  */
	      fileinfo->fd = fd;
	      path->exist = 1;

	      /* Check whether we have this file already loaded.  */
	      if (check_for_duplicate (fileinfo, statep) != 0)
		{
		  close (fd);
		  fileinfo->fd = -1;
		  fileinfo->status = closed;

		  return EAGAIN;
		}

	      /* Make a copy of the name.  */
	      fileinfo->rfname = xstrdup (rfname);

	      if (unlikely (statep->trace_files))
		printf (fileinfo->file_type == archive_file_type
			? gettext ("%s (for -l%s)\n")
			: gettext ("%s (for DT_NEEDED %s)\n"),
			rfname, fname);

	      return 0;
	    }

	  /* The file does not exist.  Maybe the whole directory doesn't.
	     Check it unless we know it exists.  */
	  if (unlikely (path->exist == 0))
	    {
	      struct stat st;

	      /* Keep only the directory name.  Note that the path
		 might be relative.  This doesn't matter here.  We do
		 the test in any case even if there is the chance that
		 somebody wants to change the programs working
		 directory at some point which would make the result
		 of this test void.  Since changing the working
		 directory is completely wrong we are not taking this
		 case into account.  */
	      rfname[dirlen] = '\0';
	      if (unlikely (stat (rfname, &st) < 0) || ! S_ISDIR (st.st_mode))
		/* The directory does not exist or the named file is no
		   directory.  */
		path->exist = -1;
	      else
		path->exist = 1;
	    }
	}

      /* Next path element.  */
      path = path->next;
    }
 while (err == ENOENT && path != firstp);

  return err;
}


static int
open_along_path (struct usedfiles *fileinfo, struct ld_state *statep)
{
  const char *fname = fileinfo->fname;
  int err = ENOENT;

  if (fileinfo->file_type == relocatable_file_type)
    {
      /* Only libraries are searched along the path.  */
      fileinfo->fd = open (fname, O_RDONLY);

      if (likely (fileinfo->fd != -1))
	{
	  /* We found the file.  */
	  if (unlikely (statep->trace_files))
	    print_file_name (stdout, fileinfo, 1, 1);

	  return check_for_duplicate (fileinfo, statep);
	}

      /* If the name is an absolute path we are done.  */
      err = errno;
    }
  else
    {
      /* If the user specified two parts to the LD_LIBRARY_PATH variable
	 try the first part now.  */
      err = open_along_path2 (fileinfo, statep->ld_library_path1, statep);

      /* Try the user-specified path next.  */
      if (err == ENOENT)
	err = open_along_path2 (fileinfo,
				fileinfo->file_type == archive_file_type
				? statep->paths : statep->rpath_link,
				statep);

      /* Then the second part of the LD_LIBRARY_PATH value.  */
      if (err == ENOENT)
	err = open_along_path2 (fileinfo, statep->ld_library_path2, statep);

      /* In case we look for a DSO handle now the RUNPATH.  */
      if (err == ENOENT && fileinfo->file_type == dso_file_type)
	err = open_along_path2 (fileinfo, statep->runpath_link, statep);

      /* Finally the path from the default linker script.  */
      if (err == ENOENT)
	err = open_along_path2 (fileinfo, statep->default_paths, statep);
    }

  if (unlikely (err != 0)
      && (err != EAGAIN || fileinfo->file_type == relocatable_file_type))
    error (0, err, gettext ("cannot open %s"), fileinfo->fname);

  return err;
}


static void
check_type_and_size (const GElf_Sym *sym, struct usedfiles *fileinfo,
		     struct symbol *oldp, struct ld_state *statep)
{
  /* We check the type and size of the symbols.  In both cases the
     information can be missing (size is zero, type is STT_NOTYPE) in
     which case we issue no warnings.  Otherwise everything must
     match.  If the type does not match there is no point in checking
     the size.  */

  if (GELF_ST_TYPE (sym->st_info) != STT_NOTYPE && oldp->type != STT_NOTYPE
      && unlikely (oldp->type != GELF_ST_TYPE (sym->st_info)))
    {
      char buf1[64];
      char buf2[64];

      error (0, 0, gettext ("\
Warning: type of `%s' changed from %s in %s to %s in %s"),
	     oldp->name,
	     ebl_symbol_type_name (statep->ebl, oldp->type,
				   buf1, sizeof (buf1)),
	     oldp->file->rfname,
	     ebl_symbol_type_name (statep->ebl, GELF_ST_TYPE (sym->st_info),
				   buf2, sizeof (buf2)),
	     fileinfo->rfname);
    }
  else if (GELF_ST_TYPE (sym->st_info) == STT_OBJECT
	   && oldp->size != 0
	   && unlikely (oldp->size != sym->st_size))
    error (0, 0, gettext ("\
Warning: size of `%s' changed from %" PRIu64 " in %s to %" PRIu64 " in %s"),
	   oldp->name, oldp->size, oldp->file->rfname,
	   sym->st_size, fileinfo->rfname);
}


static int
check_definition (const GElf_Sym *sym, size_t symidx,
		  struct usedfiles *fileinfo, struct symbol *oldp,
		  struct ld_state *statep)
{
  int result = 0;
  bool old_in_dso = (oldp->file->file_type == dso_file_type
		     || oldp->file->file_type == dso_needed_file_type);
  bool use_new_def = false;

  if (sym->st_shndx != SHN_UNDEF
      && (! oldp->defined
	  || (sym->st_shndx != SHN_COMMON && oldp->common
	      && fileinfo->ehdr.e_type != ET_DYN)
	  || (oldp->file->ehdr.e_type == ET_DYN
	      && fileinfo->ehdr.e_type != ET_DYN)))
    {
      /* We found a definition for a previously undefined symbol or a
	 real definition for a previous common-only definition or a
	 redefinition of a symbol definition in an object file
	 previously defined in a DSO.  First perform some tests which
	 will show whether the common is really matching the
	 definition.  */
      check_type_and_size (sym, fileinfo, oldp, statep);

      /* We leave the next element intact to not interrupt the list
	 with the unresolved symbols.  Whoever walks the list will
	 have to check the `defined' flag.  But we remember that this
	 list element is not unresolved anymore.  */
      if (! oldp->defined)
	{
	  --statep->nunresolved;
	  if (! oldp->weak)
	    --statep->nunresolved_nonweak;

	  /* Remove from the list.  */
	  if (oldp->next == oldp)
	    statep->unresolved = NULL;
	  else
	    {
	      oldp->next->previous = oldp->previous;
	      oldp->previous->next = oldp->next;
	      if (statep->unresolved == oldp)
		statep->unresolved = oldp->next;
	    }
	}

      /* Use the values of the definition from now on.  */
      use_new_def = true;
    }
  else if (sym->st_shndx != SHN_UNDEF && oldp->defined
	   && sym->st_shndx != SHN_COMMON
	   && unlikely (! oldp->common)
	   /* Multiple definitions are no fatal errors if the -z muldefs flag
	      is used.  We don't warn about the multiple definition unless we
	      are told to be verbose.  */
	   && (!statep->muldefs || verbose)
	   && ! old_in_dso && fileinfo->file_type == relocatable_file_type)
    {
      /* We have a double definition.  This is a problem.  */
      char buf[64];
      GElf_Sym oldsymmem;
      const GElf_Sym *oldsym;
      struct usedfiles *oldfile;
      const char *scnname;
      size_t xndx;
      size_t shndx;
      size_t shnum;

      if (elf_getshnum (fileinfo->elf, &shnum) < 0)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot determine number of sections: %s"),
	       elf_errmsg (-1));

      /* XXX Use only ebl_section_name.  */
      if ((sym->st_shndx < SHN_LORESERVE || sym->st_shndx > SHN_HIRESERVE)
	  && sym->st_shndx < shnum)
	scnname = elf_strptr (fileinfo->elf,
			      fileinfo->shstrndx,
			      fileinfo->scninfo[sym->st_shndx].shdr.sh_name);
      else
	// XXX extended section
	scnname = ebl_section_name (statep->ebl, sym->st_shndx, 0,
				    buf, sizeof (buf), NULL, shnum);

      /* XXX Print source file and line number.  */
      print_file_name (stderr, fileinfo, 1, 0);
      fprintf (stderr,
	       gettext ("(%s+%#" PRIx64 "): multiple definition of %s `%s'\n"),
	       scnname,
	       sym->st_value,
	       ebl_symbol_type_name (statep->ebl, GELF_ST_TYPE (sym->st_info),
				     buf, sizeof (buf)),
	       oldp->name);

      oldfile = oldp->file;
      oldsym = gelf_getsymshndx (oldfile->symtabdata, oldfile->xndxdata,
				 oldp->symidx, &oldsymmem, &xndx);
      if (oldsym == NULL)
	/* This should never happen since the same call
	   succeeded before.  */
	abort ();

      shndx = oldsym->st_shndx;
      if (unlikely (oldsym->st_shndx == SHN_XINDEX))
	shndx = xndx;

      /* XXX Use only ebl_section_name.  */
      if (shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE)
	scnname = elf_strptr (oldfile->elf,
			      oldfile->shstrndx,
			      oldfile->scninfo[shndx].shdr.sh_name);
      else
	scnname = ebl_section_name (statep->ebl, oldsym->st_shndx, shndx, buf,
				    sizeof (buf), NULL, shnum);

      /* XXX Print source file and line number.  */
      print_file_name (stderr, oldfile, 1, 0);
      fprintf (stderr, gettext ("(%s+%#" PRIx64 "): first defined here\n"),
	       scnname, oldsym->st_value);

      if (!statep->muldefs)
	result = 1;
    }
  else if (old_in_dso && fileinfo->file_type == relocatable_file_type
	   && sym->st_shndx != SHN_UNDEF)
    /* We use the definition from a normal relocatable file over the
       definition in a DSO.  This is what the dynamic linker would
       do, too.  */
    use_new_def = true;

  if (use_new_def)
    {
      /* Adjust the symbol record appropriately and remove
	 the symbol from the list of symbols which are taken from DSOs.  */
      if (old_in_dso && fileinfo->file_type == relocatable_file_type)
	{
	  if (oldp->file->next == oldp->file)
	    statep->from_dso = NULL;
	  else
	    {
	      if (statep->from_dso == oldp)
		statep->from_dso = oldp->next;
	      oldp->next->previous = oldp->previous;
	      oldp->previous->next = oldp->next;
	    }

	  if (oldp->type == STT_FUNC)
	    --statep->nplt;
	  else
	    --statep->ngot;
	}

      /* Use the values of the definition from now on.  */
      oldp->size = sym->st_size;
      oldp->type = GELF_ST_TYPE (sym->st_info);
      oldp->symidx = symidx;
      oldp->scndx = sym->st_shndx;
      oldp->file = fileinfo;
      oldp->defined = 1;
      oldp->common = sym->st_shndx == SHN_COMMON;
      if (fileinfo->file_type == relocatable_file_type)
	/* If the definition comes from a DSO we pertain the weak flag
	   and it's indicating whether the reference is weak or not.  */
	oldp->weak = GELF_ST_BIND (sym->st_info) == STB_WEAK;

      /* Add to the list of symbols used from DSOs if necessary.  */
      if (! old_in_dso && fileinfo->file_type != relocatable_file_type)
	{
	  if (statep->from_dso == NULL)
	    statep->from_dso = oldp->next = oldp->previous = oldp;
	  else
	    {
	      oldp->next = statep->from_dso;
	      oldp->previous = statep->from_dso->previous;
	      oldp->previous->next = oldp->next->previous = oldp;
	    }

	  /* If the object is a function we allocate a PLT entry,
	     otherwise only a GOT entry.  */
	  if (GELF_ST_TYPE (sym->st_info) == STT_FUNC)
	    ++statep->nplt;
	  else
	    ++statep->ngot;
	}
    }

  return result;
}


static struct scninfo *
find_section_group (struct usedfiles *fileinfo, Elf32_Word shndx,
		    Elf_Data **datap)
{
  struct scninfo *runp;

  for (runp = fileinfo->groups; runp != NULL; runp = runp->next)
    if (!runp->used)
      {
	Elf32_Word *grpref;
	size_t cnt;
	Elf_Data *data;

	data = elf_getdata (runp->scn, NULL);
	if (data == NULL)
	  error (EXIT_FAILURE, 0,
		 gettext ("%s: cannot get section group data: %s"),
		 fileinfo->fname, elf_errmsg (-1));

	/* There cannot be another data block.  */
	assert (elf_getdata (runp->scn, data) == NULL);

	grpref = (Elf32_Word *) data->d_buf;
	cnt = data->d_size / sizeof (Elf32_Word);
	/* Note that we stop after looking at index 1 since index 0
	   contains the flags for the section group.  */
	while (cnt > 1)
	  if (grpref[--cnt] == shndx)
	    {
	      *datap = data;
	      return runp;
	    }
      }

  /* If we come here no section group contained the given section
     despite the SHF_GROUP flag.  This is an error in the input
     file.  */
  error (EXIT_FAILURE, 0, gettext ("\
%s: section '%s' with group flag set does not belong to any group"),
	 fileinfo->fname,
	 elf_strptr (fileinfo->elf, fileinfo->shstrndx,
		     fileinfo->scninfo[shndx].shdr.sh_name));
  return NULL;
}


/* Mark all sections which belong to the same group as section SHNDX
   as used.  */
static void
mark_section_group (struct usedfiles *fileinfo, Elf32_Word shndx,
		    struct ld_state *statep, struct scninfo **grpscnp)
{
  /* First locate the section group.  There can be several (many) of
     them.  */
  size_t cnt;
  Elf32_Word *grpref;
  Elf_Data *data;
  struct scninfo *grpscn = find_section_group (fileinfo, shndx, &data);
  *grpscnp = grpscn;

  /* Mark all the sections as used.

     XXX Two possible problems here:

     - the gABI says "The section must be referenced by a section of type
       SHT_GROUP".  I hope everybody reads this as "exactly one section".

     - section groups are also useful to mark the debugging section which
       belongs to a text section.  Unconditionally adding debugging sections
       is therefore probably not what is wanted if stripping is required.  */

  /* Mark the section group as handled.  */
  grpscn->used = true;

  grpref = (Elf32_Word *) data->d_buf;
  cnt = data->d_size / sizeof (Elf32_Word);
  while (cnt > 1)
    {
      Elf32_Word idx = grpref[--cnt];

      if (fileinfo->scninfo[idx].grpid != 0)
	error (EXIT_FAILURE, 0, gettext ("\
%s: section [%2d] '%s' is in more than one section group"),
	       fileinfo->fname, (int) idx,
	       elf_strptr (fileinfo->elf,
			   fileinfo->shstrndx,
			   fileinfo->scninfo[idx].shdr.sh_name));

      fileinfo->scninfo[idx].grpid = grpscn->grpid;

      if (statep->strip == strip_none
	  /* If we are stripping remove debug sections.  */
	  || (!IS_DEBUGSCN_P (elf_strptr (fileinfo->elf,
					  fileinfo->shstrndx,
					  fileinfo->scninfo[idx].shdr.sh_name),
			      statep)
	      /* And the relocation sections for the debug
		 sections.  */
	      && ((fileinfo->scninfo[idx].shdr.sh_type != SHT_RELA
		   && fileinfo->scninfo[idx].shdr.sh_type != SHT_REL)
		  || !IS_DEBUGSCN_P (elf_strptr (fileinfo->elf,
						 fileinfo->shstrndx,
						 fileinfo->scninfo[fileinfo->scninfo[idx].shdr.sh_info].shdr.sh_name),
				     statep))))
	{
	  struct scninfo *ignore;

	  mark_section_used (&fileinfo->scninfo[idx], idx, statep, &ignore);
	}
    }
}


static void
mark_section_used (struct scninfo *scninfo, Elf32_Word shndx,
		   struct ld_state *statep, struct scninfo **grpscnp)
{
  if (scninfo->used)
    /* Nothing to be done.  */
    return;

  /* We need this section.  */
  scninfo->used = true;

  /* Handle section linked by 'sh_link'.  */
  if (scninfo->shdr.sh_link != 0)
    {
      struct scninfo *ignore;
      mark_section_used (&scninfo->fileinfo->scninfo[scninfo->shdr.sh_link],
			 scninfo->shdr.sh_link, statep, &ignore);
    }

  /* Handle section linked by 'sh_info'.  */
  if (scninfo->shdr.sh_info != 0 && (scninfo->shdr.sh_flags & SHF_INFO_LINK))
    {
      struct scninfo *ignore;
      mark_section_used (&scninfo->fileinfo->scninfo[scninfo->shdr.sh_info],
			 scninfo->shdr.sh_info, statep, &ignore);
    }

  if ((scninfo->shdr.sh_flags & SHF_GROUP) && statep->gc_sections)
    /* Find the section group which contains this section.  */
    mark_section_group (scninfo->fileinfo, shndx, statep, grpscnp);
}


/* We collect all sections in a hashing table.  All sections with the
   same name are collected in a list.  Note that we do not determine
   which sections are finally collected in the same output section
   here.  This would be terribly inefficient.  It will be done later.  */
static void
add_section (struct usedfiles *fileinfo, struct scninfo *scninfo,
	     struct ld_state *statep)
{
  struct scnhead *queued;
  struct scnhead search;
  unsigned long int hval;
  GElf_Shdr *shdr = &scninfo->shdr;
  struct scninfo *grpscn = NULL;
  Elf_Data *grpscndata = NULL;

  /* See whether we can determine right away whether we need this
     section in the output.

     XXX I assume here that --gc-sections only affects extraction
     from an archive.  If it also affects objects files given on
     the command line then somebody must explain to me how the
     dependency analysis should work.  Should the entry point be
     the root?  What if it is a numeric value?  */
  if (!scninfo->used
      && (statep->strip == strip_none
	  || (shdr->sh_flags & SHF_ALLOC) != 0
	  || shdr->sh_type == SHT_NOTE
	  || (shdr->sh_type == SHT_PROGBITS
	      && strcmp (elf_strptr (fileinfo->elf,
				     fileinfo->shstrndx,
				     shdr->sh_name), ".comment") == 0))
      && (fileinfo->status != in_archive || !statep->gc_sections))
    /* Mark as used and handle reference recursively if necessary.  */
    mark_section_used (scninfo, elf_ndxscn (scninfo->scn), statep, &grpscn);

  if ((scninfo->shdr.sh_flags & SHF_GROUP) && grpscn == NULL)
    /* Determine the symbol which name constitutes the signature
       for the section group.  */
    grpscn = find_section_group (fileinfo, elf_ndxscn (scninfo->scn),
				 &grpscndata);
  assert (grpscn == NULL || grpscn->symbols->name != NULL);

  /* Determine the section name.  */
  search.name = elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			    scninfo->shdr.sh_name);
  search.type = scninfo->shdr.sh_type;
  search.flags = scninfo->shdr.sh_flags;
  search.entsize = scninfo->shdr.sh_entsize;
  search.grp_signature = grpscn != NULL ? grpscn->symbols->name : NULL;
  search.kind = scn_normal;
  hval = elf_hash (search.name);

  /* Find already queued sections.  */
  queued = ld_section_tab_find (&statep->section_tab, hval, &search);
  if (queued != NULL)
    {
      bool is_comdat = false;

      /* If this section is part of a COMDAT section group we simply
	 ignore it since we already have a copy.  */
      if (unlikely (scninfo->shdr.sh_flags & SHF_GROUP))
	{
	  /* Get the data of the section group section.  */
	  if (grpscndata == NULL)
	    {
	      grpscndata = elf_getdata (grpscn->scn, NULL);
	      assert (grpscndata != NULL);
	    }

	  /* XXX Possibly unaligned memory access.  */
	  is_comdat = ((Elf32_Word *) grpscndata->d_buf)[0] & GRP_COMDAT;
	}

      if (!is_comdat)
	{
	  /* No COMDAT section, we use the data.  */
	  scninfo->next = queued->last->next;
	  queued->last = queued->last->next = scninfo;

	  queued->flags = SH_FLAGS_COMBINE (queued->flags,
					    scninfo->shdr.sh_flags);
	  queued->align = MAX (queued->align, scninfo->shdr.sh_addralign);
	}
    }
  else
    {
      queued = (struct scnhead *) xcalloc (sizeof (struct scnhead), 1);
      queued->kind = scn_normal;
      queued->name = search.name;
      queued->type = scninfo->shdr.sh_type;
      queued->flags = scninfo->shdr.sh_flags;
      queued->align = scninfo->shdr.sh_addralign;
      queued->entsize = scninfo->shdr.sh_entsize;
      queued->grp_signature = grpscn != NULL ? grpscn->symbols->name : NULL;
      queued->segment_nr = ~0;
      queued->last = scninfo->next = scninfo;

      /* Add to the hash table and possibly overwrite existing value.  */
      ld_section_tab_insert (&statep->section_tab, hval, queued);
    }
}


static int
add_relocatable_file (struct usedfiles *fileinfo, struct ld_state *statep,
		      int secttype)
{
  size_t scncnt;
  size_t cnt;
  Elf_Data *symtabdata = NULL;
  Elf_Data *xndxdata = NULL;
  Elf_Data *versymdata = NULL;
  Elf_Data *verdefdata = NULL;
  Elf_Data *verneeddata = NULL;
  size_t symstridx = 0;
  size_t nsymbols = 0;
  size_t nlocalsymbols = 0;
  bool has_merge_sections = false;

  /* Prerequisites.  */
  assert (fileinfo->elf != NULL);

  /* Allocate memory for the sections.  */
  if (elf_getshnum (fileinfo->elf, &scncnt) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot determine number of sections: %s"),
	   elf_errmsg (-1));

  fileinfo->scninfo = (struct scninfo *) xcalloc (scncnt,
						  sizeof (struct scninfo));

  /* Read all the section headers and find the symbol table.  Note
     that we don't skip the section with index zero.  Even though the
     section itself is always empty the section header contains
     informaton for the case when the section index for the section
     header string table is too large to fit in the ELF header.  */
  for (cnt = 0; cnt < scncnt; ++cnt)
    {
      GElf_Shdr *shdr;
      Elf_Data *data;

      /* Store the handle for the section.  */
      fileinfo->scninfo[cnt].scn = elf_getscn (fileinfo->elf, cnt);

      /* Get the ELF section header and data.  */
      shdr = gelf_getshdr (fileinfo->scninfo[cnt].scn,
			   &fileinfo->scninfo[cnt].shdr);
      data = elf_getdata (fileinfo->scninfo[cnt].scn, NULL);
      if (shdr == NULL)
	{
	  /* This should never happen.  */
	  fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		   fileinfo->rfname, __FILE__, __LINE__);
	  return 1;
	}

      /* Check whether this section is marked as merge-able.  */
      has_merge_sections |= (shdr->sh_flags & SHF_MERGE) != 0;

      /* Make the file structure available.  */
      fileinfo->scninfo[cnt].fileinfo = fileinfo;

      if (shdr->sh_type == SHT_SYMTAB)
	{
	  assert (fileinfo->symtabdata == NULL);
	  fileinfo->symtabdata = data;
	  fileinfo->nsymtab = shdr->sh_size / shdr->sh_entsize;
	  fileinfo->nlocalsymbols = shdr->sh_info;
	  fileinfo->symstridx = shdr->sh_link;

	  /* If we are looking for the normal symbol table we just
	     found it.  */
	  if (secttype == SHT_SYMTAB)
	    {
	      assert (symtabdata == NULL);
	      symtabdata = fileinfo->symtabdata;
	      symstridx = shdr->sh_link;
	      nsymbols = fileinfo->nsymtab;
	      nlocalsymbols = fileinfo->nlocalsymbols;
	    }
	}
      else if (shdr->sh_type == SHT_DYNSYM)
	{
	  assert (fileinfo->dynsymtabdata == NULL);
	  fileinfo->dynsymtabdata = data;
	  fileinfo->ndynsymtab = shdr->sh_size / shdr->sh_entsize;
	  fileinfo->dynsymstridx = shdr->sh_link;

	  /* If we are looking for the dynamic symbol table we just
	     found it.  */
	  if (secttype == SHT_DYNSYM)
	    {
	      assert (symtabdata == NULL);
	      symtabdata = fileinfo->dynsymtabdata;
	      symstridx = shdr->sh_link;
	      nsymbols = fileinfo->ndynsymtab;
	      nlocalsymbols = shdr->sh_info;
	    }
	}
      else if (shdr->sh_type == SHT_SYMTAB_SHNDX)
	{
	  assert (xndxdata == NULL);
	  fileinfo->xndxdata = xndxdata = data;
	}
      else if (shdr->sh_type == SHT_GNU_versym)
	{
	  assert (versymdata == 0);
	  fileinfo->versymdata = versymdata = data;
	}
      else if (shdr->sh_type == SHT_GNU_verdef)
	{
	  size_t nversions;

	  assert (verdefdata == 0);
	  fileinfo->verdefdata = verdefdata = data;

	  /* Allocate the arrays flagging the use of the version and
	     to track of allocated names.  */
	  fileinfo->nverdef = nversions = shdr->sh_info;
	  /* We have NVERSIONS + 1 because the indeces used to access the
	     sectino start with one; zero represents local binding.  */
	  fileinfo->verdefused = (int *) xcalloc (sizeof (int), nversions + 1);
	  fileinfo->verdefent
	    = (struct Ebl_Strent **) xmalloc (sizeof (struct Ebl_Strent *)
					      * (nversions + 1));
	}
      else if (shdr->sh_type == SHT_GNU_verneed)
	{
	  assert (verneeddata == 0);
	  fileinfo->verneeddata = verneeddata = data;
	}
      else if (shdr->sh_type == SHT_DYNAMIC)
	{
	  assert (fileinfo->dynscn == NULL);
	  fileinfo->dynscn = fileinfo->scninfo[cnt].scn;
	}
      else if (unlikely (shdr->sh_type == SHT_GROUP))
	{
	  Elf_Scn *symscn;
	  GElf_Shdr symshdr_mem;
	  GElf_Shdr *symshdr;
	  Elf_Data *symdata;

	  if (fileinfo->ehdr.e_type != ET_REL)
	    error (EXIT_FAILURE, 0, gettext ("\
%s: only files of type ET_REL might contain section groups"),
		   fileinfo->fname);

	  fileinfo->scninfo[cnt].next = fileinfo->groups;
	  fileinfo->scninfo[cnt].grpid = cnt;
	  fileinfo->groups = &fileinfo->scninfo[cnt];

	  /* Determine the signature.  We create a symbol record for
	     it.  Only the name element is important.  */
	  fileinfo->scninfo[cnt].symbols
	    = (struct symbol *) xcalloc (sizeof (struct symbol), 1);
	  symscn = elf_getscn (fileinfo->elf, shdr->sh_link);
	  symshdr = gelf_getshdr (symscn, &symshdr_mem);
	  symdata = elf_getdata (symscn, NULL);
	  if (symshdr != NULL)
	    {
	      GElf_Sym sym_mem;
	      GElf_Sym *sym;

	      /* We don't need the section index and therefore we don't
		 have to use 'gelf_getsymshndx'.  */
	      sym = gelf_getsym (symdata, shdr->sh_info, &sym_mem);
	      if (sym != NULL)
		{
		  struct symbol *symbol = fileinfo->scninfo[cnt].symbols;

		  symbol->name = elf_strptr (fileinfo->elf, symshdr->sh_link,
					     sym->st_name);
		  symbol->symidx = shdr->sh_info;
		  symbol->file = fileinfo;
		}
	    }
	  if (fileinfo->scninfo[cnt].symbols->name == NULL)
	    error (EXIT_FAILURE, 0, gettext ("\
%s: cannot determine signature of section group [%2d] '%s': %s"),
		   fileinfo->fname,
		   elf_ndxscn (fileinfo->scninfo[cnt].scn),
		   elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			       shdr->sh_name),
		   elf_errmsg (-1));

	  /* The 'used' flag is used to indicate when the information
	     in the section group is used to mark all other sections
	     as used.  So it must not be true yet.  */
	  assert (fileinfo->scninfo[cnt].used == false);
	}
      else if (! SECTION_TYPE_P (statep, shdr->sh_type)
	       && (shdr->sh_flags & SHF_OS_NONCONFORMING) != 0)
	/* According to the gABI it is a fatal error if the file contains
	   a section with unknown type and the SHF_OS_NONCONFORMING flag
	   set.  */
	error (EXIT_FAILURE, 0,
	       gettext ("%s: section '%s' has unknown type: %d"),
	       fileinfo->fname,
	       elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			   shdr->sh_name),
	       (int) shdr->sh_type);
      /* We don't have to add a few section types here.  These will be
	 generated from scratch for the new output file.  We also
	 don't add the sections of DSOs here since these sections are
	 not used in the resulting object file.  */
      else if (fileinfo->file_type == relocatable_file_type
	       && cnt > 0
	       && (shdr->sh_type == SHT_PROGBITS
		   || shdr->sh_type == SHT_RELA
		   || shdr->sh_type == SHT_REL
		   || shdr->sh_type == SHT_NOTE
		   || shdr->sh_type == SHT_NOBITS
		   || shdr->sh_type == SHT_INIT_ARRAY
		   || shdr->sh_type == SHT_FINI_ARRAY
		   || shdr->sh_type == SHT_PREINIT_ARRAY))
	add_section (fileinfo, &fileinfo->scninfo[cnt], statep);
    }

  /* Handle the symbols.  Record defined and undefined symbols in the
     hash table.  In theory there can be a file without any symbol
     table.  */
  if (likely (symtabdata != NULL))
    {
      /* In case this file contains merge-able sections we have to
	 locate the symbols which are in these sections.  */
      fileinfo->has_merge_sections = has_merge_sections;
      if (has_merge_sections)
	{
	  fileinfo->symref =
	    (struct symbol **) xcalloc (nsymbols, sizeof (struct symbol *));

	  /* Only handle the local symbols here.  */
	  for (cnt = 0; cnt < nlocalsymbols; ++cnt)
	    {
	      GElf_Sym symmem;
	      size_t shndx;
	      const GElf_Sym *sym;

	      sym = gelf_getsymshndx (symtabdata, xndxdata, cnt, &symmem,
				      &shndx);
	      if (sym == NULL)
		{
		  /* This should never happen.  */
		  fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
			   fileinfo->rfname, __FILE__, __LINE__);
		  return 1;
		}

	      if (likely (shndx != SHN_XINDEX))
		shndx = sym->st_shndx;
	      else if (unlikely (shndx == 0))
		{
		  fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
			   fileinfo->rfname, __FILE__, __LINE__);
		  return 1;
		}

	      if (GELF_ST_TYPE (sym->st_info) != STT_SECTION
		  && (shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE)
		  && (fileinfo->scninfo[shndx].shdr.sh_flags & SHF_MERGE))
		{
		  /* Create a symbol record for this symbol and add it
		     to the list for this section.  */
		  struct symbol *newp;

		  newp = (struct symbol *) xcalloc (1, sizeof (struct symbol));

		  newp->symidx = cnt;
		  newp->scndx = shndx;
		  newp->file = fileinfo;
		  fileinfo->symref[cnt] = newp;

		  if (fileinfo->scninfo[shndx].symbols == NULL)
		    fileinfo->scninfo[shndx].symbols = newp->next_in_scn
		      = newp;
		  else
		    {
		      newp->next_in_scn
			= fileinfo->scninfo[shndx].symbols->next_in_scn;
		      fileinfo->scninfo[shndx].symbols
			= fileinfo->scninfo[shndx].symbols->next_in_scn = newp;
		    }
		}
	    }
	}
      else
	/* Create array with pointers to the symbol definitions.  Note
	   that we only allocate memory for the non-local symbols
	   since we have no merge-able sections.  But we store the
	   pointer as if it was for the whole symbol table.  This
	   saves some memory.  */
	fileinfo->symref = (struct symbol **)
	  xcalloc (nsymbols - nlocalsymbols, sizeof (struct symbol *))
	  - nlocalsymbols;

      /* Don't handle local symbols here.  It's either not necessary
	 at all or has already happened.  */
      for (cnt = nlocalsymbols; cnt < nsymbols; ++cnt)
	{
	  GElf_Sym symmem;
	  size_t shndx;
	  const GElf_Sym *sym = gelf_getsymshndx (symtabdata, xndxdata, cnt,
						  &symmem, &shndx);
	  struct symbol *newp;
	  struct symbol *oldp;
	  struct symbol search;
	  unsigned long int hval;

	  if (sym == NULL)
	    {
	      /* This should never happen.  */
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  if (likely (shndx != SHN_XINDEX))
	    shndx = sym->st_shndx;
	  else if (unlikely (shndx == 0))
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  /* If the DSO uses symbols determine whether this is the default
	     version.  Otherwise we'll ignore the symbol.  */
	  if (versymdata != NULL)
	    {
	      GElf_Versym versym;

	      if (gelf_getversym (versymdata, cnt, &versym) == NULL)
		/* XXX Should we handle faulty input files more graceful?  */
		assert (! "gelf_getversym failed");

	      if ((versym & 0x8000) != 0)
		/* Ignore the symbol, it's not the default version.  */
		continue;
	    }

	  /* See whether we know anything about this symbol.  */
	  search.name = elf_strptr (fileinfo->elf, symstridx, sym->st_name);
	  hval = elf_hash (search.name);

	  /* We ignore the symbols the linker generates.  This are
	     _GLOBAL_OFFSET_TABLE_, _DYNAMIC.  */
	  if (sym->st_shndx != SHN_UNDEF
	      /* If somebody defines such a variable in a relocatable we
		 don't ignore it.  Let the user get what s/he deserves.  */
	      && fileinfo->file_type != relocatable_file_type
	      && ((hval == 165832675 && strcmp (search.name, "_DYNAMIC") == 0)
		  || (hval == 102264335
		      && strcmp (search.name, "_GLOBAL_OFFSET_TABLE_") == 0)))
	    continue;

	  oldp = ld_symbol_tab_find (&statep->symbol_tab, hval, &search);
	  if (likely (oldp == NULL))
	    {
	      /* No symbol of this name know.  Add it.  */
	      newp = (struct symbol *) xmalloc (sizeof (*newp));
	      newp->name = search.name;
	      newp->size = sym->st_size;
	      newp->type = GELF_ST_TYPE (sym->st_info);
	      newp->symidx = cnt;
	      newp->outsymidx = 0;
	      newp->scndx = shndx;
	      newp->file = fileinfo;
	      newp->defined = newp->scndx != SHN_UNDEF;
	      newp->common = newp->scndx == SHN_COMMON;
	      newp->weak = GELF_ST_BIND (sym->st_info) == STB_WEAK;
	      newp->added = 0;
	      newp->merged = 0;
	      newp->in_dso = secttype == SHT_DYNSYM;
	      newp->next_in_scn = NULL;
	      newp->previous = NULL;

	      if (newp->scndx == SHN_UNDEF)
		{
		  if (statep->unresolved == NULL)
		    {
		      statep->unresolved = newp->next = newp->previous = newp;
		      assert (statep->nunresolved == 0);
		    }
		  else
		    {
		      newp->next = statep->unresolved;
		      newp->previous = statep->unresolved->previous;
		      statep->unresolved->previous->next = newp;
		      statep->unresolved->previous = newp;
		    }
		  ++statep->nunresolved;
		  if (! newp->weak)
		    ++statep->nunresolved_nonweak;
		}
	      else
		newp->next = NULL;

	      /* Insert the new symbol.  */
	      if (unlikely (ld_symbol_tab_insert (&statep->symbol_tab,
						  hval, newp) != 0))
		/* This cannot happen.  */
		abort ();

	      fileinfo->symref[cnt] = newp;

	      /* We have a few special symbols to recognize.  The symbols
		 _init and _fini are the initialization and finalization
		 functions respectively.  They have to be made known in
		 the dynamic section and therefore we have to find out
		 now whether these functions exist or not.  */
	      if (hval == 6685956 && strcmp (newp->name, "_init") == 0)
		statep->init_symbol = newp;
	      else if (hval == 6672457 && strcmp (newp->name, "_fini") == 0)
		statep->fini_symbol = newp;
	    }
	  else if (unlikely (check_definition (sym, cnt, fileinfo,
					       oldp, statep) != 0))
	    return 1;
	  else
	    {
	      newp = fileinfo->symref[cnt] = oldp;

	      if (secttype == SHT_DYNSYM)
		newp->in_dso = 1;
	    }

	  /* Mark the section the symbol we need comes from as used.  */
	  if (shndx != SHN_UNDEF
	      && (shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE))
	    {
	      struct scninfo *ignore;

#ifndef NDEBUG
	      size_t shnum;
	      assert (elf_getshnum (fileinfo->elf, &shnum) == 0);
	      assert (shndx < shnum);
#endif

	      /* Mark section (and all dependencies) as used.  */
	      mark_section_used (&fileinfo->scninfo[shndx], shndx, statep,
				 &ignore);

	      /* Check whether the section is merge-able.  In this case we
		 have to record the symbol.  */
	      if (fileinfo->scninfo[shndx].shdr.sh_flags & SHF_MERGE)
		{
		  if (fileinfo->scninfo[shndx].symbols == NULL)
		    fileinfo->scninfo[shndx].symbols = newp->next_in_scn
		      = newp;
		  else
		    {
		      newp->next_in_scn
			= fileinfo->scninfo[shndx].symbols->next_in_scn;
		      fileinfo->scninfo[shndx].symbols
			= fileinfo->scninfo[shndx].symbols->next_in_scn = newp;
		    }
		}
	    }
	}

      /* This file is used.  */
      if (fileinfo->file_type == relocatable_file_type)
	{
	  if (unlikely (statep->relfiles == NULL))
	    statep->relfiles = fileinfo->next = fileinfo;
	  else
	    {
	      fileinfo->next = statep->relfiles->next;
	      statep->relfiles = statep->relfiles->next = fileinfo;
	    }

	  /* Update some summary information in the state structure.  */
	  statep->nsymtab += fileinfo->nsymtab;
	  statep->nlocalsymbols += fileinfo->nlocalsymbols;
	}
      else if (fileinfo->file_type == dso_file_type)
	{
	  if (unlikely (statep->dsofiles == NULL))
	    statep->dsofiles = fileinfo->next = fileinfo;
	  else
	    {
	      fileinfo->next = statep->dsofiles->next;
	      statep->dsofiles = statep->dsofiles->next = fileinfo;
	    }

	  ++statep->ndsofiles;

	  if (fileinfo->lazyload)
	    /* We have to create another dynamic section entry for the
	       DT_POSFLAG_1 entry.

	       XXX Once more functionality than the lazyloading flag
	       are suppported the test must be extended.  */
	    ++statep->ndsofiles;
	}
    }

  return 0;
}


int
ld_handle_filename_list (struct filename_list *fnames, struct ld_state *statep)
{
  struct filename_list *runp;
  int res = 0;

  for (runp = fnames; runp != NULL; runp = runp->next)
    {
      struct usedfiles *curp;

      /* Create a record for the new file.  */
      curp = runp->real = ld_new_inputfile (runp->name, relocatable_file_type,
					    statep);

      /* Set flags for group handling.  */
      runp->real->group_start = runp->group_start;
      runp->real->group_end = runp->group_end;

      /* Read the file and everything else which comes up, including
	 handling groups.  */
      do
	res |= FILE_PROCESS (-1, curp, statep, &curp);
      while (curp != NULL);
    }

  /* Free the list.  */
  while (fnames != NULL)
    {
      runp = fnames;
      fnames = fnames->next;
      free (runp);
    }

  return res;
}


/* Handle opening of the given file with ELF descriptor.  */
static int
open_elf (struct usedfiles *fileinfo, Elf *elf, struct ld_state *statep)
{
  int res = 0;

  if (elf == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get descriptor for ELF file (%s:%d): %s\n"),
	   __FILE__, __LINE__, elf_errmsg (-1));

  if (unlikely (elf_kind (elf) == ELF_K_NONE))
    {
      struct filename_list *fnames;

      /* We don't have to look at this file again.  */
      fileinfo->status = closed;

      /* Let's see whether this is a linker script.  */
      if (fileinfo->fd != -1)
	/* Create a stream from the file handle we know.  */
	ldin = fdopen (fileinfo->fd, "r");
      else
	{
	  /* Get the memory for the archive member.  */
	  char *content;
	  size_t contentsize;

	  /* Get the content of the file.  */
	  content = elf_rawfile (elf, &contentsize);
	  if (content == NULL)
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  /* The content of the file is available in memory.  Read the
	     memory region as a stream.  */
	  ldin = fmemopen (content, contentsize, "r");
	}

      if (ldin == NULL)
	error (EXIT_FAILURE, errno, gettext ("cannot open \"%s\""),
	       fileinfo->rfname);

      /* Parse the file.  If it is a linker script no problems will be
	 reported.  */
      statep->srcfiles = NULL;
      ldlineno = 1;
      ld_scan_version_script = 0;
      res = ldparse (statep);

      fclose (ldin);
      if (fileinfo->fd != -1 && !fileinfo->fd_passed)
	{
	  /* We won't need the file descriptor again.  */
	  close (fileinfo->fd);
	  fileinfo->fd = -1;
	}

      elf_end (elf);

      if (unlikely (res != 0))
	/* Something went wrong during parsing.  */
	return 1;

      /* This is no ELF file.  */
      fileinfo->elf = NULL;

      /* Now we have to handle eventual INPUT and GROUP statements in
	 the script.  Read the files mentioned.  */
      fnames = statep->srcfiles;
      if (fnames != NULL)
	{
	  struct filename_list *oldp;

	  /* Convert the list into a normal single-linked list.  */
	  oldp = fnames;
	  fnames = fnames->next;
	  oldp->next = NULL;

	  /* Remove the list from the state structure.  */
	  statep->srcfiles = NULL;

	  if (unlikely (ld_handle_filename_list (fnames, statep) != 0))
	    return 1;
	}

      return 0;
    }

  /* Store the file info.  */
  fileinfo->elf = elf;

  /* The file is ready for action.  */
  fileinfo->status = opened;

  return 0;
}


static int
add_whole_archive (struct usedfiles *fileinfo, struct ld_state *statep)
{
  Elf *arelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP_PRIVATE;
  int res = 0;

  while ((arelf = elf_begin (fileinfo->fd, cmd, fileinfo->elf)) != NULL)
    {
      Elf_Arhdr *arhdr = elf_getarhdr (arelf);
      struct usedfiles *newp;

      if (arhdr == NULL)
	abort ();

      /* Just to be sure; since these are no files in the archive
	 these names should never be returned.  */
      assert (strcmp (arhdr->ar_name, "/") != 0);
      assert (strcmp (arhdr->ar_name, "//") != 0);

      newp = ld_new_inputfile (arhdr->ar_name, relocatable_file_type, statep);
      newp->archive_file = fileinfo;

      if (unlikely (statep->trace_files))
	print_file_name (stdout, newp, 1, 1);

      /* This shows that this file is contained in an archive.  */
      newp->fd = -1;
      /* Store the ELF descriptor.  */
      newp->elf = arelf;
      /* Show that we are open for business.  */
      newp->status = opened;

      /* Proces the file, add all the symbols etc.  */
      res = file_process2 (newp, statep);
      if (unlikely (res != 0))
	    break;

      /* Advance to the next archive element.  */
      cmd = elf_next (arelf);
    }

  return res;
}


/* Define hash table for symbols in archive.  */
#include <archivehash.h>


static int
extract_from_archive (struct usedfiles *fileinfo, struct ld_state *statep)
{
  static int archive_seq;
  int any_symbol = 0;
  int res = 0;
  struct arsym_tab_fshash *arsym_tab = fileinfo->arsym_tab;
  struct symbol *unresolved;
  struct symbol *prevp;

  /* This is an archive we are not using completely.  Give it a
     unique number.  */
  fileinfo->archive_seq = ++archive_seq;

  /* If there are no unresolved symbols don't do anything.  */
  if ((likely (statep->extract_rule == defaultextract)
       && statep->nunresolved_nonweak == 0)
      || (unlikely (statep->extract_rule == weakextract)
	  && statep->nunresolved == 0))
    return 0;

  /* Get the archive symbol table.  */
  if (arsym_tab == NULL)
    {
      Elf_Arsym *syms;
      size_t nsyms;
      size_t cnt;

      /* Get all the symbols.  */
      syms = elf_getarsym (fileinfo->elf, &nsyms);
      if (syms == NULL)
	{
	cannot_read_archive:
	  error (0, 0, gettext ("cannot read archive `%s': %s"),
		 fileinfo->rfname, elf_errmsg (-1));

	  /* We cannot use this archive anymore.  */
	  fileinfo->status = closed;

	  return 1;
	}

      /* Now that we know how many symbols are in the archive we can
	 appropriately size the hashing table.  */
      fileinfo->arsym_tab = arsym_tab = arsym_tab_fshash_init (nsyms);
      if (arsym_tab == NULL)
	{
	  /* This means the number of symbols is too high (which really
	     never should happen.  */
	  error (0, 0, gettext ("archive `%s' is too large"),
		 fileinfo->rfname);

	  /* We cannot use this archive anymore.  */
	  fileinfo->status = closed;

	  return 1;
	}

      /* Now add all the symbols to the hash table.  Note that there
	 can potentially be duplicate definitions.  We'll always use
	 the first definition.

	 XXX Is this a compatible behavior?  */
      for (cnt = 0; cnt < nsyms; ++cnt)
	(void) arsym_tab_fshash_insert_hash (arsym_tab, syms[cnt].as_hash,
					     &syms[cnt]);

      /* Finally add the file to the list of archives.  */
      if (statep->archives == NULL)
	statep->archives = statep->tailarchives = fileinfo->next = fileinfo;
      else
	statep->tailarchives = statep->tailarchives->next = fileinfo;

      /* See whether the archive has to be remembered as the start of a
	 group.  */
      if (unlikely (statep->group_start_requested))
	{
	  statep->group_start_archive = fileinfo;
	  statep->group_start_requested = false;
	}
    }

  /* XXX At this point we have two choices:

     - go through the list of undefined symbols we have collected and
       look for the symbols in the archive

     or

     - iterate over the symbol table of the archive.

     Probably the decision about what is done should be made
     dynamically.  If the number of unresolved symbols is larger than
     the number of symbols in the archive the second alternative
     should be used.  Otherwise the first.

     For now we only go through the list of unresolved symbols.

     Note: statep->unresolved->next is the first element of the list.
     This is important to remember since while we process the
     unresolved symbols we might add new files which in turn can add
     new undefined symbols and we have to process them in this loop as
     well.  This can only work if we handle the end of the list
     last.  */
  unresolved = statep->unresolved->next;
  prevp = NULL;
  do
    {
      if (unlikely (unresolved->defined))
	{
	  /* The symbol is already defined, we can remove it from the
	     list.  But not the first element because of peculiarities
	     in the circular single-linked list handling.  */
	  if (prevp != NULL)
	    prevp->next = unresolved->next;
	  else
	    prevp = unresolved;
	}
      else if (fileinfo->extract_rule == weakextract || ! unresolved->weak)
	{
	  const Elf_Arsym *arsym;
	  Elf_Arsym search;

	  search.as_name = (char *) unresolved->name;
	  arsym = arsym_tab_fshash_find (arsym_tab, unresolved->name, 0,
					 &search);
	  if (arsym != NULL)
	    {
	      /* We found an object file in the archive which contains
		 a symbol which is so far undefined.  Create the data
		 structures for this file and add all the symbols.  */
	      Elf *arelf;
	      Elf_Arhdr *arhdr;
	      struct usedfiles *newp;

	      /* Find the archive member for this symbol.  */
	      if (elf_rand (fileinfo->elf, arsym->as_off) != arsym->as_off)
		goto cannot_read_archive;

	      /* Note: no test of a failing 'elf_begin' call.  That's fine
		 since 'elf'getarhdr' will report the problem.  */
	      arelf = elf_begin (fileinfo->fd, ELF_C_READ_MMAP_PRIVATE,
				 fileinfo->elf);
	      arhdr = elf_getarhdr (arelf);
	      if (arhdr == NULL)
		goto cannot_read_archive;

	      /* We have all the information and an ELF handle for the
		 archive member.  Create the normal data structure for
		 a file now.  */
	      newp = ld_new_inputfile (arhdr->ar_name, relocatable_file_type,
				       statep);
	      newp->archive_file = fileinfo;

	      if (unlikely (statep->trace_files))
		print_file_name (stdout, newp, 1, 1);

	      /* This shows that this file is contained in an archive.  */
	      newp->fd = -1;
	      /* Store the ELF descriptor.  */
	      newp->elf = arelf;
	      /* Show that we are open for business.  */
	      newp->status = in_archive;

	      /* Now read the file and add all the symbols.  */
	      res = file_process2 (newp, statep);
	      if (unlikely (res != 0))
		break;

	      /* We used this file.  */
	      any_symbol = 1;

	      /* Try to remove the element from the list.  We don't
		 remove the first element here because of some
		 peculiarities of the circular singled-linked list
		 handling.  */
	      if (prevp != NULL)
		prevp->next = unresolved->next;
	      else
		prevp = unresolved;
	    }
	  else
	    /* This entry remains on the list.  */
	    prevp = unresolved;
	}
      else
	/* This entry remains on the list.  */
	prevp = unresolved;

      unresolved = unresolved->next;
    }
  while (unresolved != statep->unresolved->next);

  if (prevp != NULL)
    statep->unresolved = prevp;

  if (likely (res == 0) && any_symbol != 0)
    {
      /* This is an archive therefore it must have a number.  */
      assert (fileinfo->archive_seq != 0);
      statep->last_archive_used = fileinfo->archive_seq;
    }

  return res;
}


static int
file_process2 (struct usedfiles *fileinfo, struct ld_state *statep)
{
  int res;

  if (likely (elf_kind (fileinfo->elf) == ELF_K_ELF))
    {
      /* The first time we get here we read the ELF header.  */
      if (likely (fileinfo->ehdr.e_type == ET_NONE))
	{
	  if (gelf_getehdr (fileinfo->elf, &fileinfo->ehdr) == NULL)
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      fileinfo->status = closed;
	      return 1;
	    }

	  if (fileinfo->ehdr.e_type != ET_REL
	      && unlikely (fileinfo->ehdr.e_type != ET_DYN))
	    /* XXX Add ebl* function to query types which are allowed
	       to link in.  */
	    {
	      char buf[64];

	      print_file_name (stderr, fileinfo, 1, 0);
	      fprintf (stderr,
		       gettext ("file of type %s cannot be linked in\n"),
		       ebl_object_type_name (statep->ebl,
					     fileinfo->ehdr.e_type,
					     buf, sizeof (buf)));
	      fileinfo->status = closed;
	      return 1;
	    }

	  /* Determine the section header string table section index.  */
	  if (elf_getshstrndx (fileinfo->elf, &fileinfo->shstrndx) < 0)
	    {
	      fprintf (stderr, gettext ("\
%s: cannot get section header string table index: %s\n"),
		       fileinfo->rfname, elf_errmsg (-1));
	      fileinfo->status = closed;
	      return 1;
	    }
	}

      /* Now handle the different types of files.  */
      if (fileinfo->ehdr.e_type == ET_REL)
	{
	  /* Add all the symbol.  Relocatable files have symbol
	     tables.  */
	  res = add_relocatable_file (fileinfo, statep, SHT_SYMTAB);
	}
      else
	{
	  GElf_Shdr dynshdr_mem;
	  GElf_Shdr *dynshdr;
	  Elf_Data *dyndata;
	  bool has_l_name = fileinfo->file_type == archive_file_type;

	  assert (fileinfo->ehdr.e_type == ET_DYN);

	  /* If the file is a DT_NEEDED dependency then the type is
	     already correctly specified.  */
	  if (fileinfo->file_type != dso_needed_file_type)
	    fileinfo->file_type = dso_file_type;

	  /* We cannot use DSOs when generating relocatable objects.  */
	  if (statep->file_type == relocatable_file_type)
	    {
	      error (0, 0, gettext ("\
cannot use DSO '%s' when generating relocatable object file"),
		     fileinfo->fname);
	      return 1;
	    }

	  /* Add all the symbols.  For DSOs we are looking at the
	     dynamic symbol table.  */
	  res = add_relocatable_file (fileinfo, statep, SHT_DYNSYM);

	  /* We always have to have a dynamic section.  */
	  assert (fileinfo->dynscn != NULL);

	  /* We have to remember the dependencies for this object.  It
	     is necessary to look them up.  */
	  dynshdr = gelf_getshdr (fileinfo->dynscn, &dynshdr_mem);
	  dyndata = elf_getdata (fileinfo->dynscn, NULL);
	  /* XXX Should we flag the failure to get the dynamic section?  */
	  if (dynshdr != NULL)
	    {
	      int cnt = dynshdr->sh_size / dynshdr->sh_entsize;
	      GElf_Dyn dynmem;
	      GElf_Dyn *dyn;

	      while (--cnt >= 0)
		if ((dyn = gelf_getdyn (dyndata, cnt, &dynmem)) != NULL)
		  {
		    if(dyn->d_tag == DT_NEEDED)
		      {
			struct usedfiles *newp;

			newp = ld_new_inputfile (elf_strptr (fileinfo->elf,
							     dynshdr->sh_link,
							     dyn->d_un.d_val),
						 dso_needed_file_type, statep);

			/* Enqueue the newly found dependencies.  */
			if (statep->needed == NULL)
			  statep->needed = newp->next = newp;
			else
			  {
			    newp->next = statep->needed->next;
			    statep->needed = statep->needed->next = newp;
			  }
		      }
		    else if (dyn->d_tag == DT_SONAME)
		      {
			/* We use the DT_SONAME (this is what's there
			   for).  */
			fileinfo->soname = elf_strptr (fileinfo->elf,
						       dynshdr->sh_link,
						       dyn->d_un.d_val);
			has_l_name = false;
		      }
		  }
	    }

	  /* Construct the file name if the DSO has no SONAME and the
	     file name comes from a -lXX parameter on the comment
	     line.  */
	  if (unlikely (has_l_name))
	    {
	      /* The FNAME is the parameter the user specified on the
		 command line.  We prepend "lib" and append ".so".  */
	      size_t len = strlen (fileinfo->fname) + 7;
	      char *newp;

	      newp = (char *) xmalloc (len);
	      strcpy (stpcpy (stpcpy (newp, "lib"), fileinfo->fname), ".so");

	      fileinfo->soname = newp;
	    }
	}
    }
  else if (likely (elf_kind (fileinfo->elf) == ELF_K_AR))
    {
      if (unlikely (statep->extract_rule == allextract))
	/* Which this option enabled we have to add all the object
	   files in the archive.  */
	res = add_whole_archive (fileinfo, statep);
      else if (statep->file_type == relocatable_file_type)
	{
	  /* When generating a relocatable object we don't find files
	     in archives.  */
	  if (verbose)
	    error (0, 0, gettext ("input file '%s' ignored"), fileinfo->fname);

	  res = 0;
	}
      else
	/* Extract only the members from the archive which are
	   currently referenced by unresolved symbols.  */
	res = extract_from_archive (fileinfo, statep);
    }
  else
    /* This should never happen, we know about no other types.  */
    abort ();

  return res;
}


/* Process a given file.  The first parameter is a file descriptor for
   the file which can be -1 to indicate the file has not yet been
   found.  The second parameter describes the file to be opened, the
   last one is the state of the linker which among other information
   contain the paths we look at.  */
static int
ld_generic_file_process (int fd, struct usedfiles *fileinfo,
			 struct ld_state *statep, struct usedfiles **nextp)
{
  int res = 0;

  /* By default we go to the next file in the list.  */
  *nextp = fileinfo->next;

  /* Set the flag to signal we are looking for a group start.  */
  if (unlikely (fileinfo->group_start))
    {
      statep->group_start_requested = true;
      fileinfo->group_start = false;
    }

  /* If the file isn't open yet, open it now.  */
  if (likely (fileinfo->status == not_opened))
    {
      bool fd_passed = true;

      if (likely (fd == -1))
	{
	  /* Find the file ourselves.  */
	  int err = open_along_path (fileinfo, statep);
	  if (unlikely (err != 0))
	    /* We allow libraries and DSOs to be named more than once.
	       Don't report an error to the caller.  */
	    return err == EAGAIN ? 0 : err;

	  fd_passed = false;
	}
      else
	fileinfo->fd = fd;

      /* Remember where we got the descriptor from.  */
      fileinfo->fd_passed = fd_passed;

      /* We found the file.  Now test whether it is a file type we can
	 handle.

	 XXX Do we have to have the ability to start from a given
	 position in the search path again to look for another file if
	 the one found has not the right type?  */
      res = open_elf (fileinfo, elf_begin (fileinfo->fd,
					   is_dso_p (fileinfo->fd)
					   ? ELF_C_READ_MMAP
					   : ELF_C_READ_MMAP_PRIVATE, NULL),
		      statep);
      if (unlikely (res != 0))
	return res;
    }

  /* Now that we have opened the file start processing it.  */
  if (likely (fileinfo->status != closed))
    res = file_process2 (fileinfo, statep);

  /* Determine which file to look at next.  */
  if (unlikely (fileinfo->group_backref != NULL))
    {
      /* We only go back if an archive other than the one we would go
	 back to has been used in the last round.  */
      if (statep->last_archive_used > fileinfo->group_backref->archive_seq)
	{
	  *nextp = fileinfo->group_backref;
	  statep->last_archive_used = 0;
	}
      else
	{
	  /* If we come here this means that the archives we read so
	     far are not needed anymore.  We can free some of the data
	     now.  */
	  struct usedfiles *runp = statep->archives;

	  do
	    {
	      /* Free the hashing table for the archive symbols.  */
	      arsym_tab_fshash_fini (runp->arsym_tab);
	      runp->arsym_tab = NULL;

	      /* We don't need the ELF descriptor anymore.  Unless there
		 are no files from the archive used this will not free
		 the whole file but only some data structures.  */
	      elf_end (runp->elf);
	      runp->elf = NULL;

	      runp = runp->next;
	    }
	  while (runp != fileinfo->next);
	}
    }
  else if (unlikely (fileinfo->group_end))
    {
      /* This is the end of a group.  We possibly of to go back.
	 Determine which file we would go back to and see whether it
	 makes sense.  If there has not been an archive we don't have
	 to do anything.  */
      if (!statep->group_start_requested)
	{
	  if (statep->group_start_archive != statep->tailarchives)
	    /* The loop would include more than one archive, add the
	       pointer.  */
	    {
	      *nextp = statep->tailarchives->group_backref =
		statep->group_start_archive;
	      statep->last_archive_used = 0;
	    }
	  else
	    /* We might still have to go back to the beginning of the
	       group if since the last archive other files have been
	       added.  But we go back exactly once.  */
	    if (statep->tailarchives != fileinfo)
	      {
		*nextp = statep->group_start_archive;
		statep->last_archive_used = 0;
	      }
	}

      /* Clear the flags.  */
      statep->group_start_requested = false;
      fileinfo->group_end = false;
    }

  return res;
}


/* Library names passed to the linker as -lXX represent files named
   libXX.YY.  The YY part can have different forms, depending on the
   platform.  The generic set is .so and .a (in this order).  */
static const char **
ld_generic_lib_extensions (struct ld_state *statep __attribute__ ((__unused__)))
{
  static const char *exts[] =
    {
      ".so", ".a", NULL
    };

  return exts;
}


/* Flag unresolved symbols.  */
static int
ld_generic_flag_unresolved (struct ld_state *statep)
{
  int retval = 0;

  if (statep->nunresolved_nonweak > 0)
    {
      /* Go through the list and determine the unresolved symbols.  */
      struct symbol *first;
      struct symbol *s;

      s = first = statep->unresolved->next;
      do
	{
	  if (! s->defined && ! s->weak)
	    {
	      /* XXX The error message should get better.  It should use
		 the debugging information if present to tell where in the
		 sources the undefined reference is.  */
	      error (0, 0, gettext ("undefined symbol `%s' in %s"), s->name,
		     s->file->fname);

	      retval = 1;
	    }

	  s = s->next;
	}
      while (s != first);
    }

  return retval;
}


/* The linker has to be able to identify sections containing debug
   information.  This cannot be done by section types etc, but only by
   names.  Except for DWARF sections the names used for the platform
   might differ.  This callback function tests whether a given name is
   that of a debug section.  */
static int
ld_generic_is_debugscn_p (const char *name, struct ld_state *statep)
{
  /* We know by default only about the DWARF debug sections which have
     fixed names.  */
  static const char *dwarf_scn_names[] =
    {
      /* DWARF 1 */
      ".debug",
      ".line",
      /* GNU DWARF 1 extensions */
      ".debug_srcinfo",
      ".debug_sfnames",
      /* DWARF 1.1 and DWARF 2 */
      ".debug_aranges",
      ".debug_pubnames",
      /* DWARF 2 */
      ".debug_info",
      ".debug_abbrev",
      ".debug_line",
      ".debug_frame",
      ".debug_str",
      ".debug_loc",
      ".debug_macinfo",
      /* SGI/MIPS DWARF 2 extensions */
      ".debug_weaknames",
      ".debug_funcnames",
      ".debug_typenames",
      ".debug_varnames"
    };
  size_t ndwarf_scn_names = (sizeof (dwarf_scn_names)
			     / sizeof (dwarf_scn_names[0]));
  size_t cnt;

  for (cnt = 0; cnt < ndwarf_scn_names; ++cnt)
    if (strcmp (name, dwarf_scn_names[cnt]) == 0)
      return 1;

  return 0;
}


/* Close the given file.  */
static int
ld_generic_file_close (struct usedfiles *fileinfo, struct ld_state *statep)
{
  /* Close the ELF descriptor.  */
  elf_end (fileinfo->elf);

  /* If we have opened the file descriptor close it.  But we might
     have done this already in which case FD is -1.  */
  if (!fileinfo->fd_passed && fileinfo->fd != -1)
    close (fileinfo->fd);

  /* We allocated the resolved file name.  */
  if (fileinfo->fname != fileinfo->rfname)
    free ((char *) fileinfo->rfname);

  return 0;
}


static void
new_generated_scn (enum scn_kind kind, const char *name, int type, int flags,
		   int entsize, int align, struct ld_state *statep)
{
  struct scnhead *newp;

  newp = (struct scnhead *) xcalloc (1, sizeof (struct scnhead));
  newp->kind = kind;
  newp->name = name;
  newp->nameent = ebl_strtabadd (statep->shstrtab, name, 0);
  newp->type = type;
  newp->flags = flags;
  newp->entsize = entsize;
  newp->align = align;
  newp->grp_signature = NULL;
  newp->used = true;

  /* All is well.  Create now the data for the section and insert it
     into the section table.  */
  ld_section_tab_insert (&statep->section_tab, elf_hash (name), newp);
}


/* Create the sections which are generated by the linker and are not
   present in the input file.  */
static void
ld_generic_generate_sections (struct ld_state *statep)
{
  /* The relocation section type.  */
  int rel_type = REL_TYPE (statep) == DT_REL ? SHT_REL : SHT_RELA;

  /* When building dynamically linked object we have to include a
     section containing a string describing the interpreter.  This
     should be at the very beginning of the file together with the
     other information the ELF loader (kernel or wherever) has to look
     at.  We put it as the first section in the file.

     We also have to create the dynamic segment which is a special
     section the dynamic linker locates through an entry in the
     program header.  */
  if (statep->file_type == dso_file_type || dynamically_linked_p (statep))
    {
      int ndt_needed;
      /* Use any versioning (defined or required)?  */
      bool use_versioning = false;
      /* Use version requirements?  */
      bool need_version = false;

      /* First the .interp section.  */
      new_generated_scn (scn_dot_interp, ".interp", SHT_PROGBITS, SHF_ALLOC,
			 0, 1, statep);

      /* Now the .dynamic section.  */
      new_generated_scn (scn_dot_dynamic, ".dynamic", SHT_DYNAMIC,
			 DYNAMIC_SECTION_FLAGS (statep),
			 gelf_fsize (statep->outelf, ELF_T_DYN, 1, EV_CURRENT),
			 gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
				     EV_CURRENT),
			 statep);

      /* We will need in any case the dynamic symbol table (even in
	 the unlikely case that no symbol is exported or referenced
	 from a DSO).  */
      statep->need_dynsym = true;
      new_generated_scn (scn_dot_dynsym, ".dynsym", SHT_DYNSYM, SHF_ALLOC,
			 gelf_fsize (statep->outelf, ELF_T_SYM, 1, EV_CURRENT),
			 gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
				     EV_CURRENT),
			 statep);
      /* It comes with a string table.  */
      new_generated_scn (scn_dot_dynstr, ".dynstr", SHT_STRTAB, SHF_ALLOC,
			 0, 1, statep);
      /* And a hashing table.  */
      new_generated_scn (scn_dot_hash, ".hash", SHT_HASH, SHF_ALLOC,
			 sizeof (Elf32_Word), sizeof (Elf32_Word), statep);

      /* By default we add all DSOs provided on the command line.  If
	 the user added '-z ignore' to the command line we only add
	 those which are actually used.  */
      ndt_needed = statep->ignore_unused_dsos ? 0 : statep->ndsofiles;

      /* Create the section associated with the PLT if necessary.  */
      if (statep->nplt > 0)
	{
	  /* Create the .plt section.  */
	  /* XXX We might need a function which returns the section flags.  */
	  new_generated_scn (scn_dot_plt, ".plt", SHT_PROGBITS,
			     SHF_ALLOC | SHF_EXECINSTR,
			     /* XXX Is the size correct?  */
			     gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					 EV_CURRENT),
			     gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					 EV_CURRENT),
			     statep);

	  /* Create the relocation section for the .plt.  This is always
	     separate even if the other relocation sections are combined.  */
	  new_generated_scn (scn_dot_pltrel, ".rel.plt", rel_type, SHF_ALLOC,
			     gelf_fsize (statep->outelf,
					 rel_type == SHT_REL
					 ? ELF_T_REL : ELF_T_RELA, 1,
					 EV_CURRENT),
			     gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					 EV_CURRENT),
			     statep);

	  /* This means we will also need the .got section.  */
	  statep->need_got = true;

	  /* Mark all used DSOs as used.  Determine whether any referenced
	     object uses symbol versioning.  */
	  if (statep->from_dso > 0)
	    {
	      struct symbol *srunp = statep->from_dso;

	      do
		{
		  srunp->file->used = true;

		  if (srunp->file->verdefdata != NULL)
		    {
		      GElf_Versym versym;

		      /* The input DSO uses versioning.  */
		      use_versioning = true;
		      /* We reference versions.  */
		      need_version = true;

		      if (gelf_getversym (srunp->file->versymdata,
					  srunp->symidx, &versym) == NULL)
			assert (! "gelf_getversym failed");

		      /* We cannot link explicitly with an older
			 version of a symbol.  */
		      assert ((versym & 0x8000) == 0);
		      /* We cannot reference local (index 0) or plain
			 global (index 1) versions.  */
		      assert (versym > 1);

		      /* Check whether we have already seen the
			 version and if not add it to the referenced
			 versions in the output file.  */
		      if (! srunp->file->verdefused[versym])
			{
			  srunp->file->verdefused[versym] = 1;

			  ++statep->nverdefused;
			}
		    }
		}
	      while ((srunp = srunp->next) != statep->from_dso);
	    }

	  /* Create the sections used to record version dependencies.  */
	  if (need_version)
	    new_generated_scn (scn_dot_version_r, ".gnu.version_r",
			       SHT_GNU_verneed, SHF_ALLOC, 0,
			       sizeof (GElf_Word), statep);

	  /* Now count the used DSOs since this is what the user
	     wants.  */
	  ndt_needed = 0;
	  if (statep->ndsofiles > 0)
	    {
	      struct usedfiles *frunp = statep->dsofiles;

	      do
		{
		  if (! statep->ignore_unused_dsos || frunp->used)
		    {
		      ++ndt_needed;
		      if (frunp->lazyload)
			/* We have to create another dynamic section
			   entry for the DT_POSFLAG_1 entry.

			   XXX Once more functionality than the
			   lazyloading flag are suppported the test
			   must be extended.  */
			++ndt_needed;
		    }

		  /* Count the file if it is using versioning.  */
		  if (frunp->verdefdata != NULL)
		    ++statep->nverdeffile;
		}
	      while ((frunp = frunp->next) != statep->dsofiles);
	    }
	}

      if (use_versioning)
	new_generated_scn (scn_dot_version, ".gnu.version", SHT_GNU_versym,
			   SHF_ALLOC, sizeof (GElf_Half), sizeof (GElf_Half),
			   statep);

      /* We need some entries all the time.  */
      statep->ndynamic = (7 + (statep->runpath != NULL
			       || statep->rpath != NULL)
			  + ndt_needed
			  + (statep->init_symbol != NULL ? 1 : 0)
			  + (statep->fini_symbol != NULL ? 1 : 0)
			  + (use_versioning ? 1 : 0)
			  + (need_version ? 2 : 0)
			  + (statep->nplt > 0 ? 4 : 0));
    }

  /* When creating a relocatable file or when we are not stripping the
     output file we create a symbol table.  */
  statep->need_symtab = (statep->file_type == relocatable_file_type
			 || statep->strip == strip_none);

  /* Add the .got section if needed.  */
  if (statep->need_got)
    {
    /* XXX We might need a function which returns the section flags.  */
    new_generated_scn (scn_dot_got, ".got", SHT_PROGBITS,
		       SHF_ALLOC | SHF_WRITE,
		       gelf_fsize (statep->outelf, ELF_T_ADDR, 1, EV_CURRENT),
		       gelf_fsize (statep->outelf, ELF_T_ADDR, 1, EV_CURRENT),
		       statep);


      if (statep->nrel_got > 0 && !statep->combreloc)
	new_generated_scn (scn_dot_gotrel, ".rel.got", rel_type,
			   SHF_ALLOC,
			   gelf_fsize (statep->outelf,
				       rel_type == SHT_REL
				       ? ELF_T_REL : ELF_T_RELA, 1,
				       EV_CURRENT),
			   gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
				       EV_CURRENT),
			   statep);
    }

  /* Add the .rel.dyn section.  */
  if (statep->combreloc && statep->relsize_total > 0)
    new_generated_scn (scn_dot_dynrel, ".rel.dyn", rel_type, SHF_ALLOC,
		       gelf_fsize (statep->outelf,
				   rel_type == SHT_REL
				   ? ELF_T_REL : ELF_T_RELA, 1,
				   EV_CURRENT),
		       gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
				   EV_CURRENT),
		       statep);
}


/* Callback function registered with on_exit to make sure the temporary
   files gets removed if something goes wrong.  */
static void
remove_tempfile (int status, void *arg)
{
  if (status != 0)
    {
      struct ld_state *statep = arg;

      if (statep->tempfname != NULL)
	unlink (statep->tempfname);
    }
}


/* Create the output file.  The file name is given or "a.out".  We
   create as much of the ELF structure as possible.  */
static int
ld_generic_open_outfile (struct ld_state *statep, int klass, int data)
{
  char *tempfname;
  int fd;
  Elf *elf;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  size_t outfname_len;

  /* We do not create the new file right away with the final name.
     This would destroy an existing file with this name before a
     replacement is finalized.  We create instead a temporary file in
     the same directory.  */
  if (statep->outfname == NULL)
    statep->outfname = "a.out";
  outfname_len = strlen (statep->outfname);
  statep->tempfname = tempfname = (char *) xmalloc (outfname_len
						    + sizeof (".XXXXXX"));
  strcpy (mempcpy (tempfname, statep->outfname, outfname_len), ".XXXXXX");

  /* Create the output file with a unique name.  Note that the mode is
     0600.  We'll change this later.  */
  statep->outfd = fd = mkstemp (tempfname);
  if (fd == -1)
    error (EXIT_FAILURE, errno, gettext ("cannot create output file"));

  /* Make sure we remove the temporary file in case something goes
     wrong.  */
  on_exit (remove_tempfile, statep);

  /* Create the ELF file data for the output file.  */
  statep->outelf = elf = elf_begin (fd, ELF_C_WRITE_MMAP, NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create ELF descriptor for output file: %s"),
	   elf_errmsg (-1));

  /* Create the basic data structures.  */
  if (gelf_newehdr (elf, klass) == 0)
    /* Couldn't create the ELF header.  Very bad.  */
    error (EXIT_FAILURE, 0,
	   gettext ("could not create ELF header for output file: %s"),
	   elf_errmsg (-1));

  /* And get the current header so that we can modify it.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  assert (ehdr != NULL);

  /* Modify it according to the info we have here and now.  */
  if (statep->file_type == executable_file_type)
    ehdr->e_type = ET_EXEC;
  else if (statep->file_type == dso_file_type)
    ehdr->e_type = ET_DYN;
  else
    {
      assert (statep->file_type == relocatable_file_type);
      ehdr->e_type = ET_REL;
    }

  /* Set the ELF version.  */
  ehdr->e_version = EV_CURRENT;

  /* Set the endianness.  */
  ehdr->e_ident[EI_DATA] = data;

  /* Write the ELF header information back.  */
  (void) gelf_update_ehdr (elf, ehdr);

  return 0;
}


static void
sort_sections_generic (struct ld_state *statep)
{
  /* XXX TBI */
  abort ();
}


static int
match_section (const char *osectname, struct filemask_section_name *sectmask,
	       struct scnhead **scnhead, bool new_section, size_t segment_nr,
	       struct ld_state *statep)
{
  struct scninfo *prevp;
  struct scninfo *runp;
  struct scninfo *notused;

  if (fnmatch (sectmask->section_name->name, (*scnhead)->name, 0) != 0)
    /* The section name does not match.  */
    return new_section;

  /* If this is a section generated by the linker it doesn't contain
     the regular information (i.e., input section data etc) and must
     be handle special.  */
  if ((*scnhead)->kind != scn_normal)
    {
      (*scnhead)->name = osectname;
      (*scnhead)->segment_nr = segment_nr;

      /* We have to count note section since they get their own
	 program header entry.  */
      if ((*scnhead)->type == SHT_NOTE)
	++statep->nnotesections;

      statep->allsections[statep->nallsections++] = (*scnhead);
      return true;
    }

  /* Now we have to match the file names of the input files.  Some of
     the sections here might not match.    */
  runp = (*scnhead)->last->next;
  prevp = (*scnhead)->last;
  notused = NULL;

  do
    {
      /* Base of the file name the section comes from.  */
      const char *brfname = basename (runp->fileinfo->rfname);

      /* If the section isn't used, the name doesn't match the positive
	 inclusion list or the name does match the negative inclusion
	 list, ignore the section.  */
      if (!runp->used
	  || (sectmask->filemask != NULL
	      && fnmatch (sectmask->filemask, brfname, 0) != 0)
	  || (sectmask->excludemask != NULL
	      && fnmatch (sectmask->excludemask, brfname, 0) == 0))
	{
	  /* This file does not match the file name masks.  */
	  if (notused == NULL)
	    notused = runp;

	  prevp = runp;
	  runp = runp->next;
	  if (runp == notused)
	    runp = NULL;
	}
      /* The section fulfills all requirements, add it to the output
	 file with the correct section name etc.  */
      else
	{
	  struct scninfo *found = runp;

	  /* Remove this input section data buffer from the list.  */
	  if (prevp != runp)
	    runp = prevp->next = runp->next;
	  else
	    {
	      free (*scnhead);
	      *scnhead = NULL;
	      runp = NULL;
	    }

	  /* Create a new esection for the output file is the 'new_section'
	     flag says so.  Otherwise append the buffer to the last
	     section which we created in one of the last calls.  */
	  if (new_section)
	    {
	      struct scnhead *newp;

	      newp = (struct scnhead *) xcalloc (1, sizeof (*newp));
	      newp->kind = scn_normal;
	      newp->name = osectname;
	      newp->type = found->shdr.sh_type;
	      newp->flags = found->shdr.sh_flags;
	      newp->segment_nr = segment_nr;
	      newp->last = found->next = found;
	      newp->used = true;
	      newp->relsize = found->relsize;

	      /* We have to count note section since they get their own
		 program header entry.  */
	      if (newp->type == SHT_NOTE)
		++statep->nnotesections;

	      statep->allsections[statep->nallsections++] = newp;
	      new_section = false;
	    }
	  else
	    {
	      struct scnhead *queued;

	      queued = statep->allsections[statep->nallsections - 1];

	      found->next = queued->last->next;
	      queued->last = queued->last->next = found;

	      /* If the linker script forces us to add incompatible
		 sections together do so.  But reflect this in the
		 type and flags of the resulting file.  */
	      if (queued->type != found->shdr.sh_type)
		/* XXX Any better choice?  */
		queued->type = SHT_PROGBITS;
	      if (queued->flags != found->shdr.sh_flags)
		queued->flags = ebl_sh_flags_combine (statep->ebl,
						      queued->flags,
						      found->shdr.sh_flags);

	      /* Accumulate the relocation section size.  */
	      queued->relsize += found->relsize;
	    }
	}
    }
  while (runp != NULL);

  return new_section;
}


static void
sort_sections_lscript (struct ld_state *statep)
{
  struct scnhead *temp[statep->nallsections];
  size_t nallsections;
  size_t cnt;
  struct output_segment *segment;
  size_t segment_nr;

  /* Make a copy of the section head pointer array.  But copy only
     those sections which will appear in the output file.  */
  for (nallsections = cnt = 0; cnt < statep->nallsections; ++cnt)
    {
      /* Relocation sections which would be empty in the output file are
	 not used.  */
      if (statep->allsections[cnt]->kind == scn_normal
	  && (statep->allsections[cnt]->type == SHT_REL
	      || statep->allsections[cnt]->type == SHT_RELA)
	  && statep->allsections[cnt]->relsize == 0)
	continue;

      temp[nallsections++] = statep->allsections[cnt];
    }

  /* Convert the output segment list in a single-linked list.  */
  segment = statep->output_segments->next;
  statep->output_segments->next = NULL;
  statep->output_segments = segment;

  /* Put the sections in the correct order in the array in the state
     structure.  This might involve merging of sections and also
     renaming the containing section in the output file.  */
  statep->nallsections = 0;
  for (segment_nr = 0; segment != NULL; segment = segment->next, ++segment_nr)
    {
      struct output_rule *orule;

      for (orule = segment->output_rules; orule != NULL; orule = orule->next)
	if (orule->tag == output_section)
	  {
	    struct input_rule *irule;
	    bool new_section = true;

	    for (irule = orule->val.section.input; irule != NULL;
		 irule = irule->next)
	      if (irule->tag == input_section)
		for (cnt = 0; cnt < nallsections; ++cnt)
		  if (temp[cnt] != NULL)
		    new_section =
		      match_section (orule->val.section.name,
				     irule->val.section, &temp[cnt],
				     new_section, segment_nr, statep);
	  }
    }
}


/* Create the output sections now.  This requires knowledge about all
   the sections we will need.  It may be necessary to sort sections in
   the order they are supposed to appear in the executable.  The
   sorting use many different kinds of information to optimize the
   resulting binary.  Important is to respect segment boundaries and
   the needed alignment.  The mode of the segments will be determined
   afterwards automatically by the output routines.

   The generic sorting routines work in one of two possible ways:

   - if a linker script specifies the sections to be used in the
     output and assigns them to a segment this information is used;

   - otherwise the linker will order the sections based on permissions
     and some special knowledge about section names.*/
static void
ld_generic_create_sections (struct ld_state *statep)
{
  struct scngroup *groups;
  size_t cnt;

  /* For relocatable object we don't have to bother sorting the
     sections.  */
  if (statep->file_type != relocatable_file_type)
    {
      if (statep->output_segments == NULL)
	/* Sort using builtin rules.  */
	sort_sections_generic (statep);
      else
	sort_sections_lscript (statep);
    }

  /* Now iterate over the input sections and create the sections in the
     order they are required in the output file.  */
  for (cnt = 0; cnt < statep->nallsections; ++cnt)
    {
      struct scnhead *head = statep->allsections[cnt];
      Elf_Scn *scn;
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;

      /* Don't handle unused sections.  */
      if (!head->used)
	continue;

      /* We first have to create the section group if necessary.
	 Section group sections must come (in section index order)
	 before any of the section contained.  This all is necessary
	 only for relocatable object as other object types are not
	 allowed to contain section groups.  */
      if (statep->file_type == relocatable_file_type
	  && unlikely (head->flags & SHF_GROUP))
	{
	  /* There is at least one section which is contained in a
	     section group in the input file.  This means we must
	     create a section group here as well.  The only problem is
	     that not all input files have to have to same kind of
	     partitioning of the sections.  I.e., sections A and B in
	     one input file and sections B and C in another input file
	     can be in one group.  That will result in a group
	     containing the sections A, B, and C in the output
	     file.  */
	  struct scninfo *runp;
	  int here_groupidx = 0;
	  struct scngroup *here_group;
	  struct member *newp;

	  /* First check whether any section is already in a group.
	     In this case we have to add this output section, too.  */
	  runp = head->last;
	  do
	    {
	      assert (runp->grpid != 0);

	      here_groupidx = runp->fileinfo->scninfo[runp->grpid].outscnndx;
	      if (here_groupidx != 0)
		break;
	    }
	  while ((runp = runp->next) != head->last);

	  if (here_groupidx == 0)
	    {
	      /* We need a new section group section.  */
	      scn = elf_newscn (statep->outelf);
	      shdr = gelf_getshdr (scn, &shdr_mem);
	      if (shdr == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      here_group = (struct scngroup *) xmalloc (sizeof (*here_group));
	      here_group->outscnidx = here_groupidx = elf_ndxscn (scn);
	      here_group->nscns = 0;
	      here_group->member = NULL;
	      here_group->next = statep->groups;
	      /* Pick a name for the section.  To keep it meaningful
		 we use a name used in the input files.  If the
		 section group in the output file should contain
		 section which were in section groups of different
		 names in the input files this is the users
		 problem.  */
	      here_group->nameent
		= ebl_strtabadd (statep->shstrtab,
				 elf_strptr (runp->fileinfo->elf,
					     runp->fileinfo->shstrndx,
					     runp->shdr.sh_name),
				 0);
	      /* Signature symbol.  */
	      here_group->symbol
		= runp->fileinfo->scninfo[runp->grpid].symbols;

	      statep->groups = here_group;
	    }
	  else
	    {
	      /* Search for the group with this index.  */
	      here_group = statep->groups;
	      while (here_group->outscnidx != here_groupidx)
		here_group = here_group->next;
	    }

	  /* Add the new output section.  */
	  newp = (struct member *) alloca (sizeof (*newp));
	  newp->scn = head;
	  if (here_group->member == NULL)
	    here_group->member = newp->next = newp;
	  else
	    {
	      newp->next = here_group->member->next;
	      here_group->member = here_group->member->next = newp;
	    }
	  ++here_group->nscns;

	  /* Store the section group index in all input files.  */
	  runp = head->last;
	  do
	    {
	      assert (runp->grpid != 0);

	      if (runp->fileinfo->scninfo[runp->grpid].outscnndx == 0)
		runp->fileinfo->scninfo[runp->grpid].outscnndx = here_groupidx;
	      else
		assert (runp->fileinfo->scninfo[runp->grpid].outscnndx
			== here_groupidx);
	    }
	  while ((runp = runp->next) != head->last);
	}

      /* We'll use this section so get it's name in the section header
	 string table.  */
      if (head->kind == scn_normal)
	head->nameent = ebl_strtabadd (statep->shstrtab, head->name, 0);

      /* Create a new section in the output file and add all data
	 from all the sections we read.  */
      scn = elf_newscn (statep->outelf);
      head->scnidx = elf_ndxscn (scn);
      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create section for output file: %s"),
	       elf_errmsg (-1));

      assert (head->type != SHT_NULL);
      assert (head->type != SHT_SYMTAB);
      assert (head->type != SHT_DYNSYM || head->kind != scn_normal);
      assert (head->type != SHT_STRTAB || head->kind != scn_normal);
      assert (head->type != SHT_GROUP);
      shdr->sh_type = head->type;
      shdr->sh_flags = head->flags;
      shdr->sh_addralign = head->align;
      shdr->sh_entsize = head->entsize;
      (void) gelf_update_shdr (scn, shdr);

      /* We have to know the sectio index of the dynamic symbol table
	 right away.  */
      if (head->kind == scn_dot_dynsym)
	statep->dynsymscnidx = elf_ndxscn (scn);
    }

  /* Actually create the section group sections.  */
  groups = statep->groups;
  while (groups != NULL)
    {
      Elf_Scn *scn;
      Elf_Data *data;
      Elf32_Word *grpdata;
      struct member *runp;

      scn = elf_getscn (statep->outelf, groups->outscnidx);
      assert (scn != NULL);

      data = elf_newdata (scn);
      if (data == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create section for output file: %s"),
	       elf_errmsg (-1));

      data->d_size = (groups->nscns + 1) * sizeof (Elf32_Word);
      data->d_buf = grpdata = (Elf32_Word *) xmalloc (data->d_size);
      data->d_type = ELF_T_WORD;
      data->d_version = EV_CURRENT;
      data->d_off = 0;
      /* XXX What better to use?  */
      data->d_align = sizeof (Elf32_Word);

      /* The first word in the section is the flag word.  */
      /* XXX Set COMDATA flag is necessary.  */
      grpdata[0] = 0;

      runp = groups->member->next;
      cnt = 1;
      do
	/* Fill in the index of the section.  */
	grpdata[cnt++] = runp->scn->scnidx;
      while ((runp = runp->next) != groups->member->next);

      groups = groups->next;
    }
}


static bool
reduce_symbol_p (GElf_Sym *sym, struct Ebl_Strent *strent,
		 struct ld_state *statep)
{
  const char *str;
  const char *version;
  struct id_list search;
  struct id_list *verp;
  bool result = statep->default_bind_local;

  if (GELF_ST_BIND (sym->st_info) == STB_LOCAL || sym->st_shndx == SHN_UNDEF)
    /* We don't have to do anything to local symbols here.  */
    /* XXX Any section value in [SHN_LORESERVER,SHN_XINDEX) need
       special treatment?  */
    return false;

  /* XXX Handle other symbol bindings.  */
  assert (GELF_ST_BIND (sym->st_info) == STB_GLOBAL
	  || GELF_ST_BIND (sym->st_info) == STB_WEAK);

  str = ebl_string (strent);
  version = strchr (str, VER_CHR);
  if (version != NULL)
    {
      search.id = strndupa (str, version - str);
      if (*++version == VER_CHR)
	/* Skip the second '@' signalling a default definition.  */
	++version;
    }
  else
    {
      search.id = str;
      version = "";
    }

  verp = ld_version_str_tab_find (&statep->version_str_tab,
				  elf_hash (search.id), &search);
  while (verp != NULL)
    {
      /* We have this symbol in the version hash table.  Now match the
	 version name.  */
      if (strcmp (verp->u.s.versionname, version) == 0)
	/* Match!  */
	return verp->u.s.local;

      verp = verp->next;
    }

  /* XXX Add test for wildcard version symbols.  */

  return result;
}


static GElf_Addr
eval_expression (struct expression *exp, GElf_Addr addr,
		 struct ld_state *statep)
{
  GElf_Addr val = ~((GElf_Addr) 0);

  switch (exp->tag)
    {
    case exp_num:
      val = exp->val.num;
      break;

    case exp_sizeof_headers:
      {
	/* The 'elf_update' call determine the offset of the first
	   section.  The the size of the header.  */
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr;

	shdr = gelf_getshdr (elf_getscn (statep->outelf, 1), &shdr_mem);
	assert (shdr != NULL);

	val = shdr->sh_offset;
      }
      break;

    case exp_pagesize:
      val = statep->pagesize;
      break;

    case exp_id:
      /* We are here computing only address expressions.  It seems not
	 to be necessary to handle any variable but ".".  Let's avoid
	 the complication.  If it turns up to be needed we can add
	 it.  */
      if (strcmp (exp->val.str, ".") != 0)
	error (EXIT_FAILURE, 0, gettext ("\
address computation expression contains variable '%s'"),
	       exp->val.str);

      val = addr;
      break;

    case exp_mult:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     * eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_div:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     / eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_mod:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     % eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_plus:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     + eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_minus:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     - eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_and:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     & eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_or:
      val = (eval_expression (exp->val.binary.left, addr, statep)
	     | eval_expression (exp->val.binary.right, addr, statep));
      break;

    case exp_align:
      val = eval_expression (exp->val.child, addr, statep);
      if ((val & (val - 1)) != 0)
	error (EXIT_FAILURE, 0, gettext ("argument '%" PRIuMAX "' of ALIGN in address computation expression is no power of two"),
	       (uintmax_t) val);
      val = (addr + val - 1) & ~(val - 1);
      break;
    }

  return val;
}


/* Find a good as possible size for the hash table so that all the
   non-zero entries in HASHCODES don't collide too much and the table
   isn't too large.  There is no exact formular for this so we use a
   heuristic.  Depending on the optimization level the search is
   longer or shorter.  */
static size_t
optimal_bucket_size (Elf32_Word *hashcodes, size_t maxcnt, int optlevel,
		     struct ld_state *statep)
{
  size_t minsize;
  size_t maxsize;
  size_t bestsize;
  uint64_t bestcost;
  size_t size;
  uint32_t *counts;
  uint32_t *lengths;

  if (maxcnt == 0)
    return 0;

  /* When we are not optimizing we run only very few tests.  */
  if (optlevel <= 0)
    {
      minsize = maxcnt;
      maxsize = maxcnt + 10000 / maxcnt;
    }
  else
    {
      /* Does not make much sense to start with a smaller table than
	 one which has at least four collisions.  */
      minsize = MAX (1, maxcnt / 4);
      /* We look for a best fit in the range of up to eigth times the
	 number of elements.  */
      maxsize = 2 * maxcnt + (6 * MIN (optlevel, 100) * maxcnt) / 100;
    }
  bestsize = maxcnt;
  bestcost = UINT_MAX;

  /* Array for counting the collisions and chain lengths.  */
  counts = (uint32_t *) xmalloc ((maxcnt + 1 + maxsize) * sizeof (uint32_t));
  lengths = &counts[maxcnt + 1];

  for (size = minsize; size <= maxsize; ++size)
    {
      size_t inner;
      uint64_t cost;
      uint32_t maxlength;
      uint64_t success;
      uint32_t acc;
      double factor;

      memset (lengths, '\0', size * sizeof (uint32_t));
      memset (counts, '\0', (maxcnt + 1) * sizeof (uint32_t));

      /* Determine how often each hash bucket is used.  */
      for (inner = 0; inner < maxcnt; ++inner)
	++lengths[hashcodes[inner] % size];

      /* Determine the lengths.  */
      maxlength = 0;
      for (inner = 0; inner < size; ++inner)
	{
	  ++counts[lengths[inner]];

	  if (lengths[inner] > maxlength)
	    maxlength = lengths[inner];
	}

      /* Determine successful lookup length.  */
      acc = 0;
      success = 0;
      for (inner = 0; inner <= maxlength; ++inner)
	{
	  acc += inner;
	  success += counts[inner] * acc;
	}

      /* We can compute two factors now: the average length of a
	 positive search and the average length of a negative search.
	 We count the number of comparisons which have to look at the
	 names themselves.  Recognizing that the chain ended is not
	 accounted for since it's almost for free.

	 Which lookup is more important depends on the kind of DSO.
	 If it is a system DSO like libc it is expected that most
	 lookups succeed.  Otherwise most lookups fail.  */
      if (statep->is_system_library)
	factor = (1.0 * (double) success / (double) maxcnt
		  + 0.3 * (double) maxcnt / (double) size);
      else
	factor = (0.3 * (double) success / (double) maxcnt
		  + 1.0 * (double) maxcnt / (double) size);

      /* Combine the lookup cost factor.  The 1/16th addend adds
	 penalties for too large table sizes.  */
      cost = (2 + maxcnt + size) * (factor + 1.0 / 16.0);

#if 0
      printf ("maxcnt = %d, size = %d, cost = %Ld, success = %g, fail = %g, factor = %g\n",
	      maxcnt, size, cost, (double) success / (double) maxcnt, (double) maxcnt / (double) size, factor);
#endif

      /* Compare with current best results.  */
      if (cost < bestcost)
	{
	  bestcost = cost;
	  bestsize = size;
	}
    }

  free (counts);

  return bestsize;
}


static GElf_Addr
find_entry_point (struct ld_state *statep)
{
  GElf_Addr result;

  if (statep->entry != NULL)
    {
      struct symbol search = { .name = statep->entry };
      struct symbol *syment;

      syment = ld_symbol_tab_find (&statep->symbol_tab,
				   elf_hash (statep->entry), &search);
      if (syment != NULL && syment->defined)
	{
	  /* We found the symbol.  */
	  GElf_Sym symmem;
	  const GElf_Sym *sym;

	  sym =  gelf_getsym (elf_getdata (elf_getscn (statep->outelf,
						       statep->symscnidx),
					   NULL),
			      statep->dblindirect[syment->outsymidx],
			      &symmem);

	  if (sym == NULL && statep->need_dynsym && syment->outdynsymidx != 0)
	    /* Use the dynamic symbol table if available.  */
	    sym =  gelf_getsym (elf_getdata (elf_getscn (statep->outelf,
							 statep->dynsymscnidx),
					     NULL),
				syment->outdynsymidx, &symmem);

	  if (sym != NULL)
	    return sym->st_value;

	  /* XXX What to do if the output has no non-dynamic symbol
	     table and the dynamic symbol table does not contain the
	     symbol?  */
	  assert (statep->need_symtab);
	  assert (statep->symscnidx != 0);
	}
    }

  /* We couldn't find the symbol or none was given.  Use the first
     address of the ".text" section then.  */


  result = 0;

  if (statep->entry != NULL)
    error (0, 0, gettext ("\
cannot find entry symbol \"%s\": defaulting to %#0*" PRIx64),
	   statep->entry,
	   gelf_getclass (statep->outelf) == ELFCLASS32 ? 10 : 18,
	   (uint64_t) result);
  else
    error (0, 0, gettext ("\
no entry symbol specified: defaulting to %#0*" PRIx64),
	   gelf_getclass (statep->outelf) == ELFCLASS32 ? 10 : 18,
	   (uint64_t) result);

  return result;
}


/* Create the output file.

   For relocatable files what basically has to happen is that all
   sections from all input files are written into the output file.
   Sections with the same name are combined (offsets adjusted
   accordingly).  The symbol tables are combined in one single table.
   When stripping certain symbol table entries are omitted.

   For executables (shared or not) we have to create the program header,
   additional sections like the .interp, eventually (in addition) create
   a dynamic symbol table and a dynamic section.  Also the relocations
have to be processed differently.  */
static int
ld_generic_create_outfile (struct ld_state *statep)
{
  struct scnlist
  {
    size_t scnidx;
    struct scninfo *scninfo;
    struct scnlist *next;
  };
  struct scnlist *rellist = NULL;
  size_t cnt;
  Elf_Scn *symscn = NULL;
  Elf_Scn *xndxscn = NULL;
  Elf_Scn *strscn = NULL;
  struct Ebl_Strtab *strtab = NULL;
  struct Ebl_Strtab *dynstrtab = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  Elf_Data *symdata = NULL;
  Elf_Data *xndxdata = NULL;
  struct Ebl_Strent **symstrent = NULL;
  struct usedfiles *file;
  size_t nsym;
  size_t nsym_local;
  size_t nsym_allocated;
  size_t nsym_dyn = 0;
  Elf32_Word *dblindirect = NULL;
  struct symbol **ndxtosym = NULL;
#ifndef NDEBUG
  bool need_xndx;
#endif
  Elf_Scn *shstrtab_scn;
  size_t shstrtab_ndx;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  struct Ebl_Strent *symtab_ent = NULL;
  struct Ebl_Strent *xndx_ent = NULL;
  struct Ebl_Strent *strtab_ent = NULL;
  struct Ebl_Strent *shstrtab_ent;
  struct scngroup *groups;
  Elf_Scn *dynsymscn = NULL;
  Elf_Data *dynsymdata = NULL;
  Elf_Data *dynstrdata = NULL;
  Elf32_Word *hashcodes = NULL;
  size_t nsym_dyn_allocated = 0;
  Elf_Scn *versymscn = NULL;
  Elf_Data *versymdata = NULL;

  if (statep->need_symtab)
    {
      /* First create the symbol table.  We need the symbol section itself
	 and the string table for it.  */
      symscn = elf_newscn (statep->outelf);
      statep->symscnidx = elf_ndxscn (symscn);
      symdata = elf_newdata (symscn);
      if (symdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create symbol table for output file: %s"),
	       elf_errmsg (-1));

      symdata->d_type = ELF_T_SYM;
      /* This is an estimated size, but it will definitely cap the real value.
	 We might have to adjust the number later.  */
      nsym_allocated = (1 + statep->nsymtab + statep->nplt + statep->ngot
			+ statep->nusedsections);
      symdata->d_size = gelf_fsize (statep->outelf, ELF_T_SYM, nsym_allocated,
				    EV_CURRENT);

      /* Optionally the extended section table.  */
      /* XXX Is SHN_LORESERVE correct?  Do we need some other sections?  */
      if (unlikely (statep->nusedsections >= SHN_LORESERVE))
	{
	  xndxscn = elf_newscn (statep->outelf);
	  statep->xndxscnidx = elf_ndxscn (xndxscn);

	  xndxdata = elf_newdata (xndxscn);
	  if (xndxdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create symbol table for output file: %s"),
		   elf_errmsg (-1));

	  /* The following relies on the fact that Elf32_Word and Elf64_Word
	     have the same size.  */
	  xndxdata->d_type = ELF_T_WORD;
	  /* This is an estimated size, but it will definitely cap the
	     real value.  we might have to adjust the number later.  */
	  xndxdata->d_size = gelf_fsize (statep->outelf, ELF_T_WORD,
					 nsym_allocated, EV_CURRENT);
	  /* The first entry is left empty, clear it here and now.  */
	  xndxdata->d_buf = memset (xmalloc (xndxdata->d_size), '\0',
				    gelf_fsize (statep->outelf, ELF_T_WORD, 1,
						EV_CURRENT));
	  xndxdata->d_off = 0;
	  /* XXX Should use an ebl function.  */
	  xndxdata->d_align = sizeof (Elf32_Word);
	}
    }
  else
    {
      assert (statep->need_dynsym);

      /* First create the symbol table.  We need the symbol section itself
	 and the string table for it.  */
      symscn = elf_getscn (statep->outelf, statep->dynsymscnidx);
      symdata = elf_newdata (symscn);
      if (symdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create symbol table for output file: %s"),
	       elf_errmsg (-1));

      symdata->d_version = EV_CURRENT;
      symdata->d_type = ELF_T_SYM;
      /* This is an estimated size, but it will definitely cap the real value.
	 We might have to adjust the number later.  */
      nsym_allocated = (1 + statep->nsymtab + statep->nplt + statep->ngot
			- statep->nlocalsymbols);
      symdata->d_size = gelf_fsize (statep->outelf, ELF_T_SYM, nsym_allocated,
				    EV_CURRENT);
    }

  /* The first entry is left empty, clear it here and now.  */
  symdata->d_buf = memset (xmalloc (symdata->d_size), '\0',
			   gelf_fsize (statep->outelf, ELF_T_SYM, 1,
				       EV_CURRENT));
  symdata->d_off = 0;
  /* XXX This is ugly but how else can it be done.  */
  symdata->d_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1, EV_CURRENT);

  /* Allocate another array to keep track of the handles for the symbol
     names.  */
  symstrent = (struct Ebl_Strent **) xcalloc (nsym_allocated,
					      sizeof (struct Ebl_Strent *));

  /* By starting at 1 we effectively add a null entry.  */
  nsym = 1;

  /* Iteration over all sections.  */
  for (cnt = 0; cnt < statep->nallsections; ++cnt)
    {
      struct scnhead *head = statep->allsections[cnt];
      Elf_Scn *scn;
      struct scninfo *runp;
      GElf_Off offset;
      Elf32_Word xndx;

      /* Don't handle unused sections at all.  */
      if (!head->used)
	continue;

      /* Get the section handle.  */
      scn = elf_getscn (statep->outelf, head->scnidx);

      if (unlikely (head->kind == scn_dot_interp))
	{
	  Elf_Data *outdata;
	  const char *interp;

	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  /* This is the string we'll put in the section.  */
	  interp = statep->interp ?: "/lib/ld.so.1";

	  /* Create the section data.  */
	  outdata->d_buf = (void *) interp;
	  outdata->d_size = strlen (interp) + 1;
	  outdata->d_type = ELF_T_BYTE;
	  outdata->d_off = 0;
	  outdata->d_align = 1;
	  outdata->d_version = EV_CURRENT;

	  /* Remember the index of this section.  */
	  statep->interpscnidx = head->scnidx;

	  continue;
	}

      if (unlikely (head->kind == scn_dot_got))
	{
	  /* Remember the index of this section.  */
	  statep->gotscnidx = elf_ndxscn (scn);

	  /* Give the backend the change to initialize the section.  */
	  INITIALIZE_GOT (statep, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_gotrel))
	{
	  /* Remember the index of this section.  */
	  statep->relgotscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynrel))
	{
	  Elf_Data *outdata;

	  assert (statep->combreloc);
	  assert (statep->relgotscnidx == 0);

	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  outdata->d_size = statep->relsize_total;
	  outdata->d_buf = xmalloc (outdata->d_size);
	  outdata->d_type = (REL_TYPE (statep) == SHT_REL
			     ? ELF_T_REL : ELF_T_RELA);
	  outdata->d_off = 0;
	  outdata->d_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					 EV_CURRENT);

	  /* Remember the index of this section.  */
	  statep->reldynscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynamic))
	{
	  /* Only create the data for now.  */
	  Elf_Data *outdata;

	  /* Account for a few more entries we have to add.  */
	  if (statep->dt_flags != 0)
	    ++statep->ndynamic;
	  if (statep->dt_flags_1 != 0)
	    ++statep->ndynamic;
	  if (statep->dt_feature_1 != 0)
	    ++statep->ndynamic;

	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  /* Create the section data.  */
	  outdata->d_size = gelf_fsize (statep->outelf, ELF_T_DYN,
					statep->ndynamic, EV_CURRENT);
	  outdata->d_buf = xcalloc (1, outdata->d_size);
	  outdata->d_type = ELF_T_DYN;
	  outdata->d_off = 0;
	  outdata->d_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					 EV_CURRENT);

	  /* Remember the index of this section.  */
	  statep->dynamicscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynsym))
	{
	  /* We already know the section index.  */
	  assert (statep->dynsymscnidx == elf_ndxscn (scn));

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynstr))
	{
	  /* Remember the index of this section.  */
	  statep->dynstrscnidx = elf_ndxscn (scn);

	  /* Create the string table.  */
	  dynstrtab = ebl_strtabinit (true);

	  /* XXX TBI
	     We have to add all the strings which are needed in the
	     dynamic section here.  This means DT_FILTER,
	     DT_AUXILIARY, ... entries.  */
	  if (statep->ndsofiles > 0)
	    {
	      struct usedfiles *frunp = statep->dsofiles;

	      do
		if (! statep->ignore_unused_dsos || frunp->used)
		  frunp->sonameent = ebl_strtabadd (dynstrtab, frunp->soname,
						    0);
	      while ((frunp = frunp->next) != statep->dsofiles);
	    }


	  /* Add the runtime path information.  The strings are stored
	     in the .dynstr section.  If both rpath and runpath are defined
	     the runpath information is used.  */
	  if (statep->runpath != NULL || statep->rpath != NULL)
	    {
	      struct pathelement *startp;
	      struct pathelement *prunp;
	      int tag;
	      size_t len;
	      char *str;
	      char *cp;

	      if (statep->runpath != NULL)
		{
		  startp = statep->runpath;
		  tag = DT_RUNPATH;
		}
	      else
		{
		  startp = statep->rpath;
		  tag = DT_RPATH;
		}

	      /* Determine how long the string will be.  */
	      for (len = 0, prunp = startp; prunp != NULL; prunp = prunp->next)
		len += strlen (prunp->pname) + 1;

	      cp = str = (char *) xmalloc (len);
	      /* Copy the string.  */
	      for (prunp = startp; prunp != NULL; prunp = prunp->next)
		{
		  cp = stpcpy (cp, prunp->pname);
		  *cp++ = ':';
		}
	      /* Remove the last colon.  */
	      cp[-1] = '\0';

	      /* Remember the values until we can generate the dynamic
		 section.  */
	      statep->rxxpath_strent = ebl_strtabadd (dynstrtab, str, len);
	      statep->rxxpath_tag = tag;
	    }

	  continue;
	}

      if (unlikely (head->kind == scn_dot_hash))
	{
	  /* Remember the index of this section.  */
	  statep->hashscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_plt))
	{
	  /* Remember the index of this section.  */
	  statep->pltscnidx = elf_ndxscn (scn);

	  /* Give the backend the change to initialize the section.  */
	  INITIALIZE_PLT (statep, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_pltrel))
	{
	  /* Remember the index of this section.  */
	  statep->pltrelscnidx = elf_ndxscn (scn);

	  /* Give the backend the change to initialize the section.  */
	  INITIALIZE_PLTREL (statep, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_version))
	{
	  /* Remember the index of this section.  */
	  statep->versymscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_version_r))
	{
	  /* Remember the index of this section.  */
	  statep->verneedscnidx = elf_ndxscn (scn);

	  continue;
	}

      /* If we come here we must be handling a normal section.  */
      assert (head->kind == scn_normal);

      /* Create an STT_SECTION entry in the symbol table.  But not for
	 the symbolic symbol table.  */
      if (statep->need_symtab)
	{
	  /* XXX Can we be cleverer and do this only if needed?  */
	  GElf_Sym sym_mem;

	  sym_mem.st_name = 0;
	  sym_mem.st_info = GELF_ST_INFO (STB_LOCAL, STT_SECTION);
	  sym_mem.st_other = 0;
	  sym_mem.st_shndx = (head->scnidx < SHN_LORESERVE
			      ? head->scnidx : SHN_XINDEX);
	  sym_mem.st_value = 0;
	  sym_mem.st_size = 0;
	  xndx = head->scnidx < SHN_LORESERVE ? 0 : head->scnidx;
	  assert (nsym < nsym_allocated);
	  (void) gelf_update_symshndx (symdata, xndxdata, nsym, &sym_mem,
				       xndx);
	  head->scnsymidx = nsym++;
	}

      if (head->type == SHT_REL || head->type == SHT_RELA)
	{
	  /* Remember that we have to fill in the symbol table section
	     index.  */
	  struct scnlist *newp;

	  newp = (struct scnlist *) alloca (sizeof (*newp));
	  newp->scnidx = head->scnidx;
	  newp->scninfo = head->last->next;
	  newp->next = rellist;
	  rellist = newp;

	  /* When we create an executable or a DSO we don't simply
	     copy the existing relocations.  Instead many will be
	     resolved, others will be converted.  Create a data buffer
	     large enough to contain the contents which we will fill
	     in later.  */
	  if (statep->file_type != relocatable_file_type)
	    {
	      int type = head->type == SHT_REL ? ELF_T_REL : ELF_T_RELA;

	      data = elf_newdata (scn);
	      if (data == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      data->d_size = gelf_fsize (statep->outelf, type, head->relsize,
					 EV_CURRENT);
	      data->d_buf = xcalloc (data->d_size, 1);
	      data->d_type = type;
	      data->d_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					  EV_CURRENT);
	      data->d_off = 0;

	      continue;
	    }
	}

      /* Recognize string and merge flag and handle them.  */
      if (head->flags & SHF_MERGE)
	{
	  /* We merge the contents of the sections.  For this we do
	     not look at the contents of section directly.  Instead we
	     look at the symbols of the section.  */
	  Elf_Data *outdata;

	  /* Concatenate the lists of symbols for all sections.  */
	  runp = head->last->next;
	  head->symbols = runp->symbols;
	  while ((runp = runp->next) != head->last->next)
	    {
	      struct symbol *oldhead = head->symbols->next_in_scn;

	      head->symbols->next_in_scn = runp->symbols->next_in_scn;
	      runp->symbols->next_in_scn = oldhead;
	      head->symbols = runp->symbols;
	    }

	  /* Create the output section.  */
	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  if (head->last->shdr.sh_entsize == 1 && (head->flags & SHF_STRINGS))
	    {
	      /* Simple, single-byte string matching.  */
	      struct Ebl_Strtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;

	      mergestrtab = ebl_strtabinit (false);

	      symrunp = head->symbols->next_in_scn;
	      file = NULL;
	      do
		{
		  GElf_Sym sym_mem;
		  GElf_Sym *sym;

		  if (symrunp->file != file)
		    {
		      file = symrunp->file;

		      locsymdata = file->symtabdata;

		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);

		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  sym = gelf_getsym (locsymdata, symrunp->symidx, &sym_mem);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_strtabadd (mergestrtab,
				     (char *) locdata->d_buf + sym->st_value,
				     0);
		}
	      while ((symrunp = symrunp->next_in_scn)
		     != head->symbols->next_in_scn);

	      /* Create the final table.  */
	      ebl_strtabfinalize (mergestrtab, outdata);

	      /* Compute the final offsets in the section.  */
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_strtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      /* We don't need the string table anymore.  */
	      ebl_strtabfree (mergestrtab);
	    }
	  else if (head->last->shdr.sh_entsize == sizeof (wchar_t)
		   && (head->flags & SHF_STRINGS))
	    {
	      /* Simple, wchar_t string merging.  */
	      struct Ebl_WStrtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;

	      mergestrtab = ebl_wstrtabinit (false);

	      symrunp = runp->symbols;
	      file = NULL;
	      do
		{
		  GElf_Sym sym_mem;
		  GElf_Sym *sym;

		  if (symrunp->file != file)
		    {
		      file = symrunp->file;

		      locsymdata = file->symtabdata;

		      /* Using the raw section data here is possible
			 since we don't interpret the string
			 themselves except for looking for the wide
			 NUL character.  The NUL character has
			 fortunately the same representation
			 regardless of the byte order.  */
		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);

		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  sym = gelf_getsym (locsymdata, symrunp->symidx, &sym_mem);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_wstrtabadd (mergestrtab,
				      (wchar_t *) ((char *) locdata->d_buf
						   + sym->st_value), 0);
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      /* Create the final table.  */
	      ebl_wstrtabfinalize (mergestrtab, outdata);

	      /* Compute the final offsets in the section.  */
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_wstrtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      /* We don't need the string table anymore.  */
	      ebl_wstrtabfree (mergestrtab);
	    }
	  else
	    {
	      /* Non-standard merging.  */
	      struct Ebl_GStrtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;
	      /* If this is no string section the length of each "string"
		 is always one.  */
	      unsigned int len = (head->flags & SHF_STRINGS) ? 0 : 1;

	      mergestrtab = ebl_gstrtabinit (head->last->shdr.sh_entsize,
					     false);

	      symrunp = runp->symbols;
	      file = NULL;
	      do
		{
		  GElf_Sym sym_mem;
		  GElf_Sym *sym;

		  if (symrunp->file != file)
		    {
		      file = symrunp->file;

		      locsymdata = file->symtabdata;

		      /* Using the raw section data here is possible
			 since we don't interpret the string
			 themselves except for looking for the wide
			 NUL character.  The NUL character has
			 fortunately the same representation
			 regardless of the byte order.  */
		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);

		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  sym = gelf_getsym (locsymdata, symrunp->symidx, &sym_mem);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_gstrtabadd (mergestrtab,
				      (char *) locdata->d_buf + sym->st_value,
				      len);
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      /* Create the final table.  */
	      ebl_gstrtabfinalize (mergestrtab, outdata);

	      /* Compute the final offsets in the section.  */
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_gstrtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      /* We don't need the string table anymore.  */
	      ebl_gstrtabfree (mergestrtab);
	    }
	}
      else
	{
	  assert (head->scnidx == elf_ndxscn (scn));

	  /* It is important to start with the first list entry (and
	     not just any one) to add the sections in the correct
	     order.  */
	  runp = head->last->next;
	  offset = 0;
	  do
	    {
	      Elf_Data *outdata;
	      GElf_Off align;

	      data = elf_getdata (runp->scn, NULL);
	      assert (data != NULL);
	      outdata = elf_newdata (scn);
	      if (outdata == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      align =  MAX (1, data->d_align);
	      offset = ((offset + align - 1) & ~(align - 1));
	      runp->offset = offset;
	      runp->outscnndx = head->scnidx;
	      runp->allsectionsidx = cnt;

	      /* We reuse the data buffer in the input file.  */
	      *outdata = *data;
	      outdata->d_off = offset;

	      offset += outdata->d_size;

	      /* Given that we read the input file from disk we know there
		 cannot be another data part.  */
	      assert (elf_getdata (runp->scn, data) == NULL);
	    }
	  while ((runp = runp->next) != head->last->next);

	  /* If necessary add the additional line to the .comment section.  */
	  if (statep->add_ld_comment
	      && head->flags == 0
	      && head->type == SHT_PROGBITS
	      && strcmp (head->name, ".comment") == 0
	      && head->entsize == 0)
	    {
	      Elf_Data *outdata = elf_newdata (scn);

	      if (outdata == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      outdata->d_buf = (void *) "\0ld (Red Hat " PACKAGE ") " VERSION;
	      outdata->d_size = strlen ((char *) outdata->d_buf + 1) + 2;
	      outdata->d_off = offset;
	      outdata->d_type = ELF_T_BYTE;
	      outdata->d_align = 1;
	    }
	  /* XXX We should create a .comment section if none exists.
	     This requires that we early on detect that none such
	     section exists.  This should probably be implemented
	     together with some merging of the section contents.
	     Currently identical entries are not merged.  */
	}
    }

  /* The table we collect the strings in.  */
  strtab = ebl_strtabinit (true);
  if (strtab == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot create string table"));


#ifndef NDEBUG
  /* Keep track of the use of the XINDEX.  */
  need_xndx = false;
#endif

  /* We we generate a normal symbol table for an executable and the
     --export-dynamic option is not given, we need an extra table
     which keeps track of the symbol entry belonging to the symbol
     table entry.  Note that EXPORT_ALL_DYNAMIC is always set if we
     generate a DSO so we do not have to test this separately.  */
  ndxtosym = (struct symbol **) xcalloc (nsym_allocated,
					 sizeof (struct symbol));

  /* Iterate over all input files to collect the symbols.  */
  file = statep->relfiles->next;
  symdata = elf_getdata (elf_getscn (statep->outelf, statep->symscnidx), NULL);
  do
    {
      size_t maxcnt;
      Elf_Data *insymdata;
      Elf_Data *inxndxdata;

      /* There must be no dynamic symbol table when creating
	 relocatable files.  */
      assert (statep->file_type != relocatable_file_type
	      || file->dynsymtabdata == NULL);

      insymdata = file->symtabdata;
      assert (insymdata != NULL);
      inxndxdata = file->xndxdata;

      maxcnt = file->nsymtab;

      file->symindirect = (Elf32_Word *) xcalloc (maxcnt, sizeof (Elf32_Word));

      for (cnt = 1; cnt < maxcnt; ++cnt)
	{
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;
	  Elf32_Word xndx;
	  struct symbol *defp = NULL;

	  sym = gelf_getsymshndx (insymdata, inxndxdata, cnt, &sym_mem, &xndx);
	  assert (sym != NULL);

	  if (!statep->need_symtab && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
	    /* The dynamic symbol table does not contain local symbols.  */
	    continue;

	  if (unlikely (GELF_ST_TYPE (sym->st_info) == STT_SECTION))
	    {
	      /* Section symbols should always be local but who knows...  */
	      if (statep->need_symtab)
		{
		  /* Determine the real section index in the source file.
		     Use the XINDEX section content if necessary.  We don't
		     add this information to the dynamic symbol table.  */
		  if (sym->st_shndx != SHN_XINDEX)
		    xndx = sym->st_shndx;

		  assert (file->scninfo[xndx].allsectionsidx
			  < statep->nallsections);
		  file->symindirect[cnt] = statep->allsections[file->scninfo[xndx].allsectionsidx]->scnsymidx;
		  /* Note that the resulting index can be zero here.  There is
		     no guarantee that the output file will contain all the
		     sections the input file did.  */
		}
	      continue;
	    }

	  if ((statep->strip >= strip_all || !statep->need_symtab)
	      /* XXX Do we need these entries?  */
	      && GELF_ST_TYPE (sym->st_info) == STT_FILE)
	    continue;

	  if (sym->st_shndx != SHN_UNDEF
	      && (sym->st_shndx < SHN_LORESERVE
		  || sym->st_shndx == SHN_XINDEX))
	    {
	      /* If we are creating an executable with no normal
		 symbol table and we do not export all symbols and
		 this symbol is not defined in a DSO as well ignore
		 it.  */
	      if (!statep->export_all_dynamic && !statep->need_symtab)
		{
		  assert (cnt >= file->nlocalsymbols);
		  defp = file->symref[cnt];
		  assert (defp != NULL);

		  if (!defp->in_dso)
		    /* Ignore it.  */
		    continue;
		}

	    adjust_secnum:
	      /* Determine the real section index in the source file.  Use
		 the XINDEX section content if necessary.  */
	      if (sym->st_shndx != SHN_XINDEX)
		xndx = sym->st_shndx;

	      sym->st_value += file->scninfo[xndx].offset;

	      assert (file->scninfo[xndx].outscnndx < SHN_LORESERVE
		      || file->scninfo[xndx].outscnndx > SHN_HIRESERVE);
	      if (unlikely (file->scninfo[xndx].outscnndx > SHN_LORESERVE))
		{
		  /* It is not possible to have an extended section index
		     table for the dynamic symbol table.  */
		  if (!statep->need_symtab)
		    error (EXIT_FAILURE, 0, gettext ("\
section index too large in dynamic symbol table"));

		  assert (xndxdata != NULL);
		  sym->st_shndx = SHN_XINDEX;
		  xndx = file->scninfo[xndx].outscnndx;
#ifndef NDEBUG
		  need_xndx = true;
#endif
		}
	      else
		{
		  sym->st_shndx = file->scninfo[xndx].outscnndx;
		  xndx = 0;
		}
	    }
	  else if (sym->st_shndx == SHN_COMMON || sym->st_shndx == SHN_UNDEF)
	    {
	      /* Check whether we have a (real) definition for this
		 symbol.  If this is the case we skip this symbol
		 table entry.  */
	      assert (cnt >= file->nlocalsymbols);
	      defp = file->symref[cnt];
	      assert (defp != NULL);

	      assert (sym->st_shndx != SHN_COMMON || defp->defined);

	      if ((sym->st_shndx == SHN_COMMON && !defp->common)
		  || (sym->st_shndx == SHN_UNDEF && defp->defined)
		  || defp->added)
		/* Ignore this symbol table entry, there is a
		   "better" one or we already added it.  */
		continue;

	      /* Remember that we already added this symbol.  */
	      defp->added = 1;

	      /* Adjust the section number for common symbols.  */
	      if (sym->st_shndx == SHN_COMMON)
		goto adjust_secnum;
	    }
	  else if (unlikely (sym->st_shndx != SHN_ABS))
	    {
	      if (SPECIAL_SECTION_NUMBER_P (statep, sym->st_shndx))
		/* XXX Add code to handle machine specific special
		   sections.  */
		abort ();
	    }

	  /* Add the symbol name to the string table.  If the user
	     chooses the highest level of stripping avoid adding names
	     for local symbols in the string table.  */
	  if (sym->st_name != 0
	      && (statep->strip < strip_everything
		  || GELF_ST_BIND (sym->st_info) != STB_LOCAL))
	    symstrent[nsym] = ebl_strtabadd (strtab,
					     elf_strptr (file->elf,
							 file->symstridx,
							 sym->st_name), 0);

	  /* Once we know the name this field will get the correct
	     offset.  For now set it to zero which means no name
	     associated.  */
	  sym->st_name = 0;

	  /* If we had to merge sections we have a completely new
	     offset for the symbol.  */
	  if (file->has_merge_sections && file->symref[cnt] != NULL
	      && file->symref[cnt]->merged)
	    sym->st_value = file->symref[cnt]->merge.value;

	  /* Create the record in the output sections.  */
	  assert (nsym < nsym_allocated);
	  (void) gelf_update_symshndx (symdata, xndxdata, nsym, sym, xndx);

	  /* Add the reference to the symbol record in case we need it.
	     Find the symbol if this has not happened yet.  We do
	     not need the information for local symbols.  */
	  if (defp == NULL && cnt >= file->nlocalsymbols)
	    {
	      defp = file->symref[cnt];
	      assert (defp != NULL);
	    }

	  /* Store the reference to the symbol record.  The
	     sorting code will have to keep this array in the
	     correct order, too.  */
	  ndxtosym[nsym] = defp;

	  /* One more entry finished.  */
	  if (cnt >= file->nlocalsymbols)
	    file->symref[cnt]->outsymidx = nsym;
	  file->symindirect[cnt] = nsym++;
	}
    }
  while ((file = file->next) != statep->relfiles->next);
  /* Make sure we didn't create the extended section index table for
     nothing.  */
  assert (xndxdata == NULL || need_xndx);


  /* Create the version related sections.  */
  if (statep->verneedscnidx != 0)
    {
      /* We know the number of input files and total number of
	 referenced versions.  This allows us to allocate the memory
	 and then we iterate over the DSOs to get the version
	 information.  */
      struct usedfiles *runp;

      runp = statep->dsofiles->next;
      do
	{
	  int offset;

	  /* If this DSO has no versions skip it.  */
	  if (runp->verdefdata == NULL)
	    continue;

	  /* Add the object name.  */
	  offset = 0;
	  while (1)
	    {
	      GElf_Verdef def_mem;
	      GElf_Verdef *def;
	      GElf_Verdaux aux_mem;
	      GElf_Verdaux *aux;

	      /* Get data at the next offset.  */
	      def = gelf_getverdef (runp->verdefdata, offset, &def_mem);
	      assert (def != NULL);
	      aux = gelf_getverdaux (runp->verdefdata, offset + def->vd_aux,
				     &aux_mem);
	      assert (aux != NULL);

	      assert (def->vd_ndx <= runp->nverdef);
	      if (def->vd_ndx == 1 || runp->verdefused[def->vd_ndx] != 0)
		{
		  runp->verdefent[def->vd_ndx]
		    = ebl_strtabadd (dynstrtab, elf_strptr (runp->elf,
							    runp->dynsymstridx,
							    aux->vda_name), 0);

		  if (def->vd_ndx > 1)
		    runp->verdefused[def->vd_ndx] = statep->nextveridx++;
		}

	      if (def->vd_next == 0)
		/* That were all versions.  */
		break;

	      offset += def->vd_next;
	    }
	}
      while ((runp = runp->next) != statep->dsofiles->next);
    }

  /* At this point we should hide symbols and so on.  */
  if (statep->default_bind_local || statep->version_str_tab.filled > 0)
    /* XXX Add one more test when handling of wildcard symbol names
       is supported.  */
    {
    /* Check all non-local symbols whether they are on the export list.  */
      bool any_reduced = false;

      for (cnt = 1; cnt < nsym; ++cnt)
	{
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;

	  /* Note that we don't have to use 'gelf_getsymshndx' since we
	     only need the binding and the symbol name.  */
	  sym = gelf_getsym (symdata, cnt, &sym_mem);
	  assert (sym != NULL);

	  if (reduce_symbol_p (sym, symstrent[cnt], statep))
	    {
	      sym->st_info = GELF_ST_INFO (STB_LOCAL,
					   GELF_ST_TYPE (sym->st_info));
	      (void) gelf_update_sym (symdata, cnt, sym);

	      /* Show that we don't need this string anymore.  */
	      if (statep->strip == strip_everything)
		{
		  symstrent[cnt] = NULL;
		  any_reduced = true;
		}
	    }
	}

      if (unlikely (any_reduced))
	{
	  /* Since we will not write names of local symbols in the
	     output file and we have reduced the binding of some
	     symbols the string table previously constructed contains
	     too many string.  Correct it.  */
	  struct Ebl_Strtab *newp = ebl_strtabinit (true);

	  for (cnt = 1; cnt < nsym; ++cnt)
	    if (symstrent[cnt] != NULL)
	      symstrent[cnt] = ebl_strtabadd (newp,
					      ebl_string (symstrent[cnt]), 0);

	  ebl_strtabfree (strtab);
	  strtab = newp;
	}
    }

  /* Add the references to DSOs.  We can add these entries this late
     (after sorting out versioning) because references to DSOs are not
     effected.  */
  if (statep->from_dso != NULL)
    {
      struct symbol *runp;

      runp = statep->from_dso;
      do
	{
	  GElf_Sym sym_mem;
	  const char *name;
	  size_t namelen = 0;

	  sym_mem.st_value = 0;
	  sym_mem.st_size = runp->size;
	  sym_mem.st_info = GELF_ST_INFO (runp->weak ? STB_WEAK : STB_GLOBAL,
					  runp->type);
	  sym_mem.st_other = STV_DEFAULT;
	  sym_mem.st_shndx = SHN_UNDEF;

	  /* Create the record in the output sections.  */
	  assert (nsym < nsym_allocated);
	  (void) gelf_update_symshndx (symdata, xndxdata, nsym, &sym_mem, 0);

	  name = runp->name;

	  if (runp->file->versymdata != NULL)
	    {
	      /* XXX Is it useful to add the versym value to struct
		 symbol?  */
	      char *newp;
	      const char *versname;
	      GElf_Versym versym;

	      gelf_getversym (runp->file->versymdata, runp->symidx, &versym);

	      /* One can only link with the default version.  */
	      assert ((versym & 0x8000) == 0);

	      versname = ebl_string (runp->file->verdefent[versym]);

	      namelen = strlen (name) + strlen (versname) + 3;
	      newp = (char *) xmalloc (namelen);
	      strcpy (stpcpy (stpcpy (newp, name), "@@"), versname);
	      name = newp;
	    }

	  symstrent[nsym] = ebl_strtabadd (strtab, name, namelen);

	  /* Remember the symbol record this ELF symbol came from.  */
	  ndxtosym[nsym++] = runp;
	}
      while ((runp = runp->next) != statep->from_dso);
    }

  /* Now we know how many symbols will be in the output file.  Adjust
     the count in the section data.  */
  symdata->d_size = gelf_fsize (statep->outelf, ELF_T_SYM, nsym, EV_CURRENT);
  if (unlikely (xndxdata != NULL))
    xndxdata->d_size = gelf_fsize (statep->outelf, ELF_T_WORD, nsym,
				   EV_CURRENT);

  /* Create the symbol string table section.  */
  strscn = elf_newscn (statep->outelf);
  statep->strscnidx = elf_ndxscn (strscn);
  data = elf_newdata (strscn);
  shdr = gelf_getshdr (strscn, &shdr_mem);
  if (strscn == NULL || data == NULL || shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section for output file: %s"),
	   elf_errmsg (-1));

  ebl_strtabfinalize (strtab, data);

  shdr->sh_type = SHT_STRTAB;
  assert (shdr->sh_entsize == 0);

  if (unlikely (gelf_update_shdr (strscn, shdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section for output file: %s"),
	   elf_errmsg (-1));

  /* Fill in the offsets of the symbol names.  */
  for (cnt = 1; cnt < nsym; ++cnt)
    if (symstrent[cnt] != NULL)
      {
	GElf_Sym sym_mem;
	GElf_Sym *sym;

	/* Note that we don't have to use 'gelf_getsymshndx' since we don't
	   modify the section index.  */
	sym = gelf_getsym (symdata, cnt, &sym_mem);
	/* This better worked, we did it before.  */
	assert (sym != NULL);
	sym->st_name = ebl_strtaboffset (symstrent[cnt]);
	(void) gelf_update_sym (symdata, cnt, sym);
      }

  /* Since we are going to reorder the symbol table but still have to
     be able to find the new position based on the old one (since the
     latter is stored in 'symindirect' information of the input file
     data structure) we have to create yet another indirection
     table.  */
  statep->dblindirect = dblindirect
    = (Elf32_Word *) xmalloc (nsym * sizeof (Elf32_Word));

  /* Sort the symbol table so that the local symbols come first.  */
  /* XXX We don't use stable sorting here.  It seems not necessary and
     would be more expensive.  If it turns out to be necessary this can
     be fixed easily.  */
  nsym_local = 1;
  cnt = nsym - 1;
  while (nsym_local < cnt)
    {
      GElf_Sym locsym_mem;
      GElf_Sym *locsym;
      Elf32_Word locxndx;
      GElf_Sym globsym_mem;
      GElf_Sym *globsym;
      Elf32_Word globxndx;

      do
	{
	  locsym = gelf_getsymshndx (symdata, xndxdata, nsym_local,
				     &locsym_mem, &locxndx);
	  /* This better works.  */
	  assert (locsym != NULL);

	  if (GELF_ST_BIND (locsym->st_info) != STB_LOCAL
	      && (statep->need_symtab || statep->export_all_dynamic))
	    {
	      do
		{
		  globsym = gelf_getsymshndx (symdata, xndxdata, cnt,
					      &globsym_mem, &globxndx);
		  /* This better works.  */
		  assert (globsym != NULL);

		  if (unlikely (GELF_ST_BIND (globsym->st_info) == STB_LOCAL))
		    {
		      struct Ebl_Strent *strtmp;
		      struct symbol *symtmp;

		      /* We swap the two entries.  */
		      (void) gelf_update_symshndx (symdata, xndxdata,
						   nsym_local, globsym,
						   globxndx);
		      (void) gelf_update_symshndx (symdata, xndxdata, cnt,
						   locsym, locxndx);

		      dblindirect[nsym_local] = cnt;
		      dblindirect[cnt] = nsym_local;

		      strtmp = symstrent[nsym_local];
		      symstrent[nsym_local] = symstrent[cnt];
		      symstrent[cnt] = strtmp;

		      symtmp = ndxtosym[nsym_local];
		      ndxtosym[nsym_local] = ndxtosym[cnt];
		      ndxtosym[cnt] = symtmp;

		      ++nsym_local;
		      --cnt;

		      break;
		    }

		  dblindirect[cnt] = cnt;
		}
	      while (nsym_local < --cnt);

	      break;
	    }

	  dblindirect[nsym_local] = nsym_local;
	}
      while (++nsym_local < cnt);
    }

  /* The symbol 'nsym_local' is currently pointing to might be local,
     too.  Check and increment the variable if this is the case.  */
  if (likely (nsym_local < nsym))
    {
      GElf_Sym locsym_mem;
      GElf_Sym *locsym;

      /* Note that it is OK to not use 'gelf_getsymshndx' here.  */
      locsym = gelf_getsym (symdata, nsym_local, &locsym_mem);
      /* This better works.  */
      assert (locsym != NULL);

      if (GELF_ST_BIND (locsym->st_info) == STB_LOCAL)
	++nsym_local;
    }


  /* We need the versym array right away to keep track of the version
     symbols.  */
  if (statep->versymscnidx != 0)
    {
      /* We allocate more memory than we need since the array is morroring
	 the dynamic symbol table and not the normal symbol table.  I.e.,
	 no local symbols are present.  */
      versymscn = elf_getscn (statep->outelf, statep->versymscnidx);
      versymdata = elf_newdata (versymscn);
      if (versymdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create versioning section: %s"),
	       elf_errmsg (-1));

      versymdata->d_size = gelf_fsize (statep->outelf, ELF_T_HALF,
				       nsym - nsym_local + 1, EV_CURRENT);
      versymdata->d_buf = xcalloc (1, versymdata->d_size);
      versymdata->d_align = gelf_fsize (statep->outelf, ELF_T_HALF, 1,
					EV_CURRENT);
      versymdata->d_off = 0;
      versymdata->d_type = ELF_T_HALF;
    }


  /* If we have to construct the dynamic symbol table we must not include
     the local symbols.  If the normal symbol has to be emitted as well
     we haven't done anything else yet and we can construct it from
     scratch now.  */
  if (!statep->need_symtab)
    {
      /* Remove all local symbols.  */
      GElf_Sym nullsym_mem;
      size_t reduce;

      /* Note that the following code works even if there is no entry
	 to remove since the zeroth entry is always local.  */
      reduce = gelf_fsize (statep->outelf, ELF_T_SYM, nsym_local - 1,
			   EV_CURRENT);

      /* Note that we don't have to use 'gelf_update_symshndx' since
	 this is the dynamic symbol table we write.  */
      (void) gelf_update_sym (symdata, nsym_local - 1,
			      memset (&nullsym_mem, '\0',
				      sizeof (nullsym_mem)));

      /* Update the buffer pointer and size in the output data.  */
      symdata->d_buf = (char *) symdata->d_buf + reduce;
      symdata->d_size -= reduce;

      /* Add the version symbol information.  */
      if (versymdata != NULL)
	{
	  nsym_dyn = 1;
	  for (cnt = nsym_local; cnt < nsym; ++cnt, ++nsym_dyn)
	    {
	      struct symbol *symp = ndxtosym[cnt];

	      if (symp->file->versymdata != NULL)
		{
		  GElf_Versym versym;

		  gelf_getversym (symp->file->versymdata, symp->symidx,
				  &versym);

		  (void) gelf_update_versym (versymdata, nsym_dyn,
					     symp->file->verdefused[versym]);
		}
	      }
	}

      /* Since we only created the dynamic symbol table the number of
	 dynamic symbols is the total number of symbols.  */
      nsym_dyn = nsym - nsym_local + 1;

      /* XXX TBI.  Create whatever data structure is missing.  */
      abort ();
    }
  else if (statep->need_dynsym)
    {
      /* Create the dynamic symbol table section data along with the
	 string table.  We look at all non-local symbols we found for
	 the normal symbol table and add those.  */
      dynsymscn = elf_getscn (statep->outelf, statep->dynsymscnidx);
      dynsymdata = elf_newdata (dynsymscn);

      dynstrdata = elf_newdata (elf_getscn (statep->outelf,
					    statep->dynstrscnidx));
      if (dynsymdata == NULL || dynstrdata == NULL)
	error (EXIT_FAILURE, 0, gettext ("\
cannot create dynamic symbol table for output file: %s"),
	       elf_errmsg (-1));

      nsym_dyn_allocated = nsym - nsym_local + 1;
      dynsymdata->d_size = gelf_fsize (statep->outelf, ELF_T_SYM,
				       nsym_dyn_allocated, EV_CURRENT);
      dynsymdata->d_buf = memset (xmalloc (dynsymdata->d_size), '\0',
				  gelf_fsize (statep->outelf, ELF_T_SYM, 1,
					      EV_CURRENT));
      dynsymdata->d_type = ELF_T_SYM;
      dynsymdata->d_off = 0;
      dynsymdata->d_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
					EV_CURRENT);

      /* We need one more array which contains the hash codes of the
	 symbol names.  */
      hashcodes = (Elf32_Word *) xcalloc (nsym_dyn_allocated,
					  sizeof (Elf32_Word));

      /* We have and empty entry at the beginning.  */
      nsym_dyn = 1;

      /* Populate the table.  */
      for (cnt = nsym_local; cnt < nsym; ++cnt)
	{
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;
	  const char *str;

	  sym = gelf_getsym (symdata, cnt, &sym_mem);
	  assert (sym != NULL);

	  if (sym->st_shndx == SHN_XINDEX)
	    error (EXIT_FAILURE, 0, gettext ("\
section index too large in dynamic symbol table"));

	  /* We do not add the symbol to the dynamic symbol table if

	     - the symbol is for a file
	     - it is not externally visible (internal, hidden)
	     - if export_all_dynamic is not set and is only defined in
	       the executable (i.e., it is defined, but not (also) in
	       in DSO)

	     Set symstrent[cnt] to NULL in case an entry is ignored.  */
	  if (GELF_ST_TYPE (sym->st_info) == STT_FILE
	      || GELF_ST_VISIBILITY (sym->st_other) == STV_INTERNAL
	      || GELF_ST_VISIBILITY (sym->st_other) == STV_HIDDEN
	      || (!ndxtosym[cnt]->in_dso && ndxtosym[cnt]->defined))
	    {
	      symstrent[cnt] = NULL;
	      continue;
	    }

	  /* Add the version information.  */
	  if (versymdata != NULL)
	    {
	      struct symbol *symp = ndxtosym[cnt];

	      if (symp->file->versymdata != NULL)
		{
		  GElf_Versym versym;

		  gelf_getversym (symp->file->versymdata, symp->symidx,
				  &versym);

		  (void) gelf_update_versym (versymdata, nsym_dyn,
					     symp->file->verdefused[versym]);
		}
	      else
		{
		  /* XXX Add support for version definitions.  */
		  (void) gelf_update_versym (versymdata, nsym_dyn,
					     VER_NDX_GLOBAL);
		}
	    }

	  /* Store the index of the symbol in the dynamic symbol table.  */
	  ndxtosym[cnt]->outdynsymidx = nsym_dyn;

	  /* Create a new string table entry.  */
	  str = ndxtosym[cnt]->name;
	  symstrent[cnt] = ebl_strtabadd (dynstrtab, str, 0);
	  hashcodes[nsym_dyn++] = elf_hash (str);
	}

      /* Update the information about the symbol section.  */
      if (versymdata != NULL)
	{
	  GElf_Shdr shdrversym_mem;
	  GElf_Shdr *shdrversym;

	  /* Correct the size now that we know how many entries the
	     dynamic symbol table has.  */
	  versymdata->d_size = gelf_fsize (statep->outelf, ELF_T_HALF,
					   nsym_dyn, EV_CURRENT);

	  /* Add the reference to the symbol table.  */
	  shdrversym = gelf_getshdr (versymscn, &shdrversym_mem);
	  assert (shdrversym != NULL);

	  shdrversym->sh_link = statep->dynsymscnidx;

	  (void) gelf_update_shdr (versymscn, shdrversym);
	}
    }

  /* We don't need the map from the symbol table index to the symbol
     structure anymore.  */
  free (ndxtosym);

  if (statep->file_type != relocatable_file_type)
    {
      size_t nbucket;
      Elf32_Word *bucket;
      Elf32_Word *chain;
      size_t nchain;
      Elf_Scn *hashscn;
      GElf_Shdr hashshdr_mem;
      GElf_Shdr *hashshdr;
      Elf_Data *hashdata;

      /* Finalize the dynamic string table.  */
      ebl_strtabfinalize (dynstrtab, dynstrdata);

      /* Determine the "optimal" bucket size.  */
      nbucket = optimal_bucket_size (hashcodes, nsym_dyn, statep->optlevel,
				     statep);

      /* Create the .hash section data structures.  */
      assert (statep->hashscnidx != 0);
      hashscn = elf_getscn (statep->outelf, statep->hashscnidx);
      hashshdr = gelf_getshdr (hashscn, &hashshdr_mem);
      hashdata = elf_newdata (hashscn);
      if (hashshdr == NULL || hashdata == NULL)
	error (EXIT_FAILURE, 0, gettext ("\
cannot create hash table section for output file: %s"),
	       elf_errmsg (-1));

      hashshdr->sh_link = statep->dynsymscnidx;
      (void) gelf_update_shdr (hashscn, hashshdr);

      hashdata->d_size = (2 + nsym_dyn + nbucket) * sizeof (Elf32_Word);
      hashdata->d_buf = xcalloc (1, hashdata->d_size);
      hashdata->d_align = sizeof (Elf32_Word);
      hashdata->d_type = ELF_T_WORD;
      hashdata->d_off = 0;

      ((Elf32_Word *) hashdata->d_buf)[0] = nbucket;
      ((Elf32_Word *) hashdata->d_buf)[1] = nsym_dyn;
      bucket = &((Elf32_Word *) hashdata->d_buf)[2];
      chain = &((Elf32_Word *) hashdata->d_buf)[2 + nbucket];

      /* We have and empty entry at the beginning.  */
      nsym_dyn = 1;

      /* Haven't yet filled in any chain value.  */
      nchain = 0;

      /* Now put the names in.  */
      for (cnt = nsym_local; cnt < nsym; ++cnt)
	if (symstrent[cnt] != NULL)
	  {
	    GElf_Sym sym_mem;
	    GElf_Sym *sym;
	    size_t hashidx;

	    sym = gelf_getsym (symdata, cnt, &sym_mem);
	    assert (sym != NULL);

	    sym->st_name = ebl_strtaboffset (symstrent[cnt]);

	    assert (nsym_dyn < nsym_dyn_allocated);
	    (void) gelf_update_sym (dynsymdata, nsym_dyn, sym);

	    /* Add to the hash table.  */
	    hashidx = hashcodes[nsym_dyn] % nbucket;
	    if (bucket[hashidx] == 0)
	      bucket[hashidx] = nsym_dyn;
	    else
	      {
		hashidx = bucket[hashidx];
		while (chain[hashidx] != 0)
		  hashidx = chain[hashidx];

		chain[hashidx] = nsym_dyn;
	      }

	    ++nsym_dyn;
	  }

      free (hashcodes);

      /* Create the required version section.  */
      if (statep->verneedscnidx != 0)
	{
	  Elf_Scn *verneedscn;
	  GElf_Shdr verneedshdr_mem;
	  GElf_Shdr *verneedshdr;
	  Elf_Data *verneeddata;
	  struct usedfiles *runp;
	  size_t verneed_size = gelf_fsize (statep->outelf, ELF_T_VNEED, 1,
					    EV_CURRENT);
	  size_t vernaux_size = gelf_fsize (statep->outelf, ELF_T_VNAUX, 1,
					    EV_CURRENT);
	  int offset;
	  int ntotal;

	  verneedscn = elf_getscn (statep->outelf, statep->verneedscnidx);
	  verneedshdr = gelf_getshdr (verneedscn, &verneedshdr_mem);
	  verneeddata = elf_newdata (verneedscn);
	  if (verneedshdr == NULL || verneeddata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create versioning data: %s"),
		   elf_errmsg (-1));

	  verneeddata->d_size = (statep->nverdeffile * verneed_size
				 + statep->nverdefused * vernaux_size);
	  verneeddata->d_buf = xmalloc (verneeddata->d_size);
	  verneeddata->d_type = ELF_T_VNEED;
	  verneeddata->d_align = gelf_fsize (statep->outelf, ELF_T_WORD, 1,
					     EV_CURRENT);
	  verneeddata->d_off = 0;

	  runp = statep->dsofiles->next;
	  offset = 0;
	  ntotal = statep->nverdeffile;
	  do
	    {
	      int need_offset;
	      bool filled = false;
	      GElf_Verneed verneed;
	      GElf_Vernaux vernaux;
	      int ndef = 0;

	      /* If this DSO has no versions skip it.  */
	      if (runp->verdefdata == NULL)
		{
		  runp = runp->next;
		  continue;
		}

	      /* We fill in the Verneed record last.  Remember the
		 offset.  */
	      need_offset = offset;
	      offset += verneed_size;

	      for (cnt = 2; cnt <= runp->nverdef; ++cnt)
		if (runp->verdefused[cnt] != 0)
		  {
		    assert (runp->verdefent[cnt] != NULL);

		    if (filled)
		      {
			vernaux.vna_next = vernaux_size;
			(void) gelf_update_vernaux (verneeddata, offset,
						    &vernaux);
			offset += vernaux_size;
		      }

		    vernaux.vna_hash
		      = elf_hash (ebl_string (runp->verdefent[cnt]));
		    vernaux.vna_flags = 0;
		    vernaux.vna_other = runp->verdefused[cnt];
		    vernaux.vna_name = ebl_strtaboffset (runp->verdefent[cnt]);
		    filled = true;
		    ++ndef;
		  }

	      assert (filled);
	      vernaux.vna_next = 0;
	      (void) gelf_update_vernaux (verneeddata, offset, &vernaux);
	      offset += vernaux_size;

	      verneed.vn_version = VER_NEED_CURRENT;
	      verneed.vn_cnt = ndef;
	      verneed.vn_file = ebl_strtaboffset (runp->verdefent[1]);
	      /* The first auxiliary entry is always found directly
		 after the verneed entry.  */
	      verneed.vn_aux = verneed_size;
	      verneed.vn_next = --ntotal > 0 ? offset - need_offset : 0;
	      (void) gelf_update_verneed (verneeddata, need_offset,
					  &verneed);

	      runp = runp->next;
	    }
	  while (ntotal > 0);

	  assert (offset == verneeddata->d_size);

	  /* Add the needed information to the section header.  */
	  verneedshdr->sh_link = statep->dynstrscnidx;
	  verneedshdr->sh_info = statep->nverdeffile;
	  (void) gelf_update_shdr (verneedscn, verneedshdr);
	}

      /* Adjust the section size.  */
      dynsymdata->d_size = gelf_fsize (statep->outelf, ELF_T_SYM, nsym_dyn,
				       EV_CURRENT);
      if (versymdata != NULL)
	versymdata->d_size = gelf_fsize (statep->outelf, ELF_T_HALF, nsym_dyn,
					 EV_CURRENT);

      /* Add the remaining information to the section header.  */
      shdr = gelf_getshdr (dynsymscn, &shdr_mem);
      /* There is always exactly one local symbol.  */
      shdr->sh_info = 1;
      /* Reference the string table.  */
      shdr->sh_link = statep->dynstrscnidx;
      /* Write the updated info back.  */
      (void) gelf_update_shdr (dynsymscn, shdr);
    }

  /* We don't need the string table anymore.  */
  free (symstrent);

  /* Fill in the section header information.  */
  symscn = elf_getscn (statep->outelf, statep->symscnidx);
  shdr = gelf_getshdr (symscn, &shdr_mem);
  if (shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create symbol table for output file: %s"),
	   elf_errmsg (-1));

  shdr->sh_type = SHT_SYMTAB;
  shdr->sh_link = statep->strscnidx;
  shdr->sh_info = nsym_local;
  shdr->sh_entsize = gelf_fsize (statep->outelf, ELF_T_SYM, 1, EV_CURRENT);

  (void) gelf_update_shdr (symscn, shdr);


  /* Add names for the generated sections.  */
  if (statep->symscnidx != 0)
      symtab_ent = ebl_strtabadd (statep->shstrtab, ".symtab", 8);
  if (statep->xndxscnidx != 0)
    xndx_ent = ebl_strtabadd (statep->shstrtab, ".symtab_shndx", 14);
  if (statep->strscnidx != 0)
    strtab_ent = ebl_strtabadd (statep->shstrtab, ".strtab", 8);
  /* At this point we would have to test for failures in the
     allocation.  But we skip this.  First, the problem will be caught
     latter when doing more allocations for the section header table.
     Even if this would not be the case all that would happen is that
     the section names are empty.  The binary would still be usable if
     it is an executable or a DSO.  Not adding the test here saves
     quite a bit of code.  */


  /* Finally create the section for the section header string table.  */
  shstrtab_scn = elf_newscn (statep->outelf);
  shstrtab_ndx = elf_ndxscn (shstrtab_scn);
  if (unlikely (shstrtab_ndx == SHN_UNDEF))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));

  /* Add the name of the section to the string table.  */
  shstrtab_ent = ebl_strtabadd (statep->shstrtab, ".shstrtab", 10);
  if (unlikely (shstrtab_ent == NULL))
    error (EXIT_FAILURE, errno,
	   gettext ("cannot create section header string section"));

  /* Finalize the section header string table.  */
  data = elf_newdata (shstrtab_scn);
  if (data == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));
  ebl_strtabfinalize (statep->shstrtab, data);

  /* Now we know the string offsets for all section names.  */
  for (cnt = 0; cnt < statep->nallsections; ++cnt)
    if (statep->allsections[cnt]->scnidx != 0)
      {
	Elf_Scn *scn;

	scn = elf_getscn (statep->outelf, statep->allsections[cnt]->scnidx);

	shdr = gelf_getshdr (scn, &shdr_mem);
	assert (shdr != NULL);

	shdr->sh_name = ebl_strtaboffset (statep->allsections[cnt]->nameent);

	if (gelf_update_shdr (scn, shdr) == 0)
	  assert (0);
      }

  /* Add the names for the generated sections to the respective
     section headers.  */
  if (symtab_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (statep->outelf, statep->symscnidx);

      shdr = gelf_getshdr (scn, &shdr_mem);
      /* This cannot fail, we already accessed the header before.  */
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (symtab_ent);

      gelf_update_shdr (scn, shdr);
    }
  if (xndx_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (statep->outelf, statep->xndxscnidx);

      shdr = gelf_getshdr (scn, &shdr_mem);
      /* This cannot fail, we already accessed the header before.  */
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (xndx_ent);

      gelf_update_shdr (scn, shdr);
    }
  if (strtab_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (statep->outelf, statep->strscnidx);

      shdr = gelf_getshdr (scn, &shdr_mem);
      /* This cannot fail, we already accessed the header before.  */
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (strtab_ent);

      gelf_update_shdr (scn, shdr);
    }

  /* And the section header table section itself.  */
  shdr = gelf_getshdr (shstrtab_scn, &shdr_mem);
  if (shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));

  shdr->sh_name = ebl_strtaboffset (shstrtab_ent);
  shdr->sh_type = SHT_STRTAB;

  if (unlikely (gelf_update_shdr (shstrtab_scn, shdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));


  /* Add the correct section header info to the section group sections.  */
  groups = statep->groups;
  while (groups != NULL)
    {
      Elf_Scn *scn;
      struct scngroup *oldp;
      Elf32_Word si;

      scn = elf_getscn (statep->outelf, groups->outscnidx);
      shdr = gelf_getshdr (scn, &shdr_mem);
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (groups->nameent);
      shdr->sh_type = SHT_GROUP;
      shdr->sh_flags = 0;
      shdr->sh_link = statep->symscnidx;
      shdr->sh_entsize = sizeof (Elf32_Word);

      /* Determine the index for the signature symbol.  */
      si = groups->symbol->file->symindirect[groups->symbol->symidx];
      if (si == 0)
	{
	  assert (groups->symbol->file->symref[groups->symbol->symidx]
		  != NULL);
	  si = groups->symbol->file->symref[groups->symbol->symidx]->outsymidx;
	  assert (si != 0);
	}
      shdr->sh_info = statep->dblindirect[si];

      (void) gelf_update_shdr (scn, shdr);

      oldp = groups;
      groups = groups->next;
      free (oldp);
    }


  if (statep->file_type != relocatable_file_type)
    {
      size_t nphdr;
      struct output_segment *segp;
      GElf_Addr addr;
      struct output_segment *segment;
      Elf_Scn *scn;
      Elf32_Word nsec;
      GElf_Phdr phdr_mem;

      /* Every executable needs a program header.  The number of entries
	 varies.  One exists for each segment.  Each SHT_NOTE section gets
	 one, too.  For dynamically linked executables we have to create
	 one for the program header, the interpreter, and the dynamic
	 section.  First count the number of segments.

	 XXX Determine whether the segment is non-empty.  */
      nphdr = 0;
      segp = statep->output_segments;
      while (segp != NULL)
	{
	  ++nphdr;
	  segp = segp->next;
	}

      /* Add the number of SHT_NOTE sections.  We counted them earlier.  */
      nphdr += statep->nnotesections;

      /* If we create a DSO or the file is linked against DSOs we have three
	 more entries: INTERP, PHDR, DYNAMIC.  */
      if (dynamically_linked_p (statep))
	nphdr += 3;

      /* Create the program header structure.  */
      if (gelf_newphdr (statep->outelf, nphdr) == 0)
	error (EXIT_FAILURE, 0, gettext ("cannot create program header: %s"),
	       elf_errmsg (-1));


      /* Determine the section sizes and offsets.  We have to do this
	 to be able to determine the memory layout (which normally
	 differs from the file layout).  */
      if (elf_update (statep->outelf, ELF_C_NULL) == -1)
	error (EXIT_FAILURE, 0, gettext ("while determining file layout: %s"),
	       elf_errmsg (-1));


      /* Now determine the memory addresses of all the sections and
	 segments.  */
      nsec = 0;
      scn = elf_getscn (statep->outelf, statep->allsections[nsec]->scnidx);
      shdr = gelf_getshdr (scn, &shdr_mem);
      assert (shdr != NULL);

      /* The address we start with is the offset of the first (not
	 zeroth) section.  */
      addr = shdr->sh_offset;

      /* The index of the first loadable segment.  */
      nphdr = 1 + (dynamically_linked_p (statep) == true) * 2;

      segment = statep->output_segments;
      while (segment != NULL)
	{
	  struct output_rule *orule;
	  bool first_section = true;

	  /* the minimum alignment is a page size.  */
	  segment->align = statep->pagesize;

	  for (orule = segment->output_rules; orule != NULL;
	       orule = orule->next)
	    if (orule->tag == output_section)
	      {
		GElf_Off oldoff;

		/* See whether this output rule corresponds to the next
		   section.  Yes, this is a pointer comparison.  */
		if (statep->allsections[nsec]->name != orule->val.section.name)
		  /* No, ignore this output rule.  */
		  continue;

		/* We assign addresses only in segments which are actually
		   loaded.  */
		if (segment->mode != 0)
		  {
		    /* Adjust the offset of the input sections.  */
		    struct scninfo *isect;
		    struct scninfo *first;

		    isect = first = statep->allsections[nsec]->last;
		    if (isect != NULL)
		      do
			isect->offset += addr;
		      while ((isect = isect->next) != first);

		    /* Set the address of current section.  */
		    shdr->sh_addr = addr;

		    /* Write the result back.  */
		    (void) gelf_update_shdr (scn, shdr);

		    /* Remember the address.  */
		    statep->allsections[nsec]->addr = addr;
		  }

		if (first_section)
		  {
		    /* The first segment starts at offset zero.  */
		    if (segment == statep->output_segments)
		      {
			segment->offset = 0;
			segment->addr = addr - shdr->sh_offset;
		      }
		    else
		      {
			segment->offset = shdr->sh_offset;
			segment->addr = addr;
		      }

		    /* Determine the maximum alignment requirement.  */
		    segment->align = MAX (segment->align, shdr->sh_addralign);

		    first_section = false;
		  }

		segment->memsize = (shdr->sh_offset - segment->offset
				    + shdr->sh_size);

		/* Determine the new address which is computed using
		   the different of the offsets on the sections.  Note
		   that this assumes that the sections following each
		   other in the section header table are also
		   consecutive in the file.  This is true here because
		   libelf constructs files this way.  */
		oldoff = shdr->sh_offset;

		if (++nsec >= statep->nallsections)
		  {
		    if (segment->mode != 0)
		      {
			/* We still have to determine the file size of
			   the segment.  */
			scn = elf_nextscn (statep->outelf, scn);
			shdr = gelf_getshdr (scn, &shdr_mem);
			assert (shdr != NULL);

			segment->filesize = shdr->sh_offset - segment->offset;
		      }
		    break;
		  }

		scn = elf_getscn (statep->outelf,
				  statep->allsections[nsec]->scnidx);
		shdr = gelf_getshdr (scn, &shdr_mem);
		assert (shdr != NULL);

		/* This is the new address resulting from the offsets
		   in the file.  */
		assert (oldoff <= shdr->sh_offset);
		addr += shdr->sh_offset - oldoff;

		segment->filesize = shdr->sh_offset - segment->offset;
	      }
	    else
	      {
		assert (orule->tag == output_assignment);

		if (strcmp (orule->val.assignment->variable, ".") == 0)
		  /* This is a change of the address.  */
		  addr = eval_expression (orule->val.assignment->expression,
					  addr, statep);
		else
		  /* XXX TBI */
		  ;
	      }

	  /* Store the segment parameter for loadable segments.  */
	  if (segment->mode != 0)
	    {
	      phdr_mem.p_type = PT_LOAD;
	      phdr_mem.p_offset = segment->offset;
	      phdr_mem.p_vaddr = segment->addr;
	      phdr_mem.p_paddr = phdr_mem.p_vaddr;
	      phdr_mem.p_filesz = segment->filesize;
	      phdr_mem.p_memsz = segment->memsize;
	      phdr_mem.p_flags = segment->mode;
	      phdr_mem.p_align = segment->align;

	      (void) gelf_update_phdr (statep->outelf, nphdr++, &phdr_mem);
	    }

	  segment = segment->next;
	}

      /* Create the other program header entries.  */
      ehdr = gelf_getehdr (statep->outelf, &ehdr_mem);
      assert (ehdr != NULL);
      phdr_mem.p_type = PT_PHDR;
      phdr_mem.p_offset = ehdr->e_phoff;
      phdr_mem.p_vaddr = statep->output_segments->addr + phdr_mem.p_offset;
      phdr_mem.p_paddr = phdr_mem.p_vaddr;
      phdr_mem.p_filesz = ehdr->e_phnum * ehdr->e_phentsize;
      phdr_mem.p_memsz = phdr_mem.p_filesz;
      phdr_mem.p_flags = 0;		/* No need to set PF_R or so.  */
      phdr_mem.p_align = gelf_fsize (statep->outelf, ELF_T_ADDR, 1,
				     EV_CURRENT);
      (void) gelf_update_phdr (statep->outelf, 0, &phdr_mem);

      /* Adjust the addresses in the addresses of the symbol according
	 to the load addresses of the sections.  */
      if (statep->need_symtab)
	for (cnt = 1; cnt < nsym; ++cnt)
	  {
	    GElf_Sym sym_mem;
	    GElf_Sym *sym;
	    Elf32_Word shndx;

	    sym = gelf_getsymshndx (symdata, xndxdata, cnt, &sym_mem, &shndx);
	    assert (sym != NULL);

	    if (sym->st_shndx != SHN_XINDEX)
	      shndx = sym->st_shndx;

	    if ((shndx > SHN_UNDEF && shndx < SHN_LORESERVE)
		|| shndx > SHN_HIRESERVE)
	      {
		/* Note we subtract 1 from the section index since ALLSECTIONS
		   does not store the dummy section with offset zero.  */
		sym->st_value += statep->allsections[shndx - 1]->addr;

		/* We don't have to use 'gelf_update_symshndx' since the
		   section number doesn't change.  */
		(void) gelf_update_sym (symdata, cnt, sym);
	      }
	  }

      if (statep->need_dynsym)
	for (cnt = 1; cnt < nsym_dyn; ++cnt)
	  {
	    GElf_Sym sym_mem;
	    GElf_Sym *sym;

	    sym = gelf_getsym (dynsymdata, cnt, &sym_mem);
	    assert (sym != NULL);

	    if (sym->st_shndx > SHN_UNDEF && sym->st_shndx < SHN_LORESERVE)
	      {
		/* Note we subtract 1 from the section index since ALLSECTIONS
		   does not store the dummy section with offset zero.  */
		sym->st_value += statep->allsections[sym->st_shndx - 1]->addr;

		/* We don't have to use 'gelf_update_symshndx' since the
		   section number doesn't change.  */
		(void) gelf_update_sym (dynsymdata, cnt, sym);
	      }
	  }

      if (dynamically_linked_p (statep))
	{
	  Elf_Scn *outscn;
	  Elf_Data *dyndata;
	  GElf_Dyn dyn_mem;

	  assert (statep->interpscnidx != 0);
	  shdr = gelf_getshdr (elf_getscn (statep->outelf,
					   statep->interpscnidx),
			       &shdr_mem);
	  assert (shdr != NULL);

	  phdr_mem.p_type = PT_INTERP;
	  phdr_mem.p_offset = shdr->sh_offset;
	  phdr_mem.p_vaddr = shdr->sh_addr;
	  phdr_mem.p_paddr = phdr_mem.p_vaddr;
	  phdr_mem.p_filesz = shdr->sh_size;
	  phdr_mem.p_memsz = phdr_mem.p_filesz;
	  phdr_mem.p_flags = 0;		/* No need to set PF_R or so.  */
	  phdr_mem.p_align = 1;		/* It's a string.  */

	  (void) gelf_update_phdr (statep->outelf, 1, &phdr_mem);

	  assert (statep->dynamicscnidx);
	  outscn = elf_getscn (statep->outelf, statep->dynamicscnidx);
	  shdr = gelf_getshdr (outscn, &shdr_mem);
	  assert (shdr != NULL);

	  phdr_mem.p_type = PT_DYNAMIC;
	  phdr_mem.p_offset = shdr->sh_offset;
	  phdr_mem.p_vaddr = shdr->sh_addr;
	  phdr_mem.p_paddr = phdr_mem.p_vaddr;
	  phdr_mem.p_filesz = shdr->sh_size;
	  phdr_mem.p_memsz = phdr_mem.p_filesz;
	  phdr_mem.p_flags = 0;		/* No need to set PF_R or so.  */
	  phdr_mem.p_align = shdr->sh_addralign;

	  (void) gelf_update_phdr (statep->outelf, 2, &phdr_mem);

	  /* Fill in the reference to the .dynstr section.  */
	  assert (statep->dynstrscnidx != 0);
	  shdr->sh_link = statep->dynstrscnidx;
	  (void) gelf_update_shdr (outscn, shdr);

	  /* And fill the remaining entries.  */
	  dyndata = elf_getdata (outscn, NULL);
	  assert (dyndata != NULL);

	  /* Add the DT_NEEDED entries.  */
	  if (statep->ndsofiles > 0)
	    {
	      struct usedfiles *runp = statep->dsofiles->next;

	      do
		if (! statep->ignore_unused_dsos || runp->used)
		  {
		    if (runp->lazyload)
		      {
			dyn_mem.d_tag = DT_POSFLAG_1;
			dyn_mem.d_un.d_val = DF_P1_LAZYLOAD;
			(void) gelf_update_dyn (dyndata,
						statep->ndynamic_filled++,
						&dyn_mem);
			assert (statep->ndynamic_filled < statep->ndynamic);
		      }

		    dyn_mem.d_tag = DT_NEEDED;
		    dyn_mem.d_un.d_val = ebl_strtaboffset (runp->sonameent);
		    (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
					    &dyn_mem);
		    assert (statep->ndynamic_filled < statep->ndynamic);
		  }
	      while ((runp = runp->next) != statep->dsofiles->next);
	    }

	  /* We can finish the DT_RUNPATH/DT_RPATH entries now.  */
	  if (statep->rxxpath_strent != NULL)
	    {
	      dyn_mem.d_tag = statep->rxxpath_tag;
	      dyn_mem.d_un.d_val = ebl_strtaboffset (statep->rxxpath_strent);
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }

	  /* Reference to initialization and finalization functions.

	     XXX This code depends on symbol table being relocated.

	     XXX We currently don't support init_array, fini_array, and
	     preinit_array.  */
	  if (statep->init_symbol != NULL)
	    {
	      GElf_Sym sym_mem;
	      GElf_Sym *sym;

	      if (statep->need_symtab)
		sym = gelf_getsym (symdata, statep->init_symbol->outsymidx,
				   &sym_mem);
	      else
		sym = gelf_getsym (dynsymdata,
				   statep->init_symbol->outdynsymidx,
				   &sym_mem);
	      assert (sym != NULL);

	      dyn_mem.d_tag = DT_INIT;
	      dyn_mem.d_un.d_ptr = sym->st_value;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }
	  if (statep->fini_symbol != NULL)
	    {
	      GElf_Sym sym_mem;
	      GElf_Sym *sym;

	      if (statep->need_symtab)
		sym = gelf_getsym (symdata, statep->fini_symbol->outsymidx,
				   &sym_mem);
	      else
		sym = gelf_getsym (dynsymdata,
				   statep->fini_symbol->outdynsymidx,
				   &sym_mem);
	      assert (sym != NULL);

	      dyn_mem.d_tag = DT_FINI;
	      dyn_mem.d_un.d_ptr = sym->st_value;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }

	  /* The hash table which comes with dynamic symbol table.  */
	  shdr = gelf_getshdr (elf_getscn (statep->outelf, statep->hashscnidx),
			       &shdr_mem);
	  assert (shdr != NULL);
	  dyn_mem.d_tag = DT_HASH;
	  dyn_mem.d_un.d_ptr = shdr->sh_addr;
	  (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				  &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  /* Reference to the symbol table section.  */
	  assert (statep->dynsymscnidx != 0);
	  shdr = gelf_getshdr (elf_getscn (statep->outelf,
					   statep->dynsymscnidx),
			       &shdr_mem);
	  assert (shdr != NULL);
	  dyn_mem.d_tag = DT_SYMTAB;
	  dyn_mem.d_un.d_ptr = shdr->sh_addr;
	  gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  dyn_mem.d_tag = DT_SYMENT;
	  dyn_mem.d_un.d_val = gelf_fsize (statep->outelf, ELF_T_SYM, 1,
					   EV_CURRENT);
	  gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  /* And the string table which comes with it.  */
	  shdr = gelf_getshdr (elf_getscn (statep->outelf,
					   statep->dynstrscnidx),
			       &shdr_mem);
	  assert (shdr != NULL);
	  dyn_mem.d_tag = DT_STRTAB;
	  dyn_mem.d_un.d_ptr = shdr->sh_addr;
	  gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  dyn_mem.d_tag = DT_STRSZ;
	  dyn_mem.d_un.d_ptr = shdr->sh_size;
	  gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  /* Add the entries related to the .plt.  */
	  if (statep->nplt > 0)
	    {
	      shdr = gelf_getshdr (elf_getscn (statep->outelf,
					       statep->gotscnidx),
				   &shdr_mem);
	      assert (shdr != NULL);
	      dyn_mem.d_tag = DT_PLTGOT;
	      /* XXX This should probably be machine dependent.  */
	      dyn_mem.d_un.d_ptr = shdr->sh_addr;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);

	      shdr = gelf_getshdr (elf_getscn (statep->outelf,
					       statep->pltrelscnidx),
				   &shdr_mem);
	      assert (shdr != NULL);
	      dyn_mem.d_tag = DT_PLTRELSZ;
	      dyn_mem.d_un.d_val = shdr->sh_size;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);

	      dyn_mem.d_tag = DT_JMPREL;
	      dyn_mem.d_un.d_ptr = shdr->sh_addr;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);

	      dyn_mem.d_tag = DT_PLTREL;
	      dyn_mem.d_un.d_val = REL_TYPE (statep);
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }

	  if (statep->verneedscnidx != 0)
	    {
	      shdr = gelf_getshdr (elf_getscn (statep->outelf,
					       statep->verneedscnidx),
				   &shdr_mem);
	      assert (shdr != NULL);
	      dyn_mem.d_tag = DT_VERNEED;
	      dyn_mem.d_un.d_ptr = shdr->sh_addr;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);

	      dyn_mem.d_tag = DT_VERNEEDNUM;
	      dyn_mem.d_un.d_val = statep->nverdeffile;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	    }

	  if (statep->versymscnidx != 0)
	    {
	      shdr = gelf_getshdr (elf_getscn (statep->outelf,
					       statep->versymscnidx),
				   &shdr_mem);
	      assert (shdr != NULL);
	      dyn_mem.d_tag = DT_VERSYM;
	      dyn_mem.d_un.d_ptr = shdr->sh_addr;
	      (void) gelf_update_dyn (dyndata, statep->ndynamic_filled++,
				      &dyn_mem);
	    }

	  /* We always create the DT_DEBUG entry.  */
	  dyn_mem.d_tag = DT_DEBUG;
	  dyn_mem.d_un.d_val = 0;
	  gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	  assert (statep->ndynamic_filled < statep->ndynamic);

	  /* Add the flag words if necessary.  */
	  if (statep->dt_flags != 0)
	    {
	      dyn_mem.d_tag = DT_FLAGS;
	      dyn_mem.d_un.d_val = statep->dt_flags;
	      gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }
	  if (statep->dt_flags_1 != 0)
	    {
	      dyn_mem.d_tag = DT_FLAGS_1;
	      dyn_mem.d_un.d_val = statep->dt_flags_1;
	      gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }
	  if (statep->dt_feature_1 != 0)
	    {
	      dyn_mem.d_tag = DT_FEATURE_1;
	      dyn_mem.d_un.d_val = statep->dt_feature_1;
	      gelf_update_dyn (dyndata, statep->ndynamic_filled++, &dyn_mem);
	      assert (statep->ndynamic_filled < statep->ndynamic);
	    }
	}
    }


  /* Now that we created the symbol table we can add the reference to
     it in the sh_link field of the section headers of the relocation
     sections.  */
  while (rellist != NULL)
    {
      Elf_Scn *outscn;

      outscn = elf_getscn (statep->outelf, rellist->scnidx);
      shdr = gelf_getshdr (outscn, &shdr_mem);
      /* This must not fail since we did it before.  */
      assert (shdr != NULL);

      /* Remember the symbol table which belongs to the relocation section.  */
      /* XXX Recognize when .dynsym must be used.  */
      shdr->sh_link = statep->symscnidx;

      /* And the reference to the section which is relocated by this
	 relocation section.  We use the info from the first input
	 section but all records should have the same information.  */
      shdr->sh_info =
	rellist->scninfo->fileinfo->scninfo[rellist->scninfo->shdr.sh_info].outscnndx;

      /* Store the changes.  */
      (void) gelf_update_shdr (outscn, shdr);


      /* Perform the actual relocations.  */
      if (statep->file_type == relocatable_file_type)
	/* We only have to adjust offsets and symbol indices.  */
	RELOCATE_SECTION (outscn, rellist->scninfo, dblindirect, statep);
      else
	{
	  /* The file has loadable segments and therefore the section
	     must have the allocation flag set.  */
	  shdr->sh_flags |= SHF_ALLOC;

	  /* TBI */
	}


      /* Up to the next relocation section.  */
      rellist = rellist->next;
    }


  /* We need the ELF header once more.  */
  ehdr = gelf_getehdr (statep->outelf, &ehdr_mem);
  assert (ehdr != NULL);

  /* Set the section header string table index.  */
  if (likely (shstrtab_ndx < SHN_HIRESERVE)
      && likely (shstrtab_ndx != SHN_XINDEX))
    ehdr->e_shstrndx = shstrtab_ndx;
  else
    {
      /* We have to put the section index in the sh_link field of the
	 zeroth section header.  */
      Elf_Scn *scn = elf_getscn (statep->outelf, 0);

      shdr = gelf_getshdr (scn, &shdr_mem);
      if (unlikely (shdr == NULL))
	error (EXIT_FAILURE, 0,
	       gettext ("cannot get header of 0th section: %s"),
	       elf_errmsg (-1));

      shdr->sh_link = shstrtab_ndx;

      (void) gelf_update_shdr (scn, shdr);

      ehdr->e_shstrndx = SHN_XINDEX;
    }

  if (statep->file_type != relocatable_file_type)
    /* DSOs and executables have to define the entry point symbol.  */
    ehdr->e_entry = find_entry_point (statep);

  if (unlikely (gelf_update_ehdr (statep->outelf, ehdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot update ELF header: %s"),
	   elf_errmsg (-1));


  /* Free the data which we don't need anymore.  */
  free (statep->dblindirect);


  /* Finalize the .plt section the what belongs to them.  */
  FINALIZE_PLT (statep, nsym, nsym_dyn);

  return 0;
}


/* This is a function which must be specified in all backends.  */
static void
ld_generic_relocate_section (Elf_Scn *outscn, struct scninfo *firstp,
			     const Elf32_Word *dblindirect,
			     struct ld_state *statep)
{
  error (EXIT_FAILURE, 0, gettext ("\
linker backend didn't specify function to relocate section"));
  /* NOTREACHED */
}


/* Finalize the output file.  */
static int
ld_generic_finalize (struct ld_state *statep)
{
  /* Write out the ELF file data.  */
  if (elf_update (statep->outelf, ELF_C_WRITE) == -1)
      error (EXIT_FAILURE, 0, gettext ("while writing output file: %s"),
	     elf_errmsg (-1));

  /* Free the resources.  */
  if (elf_end (statep->outelf) != 0)
    error (EXIT_FAILURE, 0, gettext ("while finishing output file: %s"),
	   elf_errmsg (-1));

  /* Now we can change the file access permissions.  */
  else if (fchmod (statep->outfd,
		   statep->file_type == relocatable_file_type
		   ? S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
		   : S_IRWXU | S_IRWXG | S_IRWXO)
	   == -1)
    /* Shouldn't happen but who knows.  */
    error (EXIT_FAILURE, errno,
	   gettext ("cannot change access mode of output file"));

  /* Close the file descriptor.  */
  (void) close (statep->outfd);

  /* Now it's time to rename the file.  Remove an old existing file
     first.  */
  if (rename (statep->tempfname, statep->outfname) != 0)
    /* Something went wrong.  */
    error (EXIT_FAILURE, errno, gettext ("cannot rename output file"));

  /* Signal the cleanup handler that the file is correctly created.  */
  statep->tempfname = NULL;

  return 0;
}


static bool
ld_generic_special_section_number_p (struct ld_state *statep, size_t number)
{
  /* There are no special section numbers in the gABI.  */
  return false;
}


static bool
ld_generic_section_type_p (struct ld_state *statep, GElf_Word type)
{
  if ((type >= SHT_NULL && type < SHT_NUM)
      /* XXX Enable the following two when implemented.  */
      // || type == SHT_GNU_LIBLIST
      // || type == SHT_CHECKSUM
      /* XXX Eventually include SHT_SUNW_move, SHT_SUNW_COMDAT, and
	 SHT_SUNW_syminfo.  */
      || (type >= SHT_GNU_verdef && type <= SHT_GNU_versym))
    return true;

  return false;
}


static GElf_Xword
ld_generic_dynamic_section_flags (struct ld_state *statep)
{
  /* By default the .dynamic section is writable (and is of course
     loaded).  Few architecture differ from this.  */
  return SHF_ALLOC | SHF_WRITE;
}


static void
ld_generic_initialize_plt (struct ld_state *statep, Elf_Scn *scn)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_plt");
}


static void
ld_generic_initialize_pltrel (struct ld_state *statep, Elf_Scn *scn)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_pltrel");
}


static void
ld_generic_initialize_got (struct ld_state *statep, Elf_Scn *scn)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_got");
}


static void
ld_generic_finalize_plt (struct ld_state *statep, size_t nsym, size_t nsym_dyn)
{
  /* By default we assume that nothing has to be done.  */
}


static int
ld_generic_rel_type (struct ld_state *statep)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "rel_type");
  /* Just to keep the compiler calm.  */
  return 0;
}


static void
ld_generic_count_relocations (struct ld_state *statep, struct scninfo *scninfo)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "count_relocations");
}


static void
ld_generic_create_relocations (struct ld_state *statep, Elf32_Word scnidx,
			       struct scninfo *scninfo)
{
  /* This cannot be implemented generally.  There should have been a
     machine dependent implementation and we should never have arrived
     here.  */
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "create_relocations");
}
