#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#include "rpmlib.h"

struct addedPackage {
    Header h;
    char ** provides;
    char * name, * version, * release;
    int serial, hasSerial, providesCount;
} ;

struct rpmDependencyCheck {
    rpmdb db;					/* may be NULL */
    int * removedPackages;
    int numRemovedPackages, allocedRemovedPackages;
    struct addedPackage * addedPackages;
    int numAddedPackages, allocedAddedPackages;
    char ** providesTable;
    int numProvides;
};

static int intcmp(const void * a, const void *b);
static int badDependency(rpmDependencies rpmdep, char * reqName, 
			 char * reqVersion, int reqFlags);

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

    if (!getEntry(h, RPMTAG_RELEASE, &type, (void **) &p->provides,
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
    struct rpmDependencyConflict * problems;
    int numProblems = 0;
    int allocedProblems = 5;
    char ** requires;
    int requiresCount;
    int type, rc;

    *conflicts = NULL;
    *numConflicts = 0;
    problems = malloc(sizeof(struct rpmDependencyConflict) * 
		      allocedProblems);

    qsort(rpmdep->removedPackages, rpmdep->numRemovedPackages,
	  sizeof(int), intcmp);

    if (!rpmdep->providesTable) {
	rpmdep->numProvides = 0;
	for (i = 0; i < rpmdep->numAddedPackages; i++) {
	    rpmdep->numProvides += rpmdep->addedPackages[i].providesCount;
	}

	if (rpmdep->numProvides) {
	    rpmdep->providesTable = alloca(sizeof(char *) * 
					   rpmdep->numProvides);
	    k = 0;
	    for (i = 0; i < rpmdep->numAddedPackages; i++) {
		for (j = 0; j < rpmdep->addedPackages[i].providesCount; j++) {
		    rpmdep->providesTable[k++] = 
			    rpmdep->addedPackages[i].provides[j];
		}
	    }
	}
    }

    qsort(rpmdep->providesTable, rpmdep->numProvides, sizeof(char *), 
	  (void *) strcmp);
    
    /* look at all of the added packages and make sure their dependencies
       are satisfied */
    p = rpmdep->addedPackages;
    for (i = 0; i < rpmdep->numAddedPackages; i++, p++) {
	if (!getEntry(p->h, RPMTAG_REQUIRENAME, &type, (void **) &requires, 
		 &requiresCount)) continue;
	if (!requiresCount) continue;

	for (j = 0; j < requiresCount; j++) {
	    rc = badDependency(rpmdep, requires[j], "", 0);
	    if (rc == 1) {
		message(MESS_DEBUG, "package %s require not satisfied: %s\n",
			p->name, requires[j]);
		
		if (numProblems == allocedProblems) {
		    allocedProblems += 5;
		    problems = realloc(problems, sizeof(*problems) * 
				allocedProblems);
		}
		problems[numProblems].byName = p->name;
		problems[numProblems].byVersion = p->version;
		problems[numProblems].byRelease = p->release;
		problems[numProblems].needsName = requires[j];
		problems[numProblems].needsVersion = NULL;
		problems[numProblems].needsFlags = 0;
		numProblems++;
	    } else if (rc) {
		/* something went wrong! */
		free(requires);
		free(problems);
		return 1;
	    }
	}

	free(requires);
    }

    if (!numProblems) 
	free(problems);
    else  {
	*conflicts = problems;
	*numConflicts = numProblems;
    }

    return 0;
}

/* 2 == error */
/* 1 == dependency not satisfied */
static int badDependency(rpmDependencies rpmdep, char * reqName, 
			 char * reqVersion, int reqFlags) {
    dbIndexSet matches;
    int i;

    message(MESS_DEBUG, "dependencies: looking for %s\n", reqName);

    if (bsearch(reqName, rpmdep->providesTable, rpmdep->numProvides,
		sizeof(char *), (void *) strcmp)) {
	/* this needs to check the reqFlags! ***/
	return 0;
    }

    if (!rpmdbFindByProvides(rpmdep->db, reqName, &matches))  {
	for (i = 0; i < matches.count; i++) {
	    if (bsearch(&i, rpmdep->removedPackages, 
		        rpmdep->numRemovedPackages, sizeof(int *), intcmp)) 
		continue;

	    /* this needs to check the reqFlags ! ***/
	    break;
	}

	freeDBIndexRecord(matches);
	if (i < matches.count) return 0;
    }

    return 1;
}
