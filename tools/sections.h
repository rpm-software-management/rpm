#ifndef SECTIONS_H
#define SECTIONS_H

#include <libelf/gelf.h>

typedef struct {
  GElf_Word debug_section; /* Section index in debug file. */
  GElf_Word name; /* offset in original/debug-file shstrtab */
  GElf_Off orig_offset;
} UnstripInfoSection;

typedef struct {
  GElf_Off orig_e_shoff;
  GElf_Off n_sections;
  UnstripInfoSection *sections;
} UnstripInfo;

typedef struct {
  char *filename;
  GElf_Word checksum;
} DebugLink;

#define DEBUGLINKNAME ".gnu_debuglink"
#define UNSTRIPINFONAME ".gnu_unstripinfo"

void debug_link_to_data (DebugLink *debuglink, Elf *elf, Elf_Data *data);
DebugLink *debug_link_from_data (Elf *elf, Elf_Data *data);
void unstrip_info_to_data (UnstripInfo *info, Elf *elf, Elf_Data *data);
UnstripInfo *unstrip_info_from_data (Elf *elf, Elf_Data *data);

#endif /* SECTIONS_H */
