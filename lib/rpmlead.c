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
#include "lib/header_internal.h"	/* Freadall() */
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

rpmlead rpmLeadFromHeader(Header h)
{
    rpmlead l = NULL;

    if (h != NULL) {
	int archnum, osnum;
	char * nevr = headerGetAsString(h, RPMTAG_NEVR);

	/* FIXME: should grab these from header instead (RhBug:717898) */
	rpmGetArchInfo(NULL, &archnum);
	rpmGetOsInfo(NULL, &osnum);

	l = xcalloc(1, sizeof(*l));
	l->major = 3;
	l->minor = 0;
	l->archnum = archnum;
	l->osnum = osnum;
	l->signature_type = RPMSIGTYPE_HEADERSIG;
	l->type = (headerIsSource(h) ? 1 : 0);

	memcpy(l->magic, lead_magic, sizeof(l->magic));
	rstrlcpy(l->name, nevr, sizeof(l->name));

	free(nevr);
    }

    return l;
}

rpmlead rpmLeadFree(rpmlead lead)
{
    free(lead);
    return NULL;
}

/* The lead needs to be 8 byte aligned */
rpmRC rpmLeadWrite(FD_t fd, rpmlead lead)
{
    rpmRC rc = RPMRC_FAIL;

    if (lead != NULL) {
	struct rpmlead_s l;
	memcpy(&l, lead, sizeof(l));
	
	l.type = htons(lead->type);
	l.archnum = htons(lead->archnum);
	l.osnum = htons(lead->osnum);
	l.signature_type = htons(lead->signature_type);
	    
	if (Fwrite(&l, 1, sizeof(l), fd) == sizeof(l))
	    rc = RPMRC_OK;
    }
    return rc;
}

static rpmRC rpmLeadCheck(rpmlead lead, char **msg)
{
    if (memcmp(lead->magic, lead_magic, sizeof(lead_magic))) {
	*msg = xstrdup(_("not an rpm package"));
	return RPMRC_NOTFOUND;
    }
    if (lead->signature_type != RPMSIGTYPE_HEADERSIG) {
	*msg = xstrdup(_("illegal signature type"));
	return RPMRC_FAIL;
    }
    if (lead->major < 3 || lead->major > 4) {
	*msg = xstrdup(_("unsupported RPM package version"));
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

rpmRC rpmLeadRead(FD_t fd, rpmlead *lead, int *type, char **emsg)
{
    rpmRC rc = RPMRC_OK;
    struct rpmlead_s l;
    char *err = NULL;

    memset(&l, 0, sizeof(l));
    if (Freadall(fd, &l, sizeof(l)) != sizeof(l)) {
	if (Ferror(fd)) {
	    rasprintf(&err, _("read failed: %s (%d)\n"), Fstrerror(fd), errno);
	    rc = RPMRC_FAIL;
	} else {
	    err = xstrdup(_("not an rpm package\n"));
	    rc = RPMRC_NOTFOUND;
	}
    } else {
	l.type = ntohs(l.type);
	l.archnum = ntohs(l.archnum);
	l.osnum = ntohs(l.osnum);
	l.signature_type = ntohs(l.signature_type);
	rc = rpmLeadCheck(&l, &err);
    }

    if (rc == RPMRC_OK) {
	if (lead != NULL) {
	    *lead = xmalloc(sizeof(l));
	    memcpy(*lead, &l, sizeof(l));
	}
	if (type != NULL)
	    *type = l.type;
    } else {
	if (emsg != NULL)
	    *emsg = err;
	else
	    free(err);
    }

    return rc;
}
