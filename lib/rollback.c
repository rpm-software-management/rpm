/** \ingroup rpmtrans payload
 * \file lib/rollback.c
 */

#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "install.h"

#include "debug.h"

static char * ridsub = ".rid/";
static char * ridsep = ";";
static mode_t riddmode = 0700;
static mode_t ridfmode = 0000;

int dirstashPackage(const rpmTransactionSet ts, const TFI_t fi, rollbackDir dir)
{
    unsigned int offset = fi->record;
    char tsid[20], ofn[BUFSIZ], nfn[BUFSIZ];
    const char * s, * t;
    char * se, * te;
    Header h;
    int i;

    assert(fi->type == TR_REMOVED);


    {	rpmdbMatchIterator mi = NULL;

	mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES,
				&offset, sizeof(offset));

	h = rpmdbNextIterator(mi);
	if (h == NULL) {
	    rpmdbFreeIterator(mi);
	    return 1;
	}
	h = headerLink(h);
	rpmdbFreeIterator(mi);
    }

    sprintf(tsid, "%08x", ts->id);

    /* Create rid sub-directories if necessary. */
    if (strchr(ridsub, '/')) {
	for (i = 0; i < fi->dc; i++) {

	    t = te = nfn;
	    *te = '\0';
	    te = stpcpy(te, fi->dnl[i]);
	    te = stpcpy(te, ridsub);
	    if (te[-1] == '/')
		*(--te) = '\0';
fprintf(stderr, "*** mkdir(%s,%o)\n", t, riddmode);
	}
    }

    /* Rename files about to be removed. */
    for (i = 0; i < fi->fc; i++) {

	if (S_ISDIR(fi->fmodes[i]))
	    continue;

	s = se = ofn;
	*se = '\0';
	se = stpcpy( stpcpy(se, fi->dnl[fi->dil[i]]), fi->bnl[i]);

	t = te = nfn;
	*te = '\0';
	te = stpcpy(te, fi->dnl[fi->dil[i]]);
	if (ridsub)
	    te = stpcpy(te, ridsub);
	te = stpcpy( stpcpy( stpcpy(te, fi->bnl[i]), ridsep), tsid);

	s = strrchr(s, '/') + 1;
	t = strrchr(t, '/') + 1;
fprintf(stderr, "*** rename(%s,%s%s)\n", s, (ridsub ? ridsub : ""), t);
fprintf(stderr, "*** chmod(%s%s,%o)\n", (ridsub ? ridsub : ""), t, ridfmode);
    }

    return 0;
}

