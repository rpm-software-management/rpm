#include <stdio.h>
#include "spec.h"
#include "build.h"
#include "messages.h"
#include "pack.h"
#include "rpmerr.h"

void printerror(void)
{
    fprintf(stderr, "ERRORCODE  : %d\n", errCode());
    fprintf(stderr, "ERRORSTRING: %s\n", errString());
}

void main(int argc, char **argv)
{
    FILE *f;
    Spec s;

    setVerbosity(MESS_DEBUG);
    errSetCallback(printerror);

    f = fopen(argv[1], "r");
    if ((s = parseSpec(f))) {
	execPrep(s);
/* 	dumpSpec(s); */
	packageBinaries(s);
    }

    fclose(f);
}
