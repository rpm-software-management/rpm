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

#include <rpm/rpmlib.h>


int main( int argc, char **argv )
{
	Header h;
	int stat;
	rpmdb db;
	dbiIndexSet matches;

	if( argc != 2 ) {
		fprintf( stderr, "usage: showdb2 <search term>\n" );
		exit( 1 );
	}

	rpmReadConfigFiles( NULL, NULL );

	if( rpmdbOpen( "", &db, O_RDONLY, 0644 ) != 0 ) {
		fprintf( stderr, "cannot open RPM database.\n" );
		exit( 1 );
	}

	stat = rpmdbFindPackage( db, argv[1], &matches );
	printf( "Status is: %d\n", stat );
	if( stat == 0 ) {
		if( matches.count ) {
			printf( "Number of matches: %d\n", matches.count );
			h = rpmdbGetRecord( db, matches.recs[0].recOffset );
			if( h ) {
				headerDump( h, stdout, HEADER_DUMP_INLINE, rpmTagTable );
				headerFree( h );
			}
		}
		dbiFreeIndexRecord( matches );
	}

	rpmdbClose( db );

	return 0;
}

