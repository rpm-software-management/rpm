/** \ingroup lead
 * \file lib/rpmlead.c
 */

#include "system.h"

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#include <netinet/in.h>

#include <rpmlib.h>

#include "rpmlead.h"
#include "debug.h"

/* The lead needs to be 8 byte aligned */

int writeLead(FD_t fd, const struct rpmlead *lead)
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
	
    if (Fwrite(&l, sizeof(char), sizeof(l), fd) != sizeof(l)) {
	return 1;
    }

    return 0;
}

int readLead(FD_t fd, struct rpmlead *lead)
{
    memset(lead, 0, sizeof(*lead));
    if (timedRead(fd, (char *)lead, sizeof(*lead)) != sizeof(*lead)) {
	rpmError(RPMERR_READ, _("read failed: %s (%d)\n"), Fstrerror(fd), 
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
