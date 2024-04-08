/* rpmsort: sort(1), but for RPM versions */

#include <popt.h>
#include <string.h>

#include <rpm/rpmlib.h>

#include "debug.h"
#include "system.h"

#define BUFSIZE 2048

static size_t read_file(const char *input, char **ret)
{
    FILE *in;
    size_t s, sz = BUFSIZE, offset = 0;
    char *text;

    if (!strcmp(input, "-"))
	in = stdin;
    else
	in = fopen(input, "r");

    text = (char *)xmalloc(sz);

    if (!in) {
	fprintf(stderr, "cannot open `%s'", input);
	exit(EXIT_FAILURE);
    }

    while ((s = fread(text + offset, 1, sz - offset, in)) != 0) {
	offset += s;
	if (sz - offset == 0) {
	    sz += BUFSIZE;
	    text = xrealloc(text, sz);
	}
    }

    text[offset] = '\0';
    *ret = text;

    if (in != stdin)
	fclose(in);

    return offset + 1;
}

/* Returns name, version, and release, which will be NULL string pointers
 * returned if nothing was found. */
static void split_package_string(char *package_string, char **name,
				 char **version, char **release)
{
    char *package_version, *package_release;

    /* Release */
    package_release = strrchr(package_string, '-');

    if (package_release != NULL)
	*package_release++ = '\0';

    *release = package_release;

    /* Version */
    package_version = strrchr(package_string, '-');

    if (package_version != NULL)
	*package_version++ = '\0';

    *version = package_version;
    /* Name */
    *name = package_string;

    /* Bubble up non-null values from release to name */
    if (*name == NULL) {
	*name = (*version == NULL ? *release : *version);
	*version = *release;
	*release = NULL;
    }
    if (*version == NULL) {
	*version = *release;
	*release = NULL;
    }
}

/* A package name-version-release comparator for qsort.  It expects p, q which
 * are pointers to character strings and will not be altered in this
 * function. */
static int package_version_compare(const void *p, const void *q)
{
    char *local_p, *local_q;
    char *lhs_name, *lhs_version, *lhs_release;
    char *rhs_name, *rhs_version, *rhs_release;
    int vercmpflag = 0;

    local_p = rstrdup(*(char * const *)p);
    local_q = rstrdup(*(char * const *)q);

    split_package_string(local_p, &lhs_name, &lhs_version, &lhs_release);
    split_package_string(local_q, &rhs_name, &rhs_version, &rhs_release);

    /* Check Name and return if unequal */
    vercmpflag = strcmp((lhs_name == NULL ? "" : lhs_name),
			(rhs_name == NULL ? "" : rhs_name));
    if (vercmpflag != 0)
	goto exit;

    /* Check version and return if unequal */
    vercmpflag = rpmvercmp((lhs_version == NULL ? "" : lhs_version),
			   (rhs_version == NULL ? "" : rhs_version));
    if (vercmpflag != 0)
	goto exit;

    /* Check release and return the version compare value */
    vercmpflag = rpmvercmp((lhs_release == NULL ? "" : lhs_release),
			   (rhs_release == NULL ? "" : rhs_release));
exit:
    rfree(local_p);
    rfree(local_q);
    return vercmpflag;
}

static void add_input(const char *filename, char ***package_names,
		      size_t *n_package_names)
{
    char *orig_input_buffer = NULL;
    char *input_buffer;
    char *position_of_newline;
    char **names = *package_names;
    char **new_names = NULL;
    size_t n_names = *n_package_names;

    if (!*package_names)
	new_names = names = (char **)xmalloc(sizeof(char *) * 2);

    if (read_file(filename, &orig_input_buffer) < 2) {
	if (new_names)
	    free(new_names);
	if (orig_input_buffer)
	    free(orig_input_buffer);
	return;
    }

    input_buffer = orig_input_buffer;
    while (input_buffer && *input_buffer &&
	   (position_of_newline = strchrnul(input_buffer, '\n'))) {
	size_t sz = position_of_newline - input_buffer;
	char *newl;

	if (sz == 0) {
	    input_buffer = position_of_newline + 1;
	    continue;
	}

	newl = rstrndup(input_buffer, sz);

	names = xrealloc(names, sizeof(char *) * (n_names + 1));
	names[n_names] = newl;
	n_names++;

	/* Move buffer ahead to next line. */
	input_buffer = position_of_newline + 1;
	if (*position_of_newline == '\0')
	    input_buffer = NULL;
    }

    free(orig_input_buffer);

    *package_names = names;
    *n_package_names = n_names;
}

static struct poptOption optionsTable[] = { POPT_AUTOHELP POPT_TABLEEND };

int main(int argc, const char *argv[])
{
    poptContext optCon;
    const char *arg;
    char **package_names = NULL;
    size_t n_package_names = 0;
    char seen_file = 0;

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<FILES>");
    if (poptGetNextOpt(optCon) == 0) {
	poptPrintUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    while ((arg = poptGetArg(optCon)) != NULL) {
	add_input(arg, &package_names, &n_package_names);
	seen_file = 1;
    }

    /* If there's no inputs in argv, add one for stdin. */
    if (!seen_file)
	add_input("-", &package_names, &n_package_names);

    if (package_names == NULL || n_package_names < 1) {
	fprintf(stderr, "Invalid input\n");
	exit(EXIT_FAILURE);
    }

    qsort(package_names, n_package_names, sizeof(char *),
	  package_version_compare);

    /* Send sorted list to stdout. */
    for (int i = 0; i < n_package_names; i++) {
	fprintf(stdout, "%s\n", package_names[i]);
	free(package_names[i]);
    }

    free(package_names);
    poptFreeContext(optCon);
    return 0;
}
