/* Needed for libelf */
#define _FILE_OFFSET_BITS 64

#include "system.h"

#include <elf.h>
#include <libelf.h>
#include <gelf.h>
#include <popt.h>

#include "sections.h"
#include "utils.h"

#include "debug.h"

char *output_dir = NULL;
int keep_strtab = 0;
int keep_all_section_headers = 1;
int add_unstrip_info = 0;

#if defined(NhUNUSED)
static void
copy_to_file(Elf *elf, Elf *out_elf)
{
  GElf_Ehdr ehdr;
  GElf_Phdr phdr;
  Elf_Scn *section, *out_section;
  GElf_Shdr section_header;
  Elf_Data *data, *out_data;
  int i;

  elf_flagelf (out_elf, ELF_C_SET, ELF_F_LAYOUT);
  
  gelf_getehdr (elf, &ehdr);

  /* copy elf header: */
  gelf_newehdr(out_elf, ehdr.e_ident[EI_CLASS]);
  ehdr.e_phnum = 0;
  gelf_update_ehdr (out_elf, &ehdr);

  section = NULL;
  while ((section = elf_nextscn(elf, section)) != NULL)
    {
      out_section = elf_newscn(out_elf);

      /* Copy section header */
      gelf_getshdr (section, &section_header);
      gelf_update_shdr(out_section, &section_header);

      /* Copy data */

      data = NULL;
      while ((data = elf_rawdata (section, data)))
	{
	  out_data = elf_newdata (out_section);

	  out_data->d_buf = data->d_buf;
	  out_data->d_type = data->d_type;
	  out_data->d_size = data->d_size;
	  out_data->d_off = data->d_off;
	  out_data->d_align = section_header.sh_addralign;
	  out_data->d_version = data->d_version;
	}
    }
}
#endif

static void
strip_to_file(Elf *elf, Elf *out_elf, DebugLink *debuglink)
{
  GElf_Ehdr ehdr;
  GElf_Ehdr out_ehdr;
  GElf_Phdr phdr;
  Elf_Scn *section, *out_section;
  GElf_Shdr section_header;
  Elf_Data *data, *out_data;
  unsigned char *section_strtab;
  size_t *section_map;
  int keep_section;
  int changed_offsets;
  GElf_Off last_offset;
  int i;
  int debuglink_name = 0;

  elf_flagelf (out_elf, ELF_C_SET, ELF_F_LAYOUT);
  
  gelf_getehdr (elf, &ehdr);

  /* copy elf header: */
  gelf_newehdr(out_elf, ehdr.e_ident[EI_CLASS]);
  gelf_update_ehdr(out_elf, &ehdr);

  section_map = calloc(ehdr.e_shnum, sizeof (size_t));
  
  /* Locate section header strtab */
  section = elf_getscn(elf, ehdr.e_shstrndx);
  data = elf_getdata(section, NULL);
  section_strtab = data->d_buf;

  /* Copy program headers: */
  gelf_newphdr(out_elf, ehdr.e_phnum);

  for (i = 0; i < ehdr.e_phnum; i++)
    {
      gelf_getphdr (elf, i, &phdr);
      gelf_update_phdr (out_elf, i, &phdr);
    }

  /* Copy section headers */
  changed_offsets = 0;
  last_offset = 0;
  section = NULL;
  while ((section = elf_nextscn(elf, section)) != NULL)
    {
      char *section_name;

      gelf_getshdr (section, &section_header);

      section_name = section_strtab + section_header.sh_name;
      
      keep_section =
	!string_has_prefix (section_name, ".stab") &&
	!string_has_prefix (section_name, ".debug") &&
	(keep_strtab || 
	 (!keep_strtab &&
	  !string_has_prefix (section_name, ".symtab") &&
	  !string_has_prefix (section_name, ".strtab")));

      if (keep_section)
	{
	  out_section = elf_newscn(out_elf);
      
	  section_map[elf_ndxscn(section)] = elf_ndxscn(out_section);

	  /* Update offset if necessary */
	  if (changed_offsets)
	    section_header.sh_offset = align_up (last_offset, section_header.sh_addralign);

	  /* Copy data */
	  data = NULL;
	  out_data = NULL;
	  while ((data = elf_rawdata (section, data)))
	    {
	      out_data = elf_newdata(out_section);

	      /* Add ".debuglink" to section header strtab */
	      if (ehdr.e_shstrndx == elf_ndxscn(section))
		{
		  out_data->d_size = data->d_size + strlen (DEBUGLINKNAME) + 1;
		  out_data->d_buf = malloc (out_data->d_size);
		  memcpy (out_data->d_buf, data->d_buf, data->d_size);
		  strcpy (out_data->d_buf + data->d_size, DEBUGLINKNAME);

		  section_header.sh_size = MAX (section_header.sh_size, out_data->d_off + out_data->d_size);
		  changed_offsets = 1;
		  debuglink_name = data->d_size;
		}
	      else
		{
		  out_data->d_buf = data->d_buf;
		  out_data->d_size = data->d_size;
		}
	      out_data->d_off = data->d_off;
	      out_data->d_type = data->d_type;
	      out_data->d_align = section_header.sh_addralign;
	      out_data->d_version = data->d_version;
	    }

	  last_offset = section_header.sh_offset + section_header.sh_size;
	  /* Write section header */
	  gelf_update_shdr(out_section, &section_header);
	}
      else
	changed_offsets = 1;
      
    }

  /* Add debuglink section header */
  out_section = elf_newscn(out_elf);
  section_header.sh_name = debuglink_name;
  section_header.sh_type = SHT_PROGBITS;
  section_header.sh_flags = 0;
  section_header.sh_addr = 0;
  section_header.sh_addralign = 4;
  section_header.sh_offset = align_up (last_offset, section_header.sh_addralign);
  section_header.sh_size = 0;
  section_header.sh_link = 0;
  section_header.sh_info = 0;
  section_header.sh_entsize = 0;

  out_data = elf_newdata(out_section);
  debug_link_to_data (debuglink, elf, out_data);
  
  section_header.sh_size = out_data->d_size;
  
  last_offset = section_header.sh_offset + section_header.sh_size;
  gelf_update_shdr(out_section, &section_header);

  /* Update section header stringtab ref */
  gelf_getehdr (out_elf, &out_ehdr);
  out_ehdr.e_shstrndx = section_map[out_ehdr.e_shstrndx];
  out_ehdr.e_shoff = align_up (last_offset, 8);
  gelf_update_ehdr(out_elf, &out_ehdr);

  /* Update section header links */
  out_section = NULL;
  while ((out_section = elf_nextscn(out_elf, out_section)) != NULL)
    {
      gelf_getshdr (out_section, &section_header);
      
      section_header.sh_link = section_map[section_header.sh_link];

      if (section_header.sh_type == SHT_REL ||
	  section_header.sh_type == SHT_RELA)
	section_header.sh_info = section_map[section_header.sh_info];
      
      gelf_update_shdr(out_section, &section_header);
    }
}

static void
copy_debuginfo_to_file(Elf *elf, Elf *out_elf)
{
  GElf_Ehdr ehdr;
  Elf_Scn *section, *out_section;
  GElf_Shdr section_header;
  GElf_Shdr out_section_header;
  Elf_Data *data, *out_data;
  GElf_Phdr phdr;
  unsigned char *section_strtab;
  int keep_section;
  UnstripInfo *info;
  int unstripinfo_name = 0;
  int i;

  info = malloc (sizeof (UnstripInfo));
  
  if (gelf_getehdr (elf, &ehdr) == NULL)
    {
      fprintf (stderr, "Not an elf binary, exiting\n");
      exit (1);
    }

  gelf_newehdr(out_elf, ehdr.e_ident[EI_CLASS]);

  /* copy elf header: */
  gelf_update_ehdr(out_elf, &ehdr);

  info->orig_e_shoff = ehdr.e_shoff;
  info->n_sections = ehdr.e_shnum;
  info->sections = calloc (info->n_sections, sizeof (UnstripInfoSection));
  
  /* Locate section header strtab */
  section = elf_getscn(elf, ehdr.e_shstrndx);
  data = elf_getdata(section, NULL);
  section_strtab = data->d_buf;
  
  /* Copy section headers */
  section = NULL;
  while ((section = elf_nextscn(elf, section)) != NULL)
    {
      char *section_name;
      size_t section_index;
      GElf_Off last_offset;
      gelf_getshdr (section, &section_header);

      section_index = elf_ndxscn(section);
      info->sections[section_index].name = section_header.sh_name;
      info->sections[section_index].orig_offset = section_header.sh_offset;
      info->sections[section_index].debug_section = 0;
      
      section_name = section_strtab + section_header.sh_name;
      
      keep_section =
	string_has_prefix (section_name, ".stab") ||
	string_has_prefix (section_name, ".debug") ||
	string_has_prefix (section_name, ".symtab") ||
	string_has_prefix (section_name, ".strtab") ||
	section_index == ehdr.e_shstrndx;

      if (keep_section)
	{
	  out_section = elf_newscn(out_elf);

	  info->sections[section_index].debug_section = elf_ndxscn(out_section);
	  
	  memset (&out_section_header, 0, sizeof(out_section_header));
	  out_section_header.sh_name = section_header.sh_name;
	  out_section_header.sh_type = section_header.sh_type;
	  out_section_header.sh_flags = section_header.sh_flags;
	  out_section_header.sh_addr = section_header.sh_addr;
	  out_section_header.sh_offset = section_header.sh_offset;
	  out_section_header.sh_size = section_header.sh_size;
	  out_section_header.sh_link = section_header.sh_link;
	  out_section_header.sh_info = section_header.sh_info;
	  out_section_header.sh_addralign = section_header.sh_addralign;
	  out_section_header.sh_entsize = section_header.sh_entsize;
	  gelf_update_shdr(out_section, &out_section_header);
	  
	  /* Copy data */
	  
	  data = NULL;
	  last_offset = 0;
	  while ((data = elf_rawdata (section, data)))
	    {
	      out_data = elf_newdata(out_section);
	      
	      if (ehdr.e_shstrndx == elf_ndxscn(section))
		{
		  out_data->d_size = data->d_size + strlen (UNSTRIPINFONAME) + 1;
		  out_data->d_buf = malloc (out_data->d_size);
		  memcpy (out_data->d_buf, data->d_buf, data->d_size);
		  strcpy (out_data->d_buf + data->d_size, UNSTRIPINFONAME);

		  unstripinfo_name = data->d_size;
		}
	      else
		{
		  out_data->d_buf = data->d_buf;
		  out_data->d_size = data->d_size;
		}
	      out_data->d_off = data->d_off;
	      out_data->d_type = data->d_type;
	      out_data->d_align = section_header.sh_addralign;
	      out_data->d_version = data->d_version;
	      
	    }
	}
      else if (keep_all_section_headers)
	{
	  out_section = elf_newscn(out_elf);
	  
	  info->sections[section_index].debug_section = 0;

	  section_header.sh_type = SHT_NOBITS;
	  gelf_update_shdr(out_section, &section_header);
	  
	  if ((data = elf_rawdata (section, data)))
	    {
	      out_data = elf_newdata(out_section);

	      out_data->d_buf = NULL;
	      out_data->d_size = data->d_size;
	      out_data->d_off = data->d_off;
	      out_data->d_type = data->d_type;
	      out_data->d_align = section_header.sh_addralign;
	      out_data->d_version = data->d_version;
	    }
	}

    }

  /* Add unlinkinfo section header */
  if (add_unstrip_info)
    {
      out_section = elf_newscn(out_elf);
      section_header.sh_name = unstripinfo_name;
      section_header.sh_type = SHT_PROGBITS;
      section_header.sh_flags = 0;
      section_header.sh_addr = 0;
      section_header.sh_addralign = 4;
      section_header.sh_link = 0;
      section_header.sh_info = 0;
      section_header.sh_entsize = 0;

      out_data = elf_newdata(out_section);
      unstrip_info_to_data (info, elf, out_data);
  
      gelf_update_shdr(out_section, &section_header);
    }
  
  /* Update section header stringtab ref */
  gelf_getehdr (out_elf, &ehdr);
  ehdr.e_shstrndx = info->sections[ehdr.e_shstrndx].debug_section;
  gelf_update_ehdr(out_elf, &ehdr);
  
  /* Update section header links */
  out_section = NULL;
  while ((out_section = elf_nextscn(out_elf, out_section)) != NULL)
    {
      gelf_getshdr (out_section, &out_section_header);
      out_section_header.sh_link = info->sections[out_section_header.sh_link].debug_section;
      gelf_update_shdr(out_section, &out_section_header);
    }

}

static struct poptOption optionsTable[] = {
    { "output-dir",  'o', POPT_ARG_STRING, &output_dir, 0,
      "directory to store result", "/usr/lib/debug" },
    { "strip-debug", 'g', POPT_ARG_NONE, &keep_strtab, 0,
      "Remove debugging symbols only, keep symbols", 0 },
    { "unstrip-info", 'u', POPT_ARG_NONE, &add_unstrip_info, 0,
      "Add unstripping information to the debug file", 0 },
      POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0 }
};

int
main (int argc, char *argv[])
{
  Elf *elf, *out_elf;
  int fd, out;
  const char *origname;
  char *origname_base;
  char *debugname, *strippedname;
  DebugLink *debuglink;
  poptContext optCon;   /* context for parsing command-line options */
  int nextopt;
  const char **args;
  struct stat stat_buf;
  
  optCon = poptGetContext("striptofile", argc, (const char **)argv,
			  optionsTable, 0);
  
  while ((nextopt = poptGetNextOpt (optCon)) > 0 || nextopt == POPT_ERROR_BADOPT)
    /* do nothing */ ;

  if (nextopt != -1)
    {
      fprintf (stderr, "Error on option %s: %s.\nRun '%s --help' to see a full list of available command line options.\n",
	      poptBadOption (optCon, 0),
	      poptStrerror (nextopt),
	      argv[0]);
      exit (1);
    }
  
  args = poptGetArgs (optCon);
  if (args == NULL || args[0] == NULL || args[1] != NULL)
    {
      poptPrintHelp(optCon, stdout, 0);
      exit (1);
    }
  
  origname = args[0];

  if (output_dir)
    {
      origname_base = path_basename (origname);
      debugname = strconcat (output_dir, "/", origname_base, ".debug", NULL);
      free (origname_base);
    }
  else
    debugname = strconcat (origname, ".debug", NULL);
  
  strippedname = strconcat (origname, ".XXXXXX", NULL);
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      fprintf (stderr, "library out of date\n");
      exit (1);
    }

  fd = open (origname, O_RDONLY);
  if (fd < 0)
    {
      fprintf (stderr, "Failed to open input file: %s\n", origname);
      exit (1);
    }
  
  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "Failed to elf_begin input file: %s\n", origname);
      exit (1);
    }

  /* Create debug file: */
  out = open (debugname, O_RDWR | O_TRUNC | O_CREAT, 0644);
  if (out < 0)
    {
      fprintf (stderr, "Failed to open output file: %s\n", debugname);
      exit (1);
    }

  out_elf = elf_begin (out, ELF_C_WRITE_MMAP, NULL);
  if (out_elf == NULL)
    {
      fprintf (stderr, "Failed to elf_begin output file: %s\n", debugname);
      exit (1);
    }

  copy_debuginfo_to_file (elf, out_elf);

  if (elf_update (out_elf, ELF_C_WRITE) < 0)
    {
      fprintf (stderr, "Failed to write debug file: %s\n", elf_errmsg (elf_errno()));
      exit (1);
    }
  elf_end (out_elf);
  close (out);
  
  debuglink = malloc (sizeof (DebugLink));
  debuglink->filename = path_basename (debugname);
  debuglink->checksum = crc32_file (debugname);

  /* Create stripped file: */
  out = mkstemp (strippedname);
  if (out < 0)
    {
      fprintf (stderr, "Failed to open output file: %s\n", strippedname);
      exit (1);
    }

  /* Copy access rights */
  if (fstat(fd, &stat_buf) == 0)
    fchmod(out, stat_buf.st_mode);

  out_elf = elf_begin (out, ELF_C_WRITE, NULL);
  if (out_elf == NULL)
    {
      fprintf (stderr, "Failed to elf_begin output file: %s\n", strippedname);
      exit (1);
    }

  strip_to_file (elf, out_elf, debuglink);

  if (elf_update (out_elf, ELF_C_WRITE) < 0)
    {
      fprintf (stderr, "Failed to write stripped file: %s\n", elf_errmsg (elf_errno()));
      exit (1);
    }
  elf_end (out_elf);
  close (out);        
  
  elf_end (elf);
  close (fd);

  
  if (rename (strippedname, origname) != 0)
    fprintf(stderr, "unable to write to %s\n", origname);

  unlink (strippedname);
  
  poptFreeContext (optCon);

  return 0;
}

