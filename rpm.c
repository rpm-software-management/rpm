#include <stdio.h>
#include "spec.h"
#include "build.h"
#include "messages.h"

void main(int argc, char **argv)
{
    FILE *f;
    Spec s;

    setVerbosity(MESS_DEBUG);

    f = fopen(argv[1], "r");
    s = parseSpec(f);
    fclose(f);

    dumpSpec(s, stdout);
    execPrep(s);
}
