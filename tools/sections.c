#include "system.h"

#include "sections.h"
#include "utils.h"

#include "debug.h"

typedef struct {
  Elf32_Word debug_section; 
  Elf32_Word name; 
  Elf32_Off orig_offset;
} UnstripInfoSection32;

typedef struct {
  Elf64_Word debug_section; 
  Elf64_Word name; 
  Elf64_Off orig_offset;
} UnstripInfoSection64;

typedef struct {
  Elf32_Off orig_e_shoff;
  Elf32_Off n_sections;
  UnstripInfoSection32 sections[1];
} UnstripInfo32;

typedef struct {
  Elf32_Off orig_e_shoff;
  Elf32_Off n_sections;
  UnstripInfoSection64 sections[1];
} UnstripInfo64;


static Elf32_Word
word32_to_file (Elf32_Word x, Elf *elf)
{
  /* FIXME: implement */
  return x;
}

static Elf32_Off
off32_to_file (Elf32_Off x, Elf *elf)
{
  /* FIXME: implement */
  return x;
}

static Elf32_Word
word32_from_file (Elf32_Word x, Elf *elf)
{
  /* FIXME: implement */
  return x;
}

static Elf32_Off
off32_from_file (Elf32_Off x, Elf *elf)
{
  /* FIXME: implement */
  return x;
}

static void
unstrip_info_to_data32 (UnstripInfo *info,
			Elf *elf,
			Elf_Data *data)
{
  UnstripInfo32 *info32;
  int i;
  
  data->d_align = 4;

  data->d_size =
    /* orig_e_shoff */ sizeof (Elf32_Off) +
    /* n_sections */ sizeof (Elf32_Off) +
    /* sections */ info->n_sections * sizeof (UnstripInfoSection32);

  data->d_buf = calloc (1, data->d_size);

  info32 = (UnstripInfo32 *) data->d_buf;

  info32->orig_e_shoff = off32_to_file (info->orig_e_shoff, elf);
  info32->n_sections = off32_to_file (info->n_sections, elf);

  for (i = 0; i < info->n_sections; i++)
    {
      info32->sections[i].debug_section = word32_to_file (info->sections[i].debug_section, elf);
      info32->sections[i].name = word32_to_file (info->sections[i].name, elf);
      info32->sections[i].orig_offset = off32_to_file (info->sections[i].orig_offset, elf);
    }
}

static void
unstrip_info_to_data64 (UnstripInfo *info,
			Elf *elf,
			Elf_Data *data)
{
  data->d_align = 8;
}

void
unstrip_info_to_data (UnstripInfo *info,
		      Elf *elf,
		      Elf_Data *data)
{
  data->d_type = ELF_T_BYTE;
  data->d_off = 0;

  /* FIXME: use right version */
  unstrip_info_to_data32 (info, elf, data);
}

void
unstrip_info_from_data32 (UnstripInfo *info,
			  Elf *elf,
			  Elf_Data *data)
{
  UnstripInfo32 *info32;
  int i;
  
  info32 = (UnstripInfo32 *) data->d_buf;
  
  info->orig_e_shoff = off32_from_file (info32->orig_e_shoff, elf);
  info->n_sections = off32_from_file (info32->n_sections, elf);

  info->sections = calloc (info->n_sections, sizeof (UnstripInfoSection));
  for (i = 0; i < info->n_sections; i++)
    {
      info->sections[i].debug_section = word32_from_file (info32->sections[i].debug_section, elf);
      info->sections[i].name = word32_from_file (info32->sections[i].name, elf);
      info->sections[i].orig_offset = off32_from_file (info32->sections[i].orig_offset, elf);
    }
}

UnstripInfo *
unstrip_info_from_data (Elf *elf,
			Elf_Data *data)
{
  UnstripInfo *info;

  info = malloc (sizeof (UnstripInfo));

  /* FIXME: use right version */
  unstrip_info_from_data32 (info,
			    elf,
			    data);
  
  return info;
}


static void
debug_link_to_data32 (DebugLink *debuglink,
		      Elf *elf,
		      Elf_Data *data)
{
  size_t namelen_aligned;
  char *p;
  
  data->d_align = 4;

  namelen_aligned = align_up (strlen(debuglink->filename) + 1, 4);

  data->d_size =
    /* name */ namelen_aligned +
    /* checksum */ sizeof (Elf32_Word);

  data->d_buf = calloc (1, data->d_size);

  strcpy (data->d_buf, debuglink->filename);
  p = data->d_buf + namelen_aligned;
  
  *(Elf32_Word *)p = word32_to_file (debuglink->checksum, elf);
  p += sizeof (Elf32_Word);
}

void
debug_link_to_data (DebugLink *debuglink, Elf *elf, Elf_Data *data)
{
  data->d_type = ELF_T_BYTE;
  data->d_off = 0;

  /* FIXME: use right version */
  debug_link_to_data32 (debuglink, elf, data);
}

void
debug_link_from_data32 (DebugLink *debuglink,
			Elf *elf,
			Elf_Data *data)
{
  size_t namelen_aligned;
  char *p;
  
  debuglink->filename = strdup (data->d_buf);

  namelen_aligned = align_up (strlen (debuglink->filename) + 1, 4);

  p = data->d_buf + namelen_aligned;
  
  debuglink->checksum = word32_from_file (*(Elf32_Word *)p, elf);
  p += sizeof (Elf32_Word);
}


DebugLink *
debug_link_from_data (Elf *elf, Elf_Data *data)
{
  DebugLink *debuglink;

  debuglink = malloc (sizeof (DebugLink));

  /* FIXME: use right version */
  debug_link_from_data32 (debuglink,
			  elf,
			  data);
  
  return debuglink;
}

