#include <stdio.h>
#include "spec.h"
#include "messages.h"

void main(int argc, char **argv)
{
    FILE *f;

    setVerbosity(MESS_DEBUG);
    
    f = fopen(argv[1], "r");
    parseSpec(f);
    fclose(f);
}
