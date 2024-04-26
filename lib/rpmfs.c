#include <stdlib.h>
#include <string.h>

#include "system.h"

#include <vector>

#include "rpmfs.h"
#include "debug.h"

using std::vector;

struct rpmfs_s {
    unsigned int fc;

    vector<rpm_fstate_t> states;
    vector<rpmFileAction> actions;	/*!< File disposition(s). */
    vector<sharedFileInfo_s> replaced; /*!< (TR_ADDED) to be replaced files in the rpmdb */
};

rpmfs rpmfsNew(rpm_count_t fc, int initState)
{
    rpmfs fs = new rpmfs_s {};
    fs->fc = fc;
    fs->actions.resize(fs->fc, FA_UNKNOWN);
    if (initState) {
	fs->states.resize(fs->fc, RPMFILE_STATE_NORMAL);
    }
    return fs;
}

rpmfs rpmfsFree(rpmfs fs)
{
    if (fs != NULL) {
	delete fs;
    }
    return NULL;
}

rpm_count_t rpmfsFC(rpmfs fs)
{
    return (fs != NULL) ? fs->fc : 0;
}

void rpmfsAddReplaced(rpmfs fs, int pkgFileNum, char rstate,
			int otherPkg, int otherFileNum)
{
    fs->replaced.push_back({pkgFileNum, otherPkg, otherFileNum, rstate});
}

sharedFileInfo rpmfsGetReplaced(rpmfs fs)
{
    if (fs && fs->replaced.empty() == false)
        return fs->replaced.data();
    else
        return NULL;
}

/* Eek */
sharedFileInfo rpmfsNextReplaced(rpmfs fs , sharedFileInfo replaced)
{
    if (fs && replaced) {
        replaced++;
	if (replaced - fs->replaced.data() < fs->replaced.size())
	    return replaced;
    }
    return NULL;
}

void rpmfsSetState(rpmfs fs, unsigned int ix, rpmfileState state)
{
    assert(ix < fs->fc);
    fs->states[ix] = state;
}

rpm_fstate_t * rpmfsGetStates(rpmfs fs)
{
    return (fs != NULL) ? fs->states.data() : NULL;
}

rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix)
{
    rpmFileAction action;
    if (fs && ix < fs->fc) {
	action = fs->actions[ix];
    } else {
	action = FA_UNKNOWN;
    }
    return action;
}

void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action)
{
    if (ix < fs->fc) {
	fs->actions[ix] = action;
    }
}

void rpmfsResetActions(rpmfs fs)
{
    if (fs) {
	for (int i = 0; i < fs->fc; i++) {
	    /* --excludepaths is processed early, avoid undoing that */
	    if (fs->actions[i] != FA_SKIPNSTATE)
		fs->actions[i] = FA_UNKNOWN;
	}
    }
}
