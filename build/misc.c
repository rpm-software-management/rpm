/** \ingroup rpmbuild
 * \file build/misc.c
 */
#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

int parseNum(const char *line, int *res)
{
    char * s1 = NULL;

    *res = strtoul(line, &s1, 10);
    return (((*s1) || (s1 == line) || (*res == ULONG_MAX)) ? 1 : 0);
}
