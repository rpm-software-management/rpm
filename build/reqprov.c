/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

int addReqProv(/*@unused@*/ Spec spec, Header h,
	       int depFlags, const char *depName, const char *depEVR, int index)
{
    const char **names;
    int nametag = 0;
    int versiontag = 0;
    int flagtag = 0;
    int indextag = 0;
    int len;
    int extra = 0;
    
    if (depFlags & RPMSENSE_PROVIDES) {
	nametag = RPMTAG_PROVIDENAME;
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = depFlags & RPMSENSE_FIND_PROVIDES;
    } else if (depFlags & RPMSENSE_OBSOLETES) {
	nametag = RPMTAG_OBSOLETENAME;
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
    } else if (depFlags & RPMSENSE_CONFLICTS) {
	nametag = RPMTAG_CONFLICTNAME;
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
    } else if (depFlags & RPMSENSE_PREREQ) {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = depFlags & _ALL_REQUIRES_MASK;
    } else if (depFlags & RPMSENSE_TRIGGER) {
	nametag = RPMTAG_TRIGGERNAME;
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = depFlags & RPMSENSE_TRIGGER;
    } else {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = depFlags & _ALL_REQUIRES_MASK;
    }

    depFlags = (depFlags & (RPMSENSE_SENSEMASK | RPMSENSE_MULTILIB)) | extra;

    if (depEVR == NULL)
	depEVR = "";
    
    /* Check for duplicate dependencies. */
    if (headerGetEntry(h, nametag, NULL, (void **) &names, &len)) {
	const char **versions = NULL;
	int *flags = NULL;
	int *indexes = NULL;
	int duplicate = 0;

	if (flagtag) {
	    headerGetEntry(h, versiontag, NULL, (void **) &versions, NULL);
	    headerGetEntry(h, flagtag, NULL, (void **) &flags, NULL);
	}
	if (indextag)
	    headerGetEntry(h, indextag, NULL, (void **) &indexes, NULL);

	while (len > 0) {
	    len--;
	    if (strcmp(names[len], depName))
		continue;
	    if (flagtag && versions != NULL &&
		(strcmp(versions[len], depEVR) ||
	((flags[len] | RPMSENSE_MULTILIB) != (depFlags | RPMSENSE_MULTILIB))))
		continue;
	    if (indextag && indexes != NULL && indexes[len] != index)
		continue;

	    /* This is a duplicate dependency. */
	    duplicate = 1;

	    if (flagtag && isDependsMULTILIB(depFlags) &&
		!isDependsMULTILIB(flags[len]))
		    flags[len] |= RPMSENSE_MULTILIB;

	    break;
	}
	FREE(names);
	FREE(versions);
	if (duplicate)
	    return 0;
    }

    /* Add this dependency. */
    headerAddOrAppendEntry(h, nametag, RPM_STRING_ARRAY_TYPE, &depName, 1);
    if (flagtag) {
	headerAddOrAppendEntry(h, versiontag,
			       RPM_STRING_ARRAY_TYPE, &depEVR, 1);
	headerAddOrAppendEntry(h, flagtag,
			       RPM_INT32_TYPE, &depFlags, 1);
    }
    if (indextag)
	headerAddOrAppendEntry(h, indextag, RPM_INT32_TYPE, &index, 1);

    return 0;
}

int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR)
{
    char * reqname = alloca(sizeof("rpmlib()") + strlen(feature));

    (void) stpcpy( stpcpy( stpcpy(reqname, "rpmlib("), feature), ")");

    /* XXX 1st arg is unused */
   return addReqProv(NULL, h, RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL),
	reqname, featureEVR, 0);
}
