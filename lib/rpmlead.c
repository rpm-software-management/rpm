#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "rpmerr.h"
#include "rpmlead.h"

/* The lead needs to be 8 byte aligned */

int writeLead(int fd, struct rpmlead *lead)
{
    struct rpmlead l;

    memcpy(&l, lead, sizeof(*lead));
    
    l.magic[0] = RPMLEAD_MAGIC0;
    l.magic[1] = RPMLEAD_MAGIC1;
    l.magic[2] = RPMLEAD_MAGIC2;
    l.magic[3] = RPMLEAD_MAGIC3;

    l.type = htons(l.type);
    l.archnum = htons(l.archnum);
    l.osnum = htons(l.osnum);
    l.signature_type = htons(l.signature_type);
	
    write(fd, &l, sizeof(l));

    return 0;
}

int readLead(int fd, struct rpmlead *lead)
{
    if (read(fd, lead, sizeof(*lead)) != sizeof(*lead)) {
	error(RPMERR_READERROR, "read failed: %s (%d)", strerror(errno), 
	      errno);
	return 1;
    }

    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);

    if (lead->major >= 2)
	lead->signature_type = ntohs(lead->signature_type);

    return 0;
}

