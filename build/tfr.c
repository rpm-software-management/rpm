#include "system.h"

#include <rpmbuild.h>
#include <argv.h>

#include "debug.h"

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    StringBuf sb;
    ARGV_t pav;
    int pac = 0;
    ARGV_t xav;
    ARGV_t av = NULL;
    int ac = 0;
    const char * s;
    int ec = 1;
    int xx;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    /* Find path to file(1). */
    s = rpmExpand("%{?__file}", NULL);
    if (!(s && *s))
	goto exit;

    /* Build argv, merging tokens from expansion with CLI args. */
    xx = poptParseArgvString(s, &pac, (const char ***)&pav);
    if (!(xx == 0 && pac > 0 && pav != NULL))
	goto exit;

    xav = NULL;
    xx = argvAppend(&xav, pav);
    xx = argvAppend(&xav, av);
    pav = _free(pav);
    s = _free(s);

    /* Read file(1) output. */
    sb = getOutputFrom(NULL, xav, NULL, 0, 1);

    xx = argvFree(xav);

    xav = NULL;
    xx = argvSplit(&xav, getStringBuf(sb), "\n");
    sb = freeStringBuf(sb);

    xx = argvSort(xav, argvCmp);

argvPrint("final", xav, NULL);

    xx = argvFree(xav);

exit:
    optCon = rpmcliFini(optCon);
    return ec;
}
