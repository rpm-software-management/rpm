#include <stdlib.h>
#include <stdio.h>
#include "rpmchk.h"

void usage(char *progname)
{
fprintf(stderr,"Usage: %s rpmname\n",progname);
exit(1);
}

int main(int argc, char *argv[])
{
RpmFile	*rpmfile;
char	*ptr;

if( argc != 2 ) {
	fprintf(stderr, "%s: bad argument count %d\n", argv[0], argc );
	usage(argv[0]);
	}

if( (rpmfile = OpenRpmFile(argv[1])) == NULL ) {
	fprintf(stderr, "%s: Unable to open file %s\n", argv[0], argv[1] );
	usage(argv[0]);
	}

if( (ptr=getenv("RPMCHK_DEBUG")) != NULL ) {
        rpmchkdebug=strtod(ptr,NULL);
        if( rpmchkdebug&DEBUG_ENV_OVERRIDES )
                fprintf(stderr,"rpmchk debug set to 0x%x\n", rpmchkdebug );
        }

checkRpm(rpmfile, NULL);

exit(0);
}
