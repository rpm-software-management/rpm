/** \ingroup rpmdep
 * \file lib/rpmlibprov.c
 */

#include "system.h"

#include <rpmlib.h>
#include "debug.h"

static struct rpmlibProvides {
/*@observer@*/ /*@null@*/ const char * featureName;
/*@observer@*/ /*@null@*/ const char * featureEVR;
    int featureFlags;
/*@observer@*/ /*@null@*/ const char * featureDescription;
} rpmlibProvides[] = {
    { "rpmlib(VersionedDependencies)",	"3.0.3-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("PreReq:, Provides:, and Obsoletes: dependencies support versions.") },
    { "rpmlib(CompressedFileNames)",	"3.0.4-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("file names stored as (dirName,baseName,dirIndex) tuple, not as path.")},
    { "rpmlib(PayloadIsBzip2)",		"3.0.5-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload compressed using bzip2.") },
    { "rpmlib(PayloadFilesHavePrefix)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload files have \"./\" prefix.") },
    { "rpmlib(ExplicitPackageProvide)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package name-version-release not implicitly provided.") },
    { "rpmlib(HeaderLoadSortsTags)",    "4.0.1-1",
	(                RPMSENSE_EQUAL),
    N_("header tags are always sorted after being loaded.") },
    { NULL,				NULL, 0,	NULL }
};

void rpmShowRpmlibProvides(FILE * fp)
{
    const struct rpmlibProvides * rlp;

    for (rlp = rpmlibProvides; rlp->featureName != NULL; rlp++) {
	fprintf(fp, "    %s", rlp->featureName);
	if (rlp->featureEVR && rlp->featureFlags)
	    printDepFlags(fp, rlp->featureEVR, rlp->featureFlags);
	fprintf(fp, "\n");
	if (rlp->featureDescription)
	    fprintf(fp, "\t%s\n", rlp->featureDescription);
    }
}

int rpmCheckRpmlibProvides(const char * keyName, const char * keyEVR,
	int keyFlags)
{
    const struct rpmlibProvides * rlp;
    int rc = 0;

    for (rlp = rpmlibProvides; rlp->featureName != NULL; rlp++) {
	if (rlp->featureEVR && rlp->featureFlags)
	    rc = rpmRangesOverlap(keyName, keyEVR, keyFlags,
		rlp->featureName, rlp->featureEVR, rlp->featureFlags);
	if (rc)
	    break;
    }
    return rc;
}

int rpmGetRpmlibProvides(const char *** provNames, int ** provFlags,
                         const char *** provVersions)
{
    const char ** names, ** versions;
    int * flags;
    int n = 0;
    
    while (rpmlibProvides[n].featureName != NULL)
        n++;

    names = xcalloc((n+1), sizeof(*names));
    versions = xcalloc((n+1), sizeof(*versions));
    flags = xcalloc((n+1), sizeof(*flags));
    
    for (n = 0; rpmlibProvides[n].featureName != NULL; n++) {
        names[n] = rpmlibProvides[n].featureName;
        flags[n] = rpmlibProvides[n].featureFlags;
        versions[n] = rpmlibProvides[n].featureEVR;
    }
    
    if (provNames)
	*provNames = names;
    else
	names = _free(names);

    if (provFlags)
	*provFlags = flags;
    else
	flags = _free(flags);

    if (provVersions)
	*provVersions = versions;
    else
	versions = _free(versions);

    return n;
}
