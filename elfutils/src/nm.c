/* Print information from ELF file in human-readable form.
   Copyright (C) 2000, 2001, 2002, 2003 Red Hat, Inc.
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

#include <ar.h>
#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <dwarf.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libdwarf.h>
#include <libebl.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the parameters which have no short form.  */
#define OPT_DEFINED	0x100
#define OPT_MARK_WEAK	0x101

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output selection:") },
  { "debug-syms", 'a', NULL, 0, N_("Display debugger-only symbols") },
  { "defined-only", OPT_DEFINED, NULL, 0, N_("Display only defined symbols") },
  { "dynamic", 'D', NULL, 0, N_("Display dynamic symbols instead of normal symbols") },
  { "extern-only", 'g', NULL, 0, N_("Display only external symbols") },
  { "undefined-only", 'u', NULL, 0, N_("Display only undefined symbols") },
  { "print-armap", 's', NULL, 0,
    N_("Include index for symbols from archive members") },

  { NULL, 0, NULL, 0, N_("Output format:") },
  { "print-file-name", 'A', NULL, 0, N_("Print name of the input file before every symbol") },
  { NULL, 'o', NULL, OPTION_HIDDEN, "Same as -A" },
  { "format", 'f', "FORMAT", 0, N_("Use the output format FORMAT.  FORMAT can be `bsd', `sysv' or `posix'.  The default is `sysv'") },
  { NULL, 'B', NULL, 0, N_("Same as --format=bsd") },
  { "portability", 'P', NULL, 0, N_("Same as --format=posix") },
  { "radix", 't', "RADIX", 0, N_("Use RADIX for printing symbol values") },
  { "mark-weak", OPT_MARK_WEAK, NULL, 0, N_("Mark weak symbols") },
  { "print-size", 'S', NULL, 0, N_("Print size of defined symbols") },

  { NULL, 0, NULL, 0, N_("Output options:") },
  { "numeric-sort", 'n', NULL, 0, N_("Sort symbols numerically by address") },
  { "no-sort", 'p', NULL, 0, N_("Do not sort the symbols") },
  { "reverse-sort", 'r', NULL, 0, N_("Reverse the sense of the sort") },
  { NULL, 0, NULL, 0, N_("Miscellaneous:") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("List symbols from FILEs (a.out by default).");

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
static int process_file (const char *fname, bool more_than_one);

/* Handle content of archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
		      const char *suffix);

/* Handle ELF file.  */
static int handle_elf (Elf *elf, const char *prefix, const char *fname,
		       const char *suffix);


#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR: %s"),		      \
	 fname, elf_errmsg (-1))


/* Internal representation of symbols.  */
typedef struct GElf_SymX
{
  GElf_Sym sym;
  Elf32_Word xndx;
} GElf_SymX;


/* User-selectable options.  */

/* The selected output format.  */
static enum
{
  format_sysv = 0,
  format_bsd,
  format_posix
} format;

/* Print defined, undefined, or both?  */
static bool hide_undefined;
static bool hide_defined;

/* Print local symbols also?  */
static bool hide_local;

/* Nonzero if full filename should precede every symbol.  */
static bool print_file_name;

/* If true print size of defined symbols in BSD format.  */
static bool print_size;

/* If true print archive index.  */
static bool print_armap;

/* If true reverse sorting.  */
static bool reverse_sort;

/* Type of the section we are printing.  */
static GElf_Word symsec_type = SHT_SYMTAB;

/* Sorting selection.  */
static enum
{
  sort_name = 0,
  sort_numeric,
  sort_nosort
} sort;

/* Radix for printed numbers.  */
static enum
{
  radix_hex = 0,
  radix_decimal,
  radix_octal
} radix;

/* If nonzero weak symbols are distinguished from global symbols by adding
   a `*' after the identifying letter for the symbol class and type.  */
static bool mark_weak;


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
    result = process_file ("a.out", false);
  else
    {
      /* Process all the remaining files.  */
      bool more_than_one = remaining + 1 < argc;

      do
	result |= process_file (argv[remaining], more_than_one);
      while (++remaining < argc);
    }

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "nm (Red Hat %s) %s\n", PACKAGE, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2003");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'a':
      /* XXX */
      break;

    case 'f':
      if (strcmp (arg, "bsd") == 0)
	format = format_bsd;
      else if (strcmp (arg, "posix") == 0)
	format = format_posix;
      else
	/* Be bug compatible.  The BFD implementation also defaulted to
	   using the SysV format if nothing else matches.  */
	format = format_sysv;
      break;

    case 'g':
      hide_local = true;
      break;

    case 'n':
      sort = sort_numeric;
      break;

    case 'p':
      sort = sort_nosort;
      break;

    case 't':
      if (strcmp (arg, "10") == 0 || strcmp (arg, "d") == 0)
	radix = radix_decimal;
      else if (strcmp (arg, "8") == 0 || strcmp (arg, "o") == 0)
	radix = radix_octal;
      else
	radix = radix_hex;
      break;

    case 'u':
      hide_undefined = false;
      hide_defined = true;
      break;

    case 'A':
    case 'o':
      print_file_name = true;
      break;

    case 'B':
      format = format_bsd;
      break;

    case 'D':
      symsec_type = SHT_DYNSYM;
      break;

    case 'P':
      format = format_posix;
      break;

    case OPT_DEFINED:
      hide_undefined = true;
      hide_defined = false;
      break;

    case OPT_MARK_WEAK:
      mark_weak = true;
      break;

    case 'S':
      print_size = true;
      break;

    case 's':
      print_armap = true;
      break;

    case 'r':
      reverse_sort = true;
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
process_file (const char *fname, bool more_than_one)
{
  /* Open the file and determine the type.  */
  int fd;
  Elf *elf;

  /* Open the file.  */
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, fname);
      return 1;
    }

  /* Now get the ELF descriptor.  */
  elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf != NULL)
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  int result = handle_elf (elf, more_than_one ? "" : NULL,
				   fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error (EXIT_FAILURE, errno, gettext ("while close `%s'"), fname);

	  return result;
	}
      else if (elf_kind (elf) == ELF_K_AR)
	{
	  int result = handle_ar (fd, elf, NULL, fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error (EXIT_FAILURE, errno, gettext ("while close `%s'"), fname);

	  return result;
	}

      /* We cannot handle this type.  Close the descriptor anyway.  */
      if (elf_end (elf) != 0)
	INTERNAL_ERROR (fname);
    }

  error (0, 0, gettext ("%s: File format not recognized"), fname);

  return 1;
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
	   const char *suffix)
{
  size_t fname_len = strlen (fname) + 1;
  size_t prefix_len = prefix != NULL ? strlen (prefix) : 0;
  char new_prefix[prefix_len + fname_len + 2];
  size_t suffix_len = suffix != NULL ? strlen (suffix) : 0;
  char new_suffix[suffix_len + 2];
  char *cp;
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  int result = 0;

  cp = new_prefix;
  if (prefix != NULL)
    cp = stpcpy (cp, prefix);
  cp = stpcpy (cp, fname);
  stpcpy (cp, "[");

  cp = new_suffix;
  if (suffix != NULL)
    cp = stpcpy (cp, suffix);
  stpcpy (cp, "]");

  /* First print the archive index if this is wanted.  */
  if (print_armap)
    {
      Elf_Arsym *arsym = elf_getarsym (elf, NULL);

      if (arsym != NULL)
	{
	  Elf_Arhdr *arhdr = NULL;
	  size_t arhdr_off = 0;	/* Note: 0 is no valid offset.  */

	  puts (gettext("\nArchive index:"));

	  while (arsym->as_off != 0)
	    {
	      if (arhdr_off != arsym->as_off
		  && (elf_rand (elf, arsym->as_off) != arsym->as_off
		      || (subelf = elf_begin (fd, cmd, elf)) == NULL
		      || (arhdr = elf_getarhdr (subelf)) == NULL))
		{
		  error (0, 0, gettext ("invalid offset %zu for symbol %s"),
			 arsym->as_off, arsym->as_name);
		  continue;
		}

	      printf (gettext ("%s in %s\n"), arsym->as_name, arhdr->ar_name);

	      ++arsym;
	    }

	  if (elf_rand (elf, SARMAG) != SARMAG)
	    {
	      error (0, 0,
		     gettext ("cannot reset archive offset to beginning"));
	      return 1;
	    }
	}
    }

  /* Process all the files contained in the archive.  */
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      /* Skip over the index entries.  */
      if (strcmp (arhdr->ar_name, "/") != 0
	  && strcmp (arhdr->ar_name, "//") != 0)
	{
	  if (elf_kind (subelf) == ELF_K_ELF)
	    result |= handle_elf (subelf, new_prefix, arhdr->ar_name,
				  new_suffix);
	  else if (elf_kind (subelf) == ELF_K_AR)
	    result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name,
				 new_suffix);
	  else
	    {
	      error (0, 0, gettext ("%s%s%s: file format not recognized"),
		     new_prefix, arhdr->ar_name, new_suffix);
	      result = 1;
	    }
	}

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (elf_end (subelf) != 0)
	INTERNAL_ERROR (fname);
    }

  return result;
}


struct local_name
{
  const char *name;
  size_t lineno;
  Dwarf_Addr lowpc;
  Dwarf_Addr highpc;
  char file[0];
};


static int
local_compare (const void *p1, const void *p2)
{
  struct local_name *g1 = (struct local_name *) p1;
  struct local_name *g2 = (struct local_name *) p2;
  int result;

  result = strcmp (g1->name, g2->name);
  if (result == 0)
    {
      if (g1->lowpc <= g2->lowpc && g1->highpc >= g2->highpc)
	{
	  /* g2 is contained in g1.  Update the data.  */
	  g2->lowpc = g1->lowpc;
	  g2->highpc = g1->highpc;
	  result = 0;
	}
      else if (g2->lowpc <= g1->lowpc && g2->highpc >= g1->highpc)
	{
	  /* g1 is contained in g2.  Update the data.  */
	  g1->lowpc = g2->lowpc;
	  g1->highpc = g2->highpc;
	  result = 0;
	}
      else
	result = g1->lowpc < g2->lowpc ? -1 : 1;
    }

  return result;
}


static int
get_var_range (Dwarf_Die die, Dwarf_Unsigned *lowpc, Dwarf_Unsigned *highpc)
{
  Dwarf_Attribute loc;
  Dwarf_Error err;
  Dwarf_Locdesc *locdesc;
  Dwarf_Signed len;

  if (dwarf_attr (die, DW_AT_location, &loc, &err) != DW_DLV_OK)
    return 1;

  if (dwarf_loclist (loc, &locdesc, &len, &err) != DW_DLV_OK)
    return 1;

  /* XXX incomplete.  */
  return 1;
}


static void *
get_local_names (Ebl *ebl, Dwarf_Debug dbg)
{
  /* We iterate over the content of the .debug_info section.  We only
     look at the level immediately below the compile unit DIE.  */
  int ret;
  Dwarf_Error err;
  Dwarf_Unsigned culen;
  Dwarf_Unsigned nextcu;
  Dwarf_Off offset = 0;
  void *root = NULL;

  while ((ret = dwarf_next_cu_header (dbg, &culen, NULL, NULL, NULL, &nextcu,
				      &err)) == DW_DLV_OK)
    {
      Dwarf_Half tag;
      Dwarf_Die die;
      Dwarf_Die old = NULL;
      char **files = NULL;
      Dwarf_Signed nfiles;

      offset += culen;

      if (dwarf_offdie (dbg, offset, &die, &err) == DW_DLV_OK
	  /* This better be a compile unit DIE. */
	  && dwarf_tag (die, &tag, &err) == DW_DLV_OK
	  && tag == DW_TAG_compile_unit
	  /* Get the source files for this compilation unit.  */
	  && dwarf_srcfiles (die, &files, &nfiles, &err) != DW_DLV_ERROR
	  /* Search all immediate children for subprogram and variable
	     DIEs.  */
	  && (old = die, dwarf_child (die, &die, &err) == DW_DLV_OK))
	do
	  {
	    dwarf_dealloc (dbg, old, DW_DLA_DIE);

	    if (dwarf_tag (die, &tag, &err) == DW_DLV_OK
		&& (tag == DW_TAG_subprogram || tag == DW_TAG_variable))
	      {
		/* We are interested in five attributes: name,
		   decl_file, decl_line, low_pc, and high_pc.  */
		Dwarf_Attribute name = NULL;
		Dwarf_Attribute file = NULL;
		Dwarf_Attribute line = NULL;
		char *namestr;
		Dwarf_Unsigned fileidx;
		Dwarf_Unsigned lineno;
		Dwarf_Addr lowpc;
		Dwarf_Addr highpc;

		if (dwarf_attr (die, DW_AT_name, &name, &err) == DW_DLV_OK
		    && dwarf_formstring (name, &namestr, &err) == DW_DLV_OK
		    && (dwarf_attr (die, DW_AT_decl_file, &file, &err)
			== DW_DLV_OK)
		    && dwarf_formudata (file, &fileidx, &err) == DW_DLV_OK
		    && fileidx > 0 && fileidx <= (Dwarf_Unsigned) nfiles
		    && (dwarf_attr (die, DW_AT_decl_line, &line, &err)
			== DW_DLV_OK)
		    && dwarf_formudata (line, &lineno, &err) == DW_DLV_OK
		    && lineno != 0
		    && ((tag = DW_TAG_subprogram
			 && dwarf_lowpc (die, &lowpc, &err) == DW_DLV_OK
			 && dwarf_highpc (die, &highpc, &err) == DW_DLV_OK)
			|| (tag == DW_TAG_variable
			    && get_var_range (die, &lowpc, &highpc) == 0)))
		  {
		    struct local_name *newp;
		    size_t namelen = strlen (namestr) + 1;
		    const char *bfile = basename (files[fileidx - 1]);
		    size_t filelen = strlen (bfile) + 1;

		    newp = xmalloc (sizeof (*newp) + namelen + filelen);
		    newp->name = memcpy (mempcpy (newp->file, bfile, filelen),
					 namestr, namelen);
		    newp->lineno = lineno;
		    newp->lowpc = lowpc;
		    newp->highpc = highpc;

		    /* XXX Return value shouldn't be ignored.  If the
		       new entry is not added to the tree the data
		       structure should be freed.  */
		    tsearch (newp, &root, local_compare);
		  }

		dwarf_dealloc (dbg, name, DW_DLA_ATTR);
		dwarf_dealloc (dbg, file, DW_DLA_ATTR);
		dwarf_dealloc (dbg, line, DW_DLA_ATTR);
	      }

	    old = die;
	  }
	while (dwarf_siblingof (dbg, die, &die, &err) == DW_DLV_OK);

      dwarf_dealloc (dbg, old, DW_DLA_DIE);
      while (nfiles-- > 0)
	dwarf_dealloc (dbg, files[nfiles], DW_DLA_STRING);
      dwarf_dealloc (dbg, files, DW_DLA_LIST);

      offset = nextcu;
    }

  return root;
}


/* Mapping of radix and binary class to length.  */
static const int length_map[2][3] =
{
  [ELFCLASS32 - 1] =
  {
    [radix_hex] = 8,
    [radix_decimal] = 10,
    [radix_octal] = 11
  },
  [ELFCLASS64 - 1] =
  {
    [radix_hex] = 16,
    [radix_decimal] = 20,
    [radix_octal] = 22
  }
};


struct global_name
{
  Dwarf_Global global;
  const char *name;
};


static int
global_compare (const void *p1, const void *p2)
{
  const struct global_name *g1 = (const struct global_name *) p1;
  const struct global_name *g2 = (const struct global_name *) p2;

  return strcmp (g1->name, g2->name);
}


static void
free_global (void *p)
{
  struct global_name *g = (struct global_name *) p;

  free ((char *) g->name);

  free (p);
}


/* Show symbols in SysV format.  */
static void
show_symbols_sysv (Ebl *ebl, GElf_Word strndx,
		   const char *prefix, const char *fname, const char *fullname,
		   GElf_SymX *syms, size_t nsyms, int longest_name)
{
  int digits = length_map[gelf_getclass (ebl->elf) - 1][radix];
  size_t cnt;
  size_t shnum;
  Elf_Scn *scn;
  const char **scnnames;
  bool scnnames_malloced;
  const char *fmtstr;
  size_t shstrndx;
  Dwarf_Debug dbg;
  Dwarf_Global *globals;
  Dwarf_Signed globcnt;
  Dwarf_Error err;
  const char *linenum;
  char linenumbuf[PATH_MAX + 10];
  void *global_root = NULL;
  void *local_root = NULL;

  if (elf_getshnum (ebl->elf, &shnum) < 0)
    INTERNAL_ERROR (fullname);

  scnnames_malloced = shnum * sizeof (const char *) > 128 * 1024;
  if (scnnames_malloced)
    scnnames = (const char **) xmalloc (sizeof (const char *) * shnum);
  else
    scnnames = (const char **) alloca (sizeof (const char *) * shnum);

  /* Get a DWARF debugging descriptor.  It's no problem if this isn't
     possible.  We just won't print any line number information.  */
  if (dwarf_elf_init (ebl->elf, DW_DLC_READ, NULL, NULL, &dbg, &err)
      == DW_DLV_OK)
    {
      if (dwarf_get_globals (dbg, &globals, &globcnt, &err) == DW_DLV_OK)
	{
	  /* We got a list of all the global symbols.  Put them in a
	     search tree for later use.  */
	  cnt = globcnt;	/* For the sake of systems with too large
				   Dwarf_Signed type.  */

	  while (cnt-- > 0)
	    {
	      char *name;
	      struct global_name *newp;

	      if (dwarf_globname (globals[cnt], &name, &err) == DW_DLV_OK
		  && ((newp = (struct global_name *) malloc (sizeof (*newp)))
		      != NULL))
		{
		  newp->global = globals[cnt];
		  newp->name = name;
		  /* XXX Return value shouldn't be ignored.  If the
		     new entry is not added to the tree the data
		     structure should be freed.  */
		  tsearch (newp, &global_root, global_compare);
		}
	    }
	}
      else
	globals = NULL;

      /* Try to get the local symbols which are not in the
	 .debug_pubnames section.  */
      if (! hide_local)
	local_root = get_local_names (ebl, dbg);
    }
  else
    dbg = NULL;

  /* Get the section header string table index.  */
  if (elf_getshstrndx (ebl->elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* Cache the section names.  */
  scn = NULL;
  cnt = 1;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;

      assert (elf_ndxscn (scn) == cnt++);

      scnnames[elf_ndxscn (scn)]
	= elf_strptr (ebl->elf, shstrndx,
		      gelf_getshdr (scn, &shdr_mem)->sh_name);
    }

  /* We always print this prolog.  */
  if (prefix == NULL || 1)
    printf (gettext ("\n\nSymbols from %s:\n\n"), fullname);
  else
    printf (gettext ("\n\nSymbols from %s[%s]:\n\n"), prefix, fname);

  /* The header line.  */
  printf (gettext ("%*s%-*s %-*s Class  Type     %-*s Line          Section\n\n"),
	  print_file_name ? (int) strlen (fullname) + 1: 0, "",
	  longest_name, sgettext ("sysv|Name"),
	  /* TRANS: the "sysv|" parts makes the string unique.  */
	  digits, sgettext ("sysv|Value"),
	  /* TRANS: the "sysv|" parts makes the string unique.  */
	  digits, sgettext ("sysv|Size"));

  /* Which format string to use (different radix for numbers).  */
  if (radix == radix_hex)
    fmtstr = "%-*s|%0*" PRIx64 "|%-6s|%-8s|%*" PRIx64 "|%13s|%s\n";
  else if (radix == radix_decimal)
    fmtstr = "%-*s|%*" PRId64 "|%-6s|%-8s|%*" PRId64 "|%13s|%s\n";
  else
    fmtstr = "%-*s|%0*" PRIo64 "|%-6s|%-8s|%*" PRIo64 "|%13s|%s\n";

  /* Iterate over all symbols.  */
  for (cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (ebl->elf, strndx,
				       syms[cnt].sym.st_name);
      char symbindbuf[50];
      char symtypebuf[50];
      char secnamebuf[1024];

      /* Filter out administrative symbols without a name and those
	 deselected by ther user with command line options.  */
      if ((hide_undefined && syms[cnt].sym.st_shndx == SHN_UNDEF)
	  || (hide_defined && syms[cnt].sym.st_shndx != SHN_UNDEF)
	  || (hide_local && GELF_ST_BIND (syms[cnt].sym.st_info) == STB_LOCAL))
	continue;

      linenum = "";
      if (syms[cnt].sym.st_shndx != SHN_UNDEF
	  && GELF_ST_BIND (syms[cnt].sym.st_info) != STB_LOCAL
	  && global_root != NULL)
	{
	  struct global_name fake;
	  struct global_name **found;
	  Dwarf_Off dieoff;
	  Dwarf_Off cudieoff;
	  Dwarf_Die die = NULL;
	  Dwarf_Die cudie = NULL;

	  fake.name = symstr;
	  found = tfind (&fake, &global_root, global_compare);
	  if (found != NULL
	      && dwarf_global_name_offsets ((*found)->global, NULL, &dieoff,
					    &cudieoff, &err) == DW_DLV_OK
	      && dwarf_offdie (dbg, dieoff, &die, &err) == DW_DLV_OK
	      && dwarf_offdie (dbg, cudieoff, &cudie, &err) == DW_DLV_OK)
	    {
	      Dwarf_Addr lowpc;
	      Dwarf_Line *lines = NULL;
	      Dwarf_Signed nlines;
	      int inner;

	      if (dwarf_srclines (cudie, &lines, &nlines, &err) == DW_DLV_OK
		  && dwarf_lowpc (die, &lowpc, &err) == DW_DLV_OK)
		{
		  Dwarf_Addr addr;
		  Dwarf_Unsigned lineno;
		  char *linesrc;

		  for (inner = 1; inner < nlines; ++inner)
		    if (dwarf_lineaddr (lines[inner], &addr, &err) != DW_DLV_OK
			|| addr > lowpc)
		      break;

		  if (dwarf_lineno (lines[inner - 1], &lineno, &err)
		      == DW_DLV_OK
		      && (dwarf_linesrc (lines[inner - 1], &linesrc, &err)
			  == DW_DLV_OK))
		    {
		      snprintf (linenumbuf, sizeof (linenumbuf),
				"%s:%" PRIu64,
				basename (linesrc), (uint64_t) lineno);
		      linenum = linenumbuf;

		      dwarf_dealloc (dbg, linesrc, DW_DLA_STRING);
		    }
		}

	      if (lines != NULL)
		{
		  for (inner = 0; inner < nlines; ++inner)
		    dwarf_dealloc (dbg, lines[inner], DW_DLA_LINE);
		  dwarf_dealloc (dbg, lines, DW_DLA_LIST);
		}
	    }

	  dwarf_dealloc (dbg, die, DW_DLA_DIE);
	  dwarf_dealloc (dbg, cudie, DW_DLA_DIE);
	}

      if (*linenum == '\0'
	  && *symstr != '\0'
	  && syms[cnt].sym.st_shndx != SHN_UNDEF
	  && local_root != NULL)
	{
	  struct local_name fake;
	  struct local_name **found;

	  fake.name = symstr;
	  fake.lowpc = fake.highpc = syms[cnt].sym.st_value;
	  found = tfind (&fake, &local_root, local_compare);
	  if (found != NULL)
	    {
	      snprintf (linenumbuf, sizeof (linenumbuf), "%s:%" PRIu64,
			(*found)->file, (uint64_t) (*found)->lineno);
	      linenum = linenumbuf;
	    }
	}

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	}

      /* Print the actual string.  */
      printf (fmtstr,
	      longest_name, symstr,
	      digits, syms[cnt].sym.st_value,
	      ebl_symbol_binding_name (ebl,
				       GELF_ST_BIND (syms[cnt].sym.st_info),
				       symbindbuf, sizeof (symbindbuf)),
	      ebl_symbol_type_name (ebl, GELF_ST_TYPE (syms[cnt].sym.st_info),
				    symtypebuf, sizeof (symtypebuf)),
	      digits, syms[cnt].sym.st_size, linenum,
	      ebl_section_name (ebl, syms[cnt].sym.st_shndx, syms[cnt].xndx,
				secnamebuf, sizeof (secnamebuf), scnnames,
				shnum));
    }

  if (dbg != NULL)
    {
      tdestroy (global_root, free_global);
      tdestroy (local_root, free);

      if (globals != NULL)
	{
	  cnt = globcnt;
	  while (cnt-- > 0)
	    dwarf_dealloc (dbg, globals[cnt], DW_DLA_GLOBAL);

	  dwarf_dealloc (dbg, globals, DW_DLA_LIST);
	}

      dwarf_finish (dbg, &err);
    }

  if (scnnames_malloced)
    free (scnnames);
}


static char
class_type_char (GElf_Sym *sym)
{
  int local_p = GELF_ST_BIND (sym->st_info) == STB_LOCAL;
  char result;

  /* XXX Add support for architecture specific types and classes.  */
  if (sym->st_shndx == SHN_ABS)
    return local_p ? 'a' : 'A';

  if (sym->st_shndx == SHN_UNDEF)
    /* Undefined symbols must be global.  */
    return 'U';

  result = "NDTSFB          "[GELF_ST_TYPE (sym->st_info)];

  return local_p ? tolower (result) : result;
}


static void
show_symbols_bsd (Elf *elf, GElf_Ehdr *ehdr, GElf_Word strndx,
		  const char *prefix, const char *fname, const char *fullname,
		  GElf_SymX *syms, size_t nsyms)
{
  int digits = length_map[gelf_getclass (elf) - 1][radix];
  const char *fmtstr;
  const char *sfmtstr;
  const char *ufmtstr;
  size_t cnt;

  if (prefix != NULL && ! print_file_name)
    printf ("\n%s:\n", fname);

  if (radix == radix_hex)
    {
      fmtstr = "%0*" PRIx64 " %c%s %s\n";
      sfmtstr = "%2$0*1$" PRIx64 " %7$0*6$" PRIx64 " %3$c%4$s %5$s\n";
      ufmtstr = "%*s U%s %s\n";
    }
  else if (radix == radix_decimal)
    {
      fmtstr = "%*" PRId64 " %c%s %s\n";
      sfmtstr = "%2$*1$" PRId64 " %7$*6$" PRId64 " %3$c%4$s %5$s\n";
      ufmtstr = "%*s U%s %s\n";
    }
  else
    {
      fmtstr = "%0*" PRIo64 " %c%s %s\n";
      sfmtstr = "%2$0*1$" PRIo64 " %7$0*6$" PRIo64 " %3$c%4$s %5$s\n";
      ufmtstr = "%* U%s %s\n";
    }

  /* Iterate over all symbols.  */
  for (cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (elf, strndx, syms[cnt].sym.st_name);

      /* Printing entries with a zero-length name makes the output
	 not very well parseable.  Since these entries don't carry
	 much information we leave them out.  */
      if (symstr[0] == '\0')
	continue;

      /* Filter out administrative symbols without a name and those
	 deselected by ther user with command line options.  */
      if ((hide_undefined && syms[cnt].sym.st_shndx == SHN_UNDEF)
	  || (hide_defined && syms[cnt].sym.st_shndx != SHN_UNDEF)
	  || (hide_local && GELF_ST_BIND (syms[cnt].sym.st_info) == STB_LOCAL))
	continue;

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	}

      if (syms[cnt].sym.st_shndx == SHN_UNDEF)
	printf (ufmtstr,
		digits, "",
		mark_weak
		? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK
		   ? "*" : " ")
		: "",
		elf_strptr (elf, strndx, syms[cnt].sym.st_name));
      else
	printf (print_size ? sfmtstr : fmtstr,
		digits, syms[cnt].sym.st_value,
		class_type_char (&syms[cnt].sym),
		mark_weak
		? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK
		   ? "*" : " ")
		: "",
		elf_strptr (elf, strndx, syms[cnt].sym.st_name),
		digits, (uint64_t) syms[cnt].sym.st_size);
    }
}


static void
show_symbols_posix (Elf *elf, GElf_Ehdr *ehdr, GElf_Word strndx,
		    const char *prefix, const char *fname,
		    const char *fullname, GElf_SymX *syms, size_t nsyms)
{
  int digits = length_map[gelf_getclass (elf) - 1][radix];
  const char *fmtstr;
  size_t cnt;

  if (prefix != NULL && ! print_file_name)
    printf ("%s:\n", fullname);

  if (radix == radix_hex)
    fmtstr = "%s %c%s %0*" PRIx64 "\n";
  else if (radix == radix_decimal)
    fmtstr = "%s %c%s %*" PRId64 "\n";
  else
    fmtstr = "%s %c%s %0*" PRIo64 "\n";

  /* Iterate over all symbols.  */
  for (cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (elf, strndx, syms[cnt].sym.st_name);

      /* Printing entries with a zero-length name makes the output
	 not very well parseable.  Since these entries don't carry
	 much information we leave them out.  */
      if (symstr[0] == '\0')
	continue;

      /* Filter out administrative symbols without a name and those
	 deselected by ther user with command line options.  */
      if ((hide_undefined && syms[cnt].sym.st_shndx == SHN_UNDEF)
	  || (hide_defined && syms[cnt].sym.st_shndx != SHN_UNDEF)
	  || (hide_local && GELF_ST_BIND (syms[cnt].sym.st_info) == STB_LOCAL))
	continue;

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	  putchar_unlocked (' ');
	}

      printf (fmtstr,
	      symstr,
	      class_type_char (&syms[cnt].sym),
	      mark_weak
	      ? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK ? "*" : " ")
	      : "",
	      digits, syms[cnt].sym.st_value);
    }
}


/* Maximum size of memory we allocate on the stack.  */
#define MAX_STACK_ALLOC	65536

static void
show_symbols (Ebl *ebl, GElf_Ehdr *ehdr, Elf_Scn *scn, Elf_Scn *xndxscn,
	      GElf_Shdr *shdr, const char *prefix, const char *fname,
	      const char *fullname)
{
  size_t shstrndx;
  Elf_Data *data;
  Elf_Data *xndxdata;
  size_t size;
  size_t entsize;
  size_t nentries;
  size_t cnt;
  GElf_SymX *sym_mem;
  size_t longest_name = 4;

  int sort_by_name (const void *p1, const void *p2)
    {
      GElf_SymX *s1 = (GElf_SymX *) p1;
      GElf_SymX *s2 = (GElf_SymX *) p2;
      int result;

      result = strcmp (elf_strptr (ebl->elf, shdr->sh_link, s1->sym.st_name),
		       elf_strptr (ebl->elf, shdr->sh_link, s2->sym.st_name));

      return reverse_sort ? -result : result;
    }

  int sort_by_address (const void *p1, const void *p2)
    {
      GElf_SymX *s1 = (GElf_SymX *) p1;
      GElf_SymX *s2 = (GElf_SymX *) p2;
      int result;

      result = (s1->sym.st_value < s2->sym.st_value
		? -1 : (s1->sym.st_value == s2->sym.st_value ? 0 : 1));

      return reverse_sort ? -result : result;
    }

  /* Get the section header string table index.  */
  if (elf_getshstrndx (ebl->elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* The section is that large.  */
  size = shdr->sh_size;
  /* One entry is this large.  */
  entsize = shdr->sh_entsize;

  /* Consistency checks.  */
  if (entsize != gelf_fsize (ebl->elf, ELF_T_SYM, 1, ehdr->e_version))
    error (0, 0,
	   gettext ("%s: entry size in section `%s' is not what we expect"),
	   fullname, elf_strptr (ebl->elf, shstrndx, shdr->sh_name));
  else if (size % entsize != 0)
    error (0, 0,
	   gettext ("%s: size of section `%s' is not multiple of entry size"),
	   fullname, elf_strptr (ebl->elf, shstrndx, shdr->sh_name));

  /* Compute number of entries.  Handle buggy entsize values.  */
  nentries = size / (entsize ?: 1);

  /* Allocate the memory.

     XXX We can use a dirty trick here.  Since GElf_Sym == Elf64_Sym we
     can use the data memory instead of copying again if what we read
     is a 64 bit file.  */
  if (nentries * sizeof (GElf_SymX) < MAX_STACK_ALLOC)
    sym_mem = (GElf_SymX *) alloca (nentries * sizeof (GElf_SymX));
  else
    sym_mem = (GElf_SymX *) xmalloc (nentries * sizeof (GElf_SymX));

  /* Get the data of the section.  */
  data = elf_getdata (scn, NULL);
  xndxdata = elf_getdata (xndxscn, NULL);
  if (data == NULL || (xndxscn != NULL && xndxdata == NULL))
    INTERNAL_ERROR (fullname);

  /* Iterate over all symbols.  */
  for (cnt = 0; cnt < nentries; ++cnt)
    {
      GElf_Sym *sym = gelf_getsymshndx (data, xndxdata, cnt, &sym_mem[cnt].sym,
					&sym_mem[cnt].xndx);

      if (sym == NULL)
	INTERNAL_ERROR (fullname);

      if (format == format_sysv)
	longest_name = MAX (longest_name,
			    strlen (elf_strptr (ebl->elf, shdr->sh_link,
						sym->st_name)));
    }

  /* Sort the entries according to the users wishes.  */
  if (sort == sort_name)
    qsort (sym_mem, nentries, sizeof (GElf_SymX), sort_by_name);
  else if (sort == sort_numeric)
    qsort (sym_mem, nentries, sizeof (GElf_SymX), sort_by_address);

  /* Finally print according to the users selection.  */
  switch (format)
    {
    case format_sysv:
      show_symbols_sysv (ebl, shdr->sh_link, prefix, fname,
			 fullname, sym_mem, nentries, longest_name);
      break;

    case format_bsd:
      show_symbols_bsd (ebl->elf, ehdr, shdr->sh_link, prefix, fname,
			fullname, sym_mem, nentries);
      break;

    case format_posix:
    default:
      assert (format == format_posix);
      show_symbols_posix (ebl->elf, ehdr, shdr->sh_link, prefix, fname,
			  fullname, sym_mem, nentries);
      break;
    }

  /* Free all memory.  */
  if (nentries * sizeof (GElf_Sym) >= MAX_STACK_ALLOC)
    free (sym_mem);
}


static int
handle_elf (Elf *elf, const char *prefix, const char *fname,
	    const char *suffix)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t suffix_len = suffix == NULL ? 0 : strlen (suffix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len + suffix_len];
  char *cp = fullname;
  Elf_Scn *scn = NULL;
  int any = 0;
  int result = 0;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  Ebl *ebl;

  /* Get the backend for this object file type.  */
  ebl = ebl_openbackend (elf);

  /* We need the ELF header in a few places.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fullname);

  /* If we are asked to print the dynamic symbol table and this is
     executable or dynamic executable, fail.  */
  if (symsec_type == SHT_DYNSYM
      && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    {
      /* XXX Add machine specific object file types.  */
      error (0, 0, gettext ("%s%s%s%s: Invalid operation"),
	     prefix ?: "", prefix ? "(" : "", fname, prefix ? ")" : "");
      result = 1;
      goto out;
    }

  /* Create the full name of the file.  */
  if (prefix != NULL)
    cp = mempcpy (cp, prefix, prefix_len);
  cp = mempcpy (cp, fname, fname_len);
  if (suffix != NULL)
    memcpy (cp - 1, suffix, suffix_len + 1);

  /* Find the symbol table.

     XXX Can there be more than one?  Do we print all?  Currently we do.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      if (shdr->sh_type == symsec_type)
	{
	  Elf_Scn *xndxscn = NULL;

	  /* We have a symbol table.  First make sure we remember this.  */
	  any = 1;

	  /* Look for an extended section index table for this section.  */
	  if (symsec_type == SHT_SYMTAB)
	    {
	      size_t scnndx = elf_ndxscn (scn);

	      while ((xndxscn = elf_nextscn (elf, xndxscn)) != NULL)
		{
		  GElf_Shdr xndxshdr_mem;
		  GElf_Shdr *xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);

		  if (xndxshdr == NULL)
		    INTERNAL_ERROR (fullname);

		  if (xndxshdr->sh_type == SHT_SYMTAB_SHNDX
		      && xndxshdr->sh_link == scnndx)
		    break;
		}
	    }

	  show_symbols (ebl, ehdr, scn, xndxscn, shdr, prefix, fname,
			fullname);
	}
    }

  if (! any)
    {
      error (0, 0, gettext ("%s%s%s: no symbols"),
	     prefix ?: "", prefix ? ":" : "", fname);
      result = 1;
    }

 out:
  /* Close the ELF backend library descriptor.  */
  ebl_closebackend (ebl);

  return result;
}
