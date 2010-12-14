/**
 * \file lib/misc.c
 */

#include "system.h"
#include "lib/misc.h"
#include "debug.h"

unsigned int hashFunctionString(const char * string)
{
    /* Jenkins One-at-a-time hash */
    unsigned int hash = 0xe4721b68;

    while (*string != '\0') {
      hash += *string;
      hash += (hash << 10);
      hash ^= (hash >> 6);
      string++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}
