#include "system.h"
#include <rpm/header.h>
#include "lib/rpmfs.h"
#include "debug.h"

struct rpmfs_s {
    unsigned int fc;

    rpm_fstate_t * states;
    rpmFileAction * actions;	/*!< File disposition(s). */

    sharedFileInfo replaced;	/*!< (TR_ADDED) to be replaced files in the rpmdb */
    int numReplaced;
    int allocatedReplaced;
};

rpmfs rpmfsNew(Header h, rpmElementType type)
{
    struct rpmtd_s bnames;
    rpmfs fs = xcalloc(1, sizeof(*fs));
    
    headerGet(h, RPMTAG_BASENAMES, &bnames, HEADERGET_MINMEM);
    fs->fc = rpmtdCount(&bnames);
    rpmtdFreeData(&bnames);

    fs->actions = xmalloc(fs->fc * sizeof(*fs->actions));
    memset(fs->actions, FA_UNKNOWN, fs->fc * sizeof(*fs->actions));
    if (type == TR_ADDED) {
	fs->states = xmalloc(sizeof(*fs->states) * fs->fc);
	memset(fs->states, RPMFILE_STATE_NORMAL, fs->fc);
    }
    return fs;
}

rpmfs rpmfsFree(rpmfs fs)
{
    if (fs != NULL) {
	fs->replaced = _free(fs->replaced);
	fs->states = _free(fs->states);
	fs->actions = _free(fs->actions);
	fs = _free(fs);
    }
    return NULL;
}

rpm_count_t rpmfsFC(rpmfs fs)
{
    return (fs != NULL) ? fs->fc : 0;
}

void rpmfsAddReplaced(rpmfs fs, int pkgFileNum, int otherPkg, int otherFileNum)
{
    if (!fs->replaced) {
	fs->replaced = xcalloc(3, sizeof(*fs->replaced));
	fs->allocatedReplaced = 3;
    }
    if (fs->numReplaced>=fs->allocatedReplaced) {
	fs->allocatedReplaced += (fs->allocatedReplaced>>1) + 2;
	fs->replaced = xrealloc(fs->replaced, fs->allocatedReplaced*sizeof(*fs->replaced));
    }
    fs->replaced[fs->numReplaced].pkgFileNum = pkgFileNum;
    fs->replaced[fs->numReplaced].otherPkg = otherPkg;
    fs->replaced[fs->numReplaced].otherFileNum = otherFileNum;

    fs->numReplaced++;
}

sharedFileInfo rpmfsGetReplaced(rpmfs fs)
{
    if (fs && fs->numReplaced)
        return fs->replaced;
    else
        return NULL;
}

sharedFileInfo rpmfsNextReplaced(rpmfs fs , sharedFileInfo replaced)
{
    if (fs && replaced) {
        replaced++;
	if (replaced - fs->replaced < fs->numReplaced)
	    return replaced;
    }
    return NULL;
}

void rpmfsSetState(rpmfs fs, unsigned int ix, rpmfileState state)
{
    assert(ix < fs->fc);
    fs->states[ix] = state;
}

rpmfileState rpmfsGetState(rpmfs fs, unsigned int ix)
{
    assert(ix < fs->fc);
    if (fs->states) return fs->states[ix];
    return RPMFILE_STATE_MISSING;
}

rpm_fstate_t * rpmfsGetStates(rpmfs fs)
{
    return fs->states;
}

rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix)
{
    rpmFileAction action;
    if (fs->actions != NULL && ix < fs->fc) {
	action = fs->actions[ix];
    } else {
	action = FA_UNKNOWN;
    }
    return action;
}

void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action)
{
    if (fs->actions != NULL && ix < fs->fc) {
	fs->actions[ix] = action;
    }
}
