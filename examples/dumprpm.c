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

static const struct headerTagTableEntry_s rpmSigTagTbl[] = {
        { "RPMSIGTAG_SIZE", RPMSIGTAG_SIZE, },
        { "RPMSIGTAG_PGP", RPMSIGTAG_PGP, },
        { "RPMSIGTAG_MD5", RPMSIGTAG_MD5, },
        { "RPMSIGTAG_GPG", RPMSIGTAG_GPG, },
	{ NULL, 0 }
};

int main( int argc, char **argv )
{
    HeaderIterator hi;
    Header h, sig;
    int_32 tag, type, count;
    void * p = NULL;
    char * name;
    FD_t fd;
    int rc;

    if( argc == 1 )
	fd = Fopen("-", "r.ufdio" );
    else
	fd = Fopen( argv[1], "r.ufdio" );

    if( fd == NULL || Ferror(fd) ) {
	fprintf(stderr, "cannot open %s: %s\n",
                (argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fd));
        exit(EXIT_FAILURE);
    }

    rc = rpmReadPackageInfo( fd, &sig, &h );
    if ( rc ) {
        fprintf( stderr, "rpmReadPackageInfo error status: %d\n\n", rc );
        exit(EXIT_FAILURE);
    }

    headerGetEntry( h, RPMTAG_NAME, &type, (void**)&name, &count );

    if( headerIsEntry(h,RPMTAG_PREIN) ) {
	printf( "There is a preinstall script for %s\n", name );
    }
    if( headerIsEntry(h,RPMTAG_POSTIN) ) {
	printf( "There is a postinstall script for %s\n", name );
    }


    printf( "Dumping signatures...\n" );
    /* Use HEADER_DUMP_INLINE to include inline dumps of header items */
    headerDump( sig, stdout, HEADER_DUMP_INLINE, rpmSigTagTbl );

    rpmFreeSignature( sig );

    printf( "Iterating through the header...\n" );
    hi = headerInitIterator( h );
    while( headerNextIterator( hi, &tag, &type, &p, &count ) ) {
	/* printf( "tag=%04d, type=%08lX, p=%08lX, c=%08lX\n",
		(int)tag, (long)type, (long)p, (long)count ); */

	switch( tag ) {
	case RPMTAG_SUMMARY:
		if( type == RPM_I18NSTRING_TYPE ) {
			/* We'll only print out the first string if there's an array */
			printf( "The Summary: \"%s\"\n", *(char**)p );
		}
		if( type == RPM_STRING_TYPE ) {
			printf( "The Summary: \"%s\"\n", (char*)p );
		}
		break;
	case RPMTAG_BASENAMES:
	    printf( "There are %d files in %s\n", count, name );
		break;
	}

	/* rpmlib allocates a buffer to return these two values... */
	headerFreeData(p, type);
    }

    headerFreeIterator( hi );
    headerFree( h );

    return 0;
}
