#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/byteorder.h>

#include "rpmlead.h"

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
    read(fd, lead, sizeof(*lead));

    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);
    lead->signature_type = ntohs(lead->signature_type);

    return 0;
}

