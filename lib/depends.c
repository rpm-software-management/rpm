#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

struct addedPackage {
    Header h;
    char ** provides;
    char * name, * version, * release;
    int serial, hasSerial, providesCount;
} ;

struct providesEntry {
    struct addedPackage * package;
    char * entry;
} ;

struct rpmDependencyCheck {
    rpmdb db;					/* may be NULL */
    int * removedPackages;
    int numRemovedPackages, allocedRemovedPackages;
    struct addedPackage * addedPackages;
    int numAddedPackages, allocedAddedPackages;
    struct providesEntry * providesTable;
    int numProvides;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};

static int intcmp(const void * a, const void *b);
static int providescmp(const void * a, const void *b);
static int unsatisfiedDepend(rpmDependencies rpmdep, char * reqName, 
			 char * reqVersion, int reqFlags);
static int checkDependentPackages(rpmDependencies rpmdep, 
			    struct problemsSet * psp, char * requires);
static int checkPackageDeps(rpmDependencies rpmdep, struct problemsSet * psp,
			Header h, const char * requirement);
static int dbrecMatchesDepFlags(rpmDependencies rpmdep, int recOffset, 
			        char * reqVersion, int reqFlags);
static int headerMatchesDepFlags(Header h, char * reqVersion, int reqFlags);

int providescmp(const void * a, const void *b) {
    const struct providesEntry * aptr = a;
    const struct providesEntry * bptr = b;

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

    rpmdep->numAddedPackages = 0;
    rpmdep->allocedAddedPackages = 5;
    rpmdep->providesTable = NULL;
    rpmdep->addedPackages = malloc(sizeof(struct addedPackage) * 
				     rpmdep->allocedAddedPackages);

    return rpmdep;
}

void rpmdepUpgradePackage(rpmDependencies rpmdep, Header h) {
    /* this is an install followed by uninstalls */
    dbIndexSet matches;
    char * name;
    int count, type, i;

    rpmdepAddPackage(rpmdep, h);

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
    struct addedPackage * p;
    int i, type;

    if (rpmdep->numAddedPackages == rpmdep->allocedAddedPackages) {
	rpmdep->allocedAddedPackages += 5;
	rpmdep->addedPackages = realloc(rpmdep->addedPackages,
		    sizeof(struct addedPackage) * rpmdep->allocedAddedPackages);
    }

    p = rpmdep->addedPackages + rpmdep->numAddedPackages++;
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

    if (rpmdep->providesTable) {
	free(rpmdep->providesTable);
	rpmdep->providesTable = NULL;
    }
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
     int i;

     for (i = 0; i < rpmdep->numAddedPackages; i++) {
	if (rpmdep->addedPackages[i].provides)
	    free(rpmdep->addedPackages[i].provides);
     }

     free(rpmdep->addedPackages);
     free(rpmdep->removedPackages);
     if (rpmdep->providesTable) free(rpmdep->providesTable);

     free(rpmdep);
}

int rpmdepCheck(rpmDependencies rpmdep, 
		struct rpmDependencyConflict ** conflicts, int * numConflicts) {
    struct addedPackage * p;
    int i, j, k;
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

    if (!rpmdep->providesTable) {
	rpmdep->numProvides = 0;
	for (i = 0; i < rpmdep->numAddedPackages; i++) {
	    rpmdep->numProvides += rpmdep->addedPackages[i].providesCount;
	}

	if (rpmdep->numProvides) {
	    rpmdep->providesTable = alloca(sizeof(*rpmdep->providesTable) * 
					   rpmdep->numProvides);
	    k = 0;
	    for (i = 0; i < rpmdep->numAddedPackages; i++) {
		for (j = 0; j < rpmdep->addedPackages[i].providesCount; j++) {
		    rpmdep->providesTable[k].package = 
				rpmdep->addedPackages + i;
		    rpmdep->providesTable[k].entry = 
				rpmdep->addedPackages[i].provides[j];
		    k++;
		}
	    }
	}
    }

    qsort(rpmdep->providesTable, rpmdep->numProvides, sizeof(char *), 
	  (void *) providescmp);
    
    /* look at all of the added packages and make sure their dependencies
       are satisfied */
    p = rpmdep->addedPackages;
    for (i = 0; i < rpmdep->numAddedPackages; i++, p++) {
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
    struct providesEntry prent, * provider;

    message(MESS_DEBUG, "dependencies: looking for %s\n", reqName);

    prent.entry = reqName;
    if ((provider = bsearch(&prent, rpmdep->providesTable, rpmdep->numProvides,
		sizeof(char *), (void *) providescmp))) {
 	if (headerMatchesDepFlags(provider->package->h, reqVersion, reqFlags))
	    return 0;
    }

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

	rc = unsatisfiedDepend(rpmdep, requires[i], "", requireFlags[i]);
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
