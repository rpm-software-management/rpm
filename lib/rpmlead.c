/** \ingroup lead
 * \file lib/rpmlead.c
 */

#include "system.h"

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#include <netinet/in.h>

#include <rpmlib.h>

#include "signature.h"
#include "rpmlead.h"
#include "debug.h"

/*@unchecked@*/ /*@observer@*/
static unsigned char lead_magic[] = {
    RPMLEAD_MAGIC0, RPMLEAD_MAGIC1, RPMLEAD_MAGIC2, RPMLEAD_MAGIC3
};

/* The lead needs to be 8 byte aligned */

rpmRC writeLead(FD_t fd, const struct rpmlead *lead)
{
    struct rpmlead l;

/*@-boundswrite@*/
    memcpy(&l, lead, sizeof(l));
    
    memcpy(&l.magic, lead_magic, sizeof(l.magic));
/*@=boundswrite@*/
    l.type = htons(l.type);
    l.archnum = htons(l.archnum);
    l.osnum = htons(l.osnum);
    l.signature_type = htons(l.signature_type);
	
/*@-boundswrite@*/
    if (Fwrite(&l, 1, sizeof(l), fd) != sizeof(l))
	return RPMRC_FAIL;
/*@=boundswrite@*/

    return RPMRC_OK;
}

rpmRC readLead(FD_t fd, struct rpmlead *lead)
{
/*@-boundswrite@*/
    memset(lead, 0, sizeof(*lead));
/*@=boundswrite@*/
    /*@-type@*/ /* FIX: remove timed read */
    if (timedRead(fd, (char *)lead, sizeof(*lead)) != sizeof(*lead)) {
	if (Ferror(fd)) {
	    rpmError(RPMERR_READ, _("read failed: %s (%d)\n"),
			Fstrerror(fd), errno);
	    return RPMRC_FAIL;
	}
	return RPMRC_NOTFOUND;
    }
    /*@=type@*/

    if (memcmp(lead->magic, lead_magic, sizeof(lead_magic)))
	return RPMRC_NOTFOUND;
    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);
    lead->signature_type = ntohs(lead->signature_type);
    if (lead->signature_type != RPMSIGTYPE_HEADERSIG)
	return RPMRC_NOTFOUND;

    return RPMRC_OK;
}
