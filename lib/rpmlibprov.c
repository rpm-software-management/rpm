/** \ingroup rpmdep
 * \file lib/rpmlibprov.c
 */

#include "system.h"

#include <rpmlib.h>

static struct rpmlibProvides {
    const char * featureName;
    const char * featureEVR;
    int featureFlags;
    const char * featureDescription;
} rpmlibProvides[] = {
    { "rpmlib(VersionedDependencies)",	"3.0.3-1",	RPMSENSE_EQUAL,
	"PreReq:, Provides:, and Obsoletes: dependencies support versions." },
    { "rpmlib(CompressedFileNames)",	"3.0.4-1",	RPMSENSE_EQUAL,
	"file names stored as (dirName,BaseName,dirIndex) tuple, not as path."},
    { "rpmlib(PayloadIsBzip2)",		"3.0.5-1",	RPMSENSE_EQUAL,
	"package payload compressed using bzip2." },
    { "rpmlib(PayloadFilesHavePrefix)",	"4.0-1",	RPMSENSE_EQUAL,
	"package payload files have \"./\" prefix." },
    { "rpmlib(ExplicitPackageProvide)",	"4.0-1",	RPMSENSE_EQUAL,
	"package name-version-release not implicitly provided." },
    { NULL,				NULL,	0 }
};

void rpmShowRpmlibProvides(FILE * fp)
{
    const struct rpmlibProvides * rlp;

    for (rlp = rpmlibProvides; rlp->featureName != NULL; rlp++) {
	fprintf(fp, "    %s", rlp->featureName);
	if (rlp->featureFlags)
	    printDepFlags(fp, rlp->featureEVR, rlp->featureFlags);
	fprintf(fp, "\n");
	fprintf(fp, "\t%s\n", rlp->featureDescription);
    }
}

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
