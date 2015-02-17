/** \ingroup rpmdb
 * \file lib/headerutil.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/header.h>
#include <rpm/rpmstring.h>

#include "debug.h"

static int NEVRA(Header h, const char **np,
		 uint32_t **ep, const char **vp, const char **rp,
		 const char **ap)
{
    if (np) *np = headerGetString(h, RPMTAG_NAME);
    if (vp) *vp = headerGetString(h, RPMTAG_VERSION);
    if (rp) *rp = headerGetString(h, RPMTAG_RELEASE);
    if (ap) *ap = headerGetString(h, RPMTAG_ARCH);
    if (ep) {
	struct rpmtd_s td;
	headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_DEFAULT);
	*ep = rpmtdGetUint32(&td);
    }
    return 0;
}

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    return NEVRA(h, np, NULL, vp, rp, NULL);
}

int headerNEVRA(Header h, const char **np,
		uint32_t **ep, const char **vp, const char **rp,
		const char **ap)
{
    return NEVRA(h, np, ep, vp, rp, ap);
}

static char *getNEVRA(Header h, rpmTag tag, const char **np)
{
    if (np) *np = headerGetString(h, RPMTAG_NAME);
    return headerGetAsString(h, tag);
}

char * headerGetNEVR(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_NEVR, np);
}

char * headerGetNEVRA(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_NEVRA, np);
}

char * headerGetEVR(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_EVR, np);
}

rpm_color_t headerGetColor(Header h)
{
    return headerGetNumber(h, RPMTAG_HEADERCOLOR);
}

int headerIsSource(Header h)
{
    return (!headerIsEntry(h, RPMTAG_SOURCERPM));
}

Header headerCopy(Header h)
{
    Header nh = headerNew();
    HeaderIterator hi;
    struct rpmtd_s td;
   
    hi = headerInitIterator(h);
    while (headerNext(hi, &td)) {
	if (rpmtdCount(&td) > 0) {
	    (void) headerPut(nh, &td, HEADERPUT_DEFAULT);
	}
	rpmtdFreeData(&td);
    }
    headerFreeIterator(hi);

    return headerReload(nh, RPMTAG_HEADERIMAGE);
}

void headerCopyTags(Header headerFrom, Header headerTo, 
		    const rpmTagVal * tagstocopy)
{
    const rpmTagVal * p;
    struct rpmtd_s td;

    if (headerFrom == headerTo)
	return;

    for (p = tagstocopy; *p != 0; p++) {
	if (headerIsEntry(headerTo, *p))
	    continue;
	if (!headerGet(headerFrom, *p, &td, (HEADERGET_MINMEM|HEADERGET_RAW)))
	    continue;
	(void) headerPut(headerTo, &td, HEADERPUT_DEFAULT);
	rpmtdFreeData(&td);
    }
}

char * headerGetAsString(Header h, rpmTagVal tag)
{
    char *res = NULL;
    struct rpmtd_s td;

    if (headerGet(h, tag, &td, HEADERGET_EXT)) {
	if (rpmtdCount(&td) == 1) {
	    res = rpmtdFormat(&td, RPMTD_FORMAT_STRING, NULL);
	}
	rpmtdFreeData(&td);
    }
    return res;
}

const char * headerGetString(Header h, rpmTagVal tag)
{
    const char *res = NULL;
    struct rpmtd_s td;

    if (headerGet(h, tag, &td, HEADERGET_MINMEM)) {
	if (rpmtdCount(&td) == 1) {
	    res = rpmtdGetString(&td);
	}
	rpmtdFreeData(&td);
    }
    return res;
}

uint64_t headerGetNumber(Header h, rpmTagVal tag)
{
    uint64_t res = 0;
    struct rpmtd_s td;

    if (headerGet(h, tag, &td, HEADERGET_EXT)) {
	if (rpmtdCount(&td) == 1) {
	    res = rpmtdGetNumber(&td);
	}
	rpmtdFreeData(&td);
    }
    return res;
}

/*
 * Sanity check data types against tag table before putting. Assume
 * append on all array-types.
 */
static int headerPutType(Header h, rpmTagVal tag, rpmTagType reqtype,
			rpm_constdata_t data, rpm_count_t size)
{
    struct rpmtd_s td;
    rpmTagType type = rpmTagGetTagType(tag);
    rpmTagReturnType retype = rpmTagGetReturnType(tag);
    headerPutFlags flags = HEADERPUT_APPEND; 
    int valid = 1;

    /* Basic sanity checks: type must match and there must be data to put */
    if (type != reqtype 
	|| size < 1 || data == NULL || h == NULL) {
	valid = 0;
    }

    /*
     * Non-array types can't be appended to. Binary types use size
     * for data length, for other non-array types size must be 1.
     */
    if (retype != RPM_ARRAY_RETURN_TYPE) {
	flags = HEADERPUT_DEFAULT;
	if (type != RPM_BIN_TYPE && size != 1) {
	    valid = 0;
	}
    }

    if (valid) {
	rpmtdReset(&td);
	td.tag = tag;
	td.type = type;
	td.data = (void *) data;
	td.count = size;

	valid = headerPut(h, &td, flags);
    }

    return valid;
}
	
int headerPutString(Header h, rpmTagVal tag, const char *val)
{
    rpmTagType type = rpmTagGetTagType(tag);
    const void *sptr = NULL;

    /* string arrays expect char **, arrange that */
    if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE) {
	sptr = &val;
    } else if (type == RPM_STRING_TYPE) {
	sptr = val;
    } else {
	return 0;
    }

    return headerPutType(h, tag, type, sptr, 1);
}

int headerPutStringArray(Header h, rpmTagVal tag, const char **array, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_STRING_ARRAY_TYPE, array, size);
}

int headerPutChar(Header h, rpmTagVal tag, const char *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_CHAR_TYPE, val, size);
}

int headerPutUint8(Header h, rpmTagVal tag, const uint8_t *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_INT8_TYPE, val, size);
}

int headerPutUint16(Header h, rpmTagVal tag, const uint16_t *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_INT16_TYPE, val, size);
}

int headerPutUint32(Header h, rpmTagVal tag, const uint32_t *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_INT32_TYPE, val, size);
}

int headerPutUint64(Header h, rpmTagVal tag, const uint64_t *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_INT64_TYPE, val, size);
}

int headerPutBin(Header h, rpmTagVal tag, const uint8_t *val, rpm_count_t size)
{
    return headerPutType(h, tag, RPM_BIN_TYPE, val, size);
}

