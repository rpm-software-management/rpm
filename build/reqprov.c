/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include "build/rpmbuild_internal.h"
#include "debug.h"

static int isNewDep(rpmds *dsp, rpmds bds,
		  Header h, rpmTagVal indextag, uint32_t index)
{
    int isnew = 1;

    if (!indextag) {
	/* With normal deps, we can just merge and see if anything got added */
	isnew = (rpmdsMerge(dsp, bds) > 0);
    } else {
	struct rpmtd_s idx;
	rpmds ads = *dsp;
	headerGet(h, indextag, &idx, HEADERGET_MINMEM);

	/* rpmdsFind/Merge() probably isn't realiable with triggers... */
	rpmdsInit(ads);
	while (isnew && rpmdsNext(ads) >= 0) {
	    if (!rstreq(rpmdsN(ads), rpmdsN(bds))) continue;
	    if (!rstreq(rpmdsEVR(ads), rpmdsEVR(bds))) continue;
	    if (rpmdsFlags(ads) != rpmdsFlags(bds)) continue;
	    if (indextag && rpmtdSetIndex(&idx, rpmdsIx(ads)) >= 0 &&
			    rpmtdGetNumber(&idx) != index) continue;
	    isnew = 0;
	}
	rpmtdFreeData(&idx);
	rpmdsMerge(dsp, bds);
    }

    return isnew;
}

int addReqProv(Package pkg, rpmTagVal tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index)
{
    rpmTagVal versiontag = 0;
    rpmTagVal flagtag = 0;
    rpmTagVal indextag = 0;
    rpmsenseFlags extra = RPMSENSE_ANY;
    Header h = pkg->header; /* just a shortcut */
    rpmds newds, *dsp = NULL;

    switch (tagN) {
    case RPMTAG_PROVIDENAME:
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = Flags & RPMSENSE_FIND_PROVIDES;
	dsp = &pkg->provides;
	break;
    case RPMTAG_OBSOLETENAME:
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
	dsp = &pkg->obsoletes;
	break;
    case RPMTAG_CONFLICTNAME:
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
	dsp = &pkg->conflicts;
	break;
    case RPMTAG_ORDERNAME:
	versiontag = RPMTAG_ORDERVERSION;
	flagtag = RPMTAG_ORDERFLAGS;
	dsp = &pkg->order;
	break;
    case RPMTAG_TRIGGERNAME:
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = Flags & RPMSENSE_TRIGGER;
	dsp = &pkg->triggers;
	break;
    case RPMTAG_REQUIRENAME:
    default:
	tagN = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
	dsp = &pkg->requires;
    }

    /* rpmlib() dependency sanity: only requires permitted, ensure sense bit */
    if (rstreqn(N, "rpmlib(", sizeof("rpmlib(")-1)) {
	if (tagN != RPMTAG_REQUIRENAME) return 1;
	extra |= RPMSENSE_RPMLIB;
    }

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";
    
    newds = rpmdsSinglePool(pkg->pool, tagN, N, EVR, Flags);
    /* Avoid adding duplicate dependencies. */
    if (isNewDep(dsp, newds, h, indextag, index)) {
	headerPutString(h, tagN, N);
	headerPutString(h, versiontag, EVR);
	headerPutUint32(h, flagtag, &Flags, 1);
	if (indextag) {
	    headerPutUint32(h, indextag, &index, 1);
	}
    }
    rpmdsFree(newds);

    return 0;
}

int rpmlibNeedsFeature(Package pkg, const char * feature, const char * featureEVR)
{
    char *reqname = NULL;
    int res;

    rasprintf(&reqname, "rpmlib(%s)", feature);

    res = addReqProv(pkg, RPMTAG_REQUIRENAME, reqname, featureEVR,
		     RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL), 0);

    free(reqname);

    return res;
}
