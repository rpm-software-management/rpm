/* vim:ts=8:sts=4 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <popt.h>

int main (int argc, char **argv) {
    char *out;
    int newargc, j, f, ret;
    char **newargv;
    FILE *fp;

    if (argc == 1) {
	printf ("usage: test-popt file_1 file_2 ...\n");
	printf ("you may specify many files\n");
	exit (1);
    }

    for (f = 1; f < argc; f++) {
	fp = fopen (argv[f], "r");
	if (fp == NULL) {
	    printf ("cannot read file %s.  errno=%s\n", argv[f],
		strerror(errno));
	    continue;
	}

	ret = poptConfigFileToString (fp, &out, 0);
	if (ret != 0) {
	    printf ("cannot parse %s. ret=%d\n", argv[f], ret);
	    continue;
	}

	printf ("single string: '%s'\n", out);

	poptParseArgvString (out, &newargc, &newargv);

	printf ("popt array: size=%d\n", newargc);
	for (j = 0; j < newargc; j++)
	    printf ("'%s'\n", newargv[j]);

	printf ("\n");
	free(out);
	fclose (fp);
    }
    return 0;
}
