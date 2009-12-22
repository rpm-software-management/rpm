/** \ingroup header
 * \file lib/header_internal.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include "lib/header_internal.h"

#include "debug.h"

uint64_t htonll( uint64_t n ) {
    uint32_t *i = (uint32_t*)&n;
    uint32_t b = i[0];
    i[0] = htonl(i[1]);
    i[1] = htonl(b);
    return n;
}

