#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

char *strconcat (const char *string1, ...);
off_t align_up (off_t offset, size_t alignment);
int string_has_prefix (char *string, char *prefix);
uint32_t crc32_file (const char *filename);
char *path_basename (const char *filename);

#endif /* UTILS_H */
