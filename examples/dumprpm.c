/* Example 1 from Maximum RPM book by Edward C. Bailey
 * Updated for 2.5.6 Mar 99 by Scott Bronson
 * Updated again for 3.0.3, 4 Oct 99.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <rpm/rpmlib.h>


int main( int argc, char **argv )
{
	HeaderIterator iter;
	Header h, sig;
	int_32 itertag, type, count;
	void *p = NULL;
	char *name;
	FD_t fd;
	int stat;

	if( argc == 1 ) {
		fd = fdDup( STDIN_FILENO );
	} else {
		fd = fdOpen( argv[1], O_RDONLY, 0644 );
	}

	if( !fdValid(fd) ) {
		perror( "open" );
		exit( 1 );
	}

	stat = rpmReadPackageInfo( fd, &sig, &h );
	if( stat ) {
		fprintf( stderr, "rpmReadPackageInfo error status: %d\n%s\n",
				stat, strerror(errno) );
		exit( stat );
	}

	headerGetEntry( h, RPMTAG_NAME, &type, (void**)&name, &count );

	if( headerIsEntry(h,RPMTAG_PREIN) ) {
		printf( "There is a preinstall script for %s\n", name );
	}
	if( headerIsEntry(h,RPMTAG_POSTIN) ) {
		printf( "There is a postinstall script for %s\n", name );
	}


	/* NOTE:
	 * This is actually rather a ridiculous thing to do, since headerDump
	 * will incorrectly assume header types (RPMTAG_NAME, RPMTAG_RELEASE,
     * RPMTAG_SUMMARY).  Since we're passing a signature, the correct types
	 * would be (RPMSIGTAG_SIZE, RPMSIGTAG_PGP, and RPMSIGTAG_MD5).
	 * TO FIX:
     * I think rpm's tagtable.c should define an rpmSigTable global var.
	 * This is the perfect dual to rpmTagTable and would be passed to
	 * headerDump when appropriate.  It looks like someone intended to do
	 * this at one time, but never finished it?
     */

	printf( "Dumping signatures...\n" );
	/* Use HEADER_DUMP_INLINE to include inline dumps of header items */
	headerDump( sig, stdout, HEADER_DUMP_INLINE, rpmTagTable );

	rpmFreeSignature( sig );

	printf( "Iterating through the header...\n" );
	iter = headerInitIterator( h );
	while( headerNextIterator( iter, &itertag, &type, &p, &count ) ) {
		/* printf( "itertag=%04d, type=%08lX, p=%08lX, c=%08lX\n",
			(int)itertag, (long)type, (long)p, (long)count ); */

		switch( itertag ) {
		case RPMTAG_SUMMARY:
			if( type == RPM_I18NSTRING_TYPE ) {
				/* We'll only print out the first string if there's an array */
				printf( "The Summary: \"%s\"\n", *(char**)p );
			}
			if( type == RPM_STRING_TYPE ) {
				printf( "The Summary: \"%s\"\n", (char*)p );
			}
			break;
		case RPMTAG_FILENAMES:
			printf( "There are %d files in %s\n", count, name );
			break;
		}

		/* rpmlib allocates a buffer to return these two values... */
		if( type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE ) {
			free( p );
		}
	}

	headerFreeIterator( iter );
	headerFree( h );

	return 0;
}


