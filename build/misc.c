/** \ingroup rpmbuild
 * \file build/misc.c
 */
#include "system.h"

#include <rpm/rpmbuild.h>
#include "debug.h"

uint32_t parseUnsignedNum(const char * line, uint32_t * res)
{
    char * s1 = NULL;
    unsigned long rc;
    uint32_t result;

    if (line == NULL) return 1;

    while (isspace(*line)) line++;
    if (!isdigit(*line)) return 1;

    rc = strtoul(line, &s1, 10);

    if (*s1 || s1 == line || rc == ULONG_MAX || rc > UINT_MAX)
        return 1;

    result = (uint32_t)rc;
    if (res) *res = result;

    return 0;
}
