#include "system.h"

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmlib.h>

#include "rpmlead.h"

/* The lead needs to be 8 byte aligned */

int writeLead(FD_t fd, struct rpmlead *lead)
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
	
    if (Fwrite(&l, sizeof(l), 1, fd) < 0) {
	return 1;
    }

    return 0;
}

int readLead(FD_t fd, struct rpmlead *lead)
{
    if (timedRead(fd, (char *)lead, sizeof(*lead)) != sizeof(*lead)) {
	rpmError(RPMERR_READERROR, _("read failed: %s (%d)"), strerror(errno), 
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

