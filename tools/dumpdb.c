#include "system.h"

#include "rpmlib.h"

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
	fprintf(stderr, _("cannot open /var/lib/rpm/packages.rpm\n"));
	exit(1);
    }

    {	Header h = NULL;
	unsigned int blockNum = 0;
#ifdef	DYING
	unsigned int offset;
#define	_RECNUM	offset

	for (offset = rpmdbFirstRecNum(db);
	     offset > 0;
	     offset = rpmdbNextRecNum(db, offset))
	{
	    if (h) {
		headerFree(h);
		h = NULL;
	    }
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, _("rpmdbGetRecord() failed at offset %d\n"),
			offset);
		exit(1);
	    }
#else
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
#endif

	    blockNum++;
	    if (!(dspBlockNum != 0 && dspBlockNum != blockNum))
		continue;

	    headerDump(h, stdout, 1, rpmTagTable);
	    fprintf(stdout, "Offset: %d\n", _RECNUM);
    
	    if (dspBlockNum && blockNum > dspBlockNum)
		exit(0);

#ifndef DYING
	}
	rpmdbFreeIterator(mi);
#else
	}

	if (h) {
	    headerFree(h);
	    h = NULL;
	}
#endif

    }

    rpmdbClose(db);

    return 0;
}
