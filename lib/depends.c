#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

struct availablePackage {
    Header h;
    char ** provides;
    char * name, * version, * release;
    int serial, hasSerial, providesCount;
} ;

struct availableIndexEntry {
    struct availablePackage * package;
    char * entry;
    int isProvides;
} ;

struct availableIndex {
    struct availableIndexEntry * index;
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
static void alAddPackage(struct availableList * al, Header h);

static int intcmp(const void * a, const void *b);
static int indexcmp(const void * a, const void *b);
static int unsatisfiedDepend(rpmDependencies rpmdep, char * reqName, 
			 char * reqVersion, int reqFlags);
static int checkDependentPackages(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * requires);
static int checkPackageDeps(rpmDependencies rpmdep, struct problemsSet * psp,
			Header h, const char * requirement);
static int dbrecMatchesDepFlags(rpmDependencies rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags);
static int headerMatchesDepFlags(Header h, char * reqVersion, int reqFlags);
int alSatisfiesDepend(struct availableList * al, char * reqName, 
                      char * reqVersion, int reqFlags);

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
     }

    if (al->size) free(al->list);
    alFreeIndex(al);
}

static void alAddPackage(struct availableList * al, Header h) {
    struct availablePackage * p;
    int i, type;

    if (al->size == al->alloced) {
	al->alloced += 5;
	al->list = realloc(al->list, sizeof(*al->list) * al->alloced);
    }

    p = al->list + al->size++;
    p->h = h;

    getEntry(p->h, RPMTAG_NAME, &type, (void **) &p->name, &i);
    getEntry(p->h, RPMTAG_VERSION, &type, (void **) &p->version, &i);
    getEntry(p->h, RPMTAG_RELEASE, &type, (void **) &p->release, &i);
    p->hasSerial = getEntry(h, RPMTAG_SERIAL, &type, (void **) &p->serial, &i);

    if (!getEntry(h, RPMTAG_PROVIDES, &type, (void **) &p->provides,
	&p->providesCount)) {
	p->providesCount = 0;
	p->provides = NULL;
    }

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

    if (ai->size) {
	ai->index = malloc(sizeof(*ai->index) * ai->size);
	k = 0;
	for (i = 0; i < al->size; i++) {
	    ai->index[k].package = al->list + i;
	    ai->index[k].entry = al->list[i].name;
	    ai->index[k].isProvides = 0;
	    k++;

	    for (j = 0; j < al->list[i].providesCount; j++) {
		ai->index[k].package = al->list + i;
		ai->index[k].entry = al->list[i].provides[j];
		ai->index[k].isProvides = 1;
		k++;
	    }
	}

	qsort(ai->index, ai->size, sizeof(*ai->index), (void *) indexcmp);
    }
}

int alSatisfiesDepend(struct availableList * al, char * reqName, 
		      char * reqVersion, int reqFlags) {
    struct availableIndexEntry needle, * match;

    if (!al->index.size) return 0;

    needle.entry = reqName;
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), (void *) indexcmp);
 
    if (!match) return 0;
    if (match->isProvides) return 1;

    if (headerMatchesDepFlags(match->package->h, reqVersion, reqFlags))
	return 1;

    return 0;
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
    dbIndexSet matches;
    char * name;
    int count, type, i;

    alAddPackage(&rpmdep->addedPackages, h);

    if (!rpmdep->db) return;

    getEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);

    if (!rpmdbFindPackage(rpmdep->db, name, &matches))  {
	for (i = 0; i < matches.count; i++) {
	    rpmdepRemovePackage(rpmdep, matches.recs[i].recOffset);
	}

	freeDBIndexRecord(matches);
    }
}

void rpmdepAddPackage(rpmDependencies rpmdep, Header h) {
    alAddPackage(&rpmdep->addedPackages, h);
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

int rpmdepCheck(rpmDependencies rpmdep, 
		struct rpmDependencyConflict ** conflicts, int * numConflicts) {
    struct availablePackage * p;
    int i, j;
    char ** provides;
    int providesCount;
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
    }

    /* now look at the removed packages and make sure they aren't critical */
    for (i = 0; i < rpmdep->numRemovedPackages; i++) {
	h = rpmdbGetRecord(rpmdep->db, rpmdep->removedPackages[i]);
	if (!h) {
	    error(RPMERR_DBCORRUPT, "cannot read header at %d for dependency "
		  "check", rpmdep->removedPackages[i]);
	    free(ps.problems);
	    return 1;
	}
	
	getEntry(h, RPMTAG_NAME, &type, (void **) &name, &providesCount);

	if (checkDependentPackages(rpmdep, &ps, name)) {
	    free(ps.problems);
	    return 1;
	}

	if (!getEntry(h, RPMTAG_PROVIDES, &type, (void **) &provides, 
		 &providesCount)) continue;

	for (j = 0; j < providesCount; j++) {
	    if (checkDependentPackages(rpmdep, &ps, provides[j])) {
		free(ps.problems);
		return 1;
	    }
	}
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
			 char * reqVersion, int reqFlags) {
    dbIndexSet matches;
    int i;

    message(MESS_DEBUG, "dependencies: looking for %s\n", reqName);

    if (alSatisfiesDepend(&rpmdep->addedPackages, reqName, reqVersion, 
			  reqFlags))
	return 0;

    if (!rpmdep->db) return 1;

    if (!reqFlags && !rpmdbFindByProvides(rpmdep->db, reqName, &matches))  {
	for (i = 0; i < matches.count; i++) {
	    if (bsearch(&matches.recs[i].recOffset, rpmdep->removedPackages, 
		        rpmdep->numRemovedPackages, sizeof(int *), intcmp)) 
		continue;
	    break;
	}

	freeDBIndexRecord(matches);
	if (i < matches.count) return 0;
    }

    if (!rpmdbFindPackage(rpmdep->db, reqName, &matches))  {
	for (i = 0; i < matches.count; i++) {
	    if (bsearch(&matches.recs[i].recOffset, rpmdep->removedPackages, 
		        rpmdep->numRemovedPackages, sizeof(int *), intcmp)) 
		continue;

	    if (dbrecMatchesDepFlags(rpmdep, matches.recs[i].recOffset, 
		                     reqVersion, reqFlags)) {
		break;
	    }
	}

	freeDBIndexRecord(matches);
	if (i < matches.count) return 0;
    }

    return 1;
}

static int checkDependentPackages(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * requires) {
    dbIndexSet matches;
    Header h;
    int i;

    if (rpmdbFindByRequiredBy(rpmdep->db, requires, &matches))  {
	return 0;
    }

    for (i = 0; i < matches.count; i++) {
	if (bsearch(&matches.recs[i].recOffset, rpmdep->removedPackages, 
		    rpmdep->numRemovedPackages, sizeof(int *), intcmp)) 
	    continue;

	h = rpmdbGetRecord(rpmdep->db, matches.recs[i].recOffset);
	if (!h) {
	    error(RPMERR_DBCORRUPT, "cannot read header at %d for dependency "
		  "check", rpmdep->removedPackages[i]);
	    return 1;
	}

	if (checkPackageDeps(rpmdep, psp, h, requires))
	    return 1;
    }

    return 0;
}

static int checkPackageDeps(rpmDependencies rpmdep, struct problemsSet * psp,
			Header h, const char * requirement) {
    char ** requires, ** requiresVersion;
    char * name, * version, * release;
    int requiresCount;
    int type, count;
    int i, rc;
    int * requireFlags;

    if (!getEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requires, 
	     &requiresCount)) return 0;
    if (!getEntry(h, RPMTAG_REQUIREFLAGS, &type, (void **) &requireFlags, 
	     &requiresCount)) return 0;
    if (!getEntry(h, RPMTAG_REQUIREVERSION, &type, (void **) &requiresVersion, 
	     &requiresCount)) return 0;
    if (!requiresCount) return 0;

    for (i = 0; i < requiresCount; i++) {
	if (requirement && strcmp(requirement, requires[i])) continue;

	rc = unsatisfiedDepend(rpmdep, requires[i], requiresVersion[i], 
			       requireFlags[i]);
	if (rc == 1) {
	    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
	    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

	    message(MESS_DEBUG, "package %s require not satisfied: %s\n",
		    name, requires[i]);
	    
	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = realloc(psp->problems, sizeof(*psp->problems) * 
			    psp->alloced);
	    }
	    psp->problems[psp->num].byName = name;
	    psp->problems[psp->num].byVersion = version;
	    psp->problems[psp->num].byRelease = release;
	    psp->problems[psp->num].needsName = requires[i];
	    psp->problems[psp->num].needsVersion = requiresVersion[i];
	    psp->problems[psp->num].needsFlags = requireFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_REQUIRES;

	    psp->num++;
	} else if (rc) {
	    /* something went wrong! */
	    free(requiresVersion);
	    free(requires);
	    return 1;
	}
    }

    free(requiresVersion);
    free(requires);

    return 0;
}

static int headerMatchesDepFlags(Header h, char * reqVersion, int reqFlags) {
    char * name, * version;
    int type, count;
    int_32 serial;
    char buf[20];
    int result = 0;
    int sense;

    getEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);

    if (!reqFlags) {
	return 1;
    }

    if (reqFlags & REQUIRE_SERIAL) {
	if (!getEntry(h, RPMTAG_SERIAL, &type, (void *) &serial, &count)) {
	    freeHeader(h);
	    return 0;
	}
	sprintf(buf, "%d", serial);
	version = buf;
    } else
	getEntry(h, RPMTAG_VERSION, &type, (void *) &version, &count);

    sense = vercmp(version, reqVersion);

    if ((reqFlags & REQUIRE_LESS) && sense < 0) {
	result = 1;
    } else if ((reqFlags & REQUIRE_EQUAL) && sense == 0) {
	result = 1;
    } else if ((reqFlags & REQUIRE_GREATER) && sense > 0) {
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
	message(MESS_DEBUG, "dbrecMatchesDepFlags() failed to read header");
	return 0;
    }

    rc = headerMatchesDepFlags(h, reqVersion, reqFlags);

    freeHeader(h);

    return rc;
}
