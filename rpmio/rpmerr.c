#include <stdarg.h>
#include <stdio.h>

#include "rpmerr.h"

void error(int code, char *format, ...)
{
    va_list args;

    va_start(args, format);

    fprintf(stderr, "ERROR(%d): ", code);
    vfprintf(stdout, format, args);
}
