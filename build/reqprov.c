/* reqprov.c -- require/provide handling */

#include "system.h"

#include "rpmbuild.h"

int addReqProv(Spec spec, Header h,
	       int flag, const char *name, const char *version, int index)
{
    const char **names;
    const char **versions = NULL;
    int *flags = NULL;
    int *indexes = NULL;
    int nametag = 0;
    int versiontag = 0;
    int flagtag = 0;
    int indextag = 0;
    int len;
    int extra = 0;
    
    if (flag & RPMSENSE_PROVIDES) {
	nametag = RPMTAG_PROVIDES;
    } else if (flag & RPMSENSE_OBSOLETES) {
	nametag = RPMTAG_OBSOLETES;
    } else if (flag & RPMSENSE_CONFLICTS) {
	nametag = RPMTAG_CONFLICTNAME;
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
    } else if (flag & RPMSENSE_PREREQ) {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = RPMSENSE_PREREQ;
    } else if (flag & RPMSENSE_TRIGGER) {
	nametag = RPMTAG_TRIGGERNAME;
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = flag & RPMSENSE_TRIGGER;
    } else {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
    }

    flag = (flag & RPMSENSE_SENSEMASK) | extra;
    if (!version) {
	version = "";
    }
    
    if (headerGetEntry(h, nametag, NULL, (void *) &names, &len)) {
	if (flagtag) {
	    headerGetEntry(h, versiontag, NULL,
			   (void *) &versions, NULL);
	    headerGetEntry(h, flagtag, NULL, (void *) &flags, NULL);
	}
	if (indextag) {
	    headerGetEntry(h, indextag, NULL,
			   (void *) &indexes, NULL);
	}
	while (len) {
	    len--;
	    if (!strcmp(names[len], name)) {
		if (!flagtag ||
		    (!strcmp(versions[len], version) && flags[len] == flag)) {
		    if (!indextag || (index == indexes[len])) {
			/* The same */
			FREE(names);
			FREE(versions);
			return 0;
		    }
		}
	    }
	}
	FREE(names);
	FREE(versions);
    }

    headerAddOrAppendEntry(h, nametag, RPM_STRING_ARRAY_TYPE, &name, 1);
    if (flagtag) {
	headerAddOrAppendEntry(h, versiontag,
			       RPM_STRING_ARRAY_TYPE, &version, 1);
	headerAddOrAppendEntry(h, flagtag,
			       RPM_INT32_TYPE, &flag, 1);
    }
    if (indextag) {
	headerAddOrAppendEntry(h, indextag,
			       RPM_INT32_TYPE, &index, 1);
    }

    return 0;
}
