/* Example 3 from Maximum RPM book by Edward C. Bailey
 * Updated for 2.5.6 20 Mar 99 by Scott Bronson
 * Updated again for 3.0.3 4 Oct 99 by Scott Bronson
 */

/* This uses rpmlib to search the database for the RPM
 * named on the command line.
 */


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <rpmlib.h>

#ifndef	HEADER_DUMP_INLINE
/**
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param flags		0 or HEADER_DUMP_INLINE
 * @param tags		array of tag name/value pairs
 */
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry_s * tags)
	/*@globals fileSystem @*/
	/*@modifies f, fileSystem @*/;
#define HEADER_DUMP_INLINE   1
#endif

int main( int argc, char **argv )
{
    rpmdbMatchIterator mi;
    Header h;
    rpmdb db;

    if( argc != 2 ) {
	fprintf( stderr, "usage: showdb2 <search term>\n" );
	exit(EXIT_FAILURE);
    }

    rpmReadConfigFiles( NULL, NULL );

    if( rpmdbOpen( "", &db, O_RDONLY, 0644 ) != 0 ) {
	fprintf( stderr, "cannot open RPM database.\n" );
	exit( 1 );
    }

    mi = rpmdbInitIterator(db, RPMTAG_BASENAMES, argv[1], 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {

	headerDump( h, stdout, HEADER_DUMP_INLINE, rpmTagTable );

	/*
	 * Note that the header reference is "owned" by the iterator,
	 * so no headerFree() is necessary.
	 */
    }
    mi = rpmdbFreeIterator(mi);

    rpmdbClose( db );

    return 0;
}

