#include "system.h"

#include <rpmlib.h>

#include "dbindex.h"	/* XXX prototypes */
#include "lookup.h"

/* XXX used in transaction.c */
/* 0 found matches */
/* 1 no matches */
/* 2 error */
int findMatches(rpmdb db, const char * name, const char * version,
			const char * release, dbiIndexSet * matches)
{
    int gotMatches;
    int rc;
    int i;

    if ((rc = rpmdbFindPackage(db, name, matches))) {
	rc = ((rc == -1) ? 2 : 1);
	goto exit;
    }

    if (!version && !release) {
	rc = 0;
	goto exit;
    }

    gotMatches = 0;

    /* make sure the version and releases match */
    for (i = 0; i < dbiIndexSetCount(*matches); i++) {
	unsigned int recoff = dbiIndexRecordOffset(*matches, i);
	int goodRelease, goodVersion;
	const char * pkgVersion;
	const char * pkgRelease;
	Header h;

	if (recoff == 0)
	    continue;

	h = rpmdbGetRecord(db, recoff);
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT,_("cannot read header at %d for lookup"), 
		recoff);
	    rc = 2;
	    goto exit;
	}

	headerNVR(h, NULL, &pkgVersion, &pkgRelease);
	    
	goodRelease = goodVersion = 1;

	if (release && strcmp(release, pkgRelease)) goodRelease = 0;
	if (version && strcmp(version, pkgVersion)) goodVersion = 0;

	if (goodRelease && goodVersion) 
	    gotMatches = 1;
	else 
	    dbiIndexRecordOffsetSave(*matches, i, 0);

	headerFree(h);
    }

    if (!gotMatches) {
	rc = 1;
	goto exit;
    }
    rc = 0;

exit:
    if (rc && matches && *matches) {
	dbiFreeIndexSet(*matches);
	*matches = NULL;
    }
    return rc;
}

#ifdef DYING
/* 0 found matches */
/* 1 no matches */
/* 2 error */
int rpmdbFindByHeader(rpmdb db, Header h, dbiIndexSet * matches)
{
    const char * name, * version, * release;

    headerNVR(h, &name, &version, &release);

    return findMatches(db, name, version, release, matches);
}
#endif

/* 0 found matches */
/* 1 no matches */
/* 2 error */
int rpmdbFindByLabel(rpmdb db, const char * arg, dbiIndexSet * matches)
{
    char * localarg, * chptr;
    char * release;
    int rc;
 
    if (!strlen(arg)) return 1;

    /* did they give us just a name? */
    rc = findMatches(db, arg, NULL, NULL, matches);
    if (rc != 1) return rc;

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    strcpy(localarg, arg);

    chptr = (localarg + strlen(localarg)) - 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    rc = findMatches(db, localarg, chptr + 1, NULL, matches);
    if (rc != 1) return rc;
    
    /* how about name-version-release? */

    release = chptr + 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    return findMatches(db, localarg, chptr + 1, release, matches);
}
