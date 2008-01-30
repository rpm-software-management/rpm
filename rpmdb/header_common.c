/** \ingroup header
 * \file rpmdb/header_common.c
 */

#include <rpm/header.h>
#include "rpmdb/header_method.h"

/** \ingroup header
 * Header methods for rpm headers.
 */
extern struct HV_s * hdrVec;

/** \ingroup header
 */
static inline HV_t h2hv(Header h)
{
    return ((HV_t)h);
}

Header headerNew(void)
{
    return hdrVec->hdrnew();
}

Header headerFree( Header h)
{
    if (h == NULL) return NULL;
    return (h2hv(h)->hdrfree) (h);
}

Header headerLink(Header h)
{
    return (h2hv(h)->hdrlink) (h);
}

Header headerUnlink(Header h)
{
    if (h == NULL) return NULL;
    return (h2hv(h)->hdrunlink) (h);
}

void headerSort(Header h)
{
/* FIX: add rc */
    (h2hv(h)->hdrsort) (h);
    return;
}

void headerUnsort(Header h)
{
/* FIX: add rc */
    (h2hv(h)->hdrunsort) (h);
    return;
}

unsigned int headerSizeof(Header h, enum hMagic magicp)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrsizeof) (h, magicp);
}

void * headerUnload(Header h)
{
    return (h2hv(h)->hdrunload) (h);
}

Header headerReload(Header h, int tag)
{
    return (h2hv(h)->hdrreload) (h, tag);
}

Header headerCopy(Header h)
{
    return (h2hv(h)->hdrcopy) (h);
}

Header headerLoad(void * uh)
{
    return hdrVec->hdrload(uh);
}

Header headerCopyLoad(const void * uh)
{
    return hdrVec->hdrcopyload(uh);
}

Header headerRead(FD_t fd, enum hMagic magicp)
{
    return hdrVec->hdrread(fd, magicp);
}

int headerWrite(FD_t fd, Header h, enum hMagic magicp)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrwrite) (fd, h, magicp);
}

int headerIsEntry(Header h, rpm_tag_t tag)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrisentry) (h, tag);
}

void * headerFreeTag(Header h, rpm_data_t data, rpm_tagtype_t type)
{
    return (h2hv(h)->hdrfreetag) (h, data, type);
}

int headerGetEntry(Header h, rpm_tag_t tag,
			rpm_tagtype_t * type,
			rpm_data_t * p,
			rpm_count_t * c)
{
    return (h2hv(h)->hdrget) (h, tag, type, p, c);
}

int headerGetEntryMinMemory(Header h, rpm_tag_t tag,
			rpm_tagtype_t * type,
			rpm_data_t * p, 
			rpm_count_t * c)
{
    return (h2hv(h)->hdrgetmin) (h, tag, type, p, c);
}

int headerAddEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type, 
			rpm_constdata_t p, rpm_count_t c)
{
    return (h2hv(h)->hdradd) (h, tag, type, p, c);
}

int headerAppendEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
		rpm_constdata_t p, rpm_count_t c)
{
    return (h2hv(h)->hdrappend) (h, tag, type, p, c);
}

int headerAddOrAppendEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
		rpm_constdata_t p, rpm_count_t c)
{
    return (h2hv(h)->hdraddorappend) (h, tag, type, p, c);
}

int headerAddI18NString(Header h, rpm_tag_t tag, const char * string,
		const char * lang)
{
    return (h2hv(h)->hdraddi18n) (h, tag, string, lang);
}

int headerModifyEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
			rpm_constdata_t p, rpm_count_t c)
{
    return (h2hv(h)->hdrmodify) (h, tag, type, p, c);
}

int headerRemoveEntry(Header h, rpm_tag_t tag)
{
    return (h2hv(h)->hdrremove) (h, tag);
}

char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg)
{
    return (h2hv(h)->hdrsprintf) (h, fmt, tbltags, extensions, errmsg);
}

void headerCopyTags(Header headerFrom, Header headerTo, rpm_tag_t * tagstocopy)
{
/* FIX: add rc */
    hdrVec->hdrcopytags(headerFrom, headerTo, tagstocopy);
    return;
}

HeaderIterator headerFreeIterator(HeaderIterator hi)
{
    return hdrVec->hdrfreeiter(hi);
}

HeaderIterator headerInitIterator(Header h)
{
    return hdrVec->hdrinititer(h);
}

int headerNextIterator(HeaderIterator hi,
		rpm_tag_t * tag,
		rpm_tagtype_t * type,
		rpm_data_t * p,
		rpm_count_t * c)
{
    return hdrVec->hdrnextiter(hi, tag, type, p, c);
}

void * headerFreeData(rpm_data_t data, rpm_tagtype_t type)
{
    if (data) {
	if (type == RPM_FORCEFREE_TYPE ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free(data); /* XXX _constfree() */
    }
    return NULL;
}
