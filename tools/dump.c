#include "system.h"

#include <rpmlib.h>
#include "header_internal.h"
#include "debug.h"

int main(int argc, char ** argv)
{
    Header h;
    FD_t fdi;

    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = Fopen("-", "r.ufdio");
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }

    if (Ferror(fdi)) {
	fprintf(stderr, _("cannot open %s: %s\n"),
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }

    h = headerRead(fdi, HEADER_MAGIC_YES);
    if (!h) {
	fprintf(stderr, _("headerRead error: %s\n"), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    Fclose(fdi);
  
    headerDump(h, stdout, HEADER_DUMP_INLINE, rpmTagTable);
    h = headerFree(h);

    return 0;
}
