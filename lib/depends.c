#include "config.h"
#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "rpmlib.h"
#include "messages.h"

struct availablePackage {
    Header h;
    char ** provides;
    char ** files;
    char * name, * version, * release;
    int serial, hasSerial, providesCount, filesCount;
    void * key;
} ;

enum indexEntryType { IET_NAME, IET_PROVIDES, IET_FILE };

struct availableIndexEntry {
    struct availablePackage * package;
    char * entry;
    enum indexEntryType type;
} ;

struct availableIndex {
    struct availableIndexEntry * index ;
    int size;
} ;

struct availableList {
    struct availablePackage * list;
    struct availableIndex index;
    int size, alloced;
};

struct rpmDependencyCheck {
    rpmdb db;					/* may be NULL */
    int * removedPackages;
    int numRemovedPackages, allocedRemovedPackages;
    struct availableList addedPackages, availablePackages;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};

static void alMakeIndex(struct availableList * al);
static void alCreate(struct availableList * al);
static void alFreeIndex(struct availableList * al);
static void alFree(struct availableList * al);
static void alAddPackage(struct availableList * al, Header h, void * key);

static int intcmp(const void * a, const void *b);
static int indexcmp(const void * a, const void *b);
static int unsatisfiedDepend(rpmDependencies rpmdep, char * reqName, 
			     char * reqVersion, int reqFlags,
			     struct availablePackage ** suggestion);
static int checkDependentPackages(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * key);
static int checkPackageDeps(rpmDependencies rpmdep, struct problemsSet * psp,
			Header h, const char * requirement);
static int dbrecMatchesDepFlags(rpmDependencies rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags);
static int headerMatchesDepFlags(Header h, char * reqVersion, int reqFlags);
struct availablePackage * alSatisfiesDepend(struct availableList * al, 
					    char * reqName, char * reqVersion, 
					    int reqFlags);
static int checkDependentConflicts(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * package);
static int checkPackageSet(rpmDependencies rpmdep, struct problemsSet * psp, 
			    char * package, dbiIndexSet * matches);

static void alCreate(struct availableList * al) {
    al->list = malloc(sizeof(*al->list) * 5);
    al->alloced = 5;
    al->size = 0;

    al->index.index = NULL;
    alFreeIndex(al);
};

static void alFreeIndex(struct availableList * al) {
    if (al->index.size) {
	free(al->index.index);
	al->index.index = NULL;
	al->index.size = 0;
    }
}

static void alFree(struct availableList * al) {
    int i;

    for (i = 0; i < al->size; i++) {
	if (al->list[i].provides)
	    free(al->list[i].provides);
	if (al->list[i].files)
	    free(al->list[i].files);
    }

    if (al->alloced) free(al->list);
    alFreeIndex(al);
}

static void alAddPackage(struct availableList * al, Header h, void * key) {
    struct availablePackage * p;

    if (al->size == al->alloced) {
	al->alloced += 5;
	al->list = realloc(al->list, sizeof(*al->list) * al->alloced);
    }

    p = al->list + al->size++;
    p->h = h;

    headerGetEntry(p->h, RPMTAG_NAME, NULL, (void **) &p->name, NULL);
    headerGetEntry(p->h, RPMTAG_VERSION, NULL, (void **) &p->version, NULL);
    headerGetEntry(p->h, RPMTAG_RELEASE, NULL, (void **) &p->release, NULL);
    p->hasSerial = headerGetEntry(h, RPMTAG_SERIAL, NULL, (void **) &p->serial, 
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

    p->key = key;

    alFreeIndex(al);
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

	qsort(ai->index, ai->size, sizeof(*ai->index), (void *) indexcmp);
    }
}

struct availablePackage * alSatisfiesDepend(struct availableList * al, 
					    char * reqName, char * reqVersion, 
					    int reqFlags) {
    struct availableIndexEntry needle, * match;

    if (!al->index.size) return NULL;

    needle.entry = reqName;
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), (void *) indexcmp);
 
    if (!match) return NULL;
    if (match->type != IET_NAME) return match->package;

    if (headerMatchesDepFlags(match->package->h, reqVersion, reqFlags))
	return match->package;

    return NULL;
}

int indexcmp(const void * a, const void *b) {
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

rpmDependencies rpmdepDependencies(rpmdb db) {
    rpmDependencies rpmdep;

    rpmdep = malloc(sizeof(*rpmdep));
    rpmdep->db = db;
    rpmdep->numRemovedPackages = 0;
    rpmdep->allocedRemovedPackages = 5;
    rpmdep->removedPackages = malloc(sizeof(int) * 
				     rpmdep->allocedRemovedPackages);

    alCreate(&rpmdep->addedPackages);
    alCreate(&rpmdep->availablePackages);

    return rpmdep;
}

void rpmdepUpgradePackage(rpmDependencies rpmdep, Header h) {
    /* this is an install followed by uninstalls */
    dbiIndexSet matches;
    char * name;
    int count, type, i;

    alAddPackage(&rpmdep->addedPackages, h, NULL);

    if (!rpmdep->db) return;

    headerGetEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);

    if (!rpmdbFindPackage(rpmdep->db, name, &matches))  {
	for (i = 0; i < matches.count; i++) {
	    rpmdepRemovePackage(rpmdep, matches.recs[i].recOffset);
	}

	dbiFreeIndexRecord(matches);
    }
}

void rpmdepAddPackage(rpmDependencies rpmdep, Header h) {
    alAddPackage(&rpmdep->addedPackages, h, NULL);
}

void rpmdepAvailablePackage(rpmDependencies rpmdep, Header h, void * key) {
    alAddPackage(&rpmdep->availablePackages, h, key);
}

void rpmdepRemovePackage(rpmDependencies rpmdep, int dboffset) {
    if (rpmdep->numRemovedPackages == rpmdep->allocedRemovedPackages) {
	rpmdep->allocedRemovedPackages += 5;
	rpmdep->removedPackages = realloc(rpmdep->removedPackages,
		sizeof(int *) * rpmdep->allocedRemovedPackages);
    }

    rpmdep->removedPackages[rpmdep->numRemovedPackages++] = dboffset;
}

void rpmdepDone(rpmDependencies rpmdep) {
    alFree(&rpmdep->addedPackages);
    alFree(&rpmdep->availablePackages);
    free(rpmdep->removedPackages);

    free(rpmdep);
}

void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts, int
			 numConflicts) {
    int i;

    for (i = 0; i < numConflicts; i++) {
	free(conflicts[i].byName);
	free(conflicts[i].byVersion);
	free(conflicts[i].byRelease);
	free(conflicts[i].needsName);
	free(conflicts[i].needsVersion);
    }

    free(conflicts);
}

int rpmdepCheck(rpmDependencies rpmdep, 
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
	}
    }

    /* now look at the removed packages and make sure they aren't critical */
    for (i = 0; i < rpmdep->numRemovedPackages; i++) {
	h = rpmdbGetRecord(rpmdep->db, rpmdep->removedPackages[i]);
	if (!h) {
	    rpmError(RPMERR_DBCORRUPT, 
			"cannot read header at %d for dependency "
		  "check", rpmdep->removedPackages[i]);
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
static int unsatisfiedDepend(rpmDependencies rpmdep, char * reqName, 
			     char * reqVersion, int reqFlags, 
			     struct availablePackage ** suggestion) {
    dbiIndexSet matches;
    int i;

    rpmMessage(RPMMESS_DEBUG, "dependencies: looking for %s\n", reqName);

    if (suggestion) *suggestion = NULL;

    if (alSatisfiesDepend(&rpmdep->addedPackages, reqName, reqVersion, 
			  reqFlags))
	return 0;

    if (rpmdep->db) {
	if (*reqName == '/') {
	    /* reqFlags better be 0! */
	    if (!rpmdbFindByFile(rpmdep->db, reqName, &matches)) {
		for (i = 0; i < matches.count; i++) {
		    if (bsearch(&matches.recs[i].recOffset, 
				rpmdep->removedPackages, 
				rpmdep->numRemovedPackages, 
				sizeof(int), intcmp)) 
			continue;
		    break;
		}

		dbiFreeIndexRecord(matches);
		if (i < matches.count) return 0;
	    }
	} else {
	    if (!reqFlags && !rpmdbFindByProvides(rpmdep->db, reqName, 
						  &matches)) {
		for (i = 0; i < matches.count; i++) {
		    if (bsearch(&matches.recs[i].recOffset, 
				rpmdep->removedPackages, 
				rpmdep->numRemovedPackages, 
				sizeof(int), intcmp)) 
			continue;
		    break;
		}

		dbiFreeIndexRecord(matches);
		if (i < matches.count) return 0;
	    }

	    if (!rpmdbFindPackage(rpmdep->db, reqName, &matches)) {
		for (i = 0; i < matches.count; i++) {
		    if (bsearch(&matches.recs[i].recOffset, 
				rpmdep->removedPackages, 
				rpmdep->numRemovedPackages, 
				sizeof(int), intcmp)) 
			continue;

		    if (dbrecMatchesDepFlags(rpmdep, matches.recs[i].recOffset, 
					     reqVersion, reqFlags)) {
			break;
		    }
		}

		dbiFreeIndexRecord(matches);
		if (i < matches.count) return 0;
	    }
	}
    }

    if (suggestion) 
	*suggestion = alSatisfiesDepend(&rpmdep->availablePackages, reqName, 
					reqVersion, reqFlags);

    return 1;
}

static int checkPackageSet(rpmDependencies rpmdep, struct problemsSet * psp, 
			    char * package, dbiIndexSet * matches) {
    int i;
    Header h;

    for (i = 0; i < matches->count; i++) {
	if (bsearch(&matches->recs[i].recOffset, rpmdep->removedPackages, 
		    rpmdep->numRemovedPackages, sizeof(int), intcmp)) 
	    continue;

	h = rpmdbGetRecord(rpmdep->db, matches->recs[i].recOffset);
	if (!h) {
	    rpmError(RPMERR_DBCORRUPT, "cannot read header at %d for dependency "
		  "check", rpmdep->removedPackages[i]);
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

static int checkDependentPackages(rpmDependencies rpmdep, 
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

static int checkDependentConflicts(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * package) {
    dbiIndexSet matches;
    int rc;

    if (!rpmdep->db) return 0;

    if (rpmdbFindByConflicts(rpmdep->db, package, &matches)) {
	return 0;
    }

    rc = checkPackageSet(rpmdep, psp, package, &matches);

    return rc;
}

static int checkPackageDeps(rpmDependencies rpmdep, struct problemsSet * psp,
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

	    rpmMessage(RPMMESS_DEBUG, "package %s require not satisfied: %s\n",
		    name, requires[i]);
	    
	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = realloc(psp->problems, sizeof(*psp->problems) * 
			    psp->alloced);
	    }
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

	    rpmMessage(RPMMESS_DEBUG, "package %s conflicts: %s\n",
		    name, conflicts[i]);
	    
	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = realloc(psp->problems, sizeof(*psp->problems) * 
			    psp->alloced);
	    }
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

static int headerMatchesDepFlags(Header h, char * reqInfo, int reqFlags) {
    char * name, * version, * release, * chptr;
    char * reqVersion = reqInfo;
    char * reqRelease = NULL;
    int type, count;
    int_32 serial;
    char buf[20];
    int result = 0;
    int sense;

    headerGetEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);

    if (!reqFlags) {
	return 1;
    }

    if (reqFlags & RPMSENSE_SERIAL) {
	if (!headerGetEntry(h, RPMTAG_SERIAL, &type, (void *) &serial, &count)) {
	    headerFree(h);
	    return 0;
	}
	sprintf(buf, "%d", serial);
	version = buf;
    } else {
	headerGetEntry(h, RPMTAG_VERSION, &type, (void *) &version, &count);
	chptr = strrchr(reqInfo, '-');
	if (chptr) {
	    reqVersion = alloca(strlen(reqInfo) + 1);
	    strcpy(reqVersion, reqInfo);
	    reqVersion[chptr - reqInfo] = '\0';
	    reqRelease = reqVersion + (chptr - reqInfo) + 1;
	    if (*reqRelease) 
		headerGetEntry(h, RPMTAG_RELEASE, &type, (void *) &release, &count);
	    else
		reqRelease = NULL;
	}
    }

    sense = rpmvercmp(version, reqVersion);
    if (!sense && reqRelease) {
	/* if a release number is given, use it to break ties */
	sense = rpmvercmp(release, reqRelease);
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

static int dbrecMatchesDepFlags(rpmDependencies rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags) {
    Header h;
    int rc;

    h = rpmdbGetRecord(rpmdep->db, recOffset);
    if (!h) {
	rpmMessage(RPMMESS_DEBUG, "dbrecMatchesDepFlags() failed to read header");
	return 0;
    }

    rc = headerMatchesDepFlags(h, reqVersion, reqFlags);

    headerFree(h);

    return rc;
}
