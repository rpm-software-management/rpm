#include "system.h"

#include <rpmlib.h>

static struct rpmlibProvides {
    const char * featureName;
    const char * featureEVR;
    int featureFlags;
} rpmlibProvides[] = {
    { "rpmlib(VersionedDependencies)",	"3.0.3-1",	RPMSENSE_EQUAL },
    { "rpmlib(CompressedFileNames)",	"3.0.4-1",	RPMSENSE_EQUAL },
    { "rpmlib(PayloadIsBzip2)",		"3.0.5-1",	RPMSENSE_EQUAL },
    { "rpmlib(PayloadFilesHavePrefix)",	"4.0-1",	RPMSENSE_EQUAL },
    { "rpmlib(PreTransactionSyscalls)",	"4.0-1",	RPMSENSE_EQUAL },
    { NULL,				NULL,	0 }
};

int rpmCheckRpmlibProvides(const char * keyName, const char * keyEVR,
	int keyFlags)
{
    const struct rpmlibProvides * rlp;
    int rc = 0;

    for (rlp = rpmlibProvides; rlp->featureName != NULL; rlp++) {
	rc = rpmRangesOverlap(keyName, keyEVR, keyFlags,
		rlp->featureName, rlp->featureEVR, rlp->featureFlags);
	if (rc)
	    break;
    }
    return rc;
}
