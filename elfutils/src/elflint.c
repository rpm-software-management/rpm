/* Pedantic checking of ELF files compliance with gABI/psABI spec.
   Copyright (C) 2001, 2002 Red Hat, Inc.
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
#include <gelf.h>
#include <inttypes.h>
#include <libebl.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <elf-knowledge.h>
#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


#define ARGP_strict 300

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{

  { "strict", ARGP_strict, NULL, 0,
    N_("Be extremely strict, flag level 2 features.") },
  { "quiet", 'q', NULL, 0, N_("Do not print anything if successful") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("\
Pedantic checking of ELF files compliance with gABI/psABI spec.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE...");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help
};


/* Declarations of local functions.  */
static void process_file (int fd, Elf *elf, const char *prefix,
			  const char *fname, bool only_one);
static void process_elf_file (Elf *elf, const char *prefix, const char *fname,
			      bool only_one);

/* True if we should perform very strict testing.  */
static bool be_strict;

/* True if no message is to be printed if the run is succesful.  */
static bool be_quiet;

/* Index of section header string table.  */
static uint32_t shstrndx;

/* Array to count references in section groups.  */
static int *scnref;


int
main (int argc, char *argv[])
{
  int remaining;
  bool only_one;

  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Initialize the message catalog.  */
  textdomain (PACKAGE);

  /* Parse and process arguments.  */
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* If no ELF file is given punt.  */
  if (remaining >= argc)
    {
      argp_help (&argp, stdout, ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR,
		 program_invocation_short_name);
      exit (1);
    }

  /* Before we start tell the ELF library which version we are using.  */
  elf_version (EV_CURRENT);

  /* Now process all the files given at the command line.  */
  only_one = remaining + 1 == argc;
  do
    {
      int fd;
      Elf *elf;

      /* Open the file.  */
      fd = open (argv[remaining], O_RDONLY);
      if (fd == -1)
	{
	  error (0, errno, gettext ("cannot open input file"));
	  continue;
	}

      /* Create an `Elf' descriptor.  */
      elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
      if (elf == NULL)
	error (0, 0, gettext ("cannot generate Elf descriptor: %s\n"),
	       elf_errmsg (-1));
      else
	{
	  int prev_error_message_count = error_message_count;

	  process_file (fd, elf, NULL, argv[remaining], only_one);

	  /* Now we can close the descriptor.  */
	  if (elf_end (elf) != 0)
	    error (0, 0, gettext ("error while closing Elf descriptor: %s"),
		   elf_errmsg (-1));

	  if (prev_error_message_count == error_message_count && !be_quiet)
	    puts (gettext ("No errors"));
	}

      close (fd);
    }
  while (++remaining < argc);

  return error_message_count != 0;
}


static void
no_progname (void)
{
}
void (*error_print_progname) (void) = no_progname;


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case ARGP_strict:
      be_strict = true;
      break;

    case 'q':
      be_quiet = true;
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
      return strdup (gettext ("Report bugs to <drepper@redhat.com>.\n"));
    default:
      break;
    }
  return (char *) text;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "elflint (Red Hat %s) %s\n", PACKAGE, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2002");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Process one file.  */
static void
process_file (int fd, Elf *elf, const char *prefix, const char *fname,
	      bool only_one)
{
  /* We can handle two types of files: ELF files and archives.  */
  Elf_Kind kind = elf_kind (elf);

  switch (kind)
    {
    case ELF_K_ELF:
      /* Yes!  It's an ELF file.  */
      process_elf_file (elf, prefix, fname, only_one);
      break;

    case ELF_K_AR:
      {
	Elf *subelf;
	Elf_Cmd cmd = ELF_C_READ_MMAP;
	size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
	size_t fname_len = strlen (fname) + 1;
	char new_prefix[prefix_len + 1 + fname_len];
	char *cp = new_prefix;

	/* Create the full name of the file.  */
	if (prefix != NULL)
	  {
	    cp = mempcpy (cp, prefix, prefix_len);
	    *cp++ = ':';
	  }
	memcpy (cp, fname, fname_len);

	/* It's an archive.  We process each file in it.  */
	while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
	  {
	    kind = elf_kind (subelf);

	    /* Call this function recursively.  */
	    if (kind == ELF_K_ELF || kind == ELF_K_AR)
	      {
		Elf_Arhdr *arhdr = elf_getarhdr (subelf);
		assert (arhdr != NULL);

		process_file (fd, subelf, new_prefix, arhdr->ar_name, false);
	      }

	    /* Get next archive element.  */
	    cmd = elf_next (subelf);
	    if (elf_end (subelf) != 0)
	      error (0, 0,
		     gettext (" error while freeing sub-ELF descriptor: %s\n"),
		     elf_errmsg (-1));
	  }
      }
      break;

    default:
      /* We cannot do anything.  */
      error (0, 0, gettext ("\
Not an ELF file - it has the wrong magic bytes at the start"));
      break;
    }
}


static const char *
section_name (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;

  shdr = gelf_getshdr (elf_getscn (ebl->elf, idx), &shdr_mem);

  return elf_strptr (ebl->elf, shstrndx, shdr->sh_name);
}


static const int valid_e_machine[] =
  {
    EM_M32, EM_SPARC, EM_386, EM_68K, EM_88K, EM_860, EM_MIPS, EM_S370,
    EM_MIPS_RS3_LE, EM_PARISC, EM_VPP500, EM_SPARC32PLUS, EM_960, EM_PPC,
    EM_PPC64, EM_S390, EM_V800, EM_FR20, EM_RH32, EM_RCE, EM_ARM,
    EM_FAKE_ALPHA, EM_SH, EM_SPARCV9, EM_TRICORE, EM_ARC, EM_H8_300,
    EM_H8_300H, EM_H8S, EM_H8_500, EM_IA_64, EM_MIPS_X, EM_COLDFIRE,
    EM_68HC12, EM_MMA, EM_PCP, EM_NCPU, EM_NDR1, EM_STARCORE, EM_ME16,
    EM_ST100, EM_TINYJ, EM_X86_64, EM_PDSP, EM_FX66, EM_ST9PLUS, EM_ST7,
    EM_68HC16, EM_68HC11, EM_68HC08, EM_68HC05, EM_SVX, EM_AT19, EM_VAX,
    EM_CRIS, EM_JAVELIN, EM_FIREPATH, EM_ZSP, EM_MMIX, EM_HUANY, EM_PRISM,
    EM_AVR, EM_FR30, EM_D10V, EM_D30V, EM_V850, EM_M32R, EM_MN10300,
    EM_MN10200, EM_PJ, EM_OPENRISC, EM_ARC_A5, EM_XTENSA
  };
#define nvalid_e_machine \
  (sizeof (valid_e_machine) / sizeof (valid_e_machine[0]))


/* Number of sections.  */
static int shnum;


static void
check_elf_header (Ebl *ebl, GElf_Ehdr *ehdr)
{
  char buf[512];
  int cnt;

  /* Check e_ident field.  */
  if (ehdr->e_ident[EI_MAG0] != ELFMAG0)
    error (0, 0, gettext ("e_ident[%d] != '%c'"), EI_MAG0, ELFMAG0);
  if (ehdr->e_ident[EI_MAG1] != ELFMAG1)
    error (0, 0, gettext ("e_ident[%d] != '%c'"), EI_MAG1, ELFMAG1);
  if (ehdr->e_ident[EI_MAG2] != ELFMAG2)
    error (0, 0, gettext ("e_ident[%d] != '%c'"), EI_MAG2, ELFMAG2);
  if (ehdr->e_ident[EI_MAG3] != ELFMAG3)
    error (0, 0, gettext ("e_ident[%d] != '%c'"), EI_MAG3, ELFMAG3);

  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32
      && ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    error (0, 0, gettext ("e_ident[%d] == %d is no known class"),
	   EI_CLASS, ehdr->e_ident[EI_CLASS]);

  if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB
      && ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
    error (0, 0, gettext ("e_ident[%d] == %d is no known data encoding"),
	   EI_DATA, ehdr->e_ident[EI_DATA]);

  if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
    error (0, 0,
	   gettext ("unknown ELF header version number e_ident[%d] == %d"),
	   EI_VERSION, ehdr->e_ident[EI_VERSION]);

  /* We currently don't handle any OS ABIs.  */
  if (ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE)
    error (0, 0, gettext ("unsupported OS ABI e_ident[%d] == \"%s\""),
	   EI_OSABI,
	   ebl_osabi_name (ebl, ehdr->e_ident[EI_OSABI], buf, sizeof (buf)));

  /* No ABI versions other than zero supported either.  */
  if (ehdr->e_ident[EI_ABIVERSION] != 0)
    error (0, 0, gettext ("unsupport ABI version e_ident[%d] == %d"),
	   EI_ABIVERSION, ehdr->e_ident[EI_ABIVERSION]);

  for (cnt = EI_PAD; cnt < EI_NIDENT; ++cnt)
    if (ehdr->e_ident[cnt] != 0)
      error (0, 0, gettext ("e_ident[%d] is not zero"), cnt);

  /* Check the e_type field.  */
  if (ehdr->e_type != ET_REL && ehdr->e_type != ET_EXEC
      && ehdr->e_type != ET_DYN && ehdr->e_type != ET_CORE)
    error (0, 0, gettext ("unknown object file type %d"), ehdr->e_type);

  /* Check the e_machine field.  */
  for (cnt = 0; cnt < nvalid_e_machine; ++cnt)
    if (valid_e_machine[cnt] == ehdr->e_machine)
      break;
  if (cnt == nvalid_e_machine)
    error (0, 0, gettext ("unknown machine type %d"), ehdr->e_machine);

  /* Check the e_version field.  */
  if (ehdr->e_version != EV_CURRENT)
    error (0, 0, gettext ("unknown object file version"));

  /* Check the e_phoff and e_phnum fields.  */
  if (ehdr->e_phoff == 0)
    {
      if (ehdr->e_phnum != 0)
	error (0, 0, gettext ("invalid program header offset"));
      else if (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)
	error (0, 0, gettext ("\
executables and DSOs cannot have zero program header offset"));
    }
  else if (ehdr->e_phnum == 0)
    error (0, 0, gettext ("invalid number of program header entries"));

  /* Check the e_shoff field.  */
  shnum = ehdr->e_shnum;
  shstrndx = ehdr->e_shstrndx;
  if (ehdr->e_shoff == 0)
    {
      if (ehdr->e_shnum != 0)
	error (0, 0, gettext ("invalid section header table offset"));
      else if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN
	       && ehdr->e_type != ET_CORE)
	error (0, 0, gettext ("section header table must be present"));
    }
  else
    {
      if (ehdr->e_shnum == 0)
	{
	  /* Get the header of the zeroth section.  The sh_size field
	     might contain the section number.  */
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr;

	  shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
	  if (shdr != NULL)
	    {
	      /* The error will be reported later.  */
	      if (shdr->sh_size == 0)
		error (0, 0, gettext ("\
invalid number of section header table entries"));
	      else
		shnum = shdr->sh_size;
	    }
	}

      if (ehdr->e_shstrndx == SHN_XINDEX)
	{
	  /* Get the header of the zeroth section.  The sh_size field
	     might contain the section number.  */
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr;

	  shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
	  if (shdr != NULL)
	    {
	      /* The error will be reported later.  */
	      if (shdr->sh_link >= shnum)
		error (0, 0, gettext ("invalid section header index"));
	      else
		shstrndx = shdr->sh_link;
	    }
	}
      else if (shstrndx >= shnum)
	error (0, 0, gettext ("invalid section header index"));
    }

  /* Check the e_flags field.  */
  if (!ebl_machine_flag_check (ebl, ehdr->e_flags))
    error (0, 0, gettext ("invalid machine flags: %s"),
	   ebl_machine_flag_name (ebl, ehdr->e_flags, buf, sizeof (buf)));

  /* Check e_ehsize, e_phentsize, and e_shentsize fields.  */
  if (gelf_getclass (ebl->elf) == ELFCLASS32)
    {
      if (ehdr->e_ehsize != 0 && ehdr->e_ehsize != sizeof (Elf32_Ehdr))
	error (0, 0, gettext ("invalid ELF header size: %hd"), ehdr->e_ehsize);

      if (ehdr->e_phentsize != 0 && ehdr->e_phentsize != sizeof (Elf32_Phdr))
	error (0, 0, gettext ("invalid program header size: %hd"),
	       ehdr->e_phentsize);

      if (ehdr->e_shentsize != 0 && ehdr->e_shentsize != sizeof (Elf32_Shdr))
	error (0, 0, gettext ("invalid section header size: %hd"),
	       ehdr->e_shentsize);
    }
  else if (gelf_getclass (ebl->elf) == ELFCLASS64)
    {
      if (ehdr->e_ehsize != 0 && ehdr->e_ehsize != sizeof (Elf64_Ehdr))
	error (0, 0, gettext ("invalid ELF header size: %hd"), ehdr->e_ehsize);

      if (ehdr->e_phentsize != 0 && ehdr->e_phentsize != sizeof (Elf64_Phdr))
	error (0, 0, gettext ("invalid program header size: %hd"),
	       ehdr->e_phentsize);

      if (ehdr->e_shentsize != 0 && ehdr->e_shentsize != sizeof (Elf64_Shdr))
	error (0, 0, gettext ("invalid section header size: %hd"),
	       ehdr->e_shentsize);
    }
}


/* Check that there is a section group section with index < IDX which
   contains section IDX and that there is exactly one.  */
static void
check_scn_group (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  if (scnref[idx] == 0)
    {
      /* No reference so far.  Search following sections, maybe the
	 order is wrong.  */
      int cnt;

      for (cnt = idx + 1; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn;
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr;
	  Elf_Data *data;
	  Elf32_Word *grpdata;
	  int inner;

	  scn = elf_getscn (ebl->elf, cnt);
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    /* We cannot get the section header so we cannot check it.
	       The error to get the section header will be shown
	       somewhere else.  */
	    continue;

	  if (shdr->sh_type != SHT_GROUP)
	    continue;

	  data = elf_getdata (scn, NULL);
	  if (data == NULL || data->d_size < sizeof (Elf32_Word))
	    /* Cannot check the section.  */
	    continue;

	  grpdata = (Elf32_Word *) data->d_buf;
	  for (inner = 1; inner < data->d_size / sizeof (Elf32_Word); ++inner)
	    if (grpdata[inner] == idx)
	      goto out;
	}

    out:
      if (cnt == shnum)
	error (0, 0, gettext ("\
section [%2d] '%s': section with SHF_GROUP flag set not part of a section group"),
	       idx, section_name (ebl, ehdr, idx));
      else
	error (0, 0, gettext ("\
section [%2d] '%s': section group [%2d] '%s' does not preceed group member"),
	       idx, section_name (ebl, ehdr, idx),
	       cnt, section_name (ebl, ehdr, cnt));
    }
}


static void
check_symtab (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr;
  int cnt;
  GElf_Shdr xndxshdr_mem;
  GElf_Shdr *xndxshdr = NULL;
  Elf_Data *xndxdata = NULL;
  Elf32_Word xndxscnidx = 0;
  bool no_xndx_warned = false;
  GElf_Sym sym_mem;
  GElf_Sym *sym;
  Elf32_Word xndx;

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &strshdr_mem);
  if (shdr == NULL || strshdr == NULL)
    return;
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      error (0, 0, gettext ("section [%2d] '%s': cannot get section data"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  if (strshdr->sh_type != SHT_STRTAB)
    error (0, 0, gettext ("section [%2d] '%s': referenced as string table for section [%2d] '%s' but type is not SHT_STRTAB"),
	   shdr->sh_link, section_name (ebl, ehdr, shdr->sh_link),
	   idx, section_name (ebl, ehdr, idx));

  /* Search for an extended section index table section.  */
  for (cnt = 1; cnt < shnum; ++cnt)
    if (cnt != idx)
      {
	Elf_Scn *xndxscn = elf_getscn (ebl->elf, cnt);
	xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);
	xndxdata = elf_getdata (xndxscn, NULL);
	xndxscnidx = elf_ndxscn (xndxscn);

	if (xndxshdr == NULL || xndxdata == NULL)
	  continue;

	if (xndxshdr->sh_type == SHT_SYMTAB_SHNDX
	    && xndxshdr->sh_link == idx)
	  break;
      }
  if (cnt == shnum)
    {
      xndxshdr = NULL;
      xndxdata = NULL;
    }

  if (shdr->sh_entsize != gelf_fsize (ebl->elf, ELF_T_SYM, 1, EV_CURRENT))
    error (0, 0,
	   gettext ("\
section [%2d] '%s': entry size is does not match ElfXX_Sym"),
	   cnt, section_name (ebl, ehdr, cnt));

  /* Test the zeroth entry.  */
  sym = gelf_getsymshndx (data, xndxdata, 0, &sym_mem, &xndx);
  if (sym == NULL)
      error (0, 0, gettext ("section [%2d] '%s': cannot get symbol %d: %s"),
	     idx, section_name (ebl, ehdr, idx), 0, elf_errmsg (-1));
  else
    {
      if (sym->st_name != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_name");
      if (sym->st_value != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_value");
      if (sym->st_size != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_size");
      if (sym->st_info != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_info");
      if (sym->st_other != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_other");
      if (sym->st_shndx != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': '%s' in zeroth entry not zero"),
	       idx, section_name (ebl, ehdr, idx), "st_shndx");
      if (xndxdata != NULL && xndx != 0)
	error (0, 0, gettext ("\
section [%2d] '%s': XINDEX for zeroth entry not zero"),
	       xndxscnidx, section_name (ebl, ehdr, xndxscnidx));
    }

  for (cnt = 1; cnt < shdr->sh_size / shdr->sh_entsize; ++cnt)
    {
      sym = gelf_getsymshndx (data, xndxdata, cnt, &sym_mem, &xndx);
      if (sym == NULL)
	{
	  error (0, 0,
		 gettext ("section [%2d] '%s': cannot get symbol %d: %s"),
		 idx, section_name (ebl, ehdr, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      if (sym->st_name >= strshdr->sh_size)
	error (0, 0,
	       gettext ("section [%2d] '%s': symbol %d: invalid name value"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (sym->st_shndx == SHN_XINDEX)
	{
	  if (xndxdata == NULL)
	    {
	      error (0, 0, gettext ("section [%2d] '%s': symbol %d: too large section index but no extended section index section"),
		     idx, section_name (ebl, ehdr, idx), cnt);
	      no_xndx_warned = true;
	    }
	  else if (xndx < SHN_LORESERVE)
	    error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: XINDEX used for index which would fit in st_shndx (%" PRIu32 ")"),
		   xndxscnidx, section_name (ebl, ehdr, xndxscnidx), cnt,
		   xndx);
	}
      else if ((sym->st_shndx >= SHN_LORESERVE
		&& sym->st_shndx <= SHN_HIRESERVE
		&& sym->st_shndx != SHN_ABS
		&& sym->st_shndx != SHN_COMMON)
	       || (sym->st_shndx >= shnum
		   && (sym->st_shndx < SHN_LORESERVE
		       || sym->st_shndx > SHN_HIRESERVE)))
	error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: invalid section index"),
	       idx, section_name (ebl, ehdr, idx), cnt);
      else
	xndx = sym->st_shndx;

      if (GELF_ST_TYPE (sym->st_info) >= STT_NUM)
	error (0, 0, gettext ("section [%2d] '%s': symbol %d: unknown type"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (GELF_ST_BIND (sym->st_info) >= STB_NUM)
	error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: unknow symbol binding"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (xndx > 0 && xndx < shnum)
	{
	  GElf_Shdr destshdr_mem;
	  GElf_Shdr *destshdr;

	  destshdr = gelf_getshdr (elf_getscn (ebl->elf, xndx), &destshdr_mem);
	  if (destshdr != NULL
	      && (sym->st_value - destshdr->sh_addr) > destshdr->sh_size)
	    error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: st_value out of bounds"),
		   idx, section_name (ebl, ehdr, idx), cnt);
	}

      if (GELF_ST_BIND (sym->st_info) == STB_LOCAL)
	{
	  if (shdr->sh_type == SHT_DYNSYM)
	    error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: dynamic symbol table contain local symbol"),
		   idx, section_name (ebl, ehdr, idx), cnt);
	  else if (cnt >= shdr->sh_info)
	    error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: local symbol outside range described in sh_info"),
		   idx, section_name (ebl, ehdr, idx), cnt);
	}
      else
	{
	  if (cnt < shdr->sh_info)
	    error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: non-local symbol outside range described in sh_info"),
		   idx, section_name (ebl, ehdr, idx), cnt);
	}

      if (GELF_ST_TYPE (sym->st_info) == STT_SECTION
	  && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
	error (0, 0, gettext ("\
section [%2d] '%s': symbol %d: non-local section symbol"),
	       idx, section_name (ebl, ehdr, idx), cnt);
    }
}


static void
check_rela (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr;
  GElf_Shdr destshdr_mem;
  GElf_Shdr *destshdr = NULL;
  int cnt;

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return;
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      error (0, 0, gettext ("section [%2d] '%s': cannot get section data"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  /* Check whether the link to the section we relocate is reasonable.  */
  if (shdr->sh_info >= shnum)
    error (0, 0,
	   gettext ("section [%2d] '%s': invalid destination section index"),
	   idx, section_name (ebl, ehdr, idx));
  else
    {
      destshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_info),
			       &destshdr_mem);
      if (destshdr != NULL
	  && destshdr->sh_type != SHT_PROGBITS
	  && destshdr->sh_type != SHT_NOBITS)
	error (0, 0, gettext ("\
section [%2d] '%s': invalid destination section type"),
	       idx, section_name (ebl, ehdr, idx));
    }

  if (shdr->sh_entsize != gelf_fsize (ebl->elf, ELF_T_RELA, 1, EV_CURRENT))
    error (0, 0, gettext ("\
section [%2d] '%s': section entry size does not match ElfXX_Rela"),
	   idx, section_name (ebl, ehdr, idx));

  symshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &symshdr_mem);

  for (cnt = 0; cnt < shdr->sh_size / shdr->sh_entsize; ++cnt)
    {
      GElf_Rela rela_mem;
      GElf_Rela *rela;

      rela = gelf_getrela (data, cnt, &rela_mem);
      if (rela == NULL)
	{
	  error (0, 0,
		 gettext ("section [%2d] '%s': cannot get relocation %d: %s"),
		 idx, section_name (ebl, ehdr, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      if (!ebl_reloc_type_check (ebl, GELF_R_TYPE (rela->r_info)))
	error (0, 0,
	       gettext ("section [%2d] '%s': relocation %d: invalid type"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (symshdr != NULL
	  && ((GELF_R_SYM (rela->r_info) + 1)
	      * gelf_fsize (ebl->elf, ELF_T_SYM, 1, EV_CURRENT)
	      > symshdr->sh_size))
	error (0, 0, gettext ("\
section [%2d] '%s': relocation %d: invalid symbol index"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (destshdr != NULL
	  && (rela->r_offset - destshdr->sh_addr) >= destshdr->sh_size)
	error (0, 0, gettext ("\
section [%2d] '%s': relocation %d: offset out of bounds"),
	       idx, section_name (ebl, ehdr, idx), cnt);
    }
}


static void
check_rel (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr;
  GElf_Shdr destshdr_mem;
  GElf_Shdr *destshdr = NULL;
  int cnt;

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return;
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      error (0, 0, gettext ("section [%2d] '%s': cannot get section data"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  /* Check whether the link to the section we relocate is reasonable.  */
  if (shdr->sh_info >= shnum)
    error (0, 0,
	   gettext ("section [%2d] '%s': invalid destination section index"),
	   idx, section_name (ebl, ehdr, idx));
  else
    {
      destshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_info),
			       &destshdr_mem);
      if (destshdr != NULL
	  && destshdr->sh_type != SHT_PROGBITS
	  && destshdr->sh_type != SHT_NOBITS)
	error (0, 0, gettext ("\
section [%2d] '%s': invalid destination section type"),
	       idx, section_name (ebl, ehdr, idx));
    }

  if (shdr->sh_entsize != gelf_fsize (ebl->elf, ELF_T_REL, 1, EV_CURRENT))
    error (0, 0, gettext ("\
section [%2d] '%s': section entry size does not match ElfXX_Rel"),
	   idx, section_name (ebl, ehdr, idx));

  symshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &symshdr_mem);

  for (cnt = 0; cnt < shdr->sh_size / shdr->sh_entsize; ++cnt)
    {
      GElf_Rel rel_mem;
      GElf_Rel *rel;

      rel = gelf_getrel (data, cnt, &rel_mem);
      if (rel == NULL)
	{
	  error (0, 0,
		 gettext ("section [%2d] '%s': cannot get relocation %d: %s"),
		 idx, section_name (ebl, ehdr, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      if (!ebl_reloc_type_check (ebl, GELF_R_TYPE (rel->r_info)))
	error (0, 0,
	       gettext ("section [%2d] '%s': relocation %d: invalid type"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (symshdr != NULL
	  && ((GELF_R_SYM (rel->r_info) + 1)
	      * gelf_fsize (ebl->elf, ELF_T_SYM, 1, EV_CURRENT)
	      > symshdr->sh_size))
	error (0, 0, gettext ("\
section [%2d] '%s': relocation %d: invalid symbol index"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (destshdr != NULL
	  && GELF_R_TYPE (rel->r_info) != 0
	  && (rel->r_offset - destshdr->sh_addr) >= destshdr->sh_size)
	error (0, 0, gettext ("\
section [%2d] '%s': relocation %d: offset out of bounds"),
	       idx, section_name (ebl, ehdr, idx), cnt);
    }
}


/* Number of dynamic sections.  */
static int ndynamic;


static void
check_dynamic (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr;
  int cnt;
  static const bool dependencies[DT_NUM][DT_NUM] =
    {
      [DT_NEEDED] = { [DT_STRTAB] = true },
      [DT_PLTRELSZ] = { [DT_JMPREL] = true },
      [DT_HASH] = { [DT_SYMTAB] = true },
      [DT_STRTAB] = { [DT_STRSZ] = true },
      [DT_SYMTAB] = { [DT_STRTAB] = true, [DT_HASH] = true,
		      [DT_SYMENT] = true },
      [DT_RELA] = { [DT_RELASZ] = true, [DT_RELAENT] = true },
      [DT_RELASZ] = { [DT_RELA] = true },
      [DT_RELAENT] = { [DT_RELA] = true },
      [DT_STRSZ] = { [DT_STRTAB] = true },
      [DT_SYMENT] = { [DT_SYMTAB] = true },
      [DT_SONAME] = { [DT_STRTAB] = true },
      [DT_RPATH] = { [DT_STRTAB] = true },
      [DT_REL] = { [DT_RELSZ] = true, [DT_RELENT] = true },
      [DT_RELSZ] = { [DT_REL] = true },
      [DT_RELENT] = { [DT_REL] = true },
      [DT_JMPREL] = { [DT_PLTRELSZ] = true, [DT_PLTREL] = true },
      [DT_RUNPATH] = { [DT_STRTAB] = true },
      [DT_PLTREL] = { [DT_JMPREL] = true },
      [DT_PLTRELSZ] = { [DT_JMPREL] = true }
    };
  bool has_dt[DT_NUM];
  static const bool level2[DT_NUM] =
    {
      [DT_RPATH] = true, [DT_SYMBOLIC] = true, [DT_TEXTREL] = true,
      [DT_BIND_NOW] = true
    };
  GElf_Addr reladdr = 0;
  GElf_Word relsz = 0;
  GElf_Addr pltreladdr = 0;
  GElf_Word pltrelsz = 0;

  memset (has_dt, '\0', sizeof (has_dt));

  if (++ndynamic == 2)
    error (0, 0, gettext ("more than one dynamic section present"));

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return;
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      error (0, 0,
	     gettext ("section [%2d] '%s': cannot get section data"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &strshdr_mem);
  if (strshdr != NULL && strshdr->sh_type != SHT_STRTAB)
    error (0, 0, gettext ("\
section [%2d] '%s': referenced as string table for section [%2d] '%s' but type is not SHT_STRTAB"),
	   shdr->sh_link, section_name (ebl, ehdr, shdr->sh_link),
	   idx, section_name (ebl, ehdr, idx));

  if (shdr->sh_entsize != gelf_fsize (ebl->elf, ELF_T_DYN, 1, EV_CURRENT))
    error (0, 0, gettext ("\
section [%2d] '%s': section entry size does not match ElfXX_Dyn"),
	   idx, section_name (ebl, ehdr, idx));

  if (shdr->sh_info != 0)
    error (0, 0, gettext ("section [%2d] '%s': sh_info not zero"),
	   idx, section_name (ebl, ehdr, idx));

  for (cnt = 0; cnt < shdr->sh_size / shdr->sh_entsize; ++cnt)
    {
      GElf_Dyn dyn_mem;
      GElf_Dyn *dyn;

      dyn = gelf_getdyn (data, cnt, &dyn_mem);
      if (dyn == NULL)
	{
	  error (0, 0, gettext ("\
section [%2d] '%s': cannot get dynamic section entry %d: %s"),
		 idx, section_name (ebl, ehdr, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      if (!ebl_dynamic_tag_check (ebl, dyn->d_tag))
	error (0, 0, gettext ("section [%2d] '%s': entry %d: unknown tag"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (dyn->d_tag < DT_NUM)
	{
	  if (has_dt[dyn->d_tag]
	      && dyn->d_tag != DT_NEEDED
	      && dyn->d_tag != DT_NULL
	      && dyn->d_tag != DT_POSFLAG_1)
	    {
	      char buf[50];
	      error (0, 0, gettext ("\
section [%2d] '%s': entry %d: more than one entry with tag %s"),
		     idx, section_name (ebl, ehdr, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag,
					   buf, sizeof (buf)));
	    }

	  if (be_strict && level2[dyn->d_tag])
	    {
	      char buf[50];
	      error (0, 0, gettext ("\
section [%2d] '%s': entry %d: level 2 tag %s used"),
		     idx, section_name (ebl, ehdr, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag,
					   buf, sizeof (buf)));
	    }

	  has_dt[dyn->d_tag] = true;
	}

      if (dyn->d_tag == DT_PLTREL && dyn->d_un.d_val != DT_REL
	  && dyn->d_un.d_val != DT_RELA)
	error (0, 0, gettext ("\
section [%2d] '%s': entry %d: DT_PLTREL value must be DT_REL or DT_RELA"),
	       idx, section_name (ebl, ehdr, idx), cnt);

      if (dyn->d_tag == DT_REL)
	reladdr = dyn->d_un.d_ptr;
      if (dyn->d_tag == DT_RELSZ)
	relsz = dyn->d_un.d_val;
      if (dyn->d_tag == DT_JMPREL)
	pltreladdr = dyn->d_un.d_ptr;
      if (dyn->d_tag == DT_PLTRELSZ)
	pltrelsz = dyn->d_un.d_val;
    }

  for (cnt = 1; cnt < DT_NUM; ++cnt)
    if (has_dt[cnt])
      {
	int inner;

	for (inner = 0; inner < DT_NUM; ++inner)
	  if (dependencies[cnt][inner] && ! has_dt[inner])
	    {
	      char buf1[50];
	      char buf2[50];

	      error (0, 0, gettext ("\
section [%2d] '%s': contains %s entry but not %s"),
		     idx, section_name (ebl, ehdr, idx),
		     ebl_dynamic_tag_name (ebl, cnt, buf1, sizeof (buf1)),
		     ebl_dynamic_tag_name (ebl, inner, buf2, sizeof (buf2)));
	    }
      }
}


static void
check_symtab_shndx (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr;
  Elf_Scn *symscn;
  int cnt;
  Elf_Data *data;
  Elf_Data *symdata;

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return;

  symscn = elf_getscn (ebl->elf, shdr->sh_link);
  symshdr = gelf_getshdr (symscn, &symshdr_mem);
  if (symshdr != NULL && symshdr->sh_type != SHT_SYMTAB)
    error (0, 0, gettext ("\
section [%2d] '%s': extended section index section not for symbol table"),
	   idx, section_name (ebl, ehdr, idx));
  symdata = elf_getdata (symscn, NULL);
  if (symdata == NULL)
    error (0, 0, gettext ("cannot get data for symbol section"));

  if (shdr->sh_entsize != sizeof (Elf32_Word))
    error (0, 0,
	   gettext ("section [%2d] '%s': entry size does not match Elf32_Word"),
	   idx, section_name (ebl, ehdr, idx));

  if (symshdr != NULL
      && (shdr->sh_size / shdr->sh_entsize
	  < symshdr->sh_size / symshdr->sh_entsize))
    error (0, 0, gettext ("\
section [%2d] '%s': extended index table too small for symbol table"),
	   idx, section_name (ebl, ehdr, idx));

  if (shdr->sh_info != 0)
    error (0, 0, gettext ("section [%2d] '%s': sh_info not zero"),
	   idx, section_name (ebl, ehdr, idx));

  for (cnt = idx + 1; cnt < shnum; ++cnt)
    {
      GElf_Shdr rshdr_mem;
      GElf_Shdr *rshdr;

      rshdr = gelf_getshdr (elf_getscn (ebl->elf, cnt), &rshdr_mem);
      if (rshdr != NULL && rshdr->sh_type == SHT_SYMTAB_SHNDX
	  && rshdr->sh_link == shdr->sh_link)
	{
	  error (0, 0, gettext ("\
section [%2d] '%s': extended section index in section [%2d] '%s' refers to same symbol table"),
		 idx, section_name (ebl, ehdr, idx),
		 cnt, section_name (ebl, ehdr, cnt));
	  break;
	}
    }

  data = elf_getdata (scn, NULL);

  if (*((Elf32_Word *) data->d_buf) != 0)
    error (0, 0, gettext ("symbol 0 should have zero extended section index"));

  for (cnt = 1; cnt < data->d_size / sizeof (Elf32_Word); ++cnt)
    {
      Elf32_Word xndx = ((Elf32_Word *) data->d_buf)[cnt];

      if (xndx != 0)
	{
	  GElf_Sym sym_data;
	  GElf_Sym *sym = gelf_getsym (symdata, cnt, &sym_data);
	  if (sym == NULL)
	    {
	      error (0, 0, gettext ("cannot get data for symbol %d"), cnt);
	      continue;
	    }

	  if (sym->st_shndx != SHN_XINDEX)
	    error (0, 0, gettext ("\
extended section index is %" PRIu32 " but symbol index is not XINDEX"),
		   (uint32_t) xndx);
	}
    }
}


static void
check_hash (Ebl *ebl, GElf_Ehdr *ehdr, int idx)
{
  Elf_Scn *scn;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Data *data;
  Elf32_Word nbucket;
  Elf32_Word nchain;
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr;

  scn = elf_getscn (ebl->elf, idx);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return;
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      error (0, 0, gettext ("section [%2d] '%s': cannot get section data"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  symshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &symshdr_mem);
  if (symshdr != NULL && symshdr->sh_type != SHT_DYNSYM)
    error (0, 0, gettext ("\
section [%2d] '%s': hash table not for dynamic symbol table"),
	   idx, section_name (ebl, ehdr, idx));

  if (shdr->sh_entsize != sizeof (Elf32_Word))
    error (0, 0, gettext ("\
section [%2d] '%s': entry size does not match Elf32_Word"),
	   idx, section_name (ebl, ehdr, idx));

  if ((shdr->sh_flags & SHF_ALLOC) == 0)
    error (0, 0, gettext ("section [%2d] '%s': not marked to be allocated"),
	   idx, section_name (ebl, ehdr, idx));

  if (shdr->sh_size < 2 * shdr->sh_entsize)
    {
      error (0, 0, gettext ("\
section [%2d] '%s': hash table has not even room for nbucket and nchain"),
	     idx, section_name (ebl, ehdr, idx));
      return;
    }

  nbucket = ((Elf32_Word *) data->d_buf)[0];
  nchain = ((Elf32_Word *) data->d_buf)[1];

  if (shdr->sh_size < (2 + nbucket + nchain) * shdr->sh_entsize)
    error (0, 0, gettext ("\
section [%2d] '%s': hash table section is too small (is %ld, expected %ld)"),
	   idx, section_name (ebl, ehdr, idx), (long int) shdr->sh_size,
	   (long int) ((2 + nbucket + nchain) * shdr->sh_entsize));

  if (symshdr != NULL)
    {
      size_t symsize = symshdr->sh_size / symshdr->sh_entsize;
      int cnt;

      if (nchain < symshdr->sh_size / symshdr->sh_entsize)
	error (0, 0,
	       gettext ("section [%2d] '%s': chain array not large enough"),
	       idx, section_name (ebl, ehdr, idx));

      for (cnt = 2; cnt < 2 + nbucket; ++cnt)
	if (((Elf32_Word *) data->d_buf)[cnt] >= symsize)
	  error (0, 0, gettext ("\
section [%2d] '%s': hash bucket reference %d out of bounds"),
		 idx, section_name (ebl, ehdr, idx), cnt - 2);

      for (; cnt < 2 + nbucket + nchain; ++cnt)
	if (((Elf32_Word *) data->d_buf)[cnt] >= symsize)
	  error (0, 0, gettext ("\
section [%2d] '%s': hash chain reference %d out of bounds"),
		 idx, section_name (ebl, ehdr, idx), cnt - 2 - nbucket);
    }
}


static void
check_null (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
#define TEST(name, extra) \
  if (extra && shdr->sh_##name != 0)					      \
    error (0, 0,							      \
	   gettext ("section [%2d] '%s': nonzero sh_%s for NULL section"),    \
	   idx, section_name (ebl, ehdr, idx), #name)

  TEST (name, 1);
  TEST (flags, 1);
  TEST (addr, 1);
  TEST (offset, 1);
  TEST (size, idx != 0);
  TEST (link, idx != 0);
  TEST (info, 1);
  TEST (addralign, 1);
  TEST (entsize, 1);
}


static void
check_group (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr;
  Elf_Data *data;

  /* Check that sh_link is an index of a symbol table.  */
  symshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &symshdr_mem);
  if (symshdr == NULL)
    error (0, 0, gettext ("section [%2d] '%s': cannot get symbol table: %s"),
	   idx, section_name (ebl, ehdr, idx), elf_errmsg (-1));
  else
    {
      if (symshdr->sh_type != SHT_SYMTAB)
	error (0, 0, gettext ("\
section [%2d] '%s': section reference in sh_link is no symbol table"),
	       idx, section_name (ebl, ehdr, idx));

      if (shdr->sh_info >= symshdr->sh_size / gelf_fsize (ebl->elf, ELF_T_SYM,
							  1, EV_CURRENT))
	error (0, 0, gettext ("\
section [%2d] '%s': invalid symbol index in sh_info"),
	       idx, section_name (ebl, ehdr, idx));

      if (shdr->sh_flags != 0)
	error (0, 0, gettext ("section [%2d] '%s': sh_flags not zero"),
	       idx, section_name (ebl, ehdr, idx));

      if (be_strict
	  && shdr->sh_entsize != elf32_fsize (ELF_T_WORD, 1, EV_CURRENT))
	error (0, 0,
	       gettext ("section [%2d] '%s': sh_flags not set correctly"),
	       idx, section_name (ebl, ehdr, idx));
    }

  data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    error (0, 0, gettext ("section [%2d] '%s': cannot get data: %s"),
	   idx, section_name (ebl, ehdr, idx), elf_errmsg (-1));
  else
    {
      size_t elsize = elf32_fsize (ELF_T_WORD, 1, EV_CURRENT);
      size_t cnt;
      Elf32_Word val;

      if (data->d_size % elsize != 0)
	error (0, 0, gettext ("\
section [%2d] '%s': section size not multiple of sizeof(Elf32_Word)"),
	       idx, section_name (ebl, ehdr, idx));

      if (data->d_size < elsize)
	error (0, 0, gettext ("\
section [%2d] '%s': section group without flags word"),
	       idx, section_name (ebl, ehdr, idx));
      else if (be_strict)
	{
	  if (data->d_size < 2 * elsize)
	    error (0, 0, gettext ("\
section [%2d] '%s': section group without member"),
		   idx, section_name (ebl, ehdr, idx));
	  else if (data->d_size < 3 * elsize)
	    error (0, 0, gettext ("\
section [%2d] '%s': section group with only one member"),
		   idx, section_name (ebl, ehdr, idx));
	}

#if ALLOW_UNALIGNED
      val = *((Elf32_Word *) data->d_buf);
#else
      memcpy (&val, data->d_buf, elsize);
#endif
      if ((val & ~GRP_COMDAT) != 0)
	error (0, 0,
	       gettext ("section [%2d] '%s': unknown section group flags"),
	       idx, section_name (ebl, ehdr, idx));

      for (cnt = elsize; cnt < data->d_size; cnt += elsize)
	{
#if ALLOW_UNALIGNED
	  val = *((Elf32_Word *) ((char *) data->d_buf + cnt));
#else
	  memcpy (&val, (char *) data->d_buf + cnt, elsize);
#endif

	  if (val > shnum)
	    error (0, 0, gettext ("\
section [%2d] '%s': section index %Zu out of range"),
		   idx, section_name (ebl, ehdr, idx), cnt / elsize);
	  else
	    {
	      GElf_Shdr refshdr_mem;
	      GElf_Shdr *refshdr;

	      refshdr = gelf_getshdr (elf_getscn (ebl->elf, val),
				      &refshdr_mem);
	      if (refshdr == NULL)
		error (0, 0, gettext ("\
section [%2d] '%s': cannot get section header for element %Zu: %s"),
		       idx, section_name (ebl, ehdr, idx), cnt / elsize,
		       elf_errmsg (-1));
	      else
		{
		  if (refshdr->sh_type == SHT_GROUP)
		    error (0, 0, gettext ("\
section [%2d] '%s': section group contains another group [%2d] '%s'"),
			   idx, section_name (ebl, ehdr, idx),
			   val, section_name (ebl, ehdr, val));

		  if ((refshdr->sh_flags & SHF_GROUP) == 0)
		    error (0, 0, gettext ("\
section [%2d] '%s': element %Zu references section [%2d] '%s' without SHF_GROUP flag set"),
			   idx, section_name (ebl, ehdr, idx), cnt / elsize,
			   val, section_name (ebl, ehdr, val));
		}

	      if (++scnref[val] == 2)
		error (0, 0, gettext ("\
section [%2d] '%s' is contained in more than one section group"),
		       val, section_name (ebl, ehdr, val));
	    }
	}
    }
}


static bool dot_interp_section;


static void
check_sections (Ebl *ebl, GElf_Ehdr *ehdr)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  int cnt;

  /* Allocate array to count references in section groups.  */
  scnref = (int *) xcalloc (shnum, sizeof (int));

  if (ehdr->e_shoff == 0)
    /* No section header.  */
    return;

  /* Check the zeroth section first.  It must not have any contents
     and the section header must contain nonzero value at most in the
     sh_size and sh_link fields.  */
  shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
  if (shdr == NULL)
    error (0, 0, gettext ("cannot get section header of zeroth section"));
  else
    {
      if (shdr->sh_name != 0)
	error (0, 0, gettext ("zeroth section has nonzero name"));
      if (shdr->sh_type != 0)
	error (0, 0, gettext ("zeroth section has nonzero type"));
      if (shdr->sh_flags != 0)
	error (0, 0, gettext ("zeroth section has nonzero flags"));
      if (shdr->sh_addr != 0)
	error (0, 0, gettext ("zeroth section has nonzero address"));
      if (shdr->sh_offset != 0)
	error (0, 0, gettext ("zeroth section has nonzero offset"));
      if (shdr->sh_info != 0)
	error (0, 0, gettext ("zeroth section has nonzero info field"));
      if (shdr->sh_addralign != 0)
	error (0, 0, gettext ("zeroth section has nonzero align value"));
      if (shdr->sh_entsize != 0)
	error (0, 0, gettext ("zeroth section has nonzero entry size value"));

      if (shdr->sh_size != 0 && ehdr->e_shnum != 0)
	error (0, 0, gettext ("zeroth section has nonzero size value while ELF header has nonzero shnum value"));

      if (shdr->sh_link != 0 && ehdr->e_shstrndx != SHN_XINDEX)
	error (0, 0, gettext ("zeroth section has nonzero link value while ELF header does not signal overflow in shstrndx"));
    }

  for (cnt = 1; cnt < shnum; ++cnt)
    {
      Elf_Scn *scn;

      scn = elf_getscn (ebl->elf, cnt);
      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  error (0, 0, gettext ("\
cannot get section header for section [%2d] '%s': %s"),
		 cnt, section_name (ebl, ehdr, cnt), elf_errmsg (-1));
	  continue;
	}

      if (strcmp (elf_strptr (ebl->elf, shstrndx, shdr->sh_name), ".interp")
	  == 0)
	dot_interp_section = true;

      if (shdr->sh_entsize != 0 && shdr->sh_size % shdr->sh_entsize)
	error (0, 0, gettext ("\
section [%2d] '%s': size not multiple of entry size"),
	       cnt, section_name (ebl, ehdr, cnt));

      if (elf_strptr (ebl->elf, shstrndx, shdr->sh_name) == NULL)
	error (0, 0, gettext ("cannot get section header"));

      if (shdr->sh_type >= SHT_NUM
	  && shdr->sh_type != SHT_GNU_LIBLIST
	  && shdr->sh_type != SHT_CHECKSUM
	  && shdr->sh_type != SHT_GNU_verdef
	  && shdr->sh_type != SHT_GNU_verneed
	  && shdr->sh_type != SHT_GNU_versym)
	error (0, 0, gettext ("unsupported section type %d"), shdr->sh_type);

      if (shdr->sh_flags & ~(SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR
			     | SHF_MERGE | SHF_STRINGS | SHF_INFO_LINK
			     | SHF_LINK_ORDER | SHF_OS_NONCONFORMING
			     | SHF_GROUP | SHF_TLS))
	error (0, 0, gettext ("section [%2d] '%s' contain unknown flag"),
	       cnt, section_name (ebl, ehdr, cnt));
      else if (shdr->sh_flags & SHF_TLS)
	error (0, 0, gettext ("\
section [%2d] '%s': thread-local data sections not yet supported"),
	       cnt, section_name (ebl, ehdr, cnt));

      if (shdr->sh_link >= shnum)
	error (0, 0, gettext ("\
section [%2d] '%s': invalid section reference in link value"),
	       cnt, section_name (ebl, ehdr, cnt));

      if (SH_INFO_LINK_P (shdr) && shdr->sh_info >= shnum)
	error (0, 0, gettext ("\
section [%2d] '%s': invalid section reference in info value"),
	       cnt, section_name (ebl, ehdr, cnt));

      if ((shdr->sh_flags & SHF_MERGE) == 0
	  && (shdr->sh_flags & SHF_STRINGS) != 0
	  && be_strict)
	error (0, 0, gettext ("\
section [%2d] '%s': strings flag set without merge flag"),
	       cnt, section_name (ebl, ehdr, cnt));

      if ((shdr->sh_flags & SHF_MERGE) != 0 && shdr->sh_entsize == 0)
	error (0, 0, gettext ("\
section [%2d] '%s': merge flag set but entry size is zero"),
	       cnt, section_name (ebl, ehdr, cnt));

      if (shdr->sh_flags & SHF_GROUP)
	check_scn_group (ebl, ehdr, cnt);

      if (cnt == shstrndx && shdr->sh_type != SHT_STRTAB)
	error (0, 0, gettext ("section [%2d] '%s': ELF header says this is the section header string table but type is not SHT_TYPE"),
	       cnt, section_name (ebl, ehdr, cnt));

      switch (shdr->sh_type)
	{
	case SHT_SYMTAB:
	case SHT_DYNSYM:
	  check_symtab (ebl, ehdr, cnt);
	  break;

	case SHT_RELA:
	  check_rela (ebl, ehdr, cnt);
	  break;

	case SHT_REL:
	  check_rel (ebl, ehdr, cnt);
	  break;

	case SHT_DYNAMIC:
	  check_dynamic (ebl, ehdr, cnt);
	  break;

	case SHT_SYMTAB_SHNDX:
	  check_symtab_shndx (ebl, ehdr, cnt);
	  break;

	case SHT_HASH:
	  check_hash (ebl, ehdr, cnt);
	  break;

	case SHT_NULL:
	  check_null (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_GROUP:
	  check_group (ebl, ehdr, shdr, cnt);
	  break;

	default:
	  /* Nothing.  */
	  break;
	}
    }
}


static void
check_program_header (Ebl *ebl, GElf_Ehdr *ehdr)
{
  int cnt;

  if (ehdr->e_phoff == 0)
    {
      /* No program header.  */
      if (dot_interp_section)
	error (0, 0,
	       gettext (".interp section present but no program header"));

      return;
    }

  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    error (0, 0, gettext ("\
only executables and shared objects can have program headers"));

  for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr;

      phdr = gelf_getphdr (ebl->elf, cnt, &phdr_mem);
      if (phdr == NULL)
	{
	  error (0, 0, gettext ("cannot get program header entry %d: %s"),
		 cnt, elf_errmsg (-1));
	  continue;
	}

      if (phdr->p_type >= PT_NUM)
	error (0, 0, gettext ("\
program header entry %d: unknown program header entry type"),
	       cnt);

      if (phdr->p_type == PT_INTERP && !dot_interp_section)
	error (0, 0,
	       gettext ("INTERP program header entry but no .interp section"));
    }
}


/* Process one file.  */
static void
process_elf_file (Elf *elf, const char *prefix, const char *fname,
		  bool only_one)
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  Ebl *ebl;

  /* Print the file name.  */
  if (!only_one)
    {
      if (prefix != NULL)
	printf ("\n%s(%s):\n", prefix, fname);
      else
	printf ("\n%s:\n", fname);
    }

  if (ehdr == NULL)
    {
      error (0, 0, gettext ("cannot read ELF header: %s"), elf_errmsg (-1));
      return;
    }

  ebl = ebl_openbackend (elf);
  /* If there is no appropriate backend library we cannot test
     architecture and OS specific features.  Any encountered extension
     is an error.  */

  /* Go straight by the gABI, check all the parts in turn.  */
  check_elf_header (ebl, ehdr);

  /* Next the section headers.  It is OK if there are no section
     headers at all.  */
  check_sections (ebl, ehdr);

  /* Finally check the program header.  */
  check_program_header (ebl, ehdr);
}
