#include "system.h"

#include "rpmlib.h"

#include "depends.h"
#include "misc.h"

struct orderListIndex {
    int alIndex;
    int orIndex;
};

static int orderListIndexCmp(const void * one, const void * two);
static void alMakeIndex(struct availableList * al);
static void alCreate(struct availableList * al);
static void alFreeIndex(struct availableList * al);
static void alFree(struct availableList * al);
static struct availablePackage * alAddPackage(struct availableList * al, 
					      Header h, const void * key,
			 		      FD_t fd, rpmRelocation * relocs);
static int intcmp(const void * a, const void *b);
static int indexcmp(const void * a, const void *b);
static int unsatisfiedDepend(rpmTransactionSet rpmdep, char * reqName, 
			     char * reqVersion, int reqFlags,
			     struct availablePackage ** suggestion);
static int checkDependentPackages(rpmTransactionSet rpmdep, 
			    struct problemsSet * psp, char * key);
static int checkPackageDeps(rpmTransactionSet rpmdep, struct problemsSet * psp,
			Header h, const char * requirement);
static int dbrecMatchesDepFlags(rpmTransactionSet rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags);
struct availablePackage * alSatisfiesDepend(struct availableList * al, 
					    char * reqName, char * reqVersion, 
					    int reqFlags);
static int checkDependentConflicts(rpmTransactionSet rpmdep, 
			    struct problemsSet * psp, char * package);
static int checkPackageSet(rpmTransactionSet rpmdep, struct problemsSet * psp, 
			    char * package, dbiIndexSet * matches);
static int addOrderedPack(rpmTransactionSet rpmdep, 
			struct availablePackage * package,
			int * ordering, int * orderNumPtr, 
			int * selected, int selectionClass,
			int satisfyDepends, char ** errorStack);
static void removePackage(rpmTransactionSet rpmdep, int dboffset, int depends);

static void alCreate(struct availableList * al) {
    al->list = malloc(sizeof(*al->list) * 5);
    al->alloced = 5;
    al->size = 0;

    al->index.index = NULL;
    alFreeIndex(al);
}

static void alFreeIndex(struct availableList * al) {
    if (al->index.size) {
	if (al->index.index)
		free(al->index.index);
	al->index.index = NULL;
	al->index.size = 0;
    }
}

static void alFree(struct availableList * al) {
    int i;
    rpmRelocation * r;

    for (i = 0; i < al->size; i++) {
	if (al->list[i].provides)
	    free(al->list[i].provides);
	if (al->list[i].files)
	    free(al->list[i].files);
	headerFree(al->list[i].h);

	if (al->list[i].relocs) {
	    r = al->list[i].relocs;
	    while (r->oldPath || r->newPath) {
		if (r->oldPath) free(r->oldPath);
		if (r->newPath) free(r->newPath);
		r++;
	    }

	    free(al->list[i].relocs);
	}
    }

    if (al->alloced) free(al->list);
    alFreeIndex(al);
}

static struct availablePackage * alAddPackage(struct availableList * al, 
					      Header h, const void * key,
			 		      FD_t fd, rpmRelocation * relocs) {
    struct availablePackage * p;
    rpmRelocation * r;
    int i;

    if (al->size == al->alloced) {
	al->alloced += 5;
	al->list = realloc(al->list, sizeof(*al->list) * al->alloced);
    }

    p = al->list + al->size++;
    p->h = headerLink(h);

    headerGetEntry(p->h, RPMTAG_NAME, NULL, (void **) &p->name, NULL);
    headerGetEntry(p->h, RPMTAG_VERSION, NULL, (void **) &p->version, NULL);
    headerGetEntry(p->h, RPMTAG_RELEASE, NULL, (void **) &p->release, NULL);
    p->hasEpoch = headerGetEntry(h, RPMTAG_EPOCH, NULL, (void **) &p->epoch, 
				  NULL);

    if (!headerGetEntry(h, RPMTAG_PROVIDES, NULL, (void **) &p->provides,
	&p->providesCount)) {
	p->providesCount = 0;
	p->provides = NULL;
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
	p->relocs = malloc(sizeof(*p->relocs) * (i + 1));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? strdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? strdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    } else {
	p->relocs = NULL;
    }
    
    alFreeIndex(al);

    return p;
}

static void alMakeIndex(struct availableList * al) {
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
	ai->index = malloc(sizeof(*ai->index) * ai->size);
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

struct availablePackage * alSatisfiesDepend(struct availableList * al, 
					    char * reqName, char * reqVersion, 
					    int reqFlags) {
    struct availableIndexEntry needle, * match;

    if (!al->index.size) return NULL;

    needle.entry = reqName;
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), indexcmp);
 
    if (!match) return NULL;
    if (match->type != IET_NAME) return match->package;

    if (headerMatchesDepFlags(match->package->h, reqVersion, reqFlags))
	return match->package;

    return NULL;
}

static int indexcmp(const void * a, const void *b) {
    const struct availableIndexEntry * aptr = a;
    const struct availableIndexEntry * bptr = b;

    return strcmp(aptr->entry, bptr->entry);
}

int intcmp(const void * a, const void *b) {
    const int * aptr = a;
    const int * bptr = b;

    if (*aptr < *bptr) 
	return -1;
    else if (*aptr == *bptr)
	return 0;

    return 1;
}

rpmTransactionSet rpmtransCreateSet(rpmdb db, const char * root) {
    rpmTransactionSet rpmdep;
    int rootLength;

    if (!root) root = "";

    rpmdep = malloc(sizeof(*rpmdep));
    rpmdep->db = db;
    rpmdep->scriptFd = NULL;
    rpmdep->numRemovedPackages = 0;
    rpmdep->allocedRemovedPackages = 5;
    rpmdep->removedPackages = malloc(sizeof(int) * 
				     rpmdep->allocedRemovedPackages);

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

    rpmdep->root = strdup(root);

    alCreate(&rpmdep->addedPackages);
    alCreate(&rpmdep->availablePackages);

    rpmdep->orderAlloced = 5;
    rpmdep->orderCount = 0;
    rpmdep->order = malloc(sizeof(*rpmdep->order) * rpmdep->orderAlloced);

    return rpmdep;
}

int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
			const void * key, int upgrade, rpmRelocation * relocs) {
    /* this is an install followed by uninstalls */
    dbiIndexSet matches;
    char * name;
    int count, i, j;
    char ** obsoletes;
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
	rpmdep->order = realloc(rpmdep->order, 
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }
    rpmdep->order[rpmdep->orderCount].type = TR_ADDED;
    alNum = alAddPackage(&rpmdep->addedPackages, h, key, fd, relocs) - 
    		rpmdep->addedPackages.list;
    rpmdep->order[rpmdep->orderCount++].u.addedIndex = alNum;

    if (!upgrade || rpmdep->db == NULL) return 0;

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, &count);

    if (!rpmdbFindPackage(rpmdep->db, name, &matches))  {
	Header h2;

	for (i = 0; i < dbiIndexSetCount(matches); i++) {
	    h2 = rpmdbGetRecord(rpmdep->db, dbiIndexRecordOffset(matches, i));
	    if (h2) {
		if (rpmVersionCompare(h, h2)) {
		    removePackage(rpmdep, dbiIndexRecordOffset(matches, i), 
				  alNum);
		}
		headerFree(h2);
	    }
	}

	dbiFreeIndexRecord(matches);
    }

    if (headerGetEntry(h, RPMTAG_OBSOLETES, NULL, (void **) &obsoletes, 
			&count)) {
	for (j = 0; j < count; j++) {
	    if (!rpmdbFindPackage(rpmdep->db, obsoletes[j], &matches))  {
		for (i = 0; i < dbiIndexSetCount(matches); i++) {
		    removePackage(rpmdep, dbiIndexRecordOffset(matches, i),
				  alNum);
		}

		dbiFreeIndexRecord(matches);
	    }
	}

	free(obsoletes);
    }

    return 0;
}

void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h, void * key) {
    alAddPackage(&rpmdep->availablePackages, h, key, NULL, NULL);
}

static void removePackage(rpmTransactionSet rpmdep, int dboffset, int depends) {
    if (rpmdep->numRemovedPackages == rpmdep->allocedRemovedPackages) {
	rpmdep->allocedRemovedPackages += 5;
	rpmdep->removedPackages = realloc(rpmdep->removedPackages,
		sizeof(int *) * rpmdep->allocedRemovedPackages);
    }

    rpmdep->removedPackages[rpmdep->numRemovedPackages++] = dboffset;

    if (rpmdep->orderCount == rpmdep->orderAlloced) {
	rpmdep->orderAlloced += 5;
	rpmdep->order = realloc(rpmdep->order, 
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }

    rpmdep->order[rpmdep->orderCount].type = TR_REMOVED;
    rpmdep->order[rpmdep->orderCount].u.removed.dboffset = dboffset;
    rpmdep->order[rpmdep->orderCount++].u.removed.dependsOnIndex = depends;
}

void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset) {
    removePackage(rpmdep, dboffset, -1);
}

void rpmtransFree(rpmTransactionSet rpmdep) {
    alFree(&rpmdep->addedPackages);
    alFree(&rpmdep->availablePackages);
    free(rpmdep->removedPackages);
    free(rpmdep->root);

    free(rpmdep);
}

void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts, int
			 numConflicts) {
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

int rpmdepCheck(rpmTransactionSet rpmdep, 
		struct rpmDependencyConflict ** conflicts, int * numConflicts) {
    struct availablePackage * p;
    int i, j;
    char ** provides, ** files;
    int providesCount, fileCount;
    int type;
    char * name;
    Header h;
    struct problemsSet ps;

    ps.alloced = 5;
    ps.num = 0;
    ps.problems = malloc(sizeof(struct rpmDependencyConflict) * ps.alloced);

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
	if (checkPackageDeps(rpmdep, &ps, p->h, NULL)) {
	    free(ps.problems);
	    return 1;
	}
	if (checkDependentConflicts(rpmdep, &ps, p->name)) {
	    free(ps.problems);
	    return 1;
	}

	if (headerGetEntry(p->h, RPMTAG_PROVIDES, &type, (void **) &provides, 
		 &providesCount)) {
	    for (j = 0; j < providesCount; j++) {
		if (checkDependentConflicts(rpmdep, &ps, provides[j])) {
		    free(ps.problems);
		    return 1;
		}
	    }
	    free(provides);
	}
    }

    /* now look at the removed packages and make sure they aren't critical */
    for (i = 0; i < rpmdep->numRemovedPackages; i++) {
	h = rpmdbGetRecord(rpmdep->db, rpmdep->removedPackages[i]);
	if (h == NULL) {
	    rpmError(RPMERR_DBCORRUPT, 
			_("cannot read header at %d for dependency check"),
		        rpmdep->removedPackages[i]);
	    free(ps.problems);
	    return 1;
	}
	
	headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &providesCount);

	if (checkDependentPackages(rpmdep, &ps, name)) {
	    free(ps.problems);
	    headerFree(h);
	    return 1;
	}

	if (headerGetEntry(h, RPMTAG_PROVIDES, NULL, (void **) &provides, 
		 &providesCount)) {
	    for (j = 0; j < providesCount; j++) {
		if (checkDependentPackages(rpmdep, &ps, provides[j])) {
		    free(provides);
		    free(ps.problems);
		    headerFree(h);
		    return 1;
		}
	    }

	    free(provides);
	}

	if (headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &files, 
		 &fileCount)) {
	    for (j = 0; j < fileCount; j++) {
		if (checkDependentPackages(rpmdep, &ps, files[j])) {
		    free(files);
		    free(ps.problems);
		    headerFree(h);
		    return 1;
		}
	    }

	    free(files);
	}

	headerFree(h);
    }

    if (!ps.num) 
	free(ps.problems);
    else  {
	*conflicts = ps.problems;
	*numConflicts = ps.num;
    }

    return 0;
}

/* 2 == error */
/* 1 == dependency not satisfied */
static int unsatisfiedDepend(rpmTransactionSet rpmdep, char * reqName, 
			     char * reqVersion, int reqFlags, 
			     struct availablePackage ** suggestion) {
    dbiIndexSet matches;
    int i;
    char * rcProvidesString;
    char * start;
    
    rpmMessage(RPMMESS_DEBUG, _("dependencies: looking for %s\n"), reqName);

    if (suggestion) *suggestion = NULL;

    if (!(reqFlags & RPMSENSE_SENSEMASK) &&
	(rcProvidesString = rpmGetVar(RPMVAR_PROVIDES))) {
	i = strlen(reqName);
	while ((start = strstr(rcProvidesString, reqName))) {
	    if (isspace(start[i]) || !start[i])
		return 0;
	    rcProvidesString = start + 1;
	}
    }

    if (alSatisfiesDepend(&rpmdep->addedPackages, reqName, reqVersion, 
			  reqFlags))
	return 0;

    if (rpmdep->db != NULL) {
	if (*reqName == '/') {
	    /* reqFlags better be 0! */
	    if (!rpmdbFindByFile(rpmdep->db, reqName, &matches)) {
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
		if (i < dbiIndexSetCount(matches)) return 0;
	    }
	}

	if (!reqFlags && !rpmdbFindByProvides(rpmdep->db, reqName, 
					      &matches)) {
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
	    if (i < dbiIndexSetCount(matches)) return 0;
	}

	if (!rpmdbFindPackage(rpmdep->db, reqName, &matches)) {
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		if (bsearch(&recOffset,
			    rpmdep->removedPackages, 
			    rpmdep->numRemovedPackages, 
			    sizeof(int), intcmp)) 
		    continue;

		if (dbrecMatchesDepFlags(rpmdep, recOffset, 
					 reqVersion, reqFlags)) {
		    break;
		}
	    }

	    dbiFreeIndexRecord(matches);
	    if (i < dbiIndexSetCount(matches)) return 0;
	}
    }

    if (suggestion) 
	*suggestion = alSatisfiesDepend(&rpmdep->availablePackages, reqName, 
					reqVersion, reqFlags);

    return 1;
}

static int checkPackageSet(rpmTransactionSet rpmdep, struct problemsSet * psp, 
			    char * package, dbiIndexSet * matches) {
    int i;
    Header h;

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

	if (checkPackageDeps(rpmdep, psp, h, package)) {
	    headerFree(h);
	    return 1;
	}

	headerFree(h);
    }

    return 0;
}

static int checkDependentPackages(rpmTransactionSet rpmdep, 
			    struct problemsSet * psp, char * key) {
    dbiIndexSet matches;
    int rc;

    if (rpmdbFindByRequiredBy(rpmdep->db, key, &matches))  {
	return 0;
    }

    rc = checkPackageSet(rpmdep, psp, key, &matches);
    dbiFreeIndexRecord(matches);

    return rc;
}

static int checkDependentConflicts(rpmTransactionSet rpmdep, 
			    struct problemsSet * psp, char * package) {
    dbiIndexSet matches;
    int rc;

    if (rpmdep->db == NULL) return 0;

    if (rpmdbFindByConflicts(rpmdep->db, package, &matches)) {
	return 0;
    }

    rc = checkPackageSet(rpmdep, psp, package, &matches);
    dbiFreeIndexRecord(matches);

    return rc;
}

static int checkPackageDeps(rpmTransactionSet rpmdep, struct problemsSet * psp,
			Header h, const char * requirement) {
    char ** requires, ** requiresVersion;
    char * name, * version, * release;
    char ** conflicts, ** conflictsVersion;
    int requiresCount = 0, conflictsCount;
    int type, count;
    int i, rc;
    int ourrc = 0;
    int * requireFlags, * conflictsFlags;
    struct availablePackage * suggestion;

    if (!headerGetEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requires, 
	     &requiresCount)) {
	requiresCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_REQUIREFLAGS, &type, (void **) &requireFlags, 
		 &requiresCount);
	headerGetEntry(h, RPMTAG_REQUIREVERSION, &type, 
			(void **) &requiresVersion, 
		 &requiresCount);
    }

    if (!headerGetEntry(h, RPMTAG_CONFLICTNAME, &type, (void **) &conflicts,
	     &conflictsCount)) {
	conflictsCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_CONFLICTFLAGS, &type, 
			(void **) &conflictsFlags, &conflictsCount);
	headerGetEntry(h, RPMTAG_CONFLICTVERSION, &type,
			(void **) &conflictsVersion, 
		 &conflictsCount);
    }

    for (i = 0; i < requiresCount && !ourrc; i++) {
	if (requirement && strcmp(requirement, requires[i])) continue;

	rc = unsatisfiedDepend(rpmdep, requires[i], requiresVersion[i], 
			       requireFlags[i], &suggestion);
	if (rc == 1) {
	    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, 
				&count);
	    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, 
				&count);

	    rpmMessage(RPMMESS_DEBUG, _("package %s require not satisfied: %s\n"),
		    name, requires[i]);
	    
	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = realloc(psp->problems, sizeof(*psp->problems) * 
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = strdup(name);
	    psp->problems[psp->num].byVersion = strdup(version);
	    psp->problems[psp->num].byRelease = strdup(release);
	    psp->problems[psp->num].needsName = strdup(requires[i]);
	    psp->problems[psp->num].needsVersion = strdup(requiresVersion[i]);
	    psp->problems[psp->num].needsFlags = requireFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_REQUIRES;

	    if (suggestion)
		psp->problems[psp->num].suggestedPackage = suggestion->key;
	    else
		psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	} else if (rc == 2) {
	    /* something went wrong! */
	    ourrc = 1;
	}
    }

    for (i = 0; i < conflictsCount && !ourrc; i++) {
	if (requirement && strcmp(requirement, conflicts[i])) continue;

	rc = unsatisfiedDepend(rpmdep, conflicts[i], conflictsVersion[i], 
			       conflictsFlags[i], NULL);

	/* 1 == unsatisfied, 0 == satsisfied */
	if (rc == 0) {
	    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, 
				&count);
	    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, 
				&count);

	    rpmMessage(RPMMESS_DEBUG, _("package %s conflicts: %s\n"),
		    name, conflicts[i]);
	    
	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = realloc(psp->problems, sizeof(*psp->problems) * 
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = strdup(name);
	    psp->problems[psp->num].byVersion = strdup(version);
	    psp->problems[psp->num].byRelease = strdup(release);
	    psp->problems[psp->num].needsName = strdup(conflicts[i]);
	    psp->problems[psp->num].needsVersion = strdup(conflictsVersion[i]);
	    psp->problems[psp->num].needsFlags = conflictsFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_CONFLICTS;
	    psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	} else if (rc == 2) {
	    /* something went wrong! */
	    ourrc = 1;
	}
    }

    if (conflictsCount) {
	free(conflictsVersion);
	free(conflicts);
    }

    if (requiresCount) {
	free(requiresVersion);
	free(requires);
    }

    return ourrc;
}

int headerMatchesDepFlags(Header h, const char * reqInfo, int reqFlags) {
    const char * epoch, * version, * release;
    const char * reqEpoch = "0";
    const char * reqVersion = reqInfo;
    const char * reqRelease = NULL;
    const char *s, *se;
    int type, count;
    int_32 * epochval;
    char buf[20];
    int result = 0;
    int sense;

    if (!(reqFlags & RPMSENSE_SENSEMASK) || !reqInfo || !strlen(reqInfo))
	return 1;

    /* Get package information from header */
    headerGetEntry(h, RPMTAG_EPOCH, &type, (void **) &epochval, &count);
    if (epochval == NULL) {
	/* XXX old behavior looks fishy */
	if (reqFlags & RPMSENSE_SERIAL)
	    return 0;
	epoch = "0";
    } else {
	sprintf(buf, "%d", *epochval);
	epoch = buf;
    }
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **)&version, &count);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **)&release, &count);

    /* Parse requires version into components */
    s = reqInfo;
    while (*s && isdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':' || se) {
	char *rv = alloca(strlen(reqInfo) + 1);
	strcpy(rv, reqInfo);
	if (*s == ':') {
	    reqEpoch = rv;
	    rv[s - reqInfo] = '\0';
	    reqVersion = rv + (s - reqInfo) + 1;
	   if (*reqEpoch == '\0') reqEpoch = "0";
	} else {
	    reqEpoch = "0";
	    reqVersion = rv;
	}
	if (se) {
	    rv[se - reqInfo] = '\0';
	    reqRelease = rv + (se - reqInfo) + 1;
	}
    }

    /* Compare {package,rquires} epoch:version[-release] */
    sense = rpmvercmp(epoch, reqEpoch);
    if (sense == 0) {
	sense = rpmvercmp(version, reqVersion);
	if (sense == 0 && reqRelease && *reqRelease) {
	    sense = rpmvercmp(release, reqRelease);
	}
    }

    if ((reqFlags & RPMSENSE_LESS) && sense < 0) {
	result = 1;
    } else if ((reqFlags & RPMSENSE_EQUAL) && sense == 0) {
	result = 1;
    } else if ((reqFlags & RPMSENSE_GREATER) && sense > 0) {
	result = 1;
    }

    return result;
}

static int dbrecMatchesDepFlags(rpmTransactionSet rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags) {
    Header h;
    int rc;

    h = rpmdbGetRecord(rpmdep->db, recOffset);
    if (h == NULL) {
	rpmMessage(RPMMESS_DEBUG, _("dbrecMatchesDepFlags() failed to read header"));
	return 0;
    }

    rc = headerMatchesDepFlags(h, reqVersion, reqFlags);

    headerFree(h);

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
			int satisfyDepends, char ** errorStack) {
    char ** requires, ** requiresVersion;
    int_32 * requireFlags;
    int requiresCount;
    int matchNum;
    int packageNum = package - rpmdep->addedPackages.list;
    int i, rc;
    struct availablePackage * match;
    char * errorString;
    char ** stack;

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
			(void **) &requiresVersion, NULL);

	for (i = 0; i < requiresCount; i++) {
	    if (satisfyDepends || (requireFlags[i] & RPMSENSE_PREREQ)) {
		match = alSatisfiesDepend(&rpmdep->addedPackages,
					  requires[i], requiresVersion[i],
					  requireFlags[i]);
		/* broken dependencies don't concern us */
		if (!match) continue;
		
		/* let this package satisfy its own predependencies */
		if (match == package) continue;

		/* the package has already been selected */
		matchNum = match - rpmdep->addedPackages.list;
		if (selected[matchNum] == -1 ||
		    selected[matchNum] == selectionClass)
		    continue;
		
		if (requireFlags[i] & RPMSENSE_PREREQ)
		    rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
				        selected, selectionClass + 1, 1,
				        errorStack);
		else
		    rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
				        selected, selectionClass, 1,
				        errorStack);

		/* FIXME: I suspect we should free things here */
		if (rc) return 1;
	    }
	}

	free(requires);
	free(requiresVersion);
    }

    /* whew -- add this package */
    ordering[(*orderNumPtr)++] = packageNum;
    selected[packageNum] = -1;

    return 0;
}

int rpmdepOrder(rpmTransactionSet rpmdep) {
    int i, j;
    int * selected;
    int * ordering;
    int orderingCount;
    char ** errorStack;
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
    orderList = malloc(sizeof(*orderList) * rpmdep->addedPackages.size);
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

    newOrder = malloc(sizeof(*newOrder) * rpmdep->orderCount);
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

static int orderListIndexCmp(const void * one, const void * two) {
    const struct orderListIndex * a = one;
    const struct orderListIndex * b = two;

    if (a->alIndex < b->alIndex)
	return -1;
    if (a->alIndex > b->alIndex)
	return 1;

    return 0;
}
