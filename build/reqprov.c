/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include "debug.h"

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

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";
    
    /* Check for duplicate dependencies. */
    rpmds hds = rpmdsNew(h, nametag, 0);
    rpmds newds = rpmdsSingle(nametag, N, EVR, Flags);
    /* already got it, don't bother */
    if (rpmdsFind(hds, newds) >= 0) {
	goto exit;
    }

    /* Add this dependency. */
    headerPutString(h, nametag, N);
    if (flagtag) {
	headerPutString(h, versiontag, EVR);
	headerPutUint32(h, flagtag, &Flags, 1);
    }
    if (indextag) {
	headerPutUint32(h, indextag, &index, 1);
    }

exit:
    rpmdsFree(hds);
    rpmdsFree(newds);
	
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
