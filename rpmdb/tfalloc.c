#include "system.h"
#include <rpmlib.h>
#include "rpmio_internal.h"
#include "falloc.h"
#include "debug.h"

#define	RPMDBFN	"/var/lib/rpm/packages.rpm"
static const char * rpmdbfn = RPMDBFN;

int main(int argc, const char ** argv)
{
    int lasto = 0;
    FD_t fd;


    fd = fadOpen(rpmdbfn, O_RDONLY, 0644);
    if (fd == NULL) {
	fprintf(stderr, "fadOpen(%s) failed\n", rpmdbfn);
	exit(1);
    }
fprintf(stderr, "%s: size %d(0x%08x)\n", rpmdbfn, fadGetFileSize(fd), (unsigned) fadGetFileSize(fd));

    while ((lasto = fadNextOffset(fd, lasto)) > 0) {
fprintf(stderr, "%10d\n", lasto);
    }

    (void) Fclose(fd);

    return 0;
}
