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
  Elf64_Off orig_e_shoff;
  Elf64_Off n_sections;
  UnstripInfoSection64 sections[1];
} UnstripInfo64;

static uint32_t
elf_32_to_file (uint32_t x, int file_is_little_endian)
{
  volatile uint32_t out;
  unsigned char *outbytes;

  outbytes = (unsigned char *)&out;
  if (file_is_little_endian)
    {
      outbytes[0] = (x >> 0) & 0xff;
      outbytes[1] = (x >> 8) & 0xff;
      outbytes[2] = (x >> 16) & 0xff;
      outbytes[3] = (x >> 24) & 0xff;
    }
  else /* big endian */
    {
      outbytes[0] = (x >> 24) & 0xff;
      outbytes[1] = (x >> 16) & 0xff;
      outbytes[2] = (x >> 8) & 0xff;
      outbytes[3] = (x >> 0) & 0xff;
    }
  
  return out;
}

static uint64_t
elf_64_to_file (uint64_t x, int file_is_little_endian)
{
  volatile uint64_t out;
  unsigned char *outbytes;
  int i;

  outbytes = (unsigned char *)&out;
  if (file_is_little_endian)
    {
      for (i = 0; i < 8; i++)
	outbytes[i] = (x >> (8*i)) & 0xff;
    }
  else /* big endian */
    {
      for (i = 0; i < 8; i++)
	outbytes[7-i] = (x >> (8*i)) & 0xff;
    }
  
  return out;
}

static Elf32_Word
word32_to_file (Elf32_Word x, Elf *elf)
{
  Elf32_Ehdr *ehdr = elf32_getehdr (elf);
  return elf_32_to_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf32_Off
off32_to_file (Elf32_Off x, Elf *elf)
{
  Elf32_Ehdr *ehdr = elf32_getehdr (elf);
  return elf_32_to_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf64_Word
word64_to_file (Elf64_Word x, Elf *elf)
{
  Elf64_Ehdr *ehdr = elf64_getehdr (elf);
  return elf_32_to_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf64_Off
off64_to_file (Elf64_Off x, Elf *elf)
{
  Elf64_Ehdr *ehdr = elf64_getehdr (elf);
  return elf_64_to_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static uint32_t
elf_32_from_file (uint32_t x, int file_is_little_endian)
{
  unsigned char *inbytes;

  inbytes = (unsigned char *)&x;
  if (file_is_little_endian)
    {
      return 
	(inbytes[0] << 0) |
	(inbytes[1] << 8) |
	(inbytes[2] << 16) |
	(inbytes[3] << 24);
    }
  else /* big endian */
    {
      return 
	(inbytes[0] << 24) |
	(inbytes[1] << 16) |
	(inbytes[2] << 8) |
	(inbytes[3] << 0);
    }
}

static uint64_t
elf_64_from_file (uint64_t x, int file_is_little_endian)
{
  unsigned char *inbytes;

  inbytes = (unsigned char *)&x;
  if (file_is_little_endian)
    {
      return 
	((uint64_t)inbytes[0] << 0) |
	((uint64_t)inbytes[1] << 8) |
	((uint64_t)inbytes[2] << 16) |
	((uint64_t)inbytes[3] << 24) |
	((uint64_t)inbytes[4] << 32) |
	((uint64_t)inbytes[5] << 40) |
	((uint64_t)inbytes[6] << 48) |
	((uint64_t)inbytes[7] << 56);
    }
  else /* big endian */
    {
      return 
	((uint64_t)inbytes[0] << 56) |
	((uint64_t)inbytes[1] << 48) |
	((uint64_t)inbytes[2] << 40) |
	((uint64_t)inbytes[3] << 32) |
	((uint64_t)inbytes[4] << 24) |
	((uint64_t)inbytes[5] << 16) |
	((uint64_t)inbytes[6] << 8) |
	((uint64_t)inbytes[7] << 0);
    }
}

static Elf32_Word
word32_from_file (Elf32_Word x, Elf *elf)
{
  Elf32_Ehdr *ehdr = elf32_getehdr (elf);
  return elf_32_from_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf32_Off
off32_from_file (Elf32_Off x, Elf *elf)
{
  Elf32_Ehdr *ehdr = elf32_getehdr (elf);
  return elf_32_from_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf64_Word
word64_from_file (Elf64_Word x, Elf *elf)
{
  Elf64_Ehdr *ehdr = elf64_getehdr (elf);
  return elf_32_from_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
}

static Elf64_Off
off64_from_file (Elf64_Off x, Elf *elf)
{
  Elf64_Ehdr *ehdr = elf64_getehdr (elf);
  return elf_64_from_file (x, ehdr->e_ident[EI_DATA] & ELFDATA2LSB);
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
  UnstripInfo64 *info64;
  int i;
  
  data->d_align = 8;
  
  data->d_size =
    /* orig_e_shoff */ sizeof (Elf64_Off) +
    /* n_sections */ sizeof (Elf64_Off) +
    /* sections */ info->n_sections * sizeof (UnstripInfoSection64);

  data->d_buf = calloc (1, data->d_size);

  info64 = (UnstripInfo64 *) data->d_buf;

  info64->orig_e_shoff = off64_to_file (info->orig_e_shoff, elf);
  info64->n_sections = off64_to_file (info->n_sections, elf);

  for (i = 0; i < info->n_sections; i++)
    {
      info64->sections[i].debug_section = word64_to_file (info->sections[i].debug_section, elf);
      info64->sections[i].name = word64_to_file (info->sections[i].name, elf);
      info64->sections[i].orig_offset = off64_to_file (info->sections[i].orig_offset, elf);
    }
}

void
unstrip_info_to_data (UnstripInfo *info,
		      Elf *elf,
		      Elf_Data *data)
{
  GElf_Ehdr ehdr;
  
  data->d_type = ELF_T_BYTE;
  data->d_off = 0;

  gelf_getehdr (elf, &ehdr);
  if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
    unstrip_info_to_data32 (info, elf, data);
  else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64)
    unstrip_info_to_data64 (info, elf, data);
  else
    fprintf (stderr, "Warning. unsupported elf class\n");
}

static void
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

static void
unstrip_info_from_data64 (UnstripInfo *info,
			  Elf *elf,
			  Elf_Data *data)
{
  UnstripInfo64 *info64;
  int i;
  
  info64 = (UnstripInfo64 *) data->d_buf;
  
  info->orig_e_shoff = off64_from_file (info64->orig_e_shoff, elf);
  info->n_sections = off64_from_file (info64->n_sections, elf);

  info->sections = calloc (info->n_sections, sizeof (UnstripInfoSection));
  for (i = 0; i < info->n_sections; i++)
    {
      info->sections[i].debug_section = word64_from_file (info64->sections[i].debug_section, elf);
      info->sections[i].name = word64_from_file (info64->sections[i].name, elf);
      info->sections[i].orig_offset = off64_from_file (info64->sections[i].orig_offset, elf);
    }
}

UnstripInfo *
unstrip_info_from_data (Elf *elf,
			Elf_Data *data)
{
  GElf_Ehdr ehdr;
  
  UnstripInfo *info;

  info = malloc (sizeof (UnstripInfo));

  gelf_getehdr (elf, &ehdr);
  if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
    unstrip_info_from_data32 (info, elf, data);
  else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64)
    unstrip_info_from_data64 (info, elf, data);
  else
    fprintf (stderr, "Warning. unsupported elf class\n");
  
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
  p = ((char *)data->d_buf) + namelen_aligned;
  
  *(Elf32_Word *)p = word32_to_file (debuglink->checksum, elf);
}

static void
debug_link_to_data64 (DebugLink *debuglink,
		      Elf *elf,
		      Elf_Data *data)
{
  size_t namelen_aligned;
  char *p;
  
  data->d_align = 4;

  namelen_aligned = align_up (strlen(debuglink->filename) + 1, 4);

  data->d_size =
    /* name */ namelen_aligned +
    /* checksum */ sizeof (Elf64_Word);

  data->d_buf = calloc (1, data->d_size);

  strcpy (data->d_buf, debuglink->filename);
  p = ((char *)data->d_buf) + namelen_aligned;
  
  *(Elf64_Word *)p = word64_to_file (debuglink->checksum, elf);
}

void
debug_link_to_data (DebugLink *debuglink, Elf *elf, Elf_Data *data)
{
  GElf_Ehdr ehdr;
  
  data->d_type = ELF_T_BYTE;
  data->d_off = 0;

  gelf_getehdr (elf, &ehdr);
  if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
    debug_link_to_data32 (debuglink, elf, data);
  else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64)
    debug_link_to_data64 (debuglink, elf, data);
  else
    fprintf (stderr, "Warning. unsupported elf class\n");
}

static void
debug_link_from_data32 (DebugLink *debuglink,
			Elf *elf,
			Elf_Data *data)
{
  size_t namelen_aligned;
  char *p;
  
  debuglink->filename = strdup (data->d_buf);

  namelen_aligned = align_up (strlen (debuglink->filename) + 1, 4);

  p = ((char *)data->d_buf) + namelen_aligned;
  
  debuglink->checksum = word32_from_file (*(Elf32_Word *)p, elf);
}

static void
debug_link_from_data64 (DebugLink *debuglink,
			Elf *elf,
			Elf_Data *data)
{
  size_t namelen_aligned;
  char *p;
  
  debuglink->filename = strdup (data->d_buf);

  namelen_aligned = align_up (strlen (debuglink->filename) + 1, 4);

  p = ((char *)data->d_buf) + namelen_aligned;
  
  debuglink->checksum = word64_from_file (*(Elf64_Word *)p, elf);
}


DebugLink *
debug_link_from_data (Elf *elf, Elf_Data *data)
{
  GElf_Ehdr ehdr;
  DebugLink *debuglink;

  debuglink = malloc (sizeof (DebugLink));

  gelf_getehdr (elf, &ehdr);
  if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
    debug_link_from_data32 (debuglink, elf, data);
  else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64)
    debug_link_from_data64 (debuglink, elf, data);
  else
    fprintf (stderr, "Warning. unsupported elf class\n");
  
  return debuglink;
}

