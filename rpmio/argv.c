/** \ingroup rpmio
 * \file rpmio/argv.c
 */

#include "system.h"

#include <argv.h>

#include "debug.h"

/*@-bounds@*/
/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * _free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

void argvPrint(const char * msg, ARGV_t argv, FILE * fp)
{
    ARGV_t av;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    if (argv)
    for (av = argv; *av; av++)
	fprintf(fp, "%s\n", *av);

}

ARGI_t argiFree(ARGI_t argi)
{
    if (argi) {
	argi->nvals = 0;
	argi->vals = _free(argi->vals);
    }
    argi = _free(argi);
    return NULL;
}

ARGV_t argvFree(/*@only@*/ /*@null@*/ ARGV_t argv)
{
    ARGV_t av;
    
/*@-branchstate@*/
    if (argv)
    for (av = argv; *av; av++)
	*av = _free(*av);
/*@=branchstate@*/
    argv = _free(argv);
    return NULL;
}

int argiCount(ARGI_t argi)
{
    int nvals = 0;
    if (argi)
	nvals = argi->nvals;
    return nvals;
}

const ARGint_t argiData(const ARGI_t argi)
{
    ARGint_t vals = NULL;
    if (argi && argi->nvals > 0)
	vals = argi->vals;
    return vals;
}

int argvCount(const ARGV_t argv)
{
    int argc = 0;
    if (argv)
    while (argv[argc] != NULL)
	argc++;
    return argc;
}

const ARGV_t argvData(const ARGV_t argv)
{
/*@-retalias -temptrans @*/
    return argv;
/*@=retalias =temptrans @*/
}

int argvCmp(const void * a, const void * b)
{
/*@-boundsread@*/
    ARGstr_t astr = *(ARGV_t)a;
    ARGstr_t bstr = *(ARGV_t)b;
/*@=boundsread@*/
    return strcmp(astr, bstr);
}

int argvSort(ARGV_t argv, int (*compar)(const void *, const void *))
{
    if (compar == NULL)
	compar = argvCmp;
    qsort(argv, argvCount(argv), sizeof(*argv), compar);
    return 0;
}

ARGV_t argvSearch(ARGV_t argv, ARGstr_t val,
		int (*compar)(const void *, const void *))
{
    if (argv == NULL)
	return NULL;
    if (compar == NULL)
	compar = argvCmp;
    return bsearch(&val, argv, argvCount(argv), sizeof(*argv), compar);
}

int argiAdd(/*@out@*/ ARGI_t * argip, int ix, int val)
{
    ARGI_t argi;

    if (argip == NULL)
	return -1;
    if (*argip == NULL)
	*argip = xcalloc(1, sizeof(**argip));
    argi = *argip;
    if (ix < 0)
	ix = argi->nvals;
    if (ix >= argi->nvals) {
	argi->vals = xrealloc(argi->vals, (ix + 1) * sizeof(*argi->vals));
	memset(argi->vals + argi->nvals, 0,
		(ix - argi->nvals) * sizeof(*argi->vals));
	argi->nvals = ix + 1;
    }
    argi->vals[ix] = val;
    return 0;
}

int argvAdd(/*@out@*/ ARGV_t * argvp, ARGstr_t val)
{
    ARGV_t argv;
    int argc;

    if (argvp == NULL)
	return -1;
    argc = argvCount(*argvp);
/*@-unqualifiedtrans@*/
    *argvp = xrealloc(*argvp, (argc + 1 + 1) * sizeof(**argvp));
/*@=unqualifiedtrans@*/
    argv = *argvp;
    argv[argc++] = xstrdup(val);
    argv[argc  ] = NULL;
    return 0;
}

int argvAppend(/*@out@*/ ARGV_t * argvp, const ARGV_t av)
{
    ARGV_t argv = *argvp;
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

int argvSplit(ARGV_t * argvp, const char * str, const char * seps)
{
    char * dest = alloca(strlen(str) + 1);
    ARGV_t argv;
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
/*@-nullstate@*/
    return 0;
/*@=nullstate@*/
}
/*@=bounds@*/
