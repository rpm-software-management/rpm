#include <stdio.h>

#include "rpmerr.h"

void error(int code, ...)
{
    fprintf(stderr, "error, error, error\n");
}
