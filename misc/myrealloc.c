#include "system.h"

void *myrealloc(void *ptr, size_t size) {
if (ptr == NULL)
       return malloc(size);
   else
       return realloc(ptr, size);
}
