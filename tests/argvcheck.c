#include <stdio.h>
#include <string.h>

#include <rpm/argv.h>

static const char *flagname(argvFlags flags)
{
    return (flags & ARGV_SKIPEMPTY) ? "ARGV_SKIPEMPTY" : "ARGV_NONE";
}

static void dump(const char *label, const char **av, int n)
{
    fprintf(stderr, "  %s (%d):", label, n);
    for (int i = 0; i < n; i++)
	fprintf(stderr, " \"%s\"", av[i]);
    fprintf(stderr, "\n");
}

static int check(const char *str, argvFlags flags,
		 const char **want, int nwant)
{
    ARGV_t argv = argvSplitString(str, ",", flags);
    int n = argvCount(argv);
    int rc = (n != nwant);

    for (int i = 0; !rc && i < n; i++)
	rc = (strcmp(argv[i], want[i]) != 0);

    if (rc) {
	fprintf(stderr, "argvSplitString(\"%s\", \",\", %s) mismatch:\n",
		str, flagname(flags));
	dump("got ", (const char **)argv, n);
	dump("want", want, nwant);
    }
    argvFree(argv);
    return rc;
}

int main(void)
{
    const char *none[] = { NULL };
    const char *empty[] = { "" };
    const char *emptyx2[] = { "", "" };
    const char *a_empty[] = { "a", "" };
    const char *ab[] = { "a", "b" };
    const char *a_b[] = { "a", "", "b" };
    const char *a[] = { "a" };
    int rc = 0;

    /* ARGV_NONE preserves empty fields, trailing ones included */
    rc |= check("",     ARGV_NONE, empty,	1);
    rc |= check(",",    ARGV_NONE, emptyx2,	2);
    rc |= check("a,",   ARGV_NONE, a_empty,	2);
    rc |= check("a,b",  ARGV_NONE, ab,		2);
    rc |= check("a,,b", ARGV_NONE, a_b,		3);

    /* ARGV_SKIPEMPTY drops them all, unchanged by the above */
    rc |= check("",     ARGV_SKIPEMPTY, none,	0);
    rc |= check(",",    ARGV_SKIPEMPTY, none,	0);
    rc |= check("a,",   ARGV_SKIPEMPTY, a,	1);
    rc |= check("a,,b", ARGV_SKIPEMPTY, ab,	2);

    return rc;
}
