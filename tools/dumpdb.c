#include "system.h"

#include <rpmlib.h>
#include "header_internal.h"
#include "debug.h"

int main(int argc, char ** argv)
{
    unsigned int dspBlockNum = 0;		/* default to all */
    rpmdb db;

    setprogname(argv[0]);	/* Retrofit glibc __progname */
    rpmReadConfigFiles(NULL, NULL);

    if (argc == 2) {
	dspBlockNum = atoi(argv[1]);
    } else if (argc != 1) {
	fprintf(stderr, _("dumpdb <block num>\n"));
	exit(1);
    }

    if (rpmdbOpen("", &db, O_RDONLY, 0644)) {
	fprintf(stderr, _("cannot open Packages\n"));
	exit(1);
    }

    {	Header h = NULL;
	unsigned int blockNum = 0;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, NULL, 0);

	while ((h = rpmdbNextIterator(mi)) != NULL) {

	    blockNum++;
	    if (!(dspBlockNum != 0 && dspBlockNum != blockNum))
		continue;

	    headerDump(h, stdout, HEADER_DUMP_INLINE, rpmTagTable);
	    fprintf(stdout, "Offset: %d\n", _RECNUM);
    
	    if (dspBlockNum && blockNum > dspBlockNum)
		exit(0);
	}

	mi = rpmdbFreeIterator(mi);

    }

    rpmdbClose(db);

    return 0;
}
