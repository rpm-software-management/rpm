#include "system.h"

#include "rpmbuild.h"

/** */
int parseNum(const char *line, int *res)
{
    char *s1;
    
    s1 = NULL;
    *res = strtoul(line, &s1, 10);
    if ((*s1) || (s1 == line) || (*res == ULONG_MAX)) {
	return 1;
    }

    return 0;
}
