#include <stdio.h>
#include "spec.h"

void main(int argc, char **argv)
{
    FILE *f;

    printf("hello\n");
    f = fopen(argv[1], "r");
    parse_spec(f);
    fclose(f);
}
