/* Example 2 from Maximum RPM book by Edward C. Bailey
 * Updated for 2.5.6 20 Mar 99 by Scott Bronson
 * Updated again for 3.0.3 4 Oct 99 by Scott Bronson
 */

/* This iterates through all RPMs by name, using strcmp
 * to find the RPM named on the command line.  See Example 3
 * for a much better way of doing this
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
	int offset;
	int_32 type, count;
	char *name;
	rpmdb db;

	rpmReadConfigFiles( NULL, NULL );

	printf( "Calling rpmdbOpen\n" );
	if( rpmdbOpen( "", &db, O_RDONLY, 0644 ) != 0 ) {
		fprintf( stderr, "cannot open database!\n" );
		exit( 1 );
	}
	printf( "rpmdbOpen done.\n" );

	offset = rpmdbFirstRecNum( db );
	while( offset ) {
		h = rpmdbGetRecord( db, offset );
		if( !h ) {
			fprintf( stderr, "Header Read Failed!\n" );
			exit( 1 );
		}

		headerGetEntry( h, RPMTAG_NAME, &type, (void**)&name, &count );
		if( strcmp(name,argv[1]) == 0 ) {
			headerDump( h, stdout, 1, rpmTagTable );
		}

		headerFree( h );
		offset = rpmdbNextRecNum( db, offset );
	}

	rpmdbClose( db );

	return 0;
}

