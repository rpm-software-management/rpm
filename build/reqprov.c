/* reqprov.c -- require/provide handling */

#include <stdlib.h>
#include <string.h>

#include "spec.h"
#include "reqprov.h"
#include "messages.h"
#include "rpmlib.h"
#include "misc.h"
#include "lib/misc.h"

int addReqProv(Spec spec, Package pkg,
	       int flag, char *name, char *version, int index)
{
    char **names;
    char **versions = NULL;
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
    
    if (headerGetEntry(pkg->header, nametag, NULL, (void *) &names, &len)) {
	if (flagtag) {
	    headerGetEntry(pkg->header, versiontag, NULL,
			   (void *) &versions, NULL);
	    headerGetEntry(pkg->header, flagtag, NULL, (void *) &flags, NULL);
	}
	if (indextag) {
	    headerGetEntry(pkg->header, indextag, NULL,
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

    headerAddOrAppendEntry(pkg->header, nametag,
			   RPM_STRING_ARRAY_TYPE, &name, 1);
    if (flagtag) {
	headerAddOrAppendEntry(pkg->header, versiontag,
			       RPM_STRING_ARRAY_TYPE, &version, 1);
	headerAddOrAppendEntry(pkg->header, flagtag,
			       RPM_INT32_TYPE, &flag, 1);
    }
    if (indextag) {
	headerAddOrAppendEntry(pkg->header, indextag,
			       RPM_INT32_TYPE, &index, 1);
    }

    return 0;
}

int generateAutoReqProv(Spec spec, Package pkg,
			struct cpioFileMapping *cpioList, int cpioCount)
{
    StringBuf writeBuf;
    int writeBytes;
    StringBuf readBuf;
    char *argv[2];
    char **f, **fsave;

    if (!cpioCount || !spec->autoReqProv) {
	return 0;
    }

    writeBuf = newStringBuf();
    writeBytes = 0;
    while (cpioCount--) {
	writeBytes += strlen(cpioList->fsPath) + 1;
	appendLineStringBuf(writeBuf, cpioList->fsPath);
	cpioList++;
    }

    /*** Do Provides ***/

    rpmMessage(RPMMESS_NORMAL, "Finding provides...\n");
    
    argv[0] = "find-provides";
    argv[1] = NULL;
    readBuf = getOutputFrom(NULL, argv,
			     getStringBuf(writeBuf), writeBytes, 1);
    if (!readBuf) {
	rpmError(RPMERR_EXEC, "Failed to find provides");
	freeStringBuf(writeBuf);
	return RPMERR_EXEC;
    }
    
    f = fsave = splitString(getStringBuf(readBuf),
			    strlen(getStringBuf(readBuf)), '\n');
    freeStringBuf(readBuf);
    while (*f) {
	if (**f) {
	    addReqProv(spec, pkg, RPMSENSE_PROVIDES, *f, NULL, 0);
	}
	f++;
    }
    freeSplitString(fsave);

    /*** Do Requires ***/
    
    rpmMessage(RPMMESS_NORMAL, "Finding requires...\n");

    argv[0] = "find-requires";
    argv[1] = NULL;
    readBuf = getOutputFrom(NULL, argv,
			     getStringBuf(writeBuf), writeBytes, 0);
    if (!readBuf) {
	rpmError(RPMERR_EXEC, "Failed to find requires");
	freeStringBuf(writeBuf);
	return RPMERR_EXEC;
    }

    f = fsave = splitString(getStringBuf(readBuf),
			    strlen(getStringBuf(readBuf)), '\n');
    freeStringBuf(readBuf);
    while (*f) {
	if (**f) {
	    addReqProv(spec, pkg, RPMSENSE_ANY, *f, NULL, 0);
	}
	f++;
    }
    freeSplitString(fsave);

    /*** Clean Up ***/

    freeStringBuf(writeBuf);

    return 0;
}

void printReqs(Spec spec, Package pkg)
{
    int startedPreReq = 0;
    int startedReq = 0;

    char **names;
    int x, count;
    int *flags;

    if (headerGetEntry(pkg->header, RPMTAG_PROVIDES,
		       NULL, (void **) &names, &count)) {
	rpmMessage(RPMMESS_NORMAL, "Provides:");
	x = 0;
	while (x < count) {
	    rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	    x++;
	}
	rpmMessage(RPMMESS_NORMAL, "\n");
	FREE(names);
    }

    if (headerGetEntry(pkg->header, RPMTAG_REQUIRENAME,
		       NULL, (void **) &names, &count)) {
	headerGetEntry(pkg->header, RPMTAG_REQUIREFLAGS,
		       NULL, (void **) &flags, NULL);
	x = 0;
	while (x < count) {
	    if (flags[x] & RPMSENSE_PREREQ) {
		if (! startedPreReq) {
		    rpmMessage(RPMMESS_NORMAL, "Prereqs:");
		    startedPreReq = 1;
		}
		rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	    }
	    x++;
	}
	if (startedPreReq) {
	    rpmMessage(RPMMESS_NORMAL, "\n");
	}
	x = 0;
	while (x < count) {
	    if (! (flags[x] & RPMSENSE_PREREQ)) {
		if (! startedReq) {
		    rpmMessage(RPMMESS_NORMAL, "Requires:");
		    startedReq = 1;
		}
		rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	    }
	    x++;
	}
	rpmMessage(RPMMESS_NORMAL, "\n");
	FREE(names);
    }

}
