#include "system.h"

#include <rpmbuild.h>

#include "debug.h"

/**
 */
static int argvFree(/*@only@*/ /*@null@*/ const char ** argv)
	/*@modifies argv @*/
{
    const char ** av;
    
    if ((av = argv)) {
	while (*av)
	    *av = _free(*av);
	argv = _free(argv);
    }
    return 0;
}

/**
 */
static void argvPrint(const char * msg, const char ** argv, FILE * fp)
	/*@*/
{
    const char ** av;
    int ac;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    for (ac = 0, av = argv; *av; av++, ac++)
	fprintf(fp, "%5d: %s\n", ac, *av);

}

/**
 */
static int argvCount(/*@null@*/ const char ** argv)
	/*@*/
{
    int argc = 0;
    if (argv)
    while (argv[argc] != NULL)
	argc++;
    return argc;
}

/**
 * Compare argv arrays (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int argvCmp(const void * a, const void * b)
	/*@*/
{
/*@-boundsread@*/
    const char * astr = *(const char **)a;
    const char * bstr = *(const char **)b;
/*@=boundsread@*/
    return strcmp(astr, bstr);
}

/**
 */
static int argvSort(const char ** argv,
		int (*compar)(const void *, const void *))
	/*@*/
{
    qsort(argv, argvCount(argv), sizeof(*argv), compar);
    return 0;
}

/**
 */
static const char ** argvSearch(const char ** argv, const char * s,
		int (*compar)(const void *, const void *))
	/*@*/
{
    return bsearch(&s, argv, argvCount(argv), sizeof(*argv), compar);
}

/**
 */
static int argvAppend(/*@out@*/ const char *** argvp, const char ** av)
	/*@*/
{
    const char ** argv = *argvp;
    int argc = argvCount(argv);
    int ac = argvCount(av);
    int i;

    argv = xrealloc(argv, (argc + ac + 1) * sizeof(*argv));
    for (i = 0; i < ac; i++)
	argv[argc + i] = xstrdup(av[i]);
    argv[argc + ac] = NULL;
    *argvp = argv;
    return 0;
}

/**
 */
static int argvSplit(const char *** argvp, const char * str, const char * seps)
	/*@*/
{
    char * dest = alloca(strlen(str) + 1);
    const char ** argv;
    int argc = 1;
    const char * s;
    char * t;
    int c;

    for (argc = 1, s = str, t = dest; (c = *s); s++, t++) {
	if (strchr(seps, c)) {
	    argc++;
	    c = '\0';
	}
	*t = c;
    }
    *t = '\0';

    argv = xmalloc( (argc + 1) * sizeof(*argv));

    for (c = 0, s = dest; s < t; s+= strlen(s) + 1) {
	if (*s == '\0')
	    continue;
	argv[c] = xstrdup(s);
	c++;
    }
    argv[c] = NULL;
    *argvp = argv;
    return 0;
}

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
    const char ** pav;
    int pac = 0;
    const char ** xav;
    const char ** av = NULL;
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
