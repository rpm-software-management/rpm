/** \ingroup lead
 * \file lib/rpmlead.c
 */

#include "system.h"

#include <errno.h>
#include <netinet/in.h>

#include <rpm/rpmlib.h>		/* rpmGetOs/ArchInfo() */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "lib/signature.h"
#include "lib/rpmlead.h"

#include "debug.h"

static unsigned char const lead_magic[] = {
    RPMLEAD_MAGIC0, RPMLEAD_MAGIC1, RPMLEAD_MAGIC2, RPMLEAD_MAGIC3
};

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead_s {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;       /*!< Signature header type (RPMSIG_HEADERSIG) */
    char reserved[16];      /*!< Pad to 96 bytes -- 8 byte aligned! */
};

rpmlead rpmLeadNew(void)
{
    int archnum, osnum;
    rpmlead l = xcalloc(1, sizeof(*l));

    rpmGetArchInfo(NULL, &archnum);
    rpmGetOsInfo(NULL, &osnum);

    l->major = 3;
    l->minor = 0;
    l->archnum = archnum;
    l->osnum = osnum;
    l->signature_type = RPMSIGTYPE_HEADERSIG;
    return l;
}

rpmlead rpmLeadFromHeader(Header h)
{
    char * nevr;
    assert(h != NULL);
    rpmlead l = rpmLeadNew();

    l->type = (headerIsSource(h) ? 1 : 0);
    nevr = headerGetAsString(h, RPMTAG_NEVR);
    rstrlcpy(l->name, nevr, sizeof(l->name));
    free(nevr);

    return l;
}

rpmlead rpmLeadFree(rpmlead lead)
{
    assert(lead != NULL);
    free(lead);
    return NULL;
}

/* The lead needs to be 8 byte aligned */
rpmRC rpmLeadWrite(FD_t fd, rpmlead lead)
{
    struct rpmlead_s l;
    assert(lead != NULL);

    memcpy(&l, lead, sizeof(l));
    
    memcpy(&l.magic, lead_magic, sizeof(l.magic));
    l.type = htons(lead->type);
    l.archnum = htons(lead->archnum);
    l.osnum = htons(lead->osnum);
    l.signature_type = htons(lead->signature_type);
	
    if (Fwrite(&l, 1, sizeof(l), fd) != sizeof(l))
	return RPMRC_FAIL;

    return RPMRC_OK;
}

rpmRC rpmLeadCheck(rpmlead lead, const char **msg)
{
    if (memcmp(lead->magic, lead_magic, sizeof(lead_magic))) {
	if (msg) *msg = _("not an rpm package");
	return RPMRC_NOTFOUND;
    }
    if (lead->signature_type != RPMSIGTYPE_HEADERSIG) {
	if (msg) *msg = _("illegal signature type");
	return RPMRC_FAIL;
    }
    if (lead->major < 3 || lead->major > 4) {
	if (msg) *msg = _("unsupported RPM package version");
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

rpmRC rpmLeadRead(FD_t fd, rpmlead lead)
{
    assert(lead != NULL);
    memset(lead, 0, sizeof(*lead));
    /* FIX: remove timed read */
    if (timedRead(fd, (char *)lead, sizeof(*lead)) != sizeof(*lead)) {
	if (Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("read failed: %s (%d)\n"),
			Fstrerror(fd), errno);
	    return RPMRC_FAIL;
	} else {
	    rpmlog(RPMLOG_ERR, _("not an rpm package\n"));
	    return RPMRC_NOTFOUND;
	}
    }
    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);
    lead->signature_type = ntohs(lead->signature_type);

    return RPMRC_OK;
}

int rpmLeadType(rpmlead lead)
{
    return lead ? lead->type : -1;
}
