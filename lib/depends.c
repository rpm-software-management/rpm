#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "misc.h"

/*@access rpmdb@*/
/*@access rpmTransactionSet@*/

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type, count;
    if (np && !headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count))
	*np = NULL;
    if (vp && !headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count))
	*vp = NULL;
    if (rp && !headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count))
	*rp = NULL;
    return 0;
}

static /*@only@*/ char *printDepend(const char * key, const char * keyEVR,
	int keyFlags)
{
    char *tbuf, *t;
    size_t nb;

    nb = 0;
    if (key)	nb += strlen(key);
    if (keyFlags) {
	if (nb)	nb++;
	if (keyFlags & RPMSENSE_LESS)	nb++;
	if (keyFlags & RPMSENSE_GREATER) nb++;
	if (keyFlags & RPMSENSE_EQUAL)	nb++;
    }
    if (keyEVR) {
	if (nb)	nb++;
	nb += strlen(keyEVR);
    }

    t = tbuf = xmalloc(nb + 1);
    if (key)
	while(*key)	*t++ = *key++;
    if (keyFlags) {
	if (t != tbuf)	*t++ = ' ';
	if (keyFlags & RPMSENSE_LESS)	*t++ = '<';
	if (keyFlags & RPMSENSE_GREATER) *t++ = '>';
	if (keyFlags & RPMSENSE_EQUAL)	*t++ = '=';
    }
    if (keyEVR) {
	if (t != tbuf)	*t++ = ' ';
	while(*keyEVR)	*t++ = *keyEVR++;
    }
    *t = '\0';
    return tbuf;
}

static /*@only@*/ char *buildEVR(int_32 *e, const char *v, const char *r)
{
    char *pEVR = xmalloc(21 + strlen(v) + 1 + strlen(r) + 1);
    *pEVR = '\0';
    if (e)
	sprintf(pEVR, "%d:", *e);
    strcat(pEVR, v);
    strcat(pEVR, "-");
    strcat(pEVR, r);
    return pEVR;
}

struct orderListIndex {
    int alIndex;
    int orIndex;
};

static void alFreeIndex(struct availableList * al)
{
    if (al->index.size) {
	if (al->index.index)
		free(al->index.index);
	al->index.index = NULL;
	al->index.size = 0;
    }
}

static void alCreate(/*@out@*/struct availableList * al)
{
    al->alloced = 5;
    al->size = 0;
    al->list = xcalloc(al->alloced, sizeof(*al->list));

    al->index.index = NULL;
    al->index.size = 0;
    alFreeIndex(al);
}

static void alFree(/*@only@*/struct availableList * al)
{
    int i;
    rpmRelocation * r;

    for (i = 0; i < al->size; i++) {
	if (al->list[i].provides)
	    free(al->list[i].provides);
	if (al->list[i].providesEVR)
	    free(al->list[i].providesEVR);
	if (al->list[i].files)
	    free(al->list[i].files);
	headerFree(al->list[i].h);

	if (al->list[i].relocs) {
	    for (r = al->list[i].relocs; (r->oldPath || r->newPath); r++) {
		if (r->oldPath) xfree(r->oldPath);
		if (r->newPath) xfree(r->newPath);
	    }
	    free(al->list[i].relocs);
	}
    }

    if (al->alloced && al->list)
	free(al->list);
    al->list = NULL;
    alFreeIndex(al);
}

static /*@exposed@*/ struct availablePackage * alAddPackage(struct availableList * al,
					      Header h, const void * key,
			 		      FD_t fd, rpmRelocation * relocs)
{
    struct availablePackage * p;
    rpmRelocation * r;
    int i;

    if (al->size == al->alloced) {
	al->alloced += 5;
	al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
    }

    p = al->list + al->size++;
    p->h = headerLink(h);	/* XXX reference held by transaction set */

    headerNVR(p->h, &p->name, &p->version, &p->release);

    if (!headerGetEntry(h, RPMTAG_EPOCH, NULL, (void **) &p->epoch, NULL))
	p->epoch = NULL;

    if (!headerGetEntry(h, RPMTAG_PROVIDENAME, NULL, (void **) &p->provides,
	&p->providesCount)) {
	p->providesCount = 0;
	p->provides = NULL;
	p->providesEVR = NULL;
	p->provideFlags = NULL;
    } else {
	if (!headerGetEntry(h, RPMTAG_PROVIDEVERSION,
			NULL, (void **) &p->providesEVR, NULL))
	    p->providesEVR = NULL;
	if (!headerGetEntry(h, RPMTAG_PROVIDEFLAGS,
			NULL, (void **) &p->provideFlags, NULL))
	    p->provideFlags = NULL;
    }

    if (!headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &p->files,
	&p->filesCount)) {
	p->filesCount = 0;
	p->files = NULL;
    }

    /* We don't use these entries (and rpm >= 2 never have) and they are
       pretty misleading. Let's just get rid of them so they don't confuse
       anyone. */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	headerRemoveEntry(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	headerRemoveEntry(h, RPMTAG_FILEGIDS);

    p->key = key;
    p->fd = fd;

    if (relocs) {
	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++);
	p->relocs = xmalloc(sizeof(*p->relocs) * (i + 1));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? xstrdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? xstrdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    } else {
	p->relocs = NULL;
    }

    alFreeIndex(al);

    return p;
}

static int indexcmp(const void * a, const void *b)
{
    const struct availableIndexEntry * aptr = a;
    const struct availableIndexEntry * bptr = b;

    return strcmp(aptr->entry, bptr->entry);
}

static void alMakeIndex(struct availableList * al)
{
    struct availableIndex * ai = &al->index;
    int i, j, k;

    if (ai->size) return;

    ai->size = al->size;
    for (i = 0; i < al->size; i++) {
	ai->size += al->list[i].providesCount;
    }
    for (i = 0; i < al->size; i++) {
	ai->size += al->list[i].filesCount;
    }

    if (ai->size) {
	ai->index = xcalloc(ai->size, sizeof(*ai->index));

	k = 0;
	for (i = 0; i < al->size; i++) {
	    ai->index[k].package = al->list + i;
	    ai->index[k].entry = al->list[i].name;
	    ai->index[k].type = IET_NAME;
	    k++;

	    for (j = 0; j < al->list[i].providesCount; j++) {
		ai->index[k].package = al->list + i;
		ai->index[k].entry = al->list[i].provides[j];
		ai->index[k].type = IET_PROVIDES;
		k++;
	    }

	    for (j = 0; j < al->list[i].filesCount; j++) {
		ai->index[k].package = al->list + i;
		ai->index[k].entry = al->list[i].files[j];
		ai->index[k].type = IET_FILE;
		k++;
	    }
	}

	qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
    }
}

static int intcmp(const void * a, const void *b)
{
    const int * aptr = a;
    const int * bptr = b;

    if (*aptr < *bptr)
	return -1;
    else if (*aptr == *bptr)
	return 0;

    return 1;
}

static void parseEVR(char *evr, /*@exposed@*/ /*@out@*/const char **ep, /*@exposed@*/ /*@out@*/const char **vp, /*@exposed@*/ /*@out@*/const char **rp) /*@modifies evr,*ep,*vp,*rp @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && isdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	if (*epoch == '\0') epoch = "0";
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
	*se++ = '\0';
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

const char *rpmNAME = PACKAGE;
const char *rpmEVR = VERSION;
int rpmFLAGS = RPMSENSE_EQUAL;

static int rangesOverlap(const char *AName, const char *AEVR, int AFlags,
	const char *BName, const char *BEVR, int BFlags)
{
    const char *aDepend = printDepend(AName, AEVR, AFlags);
    const char *bDepend = printDepend(BName, BEVR, BFlags);
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

    /* Different names don't overlap. */
    if (strcmp(AName, BName)) {
	result = 0;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    if (!((AFlags & RPMSENSE_SENSEMASK) && (BFlags & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    /* If either EVR is non-existent or empty, always overlap. */
    if (!(AEVR && *AEVR && BEVR && *BEVR)) {
	result = 1;
	goto exit;
    }

    /* Both AEVR and BEVR exist. */
    aEVR = xstrdup(AEVR);
    parseEVR(aEVR, &aE, &aV, &aR);
    bEVR = xstrdup(BEVR);
    parseEVR(bEVR, &bE, &bV, &bR);

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	/* XXX legacy epoch-less requires/conflicts compatibility */
	rpmMessage(RPMMESS_DEBUG, _("the \"B\" dependency needs an epoch (assuming same as \"A\")\n\tA %s\tB %s\n"),
		aDepend, bDepend);
	sense = 0;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR) {
	    sense = rpmvercmp(aR, bR);
	}
    }
    free(aEVR);
    free(bEVR);

    /* Detect overlap of {A,B} range. */
    result = 0;
    if (sense < 0 && ((AFlags & RPMSENSE_GREATER) || (BFlags & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((AFlags & RPMSENSE_LESS) || (BFlags & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((AFlags & RPMSENSE_EQUAL) && (BFlags & RPMSENSE_EQUAL)) ||
	 ((AFlags & RPMSENSE_LESS) && (BFlags & RPMSENSE_LESS)) ||
	 ((AFlags & RPMSENSE_GREATER) && (BFlags & RPMSENSE_GREATER)))) {
	result = 1;
    }

exit:
    rpmMessage(RPMMESS_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? "YES" : "NO "), aDepend, bDepend);
    if (aDepend) xfree(aDepend);
    if (bDepend) xfree(bDepend);
    return result;
}

typedef int (*dbrecMatch_t) (Header h, const char *reqName, const char * reqEVR, int reqFlags);

static int rangeMatchesDepFlags (Header h, const char *reqName, const char * reqEVR, int reqFlags)
{
    const char ** provides;
    const char ** providesEVR;
    int_32 * provideFlags;
    int providesCount;
    int result;
    int type;
    int i;

    if (!(reqFlags & RPMSENSE_SENSEMASK) || !reqEVR || !strlen(reqEVR))
	return 1;

    /* Get provides information from header */
    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides version info is available, match any requires.
     */
    if (!headerGetEntry(h, RPMTAG_PROVIDEVERSION, &type,
		(void **) &providesEVR, &providesCount))
	return 1;

    headerGetEntry(h, RPMTAG_PROVIDEFLAGS, &type,
	(void **) &provideFlags, &providesCount);

    if (!headerGetEntry(h, RPMTAG_PROVIDENAME, &type,
		(void **) &provides, &providesCount)) {
	if (providesEVR) xfree(providesEVR);
	return 0;	/* XXX should never happen */
    }

    result = 0;
    for (i = 0; i < providesCount; i++) {

	result = rangesOverlap(provides[i], providesEVR[i], provideFlags[i],
		reqName, reqEVR, reqFlags);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

    if (provides) xfree(provides);
    if (providesEVR) xfree(providesEVR);

    return result;
}

int headerMatchesDepFlags(Header h, const char *reqName, const char * reqEVR, int reqFlags)
{
    const char *name, *version, *release;
    int_32 * epoch;
    char *pkgEVR;
    int pkgFlags = RPMSENSE_EQUAL;
    int type, count;

    if (!((reqFlags & RPMSENSE_SENSEMASK) && reqEVR && *reqEVR))
	return 1;

    /* Get package information from header */
    headerNVR(h, &name, &version, &release);

    pkgEVR = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *pkgEVR = '\0';
    if (headerGetEntry(h, RPMTAG_EPOCH, &type, (void **) &epoch, &count))
	sprintf(pkgEVR, "%d:", *epoch);
    strcat(pkgEVR, version);
    strcat(pkgEVR, "-");
    strcat(pkgEVR, release);

    return rangesOverlap(name, pkgEVR, pkgFlags, reqName, reqEVR, reqFlags);

}

static inline int dbrecMatchesDepFlags(rpmTransactionSet rpmdep, int recOffset,
			        const char * reqName, const char * reqEVR,
				int reqFlags, dbrecMatch_t matchDepFlags)
{
    Header h;
    int rc;

    h = rpmdbGetRecord(rpmdep->db, recOffset);
    if (h == NULL) {
	rpmMessage(RPMMESS_DEBUG, _("dbrecMatchesDepFlags() failed to read header"));
	return 0;
    }

    rc = matchDepFlags(h, reqName, reqEVR, reqFlags);

    headerFree(h);

    return rc;
}

rpmTransactionSet rpmtransCreateSet(rpmdb db, const char * root)
{
    rpmTransactionSet rpmdep;
    int rootLength;

    if (!root) root = "";

    rpmdep = xmalloc(sizeof(*rpmdep));
    rpmdep->db = db;
    rpmdep->scriptFd = NULL;
    rpmdep->numRemovedPackages = 0;
    rpmdep->allocedRemovedPackages = 5;
    rpmdep->removedPackages = xcalloc(rpmdep->allocedRemovedPackages,
			sizeof(*rpmdep->removedPackages));

    /* This canonicalizes the root */
    rootLength = strlen(root);
    if (root && root[rootLength] == '/') {
	char * newRootdir;

	newRootdir = alloca(rootLength + 2);
	strcpy(newRootdir, root);
	newRootdir[rootLength++] = '/';
	newRootdir[rootLength] = '\0';
	root = newRootdir;
    }

    rpmdep->root = xstrdup(root);

    alCreate(&rpmdep->addedPackages);
    alCreate(&rpmdep->availablePackages);

    rpmdep->orderAlloced = 5;
    rpmdep->orderCount = 0;
    rpmdep->order = xcalloc(rpmdep->orderAlloced, sizeof(*rpmdep->order)); /* XXX memory leak */

    return rpmdep;
}

static void removePackage(rpmTransactionSet rpmdep, int dboffset, int depends)
{
    if (rpmdep->numRemovedPackages == rpmdep->allocedRemovedPackages) {
	rpmdep->allocedRemovedPackages += 5;
	rpmdep->removedPackages = xrealloc(rpmdep->removedPackages,
		sizeof(int *) * rpmdep->allocedRemovedPackages);
    }

    rpmdep->removedPackages[rpmdep->numRemovedPackages++] = dboffset;

    if (rpmdep->orderCount == rpmdep->orderAlloced) {
	rpmdep->orderAlloced += 5;
	rpmdep->order = xrealloc(rpmdep->order,
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }

    rpmdep->order[rpmdep->orderCount].type = TR_REMOVED;
    rpmdep->order[rpmdep->orderCount].u.removed.dboffset = dboffset;
    rpmdep->order[rpmdep->orderCount++].u.removed.dependsOnIndex = depends;
}

int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
			const void * key, int upgrade, rpmRelocation * relocs)
{
    /* this is an install followed by uninstalls */
    dbiIndexSet matches;
    char * name;
    int count, i, j;
    const char ** obsoletes;
    int alNum;
    int * caps;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	return 1;

    /* Make sure we've implemented all of the capabilities we need */
    if (headerGetEntry(h, RPMTAG_CAPABILITY, NULL, (void **)&caps, &count)) {
	if (count != 1 || *caps) {
	    return 2;
	}
    }

    /* FIXME: handling upgrades like this is *almost* okay. It doesn't
       check to make sure we're upgrading to a newer version, and it
       makes it difficult to generate a return code based on the number of
       packages which failed. */

    if (rpmdep->orderCount == rpmdep->orderAlloced) {
	rpmdep->orderAlloced += 5;
	rpmdep->order = xrealloc(rpmdep->order,
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }
    rpmdep->order[rpmdep->orderCount].type = TR_ADDED;
    alNum = alAddPackage(&rpmdep->addedPackages, h, key, fd, relocs) -
    		rpmdep->addedPackages.list;
    rpmdep->order[rpmdep->orderCount++].u.addedIndex = alNum;

    if (!upgrade || rpmdep->db == NULL) return 0;

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, &count);

    if (!rpmdbFindPackage(rpmdep->db, name, &matches))  {

	for (i = 0; i < dbiIndexSetCount(matches); i++) {
	    Header h2;
	    h2 = rpmdbGetRecord(rpmdep->db, dbiIndexRecordOffset(matches, i));
	    if (h2 == NULL)
		continue;
	    if (rpmVersionCompare(h, h2))
		removePackage(rpmdep, dbiIndexRecordOffset(matches, i), alNum);
	    headerFree(h2);
	}

	dbiFreeIndexRecord(matches);
    }

    if (headerGetEntry(h, RPMTAG_OBSOLETENAME, NULL, (void **) &obsoletes, &count)) {
	const char **obsoletesEVR;
	int_32 *obsoletesFlags;

	headerGetEntry(h, RPMTAG_OBSOLETEVERSION, NULL, (void **) &obsoletesEVR, NULL);
	headerGetEntry(h, RPMTAG_OBSOLETEFLAGS, NULL, (void **) &obsoletesFlags, NULL);

	for (j = 0; j < count; j++) {

	    /* XXX avoid self-obsoleting packages. */
	    if (!strcmp(name, obsoletes[j]))
		continue;

	    if (rpmdbFindPackage(rpmdep->db, obsoletes[j], &matches))
		continue;
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		 if (bsearch(&recOffset,
			rpmdep->removedPackages, rpmdep->numRemovedPackages,
			sizeof(int), intcmp))
		    continue;

		/*
		 * Rpm prior to 3.0.3 does not have versioned obsoletes.
		 * If no obsoletes version info is available, match all names.
		 */
		if (obsoletesEVR == NULL ||
		    dbrecMatchesDepFlags(rpmdep, recOffset,
		      obsoletes[j], obsoletesEVR[j], obsoletesFlags[j],
		      headerMatchesDepFlags)) {
			removePackage(rpmdep, recOffset, alNum);
		}
	    }

	    dbiFreeIndexRecord(matches);
	}

	if (obsoletesEVR) free(obsoletesEVR);
	free(obsoletes);
    }

    return 0;
}

void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h, void * key)
{
    struct availablePackage * al;
    al = alAddPackage(&rpmdep->availablePackages, h, key, NULL, NULL);
}

void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset)
{
    removePackage(rpmdep, dboffset, -1);
}

void rpmtransFree(rpmTransactionSet rpmdep)
{
    alFree(&rpmdep->addedPackages);
    alFree(&rpmdep->availablePackages);
    free(rpmdep->removedPackages);
    xfree(rpmdep->root);

    free(rpmdep);
}

void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts, int
			 numConflicts)
{
    int i;

    for (i = 0; i < numConflicts; i++) {
	headerFree(conflicts[i].byHeader);
	free(conflicts[i].byName);
	free(conflicts[i].byVersion);
	free(conflicts[i].byRelease);
	free(conflicts[i].needsName);
	free(conflicts[i].needsVersion);
    }

    free(conflicts);
}

static /*@exposed@*/ struct availablePackage * alSatisfiesDepend(struct availableList * al,
	const char *keyType, const char *keyDepend,
	const char * keyName, const char * keyEVR, int keyFlags)
{
    struct availableIndexEntry needle, * match;
    struct availablePackage *p;
    int i, rc;

    if (!al->index.size) return NULL;

    needle.entry = keyName;
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), indexcmp);

    if (match == NULL) return NULL;

    p = match->package;
    rc = 0;
    switch (match->type) {
    case IET_NAME:
    {	char *pEVR;
	int pFlags = RPMSENSE_EQUAL;
	pEVR = buildEVR(p->epoch, p->version, p->release);
	rc = rangesOverlap(p->name, pEVR, pFlags, keyName, keyEVR, keyFlags);
	free(pEVR);
	if (keyType && keyDepend && rc)
	    rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by added package.\n"), keyType, keyDepend);
    }	break;
    case IET_PROVIDES:
	for (i = 0; i < p->providesCount; i++) {
	    const char *proEVR;
	    int proFlags;

	    /* Filter out provides that came along for the ride. */
	    if (strcmp(p->provides[i], keyName))	continue;

	    proEVR = (p->providesEVR ? p->providesEVR[i] : NULL);
	    proFlags = (p->provideFlags ? p->provideFlags[i] : 0);
	    rc = rangesOverlap(p->provides[i], proEVR, proFlags,
			keyName, keyEVR, keyFlags);
	    if (rc) break;
	}
	if (keyType && keyDepend && rc)
	    rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by added provide.\n"), keyType, keyDepend);
    	break;
    case IET_FILE:
	rc = 1;
	if (keyType && keyDepend && rc)
	    rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by added file list.\n"), keyType, keyDepend);
	break;
    }

    if (rc)
	return p;

    return NULL;
}

/* 2 == error */
/* 1 == dependency not satisfied */
static int unsatisfiedDepend(rpmTransactionSet rpmdep,
	const char *keyType, const char *keyDepend,
	const char * keyName, const char * keyEVR, int keyFlags,
	/*@out@*/struct availablePackage ** suggestion)
{
    dbiIndexSet matches;
    int rc = 0;	/* assume dependency is satisfied */
    int i;

    if (suggestion) *suggestion = NULL;

  { const char * rcProvidesString;
    const char * start;
    if (!(keyFlags & RPMSENSE_SENSEMASK) &&
	(rcProvidesString = rpmGetVar(RPMVAR_PROVIDES))) {
	i = strlen(keyName);
	while ((start = strstr(rcProvidesString, keyName))) {
	    if (isspace(start[i]) || start[i] == '\0' || start[i] == ',') {
		rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by rpmrc provides.\n"), keyType, keyDepend);
		goto exit;
	    }
	    rcProvidesString = start + 1;
	}
    }
  }

    if (alSatisfiesDepend(&rpmdep->addedPackages, keyType, keyDepend, keyName, keyEVR, keyFlags)) {
	goto exit;
    }

    if (rpmdep->db != NULL) {
	if (*keyName == '/') {
	    /* keyFlags better be 0! */
	    if (!rpmdbFindByFile(rpmdep->db, keyName, &matches)) {
		for (i = 0; i < dbiIndexSetCount(matches); i++) {
		    unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		    if (bsearch(&recOffset,
				rpmdep->removedPackages,
				rpmdep->numRemovedPackages,
				sizeof(int), intcmp))
			continue;
		    break;
		}

		dbiFreeIndexRecord(matches);
		if (i < dbiIndexSetCount(matches)) {
		    rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by db file lists.\n"), keyType, keyDepend);
		    goto exit;
		}
	    }
	}

	if (!rpmdbFindByProvides(rpmdep->db, keyName, &matches)) {
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		if (bsearch(&recOffset,
			    rpmdep->removedPackages,
			    rpmdep->numRemovedPackages,
			    sizeof(int), intcmp))
		    continue;
		if (dbrecMatchesDepFlags(rpmdep, recOffset,
			 keyName, keyEVR, keyFlags, rangeMatchesDepFlags)) {
		    break;
		}
	    }

	    dbiFreeIndexRecord(matches);
	    if (i < dbiIndexSetCount(matches)) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by db provides.\n"), keyType, keyDepend);
		goto exit;
	    }
	}

	if (!rpmdbFindPackage(rpmdep->db, keyName, &matches)) {
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		if (bsearch(&recOffset,
			    rpmdep->removedPackages,
			    rpmdep->numRemovedPackages,
			    sizeof(int), intcmp))
		    continue;

		if (dbrecMatchesDepFlags(rpmdep, recOffset,
			 keyName, keyEVR, keyFlags, headerMatchesDepFlags)) {
		    break;
		}
	    }

	    dbiFreeIndexRecord(matches);
	    if (i < dbiIndexSetCount(matches)) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by db packages.\n"), keyType, keyDepend);
		goto exit;
	    }
	}

	/*
	 * New features in rpm spec files add implicit dependencies on rpm
	 * version. Provide implicit rpm version in last ditch effort to
	 * satisfy an rpm dependency.
	 */
	if (!strcmp(keyName, rpmNAME)) {
	    i = rangesOverlap(keyName, keyEVR, keyFlags, rpmNAME, rpmEVR, rpmFLAGS);
	    if (i) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %s satisfied by rpmlib version.\n"), keyType, keyDepend);
		goto exit;
	    }
	}
    }

    if (suggestion)
	*suggestion = alSatisfiesDepend(&rpmdep->availablePackages, NULL, NULL,
				keyName, keyEVR, keyFlags);

    rpmMessage(RPMMESS_DEBUG, _("%s: %s unsatisfied.\n"), keyType, keyDepend);
    rc = 1;	/* dependency is unsatisfied */

exit:
    return rc;
}

static int checkPackageDeps(rpmTransactionSet rpmdep, struct problemsSet * psp,
		Header h, const char *keyName)
{
    const char * name, * version, * release;
    const char ** requires, ** requiresEVR = NULL;
    const char ** conflicts, ** conflictsEVR = NULL;
    int requiresCount = 0, conflictsCount = 0;
    int type;
    int i, rc;
    int ourrc = 0;
    int_32 * requireFlags = NULL, * conflictFlags = NULL;
    struct availablePackage * suggestion;

    headerNVR(h, &name, &version, &release);

    if (!headerGetEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requires,
	     &requiresCount)) {
	requiresCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_REQUIREFLAGS, &type, (void **) &requireFlags,
		 &requiresCount);
	headerGetEntry(h, RPMTAG_REQUIREVERSION, &type,
		(void **) &requiresEVR, &requiresCount);
    }

    for (i = 0; i < requiresCount && !ourrc; i++) {
	const char *keyDepend;

	/* Filter out requires that came along for the ride. */
	if (keyName && strcmp(keyName, requires[i]))
	    continue;

	keyDepend = printDepend(requires[i], requiresEVR[i], requireFlags[i]);

	rc = unsatisfiedDepend(rpmdep, " requires", keyDepend,
		requires[i], requiresEVR[i], requireFlags[i], &suggestion);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    break;
	case 1:		/* requirements are not satisfied. */
	    rpmMessage(RPMMESS_DEBUG, _("package %s require not satisfied: %s\n"),
		    name, keyDepend);

	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = xrealloc(psp->problems, sizeof(*psp->problems) *
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = xstrdup(name);
	    psp->problems[psp->num].byVersion = xstrdup(version);
	    psp->problems[psp->num].byRelease = xstrdup(release);
	    psp->problems[psp->num].needsName = xstrdup(requires[i]);
	    psp->problems[psp->num].needsVersion = xstrdup(requiresEVR[i]);
	    psp->problems[psp->num].needsFlags = requireFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_REQUIRES;

	    if (suggestion)
		psp->problems[psp->num].suggestedPackage = suggestion->key;
	    else
		psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
	xfree(keyDepend);
    }

    if (requiresCount) {
	free(requiresEVR);
	free(requires);
    }

    if (!headerGetEntry(h, RPMTAG_CONFLICTNAME, &type, (void **) &conflicts,
	     &conflictsCount)) {
	conflictsCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_CONFLICTFLAGS, &type,
		(void **) &conflictFlags, &conflictsCount);
	headerGetEntry(h, RPMTAG_CONFLICTVERSION, &type,
		(void **) &conflictsEVR, &conflictsCount);
    }

    for (i = 0; i < conflictsCount && !ourrc; i++) {
	const char *keyDepend;

	/* Filter out conflicts that came along for the ride. */
	if (keyName && strcmp(keyName, conflicts[i]))
	    continue;

	keyDepend = printDepend(conflicts[i], conflictsEVR[i], conflictFlags[i]);

	rc = unsatisfiedDepend(rpmdep, "conflicts", keyDepend,
		conflicts[i], conflictsEVR[i], conflictFlags[i], NULL);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmMessage(RPMMESS_DEBUG, _("package %s conflicts: %s\n"),
		    name, keyDepend);

	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = xrealloc(psp->problems, sizeof(*psp->problems) *
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = xstrdup(name);
	    psp->problems[psp->num].byVersion = xstrdup(version);
	    psp->problems[psp->num].byRelease = xstrdup(release);
	    psp->problems[psp->num].needsName = xstrdup(conflicts[i]);
	    psp->problems[psp->num].needsVersion = xstrdup(conflictsEVR[i]);
	    psp->problems[psp->num].needsFlags = conflictFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_CONFLICTS;
	    psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	    break;
	case 1:		/* conflicts don't exist. */
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
	xfree(keyDepend);
    }

    if (conflictsCount) {
	free(conflictsEVR);
	free(conflicts);
    }

    return ourrc;
}

/* Adding: check name/provides key against each conflict match. */
/* Erasing: check name/provides/filename key against each requiredby match. */
static int checkPackageSet(rpmTransactionSet rpmdep, struct problemsSet * psp,
	const char *key, dbiIndexSet *matches)
{
    Header h;
    int i;

    for (i = 0; i < matches->count; i++) {
	unsigned int recOffset = dbiIndexRecordOffset(*matches, i);
	if (bsearch(&recOffset, rpmdep->removedPackages,
		    rpmdep->numRemovedPackages, sizeof(int), intcmp))
	    continue;

	h = rpmdbGetRecord(rpmdep->db, recOffset);
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT,
                     _("cannot read header at %d for dependency check"),
		     rpmdep->removedPackages[i]);
	    return 1;
	}

	if (checkPackageDeps(rpmdep, psp, h, key)) {
	    headerFree(h);
	    return 1;
	}

	headerFree(h);
    }

    return 0;
}

/* Erasing: check name/provides/filename key against requiredby matches. */
static int checkDependentPackages(rpmTransactionSet rpmdep,
			struct problemsSet * psp, const char * key)
{
    dbiIndexSet matches;
    int rc;

    if (rpmdbFindByRequiredBy(rpmdep->db, key, &matches))
	return 0;

    rc = checkPackageSet(rpmdep, psp, key, &matches);
    dbiFreeIndexRecord(matches);

    return rc;
}

/* Adding: check name/provides key against conflicts matches. */
static int checkDependentConflicts(rpmTransactionSet rpmdep,
		struct problemsSet * psp, const char * key)
{
    dbiIndexSet matches;
    int rc;

    if (rpmdep->db == NULL)
	return 0;

    if (rpmdbFindByConflicts(rpmdep->db, key, &matches))
	return 0;

    rc = checkPackageSet(rpmdep, psp, key, &matches);
    dbiFreeIndexRecord(matches);

    return rc;
}

/* selection status is one of:

	-1:	selected
	0:	not selected
	> 0:	selection class

   the current selection pass is included as a separate parameter, and is
   incremented when satisfying a prerequisite */

static int addOrderedPack(rpmTransactionSet rpmdep,
			struct availablePackage * package,
			int * ordering, int * orderNumPtr,
			int * selected, int selectionClass,
			int satisfyDepends, const char ** errorStack)
{
    const char ** requires;
    const char ** requiresEVR;
    int_32 * requireFlags;
    int requiresCount;
    int matchNum;
    int packageNum = package - rpmdep->addedPackages.list;
    int i;
    struct availablePackage * match;
    char * errorString;
    const char ** stack;
    int rc = 0;

    *errorStack++ = package->name;

    if (selected[packageNum] > 0) {
	i = 0;
	stack = errorStack - 1;
	while (*(--stack)) {
	    i += strlen(*stack) + 1;
	}

	errorString = alloca(i + 2);
	*errorString = '\0';

	while ((++stack) < errorStack) {
	    strcat(errorString, *stack);
	    strcat(errorString, " ");
	}

	rpmError(RPMMESS_PREREQLOOP, _("loop in prerequisite chain: %s"),
		 errorString);

	return 1;
    }

    selected[packageNum] = selectionClass;

    if (headerGetEntry(package->h, RPMTAG_REQUIRENAME, NULL,
			(void **) &requires, &requiresCount)) {
	headerGetEntry(package->h, RPMTAG_REQUIREFLAGS, NULL,
			(void **) &requireFlags, NULL);
	headerGetEntry(package->h, RPMTAG_REQUIREVERSION, NULL,
			(void **) &requiresEVR, NULL);

	for (i = 0; rc == 0 && i < requiresCount; i++) {
	    if (!(satisfyDepends || (requireFlags[i] & RPMSENSE_PREREQ)))
		continue;
	    match = alSatisfiesDepend(&rpmdep->addedPackages, NULL, NULL,
			  requires[i], requiresEVR[i], requireFlags[i]);
	    /* broken dependencies don't concern us */
	    if (!match) continue;

	    /* let this package satisfy its own predependencies */
	    if (match == package) continue;

	    /* the package has already been selected */
	    matchNum = match - rpmdep->addedPackages.list;
	    if(selected[matchNum] == -1 || selected[matchNum] == selectionClass)
		continue;

	    if (requireFlags[i] & RPMSENSE_PREREQ)
		rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
			        selected, selectionClass + 1, 1, errorStack);
	    else
		rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
			        selected, selectionClass, 1, errorStack);
	}

	free(requires);
	free(requiresEVR);
    }

    /* whew -- add this package */
    if (rc == 0) {
	ordering[(*orderNumPtr)++] = packageNum;
	selected[packageNum] = -1;
    }

    return rc;
}

static int orderListIndexCmp(const void * one, const void * two)
{
    const struct orderListIndex * a = one;
    const struct orderListIndex * b = two;

    if (a->alIndex < b->alIndex)
	return -1;
    if (a->alIndex > b->alIndex)
	return 1;

    return 0;
}

int rpmdepOrder(rpmTransactionSet rpmdep)
{
    int i, j;
    int * selected;
    int * ordering;
    int orderingCount;
    const char ** errorStack;
    struct transactionElement * newOrder;
    int newOrderCount = 0;
    struct orderListIndex * orderList, * needle, key;

    alMakeIndex(&rpmdep->addedPackages);
    alMakeIndex(&rpmdep->availablePackages);

    selected = alloca(sizeof(*selected) * rpmdep->addedPackages.size);
    memset(selected, 0, sizeof(*selected) * rpmdep->addedPackages.size);

    errorStack = alloca(sizeof(*errorStack) * (rpmdep->addedPackages.size + 1));
    *errorStack++ = NULL;

    ordering = alloca(sizeof(*ordering) * (rpmdep->addedPackages.size + 1));
    orderingCount = 0;

    for (i = 0; i < rpmdep->addedPackages.size; i++) {
	if (!selected[i]) {
	    if (addOrderedPack(rpmdep, rpmdep->addedPackages.list + i,
			       ordering, &orderingCount, selected, 1, 0,
			       errorStack)) {
		return 1;
	    }
	}
    }

    /* The order ends up as installed packages followed by removed packages,
       with removes for upgrades immediately follwing the installation of
       the new package. This would be easier if we could sort the
       addedPackages array, but we store indexes into it in various places. */
    orderList = xmalloc(sizeof(*orderList) * rpmdep->addedPackages.size);
    for (i = 0, j = 0; i < rpmdep->orderCount; i++) {
	if (rpmdep->order[i].type == TR_ADDED) {
	    orderList[j].alIndex = rpmdep->order[i].u.addedIndex;
	    orderList[j].orIndex = i;
	    j++;
	}
    }
    if (j > rpmdep->addedPackages.size) abort();

    qsort(orderList, rpmdep->addedPackages.size, sizeof(*orderList),
	  orderListIndexCmp);

    newOrder = xmalloc(sizeof(*newOrder) * rpmdep->orderCount); /* XXX memory leak */
    for (i = 0, newOrderCount = 0; i < orderingCount; i++) {
	key.alIndex = ordering[i];
	needle = bsearch(&key, orderList, rpmdep->addedPackages.size,
			 sizeof(key), orderListIndexCmp);
	/* bsearch should never, ever fail */

	newOrder[newOrderCount++] = rpmdep->order[needle->orIndex];
	for (j = needle->orIndex + 1; j < rpmdep->orderCount; j++) {
	    if (rpmdep->order[j].type == TR_REMOVED &&
		rpmdep->order[j].u.removed.dependsOnIndex == needle->alIndex) {
		newOrder[newOrderCount++] = rpmdep->order[j];
	    } else {
		break;
	    }
	}
    }

    for (i = 0; i < rpmdep->orderCount; i++) {
	if (rpmdep->order[i].type == TR_REMOVED &&
	    rpmdep->order[i].u.removed.dependsOnIndex == -1)  {
	    newOrder[newOrderCount++] = rpmdep->order[i];
	}
    }

    if (newOrderCount != rpmdep->orderCount) abort();

    free(rpmdep->order);
    rpmdep->order = newOrder;
    rpmdep->orderAlloced = rpmdep->orderCount;
    free(orderList);

    return 0;
}

int rpmdepCheck(rpmTransactionSet rpmdep,
		struct rpmDependencyConflict ** conflicts, int * numConflicts)
{
    struct availablePackage * p;
    int i, j;
    const char ** files;
    int fileCount;
    int rc;
    Header h = NULL;
    struct problemsSet ps;

    ps.alloced = 5;
    ps.num = 0;
    ps.problems = xcalloc(ps.alloced, sizeof(struct rpmDependencyConflict));

    *conflicts = NULL;
    *numConflicts = 0;

    qsort(rpmdep->removedPackages, rpmdep->numRemovedPackages,
	  sizeof(int), intcmp);

    alMakeIndex(&rpmdep->addedPackages);
    alMakeIndex(&rpmdep->availablePackages);

    /* look at all of the added packages and make sure their dependencies
       are satisfied */
    p = rpmdep->addedPackages.list;
    for (i = 0; i < rpmdep->addedPackages.size; i++, p++) {

	if (checkPackageDeps(rpmdep, &ps, p->h, NULL))
	    goto exit;

	/* Adding: check name against conflicts matches. */
	if (checkDependentConflicts(rpmdep, &ps, p->name))
	    goto exit;

	if (p->providesCount == 0 || p->provides == NULL)
	    continue;

	rc = 0;
	for (j = 0; j < p->providesCount; j++) {
	    /* Adding: check provides key against conflicts matches. */
	    if (checkDependentConflicts(rpmdep, &ps, p->provides[j])) {
		rc = 1;
		break;
	    }
	}
	if (rc)	goto exit;
    }

    /* now look at the removed packages and make sure they aren't critical */
    for (i = 0; i < rpmdep->numRemovedPackages; i++) {
	const char *name;
	const char ** provides;
	int providesCount;

	h = rpmdbGetRecord(rpmdep->db, rpmdep->removedPackages[i]);
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT,
			_("cannot read header at %d for dependency check"),
		        rpmdep->removedPackages[i]);
	    goto exit;
	}

	headerNVR(h, &name, NULL, NULL);

	/* Erasing: check name against requiredby matches. */
	if (checkDependentPackages(rpmdep, &ps, name))
	    goto exit;

	if (headerGetEntry(h, RPMTAG_PROVIDENAME, NULL, (void **) &provides,
		 &providesCount)) {
	    rc = 0;
	    for (j = 0; j < providesCount; j++) {
		/* Erasing: check provides against requiredby matches. */
		if (checkDependentPackages(rpmdep, &ps, provides[j])) {
		    rc = 1;
		    break;
		}
	    }
	    free(provides);
	    if (rc)	goto exit;
	}

	if (headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &files,
		 &fileCount)) {
	    rc = 0;
	    for (j = 0; j < fileCount; j++) {
		/* Erasing: check filename against requiredby matches. */
		if (checkDependentPackages(rpmdep, &ps, files[j])) {
		    rc = 1;
		    break;
		}
	    }

	    free(files);
	    if (rc) goto exit;
	}

	headerFree(h);	h = NULL;
    }

    if (!ps.num) {
	free(ps.problems);
    } else {
	*conflicts = ps.problems;
	*numConflicts = ps.num;
    }
    ps.problems = NULL;

    return 0;

exit:
    if (h) {
	headerFree(h);
    }
    if (ps.problems)	free(ps.problems);
    return 1;
}
