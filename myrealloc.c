#include <stdio.h>
#include <malloc.h>
#define size_t unsigned

void *myrealloc(void *ptr, size_t size);

void *myrealloc(void *ptr, size_t size) {
if (ptr == NULL)
       return malloc(size);
   else
       return realloc(ptr, size);
}
