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
    int ac;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    for (ac = 0, av = argv; *av; av++, ac++)
	fprintf(fp, "%5d: %s\n", ac, *av);

}

int argvFree(/*@only@*/ /*@null@*/ ARGV_t argv)
{
    ARGV_t av;
    
    if ((av = argv)) {
	while (*av)
	    *av = _free(*av);
	argv = _free(argv);
    }
    return 0;
}

int argvCount(/*@null@*/ const ARGV_t argv)
{
    int argc = 0;
    if (argv)
    while (argv[argc] != NULL)
	argc++;
    return argc;
}

int argvCmp(const void * a, const void * b)
{
/*@-boundsread@*/
    ARG_t astr = *(ARGV_t)a;
    ARG_t bstr = *(ARGV_t)b;
/*@=boundsread@*/
    return strcmp(astr, bstr);
}

int argvSort(ARGV_t argv, int (*compar)(const void *, const void *))
{
    qsort(argv, argvCount(argv), sizeof(*argv), compar);
    return 0;
}

ARGV_t argvSearch(ARGV_t argv, ARG_t s,
		int (*compar)(const void *, const void *))
{
    return bsearch(&s, argv, argvCount(argv), sizeof(*argv), compar);
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
