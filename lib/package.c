#include <fcntl.h>
#include <unistd.h>

#include "errno.h"
#include "header.h"
#include "package.h"
#include "rpmerr.h"
#include "rpmlead.h"

/* 0 = success */
/* 1 = bad magic */
/* 2 = error */
int pkgReadHeader(int fd, Header * hdr, int * isSource) {
    struct rpmlead lead;

    if (read(fd, &lead, sizeof(lead)) != sizeof(lead)) {
	error(RPMERR_READERROR, "read failed: %s (%d)", strerror(errno), 
	      errno);
	return 2;
    }
   
    if (lead.magic[0] != RPMLEAD_MAGIC0 || lead.magic[1] != RPMLEAD_MAGIC1 ||
	lead.magic[2] != RPMLEAD_MAGIC2 || lead.magic[3] != RPMLEAD_MAGIC3) {
	return 1;
    }

    *isSource = lead.type == RPMLEAD_SOURCE;

    if (lead.major == 1) {
	printf("old style packages are not yet supported\n");
	exit(1);
    } else if (lead.major == 2) {
	*hdr = readHeader(fd);
	if (! *hdr) return 2;
    } else {
	error(RPMERR_NEWPACKAGE, "old packages with major numbers <= 2 are"
		" supported by this version of RPM");
	return 2;
    } 

    return 0;
}
