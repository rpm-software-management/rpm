#include "system.h"

#include <elf.h>
#include <libelf/libelf.h>
#include <libelf/gelf.h>

#include "sections.h"
#include "utils.h"

#include "debug.h"

DebugLink *
read_debuglink (Elf *elf)
{
  GElf_Ehdr ehdr;
  Elf_Scn *section;
  GElf_Shdr section_header;
  Elf_Data *data;
  unsigned char *section_strtab;

  if (gelf_getehdr (elf, &ehdr) == NULL)
    {
      printf ("Not an elf binary, exiting\n");
      exit (1);
    }

  /* Locate section header strtab */
  section = elf_getscn (elf, ehdr.e_shstrndx);
  data = elf_getdata (section, NULL);
  section_strtab = data->d_buf;

  section = NULL;
  while ((section = elf_nextscn (elf, section)) != NULL)
    {
      char *section_name;

      gelf_getshdr (section, &section_header);

      section_name = section_strtab + section_header.sh_name;

      if (strcmp (section_name, DEBUGLINKNAME) == 0)
	{
	  data = elf_rawdata (section, NULL);
	  return debug_link_from_data (elf, data);
	}
    }
  return NULL;
}

Elf_Scn *
find_section (Elf *elf, const unsigned char *name, const unsigned char *strtab)
{
  Elf_Scn *section;
  GElf_Shdr section_header;
  const unsigned char *section_name;
  
  section = NULL;
  while ((section = elf_nextscn (elf, section)) != NULL)
    {

      gelf_getshdr (section, &section_header);
      
      section_name = strtab + section_header.sh_name;

      if (strcmp (section_name, name) == 0)
	break;
    }
  
  return section;
}

size_t
find_in_strtab (char *name, char *strtab, size_t strtab_len)
{
  int name_len, i;

  name_len = strlen (name);

  for (i = 0; i < strtab_len - (name_len + 1); i++)
    if (strcmp (strtab+i, name) == 0)
      return i;
  return 0;
}

void
unstrip_file (Elf *elf, Elf *debug_elf, Elf *out_elf)
{
  UnstripInfo *info;
  GElf_Ehdr ehdr, debug_ehdr;
  GElf_Phdr phdr;
  GElf_Shdr section_header;
  Elf_Scn *section, *out_section;
  Elf_Data *data, *out_data;
  unsigned char *section_strtab;
  size_t section_strtab_len;
  unsigned char *debug_section_strtab;
  size_t debug_section_strtab_len;
  size_t *debug_section_map;
  size_t *stripped_section_map;
  char *section_name;
  int i;
  size_t new_strtab_index;

  elf_flagelf (out_elf, ELF_C_SET, ELF_F_LAYOUT);
  
  gelf_getehdr (elf, &ehdr);
  if (gelf_getehdr (debug_elf, &debug_ehdr) == NULL)
    {
      printf ("debug file not an elf binary, exiting\n");
      exit (1);
    }

  /* copy elf header: */
  gelf_newehdr (out_elf, ehdr.e_ident[EI_CLASS]);
  gelf_update_ehdr (out_elf, &ehdr);

  /* Copy program headers: */
  gelf_newphdr (out_elf, ehdr.e_phnum);

  for (i = 0; i < ehdr.e_phnum; i++)
    {
      gelf_getphdr (elf, i, &phdr);
      gelf_update_phdr(out_elf, i, &phdr);
    }

  /* Locate section header strtabs */
  section = elf_getscn (elf, ehdr.e_shstrndx);
  data = elf_getdata (section, NULL);
  section_strtab = data->d_buf;
  section_strtab_len = data->d_size;

  section = elf_getscn (debug_elf, debug_ehdr.e_shstrndx);
  data = elf_getdata (section, NULL);
  debug_section_strtab = data->d_buf;
  debug_section_strtab_len = data->d_size;

  /* Read unlinkinfo */
  info = NULL;
  section = find_section (debug_elf, UNSTRIPINFONAME, debug_section_strtab);
  if (section)
    {
      data = elf_rawdata (section, NULL);
      info = unstrip_info_from_data (elf, data);
    }
  
  if (info == NULL)
    {
      printf ("Can't find unstrip info in debug file\n");
      exit (1);
    }

  /* Construct backward section index maps */
  debug_section_map = calloc (info->n_sections, sizeof (size_t));
  for (i = 0; i < info->n_sections; i++)
    {
      if (info->sections[i].debug_section > 0)
	debug_section_map[info->sections[i].debug_section] = i;
    }

  stripped_section_map = calloc (ehdr.e_shnum, sizeof (size_t));
  section = NULL;
  while ((section = elf_nextscn(elf, section)) != NULL)
    {
      gelf_getshdr (section, &section_header);
      section_name = section_strtab + section_header.sh_name;
      
      for (i = 0; i < info->n_sections; i++)
	{
	  char *debug_section_name;
	  
	  debug_section_name = debug_section_strtab + info->sections[i].name;
	  
	  /* If section name is same as an original section, and the original
	   * section is not in debugfile, use this section */
	  if (info->sections[i].debug_section == 0 &&
	      strcmp (debug_section_name, section_name) == 0)
	    {
	      stripped_section_map[elf_ndxscn(section)] = i;
	      break;
	    }
	}
    }
  
  /* combine sections */
  new_strtab_index = 0;
  for (i = 0; i < info->n_sections; i++)
    {
      if (info->sections[i].debug_section != 0)
	{
	  section = elf_getscn(debug_elf, info->sections[i].debug_section);

	  out_section = elf_newscn(out_elf);

	  /* Copy section header */
	  gelf_getshdr (section, &section_header);
	  section_header.sh_offset = info->sections[i].orig_offset;
	  section_header.sh_link = debug_section_map[section_header.sh_link];
	  if (section_header.sh_type == SHT_REL ||
	      section_header.sh_type == SHT_RELA)
	    section_header.sh_info = stripped_section_map[section_header.sh_info];

	  /* Copy data */
	  data = NULL;
	  while ((data = elf_rawdata (section, data)))
	    {
	      out_data = elf_newdata (out_section);
	      
	      /* TODO: remove .unstripinfo for shstrtab */
	      
	      out_data->d_buf = data->d_buf;
	      out_data->d_type = data->d_type;
	      out_data->d_size = data->d_size;
	      if (debug_ehdr.e_shstrndx == info->sections[i].debug_section)
		{
		  new_strtab_index = i;
		  if (strcmp (data->d_buf + data->d_size - (strlen(UNSTRIPINFONAME) + 1), UNSTRIPINFONAME) == 0)
		    {
		      out_data->d_size -= strlen(UNSTRIPINFONAME) + 1;
		      section_header.sh_size = out_data->d_size;
		    }
		}
	      out_data->d_off = data->d_off;
	      out_data->d_align = section_header.sh_addralign;
	      out_data->d_version = data->d_version;
	    }
	  gelf_update_shdr(out_section, &section_header);
	}
      else
	{
	  section_name = debug_section_strtab + info->sections[i].name;
	  section = find_section (elf, section_name, section_strtab);

	  if (section)
	    {
	      out_section = elf_newscn(out_elf);

	      /* Copy section header */
	      gelf_getshdr (section, &section_header);
	      section_header.sh_offset = info->sections[i].orig_offset;
	      section_header.sh_link = stripped_section_map[section_header.sh_link];
	      if (section_header.sh_type == SHT_REL ||
		  section_header.sh_type == SHT_RELA)
		section_header.sh_info = stripped_section_map[section_header.sh_info];
	      section_header.sh_name = find_in_strtab (section_name, debug_section_strtab, debug_section_strtab_len);
	      gelf_update_shdr (out_section, &section_header);

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
    }

  gelf_getehdr (out_elf, &ehdr);
  ehdr.e_shstrndx = new_strtab_index;
  ehdr.e_shoff = info->orig_e_shoff;
  gelf_update_ehdr (out_elf, &ehdr);
}


int
main (int argc, char *argv[])
{
  Elf *elf, *debug_elf, *out_elf;
  int fd, debug_fd, out;
  char *origname, *unstrippedname;
  DebugLink *debuglink;
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      printf ("library out of date\n");
      exit (1);
    }
  
  if (argc != 2)
    {
      printf ("usage: unstriptofile filename\n");
      exit (1);
    }
  
  origname = argv[1];

  fd = open (origname, O_RDONLY);
  if (fd < 0)
    {
      printf ("Failed to open input file\n");
      exit (1);
    }

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("Failed to elf_begin input file\n");
      exit (1);
    }

  debuglink = read_debuglink (elf);
  if (debuglink == NULL)
    {
      printf ("Cannot find .debuglink section in input file\n");
      exit (1);
    }

  if (debuglink->checksum != crc32_file (debuglink->filename))
    {
      printf ("Invalid checksum for debug file. File has been modified.\n");
      exit (1);
    }
  
  debug_fd = open (debuglink->filename, O_RDONLY);
  if (debug_fd < 0)
    {
      printf ("Failed to open debug file\n");
      exit (1);
    }
 
  debug_elf = elf_begin (debug_fd, ELF_C_READ, NULL);
  if (debug_elf == NULL)
    {
      printf ("Failed to elf_begin debug file\n");
      exit (1);
    }

  unstrippedname = malloc (strlen (origname) + strlen (".unstripped") + 1);
  strcpy (unstrippedname, origname);
  strcat (unstrippedname, ".unstripped");
  
  out = open (unstrippedname, O_RDWR | O_TRUNC | O_CREAT, 0644);
  if (out < 0)
    {
      printf ("Failed to open output file\n");
      exit (1);
    }

  out_elf = elf_begin (out, ELF_C_WRITE, NULL);
  if (out_elf == NULL)
    {
      printf ("Failed to elf_begin output file\n");
      exit (1);
    }

  unstrip_file (elf, debug_elf, out_elf);

  elf_update (out_elf, ELF_C_WRITE);
  elf_end (out_elf);
  close (out);
  
  elf_end (debug_elf);
  close (debug_fd);
  
  elf_end (elf);
  close (fd);
  
  return 0;
}

