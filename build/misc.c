/** \ingroup rpmbuild
 * \file build/misc.c
 */
#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

int parseNum(const char * line, int * res)
{
    char * s1 = NULL;
    unsigned long rc;

    if (line == NULL) return 1;
    rc = strtoul(line, &s1, 10);
    if (res) *res = rc;
    return (((*s1) || (s1 == line) || (rc == ULONG_MAX)) ? 1 : 0);
}
