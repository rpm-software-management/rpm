/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#if !defined(__LCLINT__)
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmlib.h>

#include "misc.h"
#include "rpmlead.h"
#include "signature.h"
#include "debug.h"

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/*@access Header@*/		/* XXX compared with NULL */

void headerMergeLegacySigs(Header h, const Header sig)
{
    HeaderIterator hi;
    int_32 tag, type, count;
    const void * ptr;

    for (hi = headerInitIterator(sig);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = headerFreeData(ptr, type))
    {
	switch (tag) {
	case RPMSIGTAG_SIZE:	tag = RPMTAG_SIGSIZE;	break;
	case RPMSIGTAG_LEMD5_1:	tag = RPMTAG_SIGLEMD5_1;break;
	case RPMSIGTAG_PGP:	tag = RPMTAG_SIGPGP;	break;
	case RPMSIGTAG_LEMD5_2:	tag = RPMTAG_SIGLEMD5_2;break;
	case RPMSIGTAG_MD5:	tag = RPMTAG_SIGMD5;	break;
	case RPMSIGTAG_GPG:	tag = RPMTAG_SIGGPG;	break;
	case RPMSIGTAG_PGP5:	tag = RPMTAG_SIGPGP5;	break;
	default:					break;
	    continue;
	    /*@notreached@*/ break;
	}
	if (!headerIsEntry(h, tag))
	    headerAddEntry(h, tag, type, ptr, count);
    }
    headerFreeIterator(hi);
}

Header headerRegenSigHeader(const Header h)
{
    Header sig = rpmNewSignature();
    HeaderIterator hi;
    int_32 tag, stag, type, count;
    const void * ptr;

    for (hi = headerInitIterator(h);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = headerFreeData(ptr, type))
    {
	switch (tag) {
	case RPMTAG_SIGSIZE:	stag = RPMSIGTAG_SIZE;	break;
	case RPMTAG_SIGLEMD5_1:	stag = RPMSIGTAG_LEMD5_1;break;
	case RPMTAG_SIGPGP:	stag = RPMSIGTAG_PGP;	break;
	case RPMTAG_SIGLEMD5_2:	stag = RPMSIGTAG_LEMD5_2;break;
	case RPMTAG_SIGMD5:	stag = RPMSIGTAG_MD5;	break;
	case RPMTAG_SIGGPG:	stag = RPMSIGTAG_GPG;	break;
	case RPMTAG_SIGPGP5:	stag = RPMSIGTAG_PGP5;	break;
	default:
	    continue;
	    /*@notreached@*/ break;
	}
	if (!headerIsEntry(sig, stag))
	    headerAddEntry(sig, stag, type, ptr, count);
    }
    headerFreeIterator(hi);
    return sig;
}

/**
 * Retrieve package components from file handle.
 * @param fd		file handle
 * @param leadPtr	address of lead (or NULL)
 * @param sigs		address of signatures (or NULL)
 * @param hdrPtr	address of header (or NULL)
 * @return		0 on success, 1 on bad magic, 2 on error
 */
static int readPackageHeaders(FD_t fd, /*@out@*/ struct rpmlead * leadPtr, 
			      /*@out@*/ Header * sigs, /*@out@*/ Header *hdrPtr)
	/*@modifies fd, *leadPtr, *sigs, *hdrPtr @*/
{
    Header hdrBlock;
    struct rpmlead leadBlock;
    Header * hdr = NULL;
    struct rpmlead * lead;
    char * defaultPrefix;
    struct stat sb;
    int_32 true = 1;

    hdr = hdrPtr ? hdrPtr : &hdrBlock;
    lead = leadPtr ? leadPtr : &leadBlock;

    fstat(Fileno(fd), &sb);
    /* if fd points to a socket, pipe, etc, sb.st_size is *always* zero */
    if (S_ISREG(sb.st_mode) && sb.st_size < sizeof(*lead)) return 1;

    if (readLead(fd, lead))
	return 2;

    if (lead->magic[0] != RPMLEAD_MAGIC0 || lead->magic[1] != RPMLEAD_MAGIC1 ||
	lead->magic[2] != RPMLEAD_MAGIC2 || lead->magic[3] != RPMLEAD_MAGIC3) {
	return 1;
    }

    switch (lead->major) {
    case 1:
	rpmError(RPMERR_NEWPACKAGE,
	    _("packaging version 1 is not supported by this version of RPM\n"));
	return 2;
	/*@notreached@*/ break;
    case 2:
    case 3:
    case 4:
	if (rpmReadSignature(fd, sigs, lead->signature_type))
	    return 2;
	*hdr = headerRead(fd, (lead->major >= 3)
			  ? HEADER_MAGIC_YES : HEADER_MAGIC_NO);
	if (*hdr == NULL) {
	    if (sigs != NULL)
		headerFree(*sigs);
	    return 2;
	}

	/* We don't use these entries (and rpm >= 2 never have) and they are
	   pretty misleading. Let's just get rid of them so they don't confuse
	   anyone. */
	if (headerIsEntry(*hdr, RPMTAG_FILEUSERNAME))
	    headerRemoveEntry(*hdr, RPMTAG_FILEUIDS);
	if (headerIsEntry(*hdr, RPMTAG_FILEGROUPNAME))
	    headerRemoveEntry(*hdr, RPMTAG_FILEGIDS);

	/* We switched the way we do relocateable packages. We fix some of
	   it up here, though the install code still has to be a bit 
	   careful. This fixup makes queries give the new values though,
	   which is quite handy. */
	if (headerGetEntry(*hdr, RPMTAG_DEFAULTPREFIX, NULL,
			   (void **) &defaultPrefix, NULL)) {
	    defaultPrefix =
		stripTrailingChar(alloca_strdup(defaultPrefix), '/');
	    headerAddEntry(*hdr, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
			   &defaultPrefix, 1); 
	}

	/* The file list was moved to a more compressed format which not
	   only saves memory (nice), but gives fingerprinting a nice, fat
	   speed boost (very nice). Go ahead and convert old headers to
	   the new style (this is a noop for new headers) */
	if (lead->major < 4)
	    compressFilelist(*hdr);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
        if (lead->type == RPMLEAD_SOURCE) {
	    if (!headerIsEntry(*hdr, RPMTAG_SOURCEPACKAGE))
	    	headerAddEntry(*hdr, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&true, 1);
	} else if (lead->major < 4) {
    /* Retrofit "Provide: name = EVR" for binary packages. */
	    providePackageNVR(*hdr);
	}
	break;

    default:
	rpmError(RPMERR_NEWPACKAGE, _("only packaging with major numbers <= 4 "
		"is supported by this version of RPM\n"));
	return 2;
	/*@notreached@*/ break;
    } 

    if (hdrPtr == NULL)
	headerFree(*hdr);
    
    return 0;
}

int rpmReadPackageInfo(FD_t fd, Header * sigp, Header * hdrp)
{
    int rc = readPackageHeaders(fd, NULL, sigp, hdrp);
    if (rc)
	return rc;
    if (hdrp && *hdrp && sigp && *sigp)
	headerMergeLegacySigs(*hdrp, *sigp);
    return rc;
}

int rpmReadPackageHeader(FD_t fd, Header * hdrp, int * isSource, int * major,
		  int * minor)
{
    struct rpmlead lead;
    Header sig = NULL;
    int rc = readPackageHeaders(fd, &lead, &sig, hdrp);

    if (rc)
	goto exit;

    if (hdrp && *hdrp && sig) {
	headerMergeLegacySigs(*hdrp, sig);
	headerFree(sig);
    }
   
    if (isSource) *isSource = lead.type == RPMLEAD_SOURCE;
    if (major) *major = lead.major;
    if (minor) *minor = lead.minor;
   
exit:
    return rc;
}
