#include "system.h"

#include "rpmlib.h"

int main(int argc, char ** argv)
{
    Header h;
    int offset;
    int dspBlockNum = 0;			/* default to all */
    int blockNum = 0;
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
	fprintf(stderr, _("cannot open /var/lib/rpm/packages.rpm\n"));
	exit(1);
    }

    offset = rpmdbFirstRecNum(db);
    while (offset) {
	blockNum++;

	if (!dspBlockNum || dspBlockNum == blockNum) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, _("headerRead failed\n"));
		exit(1);
	    }
	  
	    headerDump(h, stdout, 1, rpmTagTable);
	    fprintf(stdout, "Offset: %d\n", offset);
	    headerFree(h);
	}
    
	if (dspBlockNum && blockNum > dspBlockNum) exit(0);

	offset = rpmdbNextRecNum(db, offset);
    }

    rpmdbClose(db);

    return 0;
}
