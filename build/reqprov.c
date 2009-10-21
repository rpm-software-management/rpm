/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include "debug.h"

static int isNewDep(Header h, rpmTag nametag,
		  const char *N, const char *EVR, rpmsenseFlags Flags,
		  rpmTag indextag, uint32_t index)
{
    int new = 1;
    struct rpmtd_s idx;
    rpmds ads = rpmdsNew(h, nametag, 0);
    rpmds bds = rpmdsSingle(nametag, N, EVR, Flags);

    if (indextag) {
	headerGet(h, indextag, &idx, HEADERGET_MINMEM);
    }

    /* XXX there's no guarantee the ds is sorted here so rpmdsFind() wont do */
    rpmdsInit(ads);
    while (new && rpmdsNext(ads) >= 0) {
	if (!rstreq(rpmdsN(ads), rpmdsN(bds))) continue;
	if (!rstreq(rpmdsEVR(ads), rpmdsEVR(bds))) continue;
	if (rpmdsFlags(ads) != rpmdsFlags(bds)) continue;
	if (indextag && rpmtdSetIndex(&idx, rpmdsIx(ads)) >= 0 &&
			rpmtdGetNumber(&idx) != index) continue;
	new = 0;
    }
    
    if (indextag) {
	rpmtdFreeData(&idx);
    }
    rpmdsFree(ads);
    rpmdsFree(bds);
    return new;
}

int addReqProv(rpmSpec spec, Header h, rpmTag tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index)
{
    rpmTag nametag = 0;
    rpmTag versiontag = 0;
    rpmTag flagtag = 0;
    rpmTag indextag = 0;
    rpmsenseFlags extra = RPMSENSE_ANY;
    
    if (Flags & RPMSENSE_PROVIDES) {
	nametag = RPMTAG_PROVIDENAME;
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = Flags & RPMSENSE_FIND_PROVIDES;
    } else if (Flags & RPMSENSE_OBSOLETES) {
	nametag = RPMTAG_OBSOLETENAME;
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
    } else if (Flags & RPMSENSE_CONFLICTS) {
	nametag = RPMTAG_CONFLICTNAME;
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
    } else if (Flags & RPMSENSE_PREREQ) {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    } else if (Flags & RPMSENSE_TRIGGER) {
	nametag = RPMTAG_TRIGGERNAME;
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = Flags & RPMSENSE_TRIGGER;
    } else {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    }

    /* rpmlib() dependency sanity: only requires permitted, ensure sense bit */
    if (rstreqn(N, "rpmlib(", sizeof("rpmlib(")-1)) {
	if (nametag != RPMTAG_REQUIRENAME) return 1;
	extra |= RPMSENSE_RPMLIB;
    }

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";
    
    /* Avoid adding duplicate dependencies. */
    if (isNewDep(h, nametag, N, EVR, Flags, indextag, index)) {
	headerPutString(h, nametag, N);
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

    /* XXX 1st arg is unused */
    res = addReqProv(NULL, h, RPMTAG_REQUIRENAME, reqname, featureEVR,
		     RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL), 0);

    free(reqname);

    return res;
}
