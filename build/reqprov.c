/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmstring.h>
#include "build/rpmbuild_misc.h"
#include "debug.h"

static int isNewDep(Header h, rpmTagVal nametag,
		  const char *N, const char *EVR, rpmsenseFlags Flags,
		  rpmTagVal indextag, uint32_t index)
{
    int isnew = 1;
    struct rpmtd_s idx;
    rpmds ads = rpmdsNew(h, nametag, 0);
    rpmds bds = rpmdsSingle(nametag, N, EVR, Flags);

    if (indextag) {
	headerGet(h, indextag, &idx, HEADERGET_MINMEM);
    }

    /* XXX there's no guarantee the ds is sorted here so rpmdsFind() wont do */
    rpmdsInit(ads);
    while (isnew && rpmdsNext(ads) >= 0) {
	if (!rstreq(rpmdsN(ads), rpmdsN(bds))) continue;
	if (!rstreq(rpmdsEVR(ads), rpmdsEVR(bds))) continue;
	if (rpmdsFlags(ads) != rpmdsFlags(bds)) continue;
	if (indextag && rpmtdSetIndex(&idx, rpmdsIx(ads)) >= 0 &&
			rpmtdGetNumber(&idx) != index) continue;
	isnew = 0;
    }
    
    if (indextag) {
	rpmtdFreeData(&idx);
    }
    rpmdsFree(ads);
    rpmdsFree(bds);
    return isnew;
}

int addReqProv(Header h, rpmTagVal tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index)
{
    rpmTagVal versiontag = 0;
    rpmTagVal flagtag = 0;
    rpmTagVal indextag = 0;
    rpmsenseFlags extra = RPMSENSE_ANY;

    switch (tagN) {
    case RPMTAG_PROVIDENAME:
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = Flags & RPMSENSE_FIND_PROVIDES;
	break;
    case RPMTAG_OBSOLETENAME:
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
	break;
    case RPMTAG_CONFLICTNAME:
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
	break;
    case RPMTAG_ORDERNAME:
	versiontag = RPMTAG_ORDERVERSION;
	flagtag = RPMTAG_ORDERFLAGS;
	break;
    case RPMTAG_TRIGGERNAME:
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = Flags & RPMSENSE_TRIGGER;
	break;
    case RPMTAG_REQUIRENAME:
    default:
	tagN = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    }

    /* rpmlib() dependency sanity: only requires permitted, ensure sense bit */
    if (rstreqn(N, "rpmlib(", sizeof("rpmlib(")-1)) {
	if (tagN != RPMTAG_REQUIRENAME) return 1;
	extra |= RPMSENSE_RPMLIB;
    }

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";
    
    /* Avoid adding duplicate dependencies. */
    if (isNewDep(h, tagN, N, EVR, Flags, indextag, index)) {
	headerPutString(h, tagN, N);
	headerPutString(h, versiontag, EVR);
	headerPutUint32(h, flagtag, &Flags, 1);
	if (indextag) {
	    headerPutUint32(h, indextag, &index, 1);
	}
    }

    return 0;
}

int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR)
{
    char *reqname = NULL;
    int res;

    rasprintf(&reqname, "rpmlib(%s)", feature);

    res = addReqProv(h, RPMTAG_REQUIRENAME, reqname, featureEVR,
		     RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL), 0);

    free(reqname);

    return res;
}
