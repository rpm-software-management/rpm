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

#include <argp.h>
#include <assert.h>
#include <error.h>
#include <fcntl.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <system.h>
#include "ld.h"


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the various options.  */
enum
  {
    ARGP_whole_archive = 300,
    ARGP_no_whole_archive,
    ARGP_static,
    ARGP_dynamic,
    ARGP_pagesize,
    ARGP_rpath,
    ARGP_rpath_link,
    ARGP_runpath,
    ARGP_runpath_link,
    ARGP_version_script,
    ARGP_gc_sections,
    ARGP_no_gc_sections,
  };


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  /* XXX This list will be reordered and section names will be added.
     Just not right now.  */
  { "whole-archive", ARGP_whole_archive, NULL, 0,
    N_("Include whole archives in the output from now on.") },
  { "no-whole-archive", ARGP_no_whole_archive, NULL, 0,
    N_("Stop including the whole arhives in the output.") },

  { "output", 'o', N_("FILE"), 0, N_("Place output in FILE.") },

  { NULL, 'O', N_("LEVEL"), OPTION_ARG_OPTIONAL,
    N_("Set optimization level to LEVEL.") },

  { "verbose", 'v', NULL, 0, N_("Verbose messages.") },
  { "trace", 't', NULL, 0, N_("Trace file opens.") },

  { NULL, 'z', "KEYWORD", OPTION_HIDDEN, NULL },
  { "-z nodefaultlib", '\0', NULL, OPTION_DOC,
    N_("Object is marked to not use default search path at runtime.") },
  { "-z allextract", '\0', NULL, OPTION_DOC,
    N_("Same as --whole-archive.") },
  { "-z defaultextract", '\0', NULL, OPTION_DOC, N_("\
Default rules of extracting from archive; weak references are not enough.") },
  { "-z weakextract", '\0', NULL, OPTION_DOC,
    N_("Weak references cause extraction from archive.") },
  { "-z muldefs", '\0', NULL, OPTION_DOC,
    N_("Allow multiple definitions; first is used.") },
  { "-z defs | nodefs", '\0', NULL, OPTION_DOC,
    N_("Disallow/allow undefined symbols in DSOs.") },
  { "-z origin", '\0', NULL, OPTION_DOC,
    N_("Object requires immediate handling of $ORIGIN.") },
  { "-z now", '\0', NULL, OPTION_DOC,
    N_("Relocation will not be processed lazily.") },
  { "-z nodelete", '\0', NULL, OPTION_DOC,
    N_("Object cannot be unloaded at runtime.") },
  { "-z initfirst", '\0', NULL, OPTION_DOC,
    N_("Mark object to be initialized first.") },
  { "-z lazyload | nolazyload", '\0', NULL, OPTION_DOC,
    N_("Enable/disable lazy-loading flag for following dependencies.") },
  { "-z nodlopen", '\0', NULL, OPTION_DOC,
    N_("Mark object as not loadable with 'dlopen'.") },
  { "-z ignore | record", '\0', NULL, OPTION_DOC,
    N_("Ignore/record dependencies on unused DSOs.") },
  { "-z combreloc | nocombreloc", '\0', NULL, OPTION_DOC,
    N_("Do/do not combine relocation sections.") },
  { "-z systemlibrary", '\0', NULL, OPTION_DOC,
    N_("Generated DSO will be a system library.") },

  { NULL, 'l', N_("FILE"), OPTION_HIDDEN, NULL },

  { NULL, '(', NULL, 0, N_("Start a group.") },
  { NULL, ')', NULL, 0, N_("End a group.") },

  { NULL, 'L', N_("PATH"), 0,
    N_("Add PATH to list of directories files are searched in.") },

  { NULL, 'c', N_("FILE"), 0, N_("Use linker script in FILE.") },

  { "entry", 'e', N_("ADDRESS"), 0, N_("Set entry point address.") },

  { "static", ARGP_static, NULL, OPTION_HIDDEN, NULL },
  { "-B static", ARGP_static, NULL, OPTION_DOC,
    N_("Do not link against shared libraries.") },
  { "dynamic", ARGP_dynamic, NULL, OPTION_HIDDEN, NULL },
  { "-B dynamic", ARGP_dynamic, NULL, OPTION_DOC,
    N_("Prefer linking against shared libraries.") },

  { "export-dynamic", 'E', NULL, 0, N_("Export all dynamic symbols.") },

  { "strip-all", 's', NULL, 0, N_("Strip all symbols.") },
  { "strip-debug", 'S', NULL, 0, N_("Strip debugging symbols.") },

  { "pagesize", ARGP_pagesize, "SIZE", 0,
    N_("Assume pagesize for the target system to be SIZE.") },

  { "rpath", ARGP_rpath, "PATH", OPTION_HIDDEN, NULL },
  { "rpath-link", ARGP_rpath_link, "PATH", OPTION_HIDDEN, NULL },

  { "runpath", ARGP_runpath, "PATH", 0, N_("Set runtime DSO search path.") },
  { "runpath-link", ARGP_runpath_link, "PATH", 0,
    N_("Set link time DSO search path.") },

  { NULL, 'i', NULL, 0, N_("Ignore LD_LIBRARY_PATH environment variable.") },

  { "version-script", ARGP_version_script, "FILE", 0,
    N_("Read version information from FILE.") },

  { "emulation", 'm', "NAME", 0, N_("Set emulation to NAME.") },

  { "shared", 'G', NULL, 0, N_("Generate dynamic shared object.") },
  { NULL, 'r', NULL, 0L, N_("Generate relocatable object.") },

  { NULL, 'B', "KEYWORD", OPTION_HIDDEN, "" },
  { "-B local", 'B', NULL, OPTION_DOC,
    N_("Causes symbol not assigned to a version be reduced to local.") },

  { "gc-sections", ARGP_gc_sections, NULL, 0, N_("Remove unused sections.") },
  { "no-gc-sections", ARGP_no_gc_sections, NULL, 0,
    N_("Don't remove unused sections.") },

  { "soname", 'h', "NAME", 0, N_("Set soname of shared object.") },
  { "dynamic-linker", 'I', "NAME", 0, N_("Set the dynamic linker name.") },

  { NULL, 'Q', "YN", OPTION_HIDDEN, NULL },
  { "-Q y | n", 'Q', NULL, OPTION_DOC,
    N_("Add/suppress addition indentifying link-editor to .comment section") },

  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("Combine object and archive files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE]...");

/* Prototype for option handler.  */
static error_t parse_opt_1st (int key, char *arg, struct argp_state *state);
static error_t parse_opt_2nd (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp_1st =
{
  options, parse_opt_1st, args_doc, doc
};
static struct argp argp_2nd =
{
  options, parse_opt_2nd, args_doc, doc
};


/* Linker state.  This contains all global information.  */
static struct ld_state ld_state;

/* List of the input files.  */
static struct file_list
{
  const char *name;
  struct file_list *next;
} *input_file_list;

/* If nonzero be verbose.  */
int verbose;

/* The emulation name to use.  */
static const char *emulation;

/* Keep track of the nesting level.  Even though we don't handle nested
   groups we still keep track to improve the error messages.  */
static int group_level;

/* The last file we processed.  */
static struct usedfiles *last_file;

/* The default linker script.  */
/* XXX We'll do this a bit different in the real solution.  */
static const char *linker_script = SRCDIR "/elf32-i386.script";

/* Nonzero if an error occurred while loading the input files.  */
static int error_loading;


/* Intermediate storage for the LD_LIBRARY_PATH information from the
   environment.  */
static char *ld_library_path1;

/* Flag used to communicate with the scanner.  */
int ld_scan_version_script;


/* Prototypes for local functions.  */
static void parse_z_option (const char *arg);
static void parse_z_option_2 (const char *arg);
static void parse_B_option (const char *arg);
static void parse_B_option_2 (const char *arg);
static void determine_output_format (void);
static void load_needed (void);
static void collect_sections (struct ld_state *statep);
static void add_rxxpath (struct pathelement **pathp, const char *str);
static void gen_rxxpath_data (struct ld_state *statep);
static void read_version_script (const char *fname, struct ld_state *statep);


int
main (int argc, char *argv[])
{
  int remaining;
  int err;

#ifndef NDEBUG
  /* Enable memory debugging.  */
  mtrace ();
#endif

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

  /* Before we start tell the ELF library which version we are using.  */
  elf_version (EV_CURRENT);

  /* The user can use the LD_LIBRARY_PATH environment variable to add
     additional lookup directories.  */
  ld_library_path1 = getenv ("LD_LIBRARY_PATH");

  /* One quick pass over the parameters which allows us to scan for options
     with global effect which influence the rest of the processing.  */
  argp_parse (&argp_1st, argc, argv, ARGP_IN_ORDER, &remaining, NULL);

  /* We need at least one input file.  */
  if (input_file_list == NULL)
    {
      error (0, 0, gettext ("At least one input file needed"));
      argp_help (&argp_1st, stderr, ARGP_HELP_SEE, "ld");
      exit (EXIT_FAILURE);
    }

  /* Determine which ELF backend to use.  */
  determine_output_format ();

  /* Prepare state.  */
  err = ld_prepare_state (&ld_state, emulation);
  if (err != 0)
    error (EXIT_FAILURE, 0, gettext ("error while preparing linking"));

  /* XXX Read the linker script now.  Since we later will have the linker
     script built in we don't go into trouble to make sure we handle GROUP
     statements in the script.  This simply must not happen.  */
  ldin = fopen (linker_script, "r");
  if (ldin == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot open linker script \"%s\""),
	   linker_script);
  ld_state.srcfiles = NULL;
  ldlineno = 1;
  ld_scan_version_script = 0;
  if (ldparse (&ld_state) != 0)
    /* Something went wrong during parsing.  */
    exit (EXIT_FAILURE);
  fclose (ldin);

  /* We now might have a list of directories to look for libraries in
     named by the linker script.  Put them in a different list so that
     they are searched after all paths given by the user on the
     command line.  */
  ld_state.default_paths = ld_state.paths;
  ld_state.paths = ld_state.tailpaths = NULL;

  /* Get runpath/rpath information in usable form.  */
  gen_rxxpath_data (&ld_state);

  /* Parse and process arguments for real.  */
  argp_parse (&argp_2nd, argc, argv, ARGP_IN_ORDER, &remaining, NULL);
  /* All options should have been processed by the argp parser.  */
  assert (remaining == argc);

  /* Process the last file.  */
  while (last_file != NULL)
    /* Try to open the file.  */
    error_loading |= FILE_PROCESS (-1, last_file, &ld_state, &last_file);

  /* Stop if there has been a problem while reading the input files.  */
  if (error_loading)
    exit (error_loading);

  /* See whether all opened -( were closed.  */
  if (group_level > 0)
    {
      error (0, 0, gettext ("-( without matching -)"));
      argp_help (&argp_1st, stderr, ARGP_HELP_SEE, "ld");
      exit (EXIT_FAILURE);
    }

  /* When we create a relocatable file we don't have to look for the
     DT_NEEDED DSOs and we also don't test for undefined symbols.  */
  if (ld_state.file_type != relocatable_file_type)
    {
      /* At this point we have loaded all the direct dependencies.  What
	 remains to be done is find the indirect dependencies.  These are
	 DSOs which are referenced by the DT_NEEDED entries in the DSOs
	 which are direct dependencies.  We have to transitively find and
	 load all these dependencies.  */
      load_needed ();

      /* Now that we have loaded all the object files we can determine
	 whether we have any non-weak unresolved references left.  If
	 there are any we stop.  If the user used the '-z nodefs' option
	 and we are creating a DSO don't perform the tests.  */
      if ((ld_state.file_type != dso_file_type || !ld_state.nodefs)
	  && FLAG_UNRESOLVED (&ld_state) != 0)
	exit (1);
    }

  /* Collect information about the relocations which will be carried
     forward into the output.  We have to do this here and now since
     we need to know which sections have to be created.  */
  if (ld_state.file_type != relocatable_file_type)
    {
      void *p ;
      struct scnhead *h;

      p = NULL;
      while ((h = ld_section_tab_iterate (&ld_state.section_tab, &p)) != NULL)
	if (h->type == SHT_REL || h->type == SHT_RELA)
	  {
	    struct scninfo *runp = h->last;
	    do
	      {
		/* If we are processing the relocations determine how
		   many will be in the output file.  Also determine
		   how many GOT entries are needed.  */
		COUNT_RELOCATIONS (&ld_state, runp);

		ld_state.relsize_total += runp->relsize;
	      }
	    while ((runp = runp->next) != h->last);
	  }
    }

  /* Create the sections which are generated by the linker and are not
     present in the input file.  */
  GENERATE_SECTIONS (&ld_state);

  /* We are ready to start working on the output file.  Not all
     information has been gather or created yet.  This will be done as
     we go.  Open the file now.  */
  if (OPEN_OUTFILE (&ld_state, ELFCLASSNONE, ELFDATANONE) != 0)
    exit (1);

  /* At this point we have read all the files and know all the
     sections which have to be linked into the application.  We do now
     create an array listing all the sections.  We will than pass this
     array to a system specific function which can reorder it at will.
     The functions can also merge sections if this is what is
     wanted.  */
  collect_sections (&ld_state);

  /* Create the output sections now.  This may requires sorting them
     first.  */
  CREATE_SECTIONS (&ld_state);

  /* Create the output file data.  Appropriate code for the selected
     output file type is called.  */
  if (CREATE_OUTFILE (&ld_state) != 0)
    exit (1);

  /* Finalize the output file, write the data out.  */
  err |= FINALIZE (&ld_state);

  return err;
}


/* Quick scan of the parameter list for options with global effect.  */
static error_t
parse_opt_1st (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'B':
      parse_B_option (arg);
      break;

    case 'c':
      linker_script = arg;
      break;

    case 'E':
      ld_state.export_all_dynamic = true;
      break;

    case 'G':
      if (ld_state.file_type != no_file_type)
	error (EXIT_FAILURE, 0,
	       gettext ("only one option of -G and -r is allowed"));
      ld_state.file_type = dso_file_type;

      /* If we generate a DSO we have to export all symbols.  */
      ld_state.export_all_dynamic = true;
      break;

    case 'h':
      ld_state.soname = arg;
      break;

    case 'i':
      /* Discard the LD_LIBRARY_PATH value we found.  */
      ld_library_path1 = NULL;
      break;

    case 'I':
      ld_state.interp = arg;
      break;

    case 'm':
      if (emulation != NULL)
	error (EXIT_FAILURE, 0, gettext ("more than one '-m' parameter"));
      emulation = arg;
      break;

    case 'Q':
      if (arg[1] == '\0' && (arg[0] == 'y' || arg[0] == 'Y'))
	ld_state.add_ld_comment = true;
      else if (arg[1] == '\0' && (arg[0] == 'n' || arg[0] == 'N'))
	ld_state.add_ld_comment = true;
      else
	error (EXIT_FAILURE, 0, gettext ("unknown option `-%c %s'"), 'Q', arg);
      break;

    case 'r':
      if (ld_state.file_type != no_file_type)
	error (EXIT_FAILURE, 0,
	       gettext ("only one option of -G and -r is allowed"));
      ld_state.file_type = relocatable_file_type;
      break;

    case 'S':
      ld_state.strip = strip_debug;
      break;

    case 't':
      ld_state.trace_files = true;
      break;

    case 'v':
      verbose = 1;
      break;

    case 'z':
      /* The SysV linker used 'z' to pass various flags to the linker.
	 We follow this.  See 'parse_z_option' for the options we
	 recognize.  */
      parse_z_option (arg);
      break;

    case ARGP_pagesize:
      {
	char *endp;
	ld_state.pagesize = strtoul (arg, &endp, 0);
	if (*endp != '\0')
	  {
	    if (endp[1] == '\0' && tolower (*endp) == 'k')
	      ld_state.pagesize *= 1024;
	    else if (endp[1] == '\0' && tolower (*endp) == 'm')
	      ld_state.pagesize *= 1024 * 1024;
	    else
	      {
		error (0, 0,
		       gettext ("invalid page size value \"%s\": ignored"),
		       arg);
		ld_state.pagesize = 0;
	      }
	  }
      }
      break;

    case ARGP_rpath:
      add_rxxpath (&ld_state.rpath, arg);
      break;

    case ARGP_rpath_link:
      add_rxxpath (&ld_state.rpath_link, arg);
      break;

    case ARGP_runpath:
      add_rxxpath (&ld_state.runpath, arg);
      break;

    case ARGP_runpath_link:
      add_rxxpath (&ld_state.runpath_link, arg);
      break;

    case ARGP_gc_sections:
    case ARGP_no_gc_sections:
      ld_state.gc_sections = key == ARGP_gc_sections;
      break;

    case 's':
      if (arg == NULL)
	{
	  if (ld_state.strip == strip_all)
	    ld_state.strip = strip_everything;
	  else
	    ld_state.strip = strip_all;
	  break;
	}
      /* FALLTHROUGH */

    case 'e':
    case 'o':
    case 'O':
    case ARGP_whole_archive:
    case ARGP_no_whole_archive:
    case 'L':
    case '(':
    case ')':
    case 'l':
    case ARGP_static:
    case ARGP_dynamic:
    case ARGP_version_script:
      /* We'll handle these in the second pass.  */
      break;

    case ARGP_KEY_ARG:
      {
	struct file_list *newp;

	newp = (struct file_list *) xmalloc (sizeof (struct file_list));
	newp->name = arg;
	if (input_file_list == NULL)
	  input_file_list = newp->next = newp;
	else
	  {
	    newp->next = input_file_list->next;
	    input_file_list = input_file_list->next = newp;
	  }
      }
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


/* Handle program arguments for real.  */
static error_t
parse_opt_2nd (int key, char *arg, struct argp_state *state)
{
  static bool group_start_requested;
  static bool group_end_requested;

  switch (key)
    {
    case 'B':
      parse_B_option_2 (arg);
      break;

    case 'e':
      ld_state.entry = arg;
      break;

    case 'o':
      if (ld_state.outfname != NULL)
	{
	  error (0, 0, gettext ("More than one output file name given."));
	see_help:
	  argp_help (&argp_2nd, stderr, ARGP_HELP_SEE, "ld");
	  exit (EXIT_FAILURE);
	}
      ld_state.outfname = arg;
      break;

    case 'O':
      if (arg == NULL)
	ld_state.optlevel = 1;
      else
	{
	  char *endp;
	  unsigned long int level = strtoul (arg, &endp, 10);
	  if (*endp != '\0')
	    {
	      error (0, 0, gettext ("Invalid optimization level `%s'"), arg);
	      goto see_help;
	    }
	  ld_state.optlevel = level;
	}
      break;

    case ARGP_whole_archive:
      ld_state.extract_rule = allextract;
      break;
    case ARGP_no_whole_archive:
      ld_state.extract_rule = defaultextract;
      break;

    case ARGP_static:
    case ARGP_dynamic:
      /* Enable/disable use for DSOs.  */
      ld_state.statically = key == ARGP_static;
      break;

    case 'z':
      /* The SysV linker used 'z' to pass various flags to the linker.
	 We follow this.  See 'parse_z_option' for the options we
	 recognize.  */
      parse_z_option_2 (arg);
      break;

    case ARGP_version_script:
      read_version_script (arg, &ld_state);
      break;

    case 'L':
      /* Add a new search directory.  */
      ld_new_searchdir (arg, &ld_state);
      break;

    case '(':
      /* Start a link group.  We have to be able to determine the object
	 file which is named next.  Do this by remembering a pointer to
	 the pointer which will point to the next object.  */
      if (verbose && (group_start_requested || !group_end_requested))
	error (0, 0, gettext ("nested -( -) groups are not allowed"));

      /* Increment the nesting level.  */
      ++group_level;

      /* Record group start.  */
      group_start_requested = true;
      group_end_requested = false;
      break;

    case ')':
      /* End a link group.  If there is no group open this is clearly
	 a bug.  If there is a group open insert a back reference
	 pointer in the record for the last object of the group.  If
	 there is no new object or just one don't do anything.  */
      if (!group_end_requested)
	{
	  if (group_level == 0)
	    {
	      error (0, 0, gettext ("-) without matching -("));
	      goto see_help;
	    }
	}
      else
	last_file->group_end = true;

      if (group_level > 0)
	--group_level;
      break;

    case 'l':
    case ARGP_KEY_ARG:
      {
	while (last_file != NULL)
	  /* Try to open the file.  */
	  error_loading |= FILE_PROCESS (-1, last_file, &ld_state, &last_file);

	last_file = ld_new_inputfile (arg,
				      key == 'l'
				      ? archive_file_type
				      : relocatable_file_type,
				      &ld_state);
	if (group_start_requested)
	  {
	    last_file->group_start = true;

	    group_start_requested = false;
	    group_end_requested = true;
	  }
      }
      break;

    default:
      /* We can catch all other options here.  They either have
	 already been handled or, if the parameter was not correct,
	 the error has been reported.  */
      break;
    }
  return 0;
}


/* Load all the DSOs named as dependencies in other DSOs we already
   loaded.  */
static void
load_needed (void)
{
  struct usedfiles *first;
  struct usedfiles *runp;

  /* XXX There is one problem here: do we allow references from
     regular object files to be satisfied by these implicit
     dependencies?  The old linker allows this and several libraries
     depend on this.  Solaris' linker does not allow this; it provides
     the user with a comprehensive error message explaining the
     situation.

     XXX IMO the old ld behavior is correct since this is also how the
     dynamic linker will work.  It will look for unresolved references
     in all loaded DSOs.

     XXX Should we add an option to get Solaris compatibility?  */
  if (ld_state.needed == NULL)
    return;

  runp = first = ld_state.needed->next;
  do
    {
      struct usedfiles *ignore;
      struct usedfiles *next = runp->next;
      int err;

      err = FILE_PROCESS (-1, runp, &ld_state, &ignore);
      if (err != 0)
	/* Something went wrong.  */
	exit (err);

      runp = next;
    }
  while (runp != first);
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "ld (Red Hat %s) %s\n", PACKAGE, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2002");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* There are a lot of -z options, parse them here.  Some of them have
   to be parsed in the first pass, others must be handled in the
   second pass.  */
static void
parse_z_option (const char *arg)
{
  if (strcmp (arg, "nodefaultlib") == 0
      /* This is only meaningful if we create a DSO.  */
      && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NODEFLIB;
  else if (strcmp (arg, "muldefs") == 0)
    ld_state.muldefs = true;
  else if (strcmp (arg, "nodefs") == 0)
    ld_state.nodefs = true;
  else if (strcmp (arg, "defs") == 0)
    ld_state.nodefs = false;
  else if (strcmp (arg, "now") == 0)
    /* We could also set the DF_1_NOW flag in DT_FLAGS_1 but this isn't
       necessary.  */
    ld_state.dt_flags |= DF_BIND_NOW;
  else if (strcmp (arg, "origin") == 0)
    /* We could also set the DF_1_ORIGIN flag in DT_FLAGS_1 but this isn't
       necessary.  */
    ld_state.dt_flags |= DF_ORIGIN;
  else if (strcmp (arg, "nodelete") == 0
	   /* This is only meaningful if we create a DSO.  */
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NODELETE;
  else if (strcmp (arg, "initfirst") == 0)
    ld_state.dt_flags_1 |= DF_1_INITFIRST;
  else if (strcmp (arg, "nodlopen") == 0
	   /* This is only meaningful if we create a DSO.  */
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NOOPEN;
  else if (strcmp (arg, "ignore") == 0)
    ld_state.ignore_unused_dsos = true;
  else if (strcmp (arg, "record") == 0)
    ld_state.ignore_unused_dsos = false;
  else if (strcmp (arg, "combreloc") == 0)
    ld_state.combreloc = true;
  else if (strcmp (arg, "nocombreloc") == 0)
    ld_state.combreloc = false;
  else if (strcmp (arg, "systemlibrary") == 0)
    ld_state.is_system_library = true;
  else if (strcmp (arg, "allextract") != 0
	   && strcmp (arg, "defaultextract") != 0
	   && strcmp (arg, "weakextract") != 0
	   && strcmp (arg, "lazyload") != 0
	   && strcmp (arg, "nolazyload") != 0)
    error (0, 0, gettext ("unknown option `-%c %s'"), 'z', arg);
}


static void
parse_z_option_2 (const char *arg)
{
  if (strcmp (arg, "allextract") == 0)
    ld_state.extract_rule = allextract;
  else if (strcmp (arg, "defaultextract") == 0)
    ld_state.extract_rule = defaultextract;
  else if (strcmp (arg, "weakextract") == 0)
    ld_state.extract_rule = weakextract;
  else if (strcmp (arg, "lazyload") == 0)
    ld_state.lazyload = true;
  else if (strcmp (arg, "nolazyload") == 0)
    ld_state.lazyload = false;
}


/* There are a lot of -B options, parse them here.  */
static void
parse_B_option (const char *arg)
{
  if (strcmp (arg, "local") == 0)
    ld_state.default_bind_local = true;
  else if (strcmp (arg, "symbolic") != 0
	   && strcmp (arg, "static") != 0
	   && strcmp (arg, "dynamic") != 0)
    error (0, 0, gettext ("unknown option '-%c %s'"), 'B', arg);
}


/* The same functionality, but called in the second pass over the
   parameters.  */
static void
parse_B_option_2 (const char *arg)
{
  if (strcmp (arg, "static") == 0)
    ld_state.statically = true;
  else if (strcmp (arg, "dynamic") == 0)
    ld_state.statically = false;
  else if (strcmp (arg, "symbolic") == 0
	   /* This is only meaningful if we create a DSO.  */
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags |= DF_SYMBOLIC;
}


static void
determine_output_format (void)
{
  /* First change the 'input_file_list' variable in a simple
     single-linked list.  */
  struct file_list *last = input_file_list;
  input_file_list = input_file_list->next;
  last->next = NULL;

  /* Determine the target configuration which we are supposed to use.
     The user can use the '-m' option to select one.  If this is
     missing we are trying to load one file and determine the
     architecture from that.  */
  if (emulation != NULL)
    {
      ld_state.ebl = ebl_openbackend_emulation (emulation);

      assert (ld_state.ebl != NULL);
    }
  else
    {
      /* Find an ELF input file and let it determine the ELf backend.  */
      struct file_list *runp = input_file_list;

      while (runp != NULL)
	{
	  int fd = open (runp->name, O_RDONLY);
	  if (fd != -1)
	    {
	      int try (Elf *elf)
		{
		  int result = 0;

		  if (elf == NULL)
		    return 0;

		  if (elf_kind (elf) == ELF_K_ELF)
		    {
		      /* We have an ELF file.  We now can find out
			 what the output format should be.  */
		      GElf_Ehdr ehdr_mem;
		      GElf_Ehdr *ehdr;

		      /* Get the ELF header of the object.  */
		      ehdr = gelf_getehdr (elf, &ehdr_mem);
		      if (ehdr != NULL)
			ld_state.ebl =
			  ebl_openbackend_machine (ehdr->e_machine);

		      result = 1;
		    }
		  else if (elf_kind (elf) == ELF_K_AR)
		    {
		      /* Try the archive members.  This could
			 potentially lead to wrong results if the
			 archive contains files for more than one
			 architecture.  But this is the user's
			 problem.  */
		      Elf *subelf;
		      Elf_Cmd cmd = ELF_C_READ_MMAP;

		      while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
			{
			  cmd = elf_next (subelf);

			  if (try (subelf) != 0)
			    break;
			}
		    }

		  elf_end (elf);

		  return result;
		}

	      if (try (elf_begin (fd, ELF_C_READ_MMAP, NULL)) != 0)
		/* Found a file.  */
		break;
	    }

	  runp = runp->next;
	}

      if (ld_state.ebl == NULL)
	{
	  error (0, 0, gettext ("\
could not find input file to determine output file format"));
	  error (EXIT_FAILURE, 0, gettext ("\
try again with an appropriate '-m' parameter"));
	}
    }

  /* We don't need the list of input files anymore.  The second run over
     the parameters will handle them.  */
  while (input_file_list != NULL)
    {
      struct file_list *oldp = input_file_list;
      input_file_list = input_file_list->next;
      free (oldp);
    }

  /* We also know now what kind of file we are supposed to create.  If
     the user hasn't selected anythign we create and executable.  */
  if (ld_state.file_type == no_file_type)
    ld_state.file_type = executable_file_type;
}

/* Add DIR to the list of directories searched for object files and
   libraries.  */
void
ld_new_searchdir (const char *dir, struct ld_state *statep)
{
  struct pathelement *newpath;

  newpath = (struct pathelement *) xcalloc (1, sizeof (struct pathelement));

  newpath->pname = dir;

  /* Enqueue the file.  */
  if (statep->tailpaths == NULL)
    statep->paths = statep->tailpaths = newpath;
  else
    {
      statep->tailpaths->next = newpath;
      statep->tailpaths = newpath;
    }
}


struct usedfiles *
ld_new_inputfile (const char *fname, enum file_type type,
		  struct ld_state *statep)
{
  struct usedfiles *newfile;

  newfile = (struct usedfiles *) xcalloc (1, sizeof (struct usedfiles));

  newfile->soname = newfile->fname = newfile->rfname = fname;
  newfile->file_type = type;
  newfile->extract_rule = statep->extract_rule;
  newfile->lazyload = statep->lazyload;
  newfile->status = not_opened;

  return newfile;
}


/* Create an array listing all the sections.  We will than pass this
   array to a system specific function which can reorder it at will.
   The functions can also merge sections if this is what is
   wanted.  */
static void
collect_sections (struct ld_state *statep)
{
  void *p ;
  struct scnhead *h;
  size_t cnt;

  /* We have that many sections.  At least for now.  */
  ld_state.nallsections = ld_state.section_tab.filled;

  /* Allocate the array.  */
  ld_state.allsections =
    (struct scnhead **) xmalloc (ld_state.nallsections
				 * sizeof (struct scnhead *));

  /* Fill the array.  We rely here on the hash table iterator to
     return the entries in the order they were added.  */
  cnt = 0;
  p = NULL;
  while ((h = ld_section_tab_iterate (&ld_state.section_tab, &p)) != NULL)
    {
      struct scninfo *runp;
      bool used = false;

      if (h->kind == scn_normal)
	{
	  runp = h->last;
	  do
	    {
	      if (h->type == SHT_REL || h->type == SHT_RELA)
		{
		  if (runp->used)
		    /* This is a relocation section.  If the section
		       it is relocating is used in the result so must
		       the relocation section.  */
		    runp->used
		      = runp->fileinfo->scninfo[runp->shdr.sh_info].used;
		}

	      /* Accumulate the result.  */
	      used |= runp->used;

	      /* Next input section.  */
	      runp = runp->next;
	    }
	  while (runp != h->last);

	  h->used = used;
	}

      ld_state.allsections[cnt++] = h;
    }
  ld_state.nusedsections = cnt;

  assert (cnt == ld_state.nallsections);
}


/* Add given path to the end of list.  */
static void
add_rxxpath (struct pathelement **pathp, const char *str)
{
  struct pathelement *newp;

  newp = (struct pathelement *) xmalloc (sizeof (*newp));
  newp->pname = str;
  newp->exist = 0;

  if (*pathp == NULL)
    *pathp = newp->next = newp;
  else
    {
      newp->next = (*pathp)->next;
      *pathp = (*pathp)->next = newp;
    }
}


/* Convert lists of possibly colon-separated directory lists into lists
   where each entry is for a single directory.  */
static void
normalize_dirlist (struct pathelement **pathp)
{
  struct pathelement *firstp = *pathp;

  do
    {
      const char *pname = (*pathp)->pname;
      const char *colonp = strchrnul (pname, ':');

      if (colonp != NULL)
	{
	  struct pathelement *lastp = *pathp;
	  struct pathelement *newp;

	  while (1)
	    {
	      if (colonp == pname)
		lastp->pname = ".";
	      else
		lastp->pname = xstrndup (pname, colonp - pname);

	      if (*colonp == '\0')
		break;
	      pname = colonp + 1;

	      newp = (struct pathelement *) xmalloc (sizeof (*newp));
	      newp->next = lastp->next;
	      lastp = lastp->next = newp;

	      colonp = strchrnul (pname, ':');
	    }

	  pathp = &lastp->next;
	}
      else
	pathp = &(*pathp)->next;
    }
  while (*pathp != firstp);
}


/* Called after all parameters are parsed to bring the runpath/rpath
   information into a usable form.  */
static void
gen_rxxpath_data (struct ld_state *statep)
{
  char *ld_library_path2;

  /* Convert the information in true single-linked lists for easy use.
     At this point we also discard the rpath information if runpath
     information is provided.  rpath is deprecated and should not be
     used (or ever be invented for that matter).  */
  if (statep->rpath != NULL)
    {
      struct pathelement *endp = statep->rpath;
      statep->rpath = statep->rpath->next;
      endp->next = NULL;
    }
  if (statep->rpath_link != NULL)
    {
      struct pathelement *endp = statep->rpath_link;
      statep->rpath_link = statep->rpath_link->next;
      endp->next = NULL;
    }

  if (statep->runpath != NULL)
    {
      struct pathelement *endp = statep->runpath;
      statep->runpath = statep->runpath->next;
      endp->next = NULL;

      /* If rpath information is also available discard it.
	 XXX Should there be a possibility to avoid this?  */
      while (statep->rpath != NULL)
	{
	  struct pathelement *old = statep->rpath;
	  statep->rpath = statep->rpath->next;
	  free (old);
	}
    }
  if (statep->runpath_link != NULL)
    {
      struct pathelement *endp = statep->runpath_link;
      statep->runpath_link = statep->runpath_link->next;
      endp->next = NULL;

      /* If rpath information is also available discard it.
	 XXX Should there be a possibility to avoid this?  */
      while (statep->rpath_link != NULL)
	{
	  struct pathelement *old = statep->rpath_link;
	  statep->rpath_link = statep->rpath_link->next;
	  free (old);
	}

      /* The information in the strings in the list can actually be
	 directory lists themselves, with entries separated by colons.
	 Convert the list now to a list with one list entry for each
	 directory.  */
      normalize_dirlist (&statep->runpath_link);
    }
  else if (statep->rpath_link != NULL)
    /* Same as for the runpath_link above.  */
    normalize_dirlist (&statep->rpath_link);


  /* As a related task, handle the LD_LIBRARY_PATH value here.  First
     we have to possibly split the value found (if it contains a
     semicolon).  Then we have to split the value in list of
     directories, i.e., split at the colons.  */
  if (ld_library_path1 != NULL)
    {
      ld_library_path2 = strchr (ld_library_path1, ';');
      if (ld_library_path2 == NULL)
	{
	  /* If no semicolon is present the directories are looked at
	     after the -L parameters (-> ld_library_path2).  */
	  ld_library_path2 = ld_library_path1;
	  ld_library_path1 = NULL;
	}
      else
	{
	  /* NUL terminate the first part.  */
	  *ld_library_path2++ = '\0';

	  /* Convert the string value in a list.  */
	  add_rxxpath (&statep->ld_library_path1, ld_library_path1);
	  normalize_dirlist (&statep->ld_library_path1);
	}

      add_rxxpath (&statep->ld_library_path2, ld_library_path2);
      normalize_dirlist (&statep->ld_library_path2);
    }
}


static void
read_version_script (const char *fname, struct ld_state *statep)
{
  /* Open the file.  The name is supposed to be the complete (relative
     or absolute) path.  No search along a path will be performed.  */
  ldin = fopen (fname, "r");
  if (ldin == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot read version script \"%s\""),
	   fname);

  /* Tell the parser that this is a version script.  */
  ld_scan_version_script = 1;

  ldlineno = 1;
  if (ldparse (&ld_state) != 0)
    /* Something went wrong during parsing.  */
    exit (EXIT_FAILURE);

  fclose (ldin);
}
