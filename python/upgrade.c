#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <glob.h>	/* XXX rpmio.h */
#include <dirent.h>	/* XXX rpmio.h */

#include <rpmlib.h>

#include "hash.h"
#include "upgrade.h"

#define MAXPKGS 1024

#define USEDEBUG 0

#define DEBUG(x) {   \
     if (USEDEBUG)   \
         printf x; \
     }

#if 0
static void printMemStats(char *mess)
{
    char buf[1024];
    printf("%s\n", mess);
    sprintf(buf, "cat /proc/%d/status | grep VmSize", getpid());
    system(buf);
}
#endif

int pkgCompare(void * first, void * second) {
    struct packageInfo ** a = first;
    struct packageInfo ** b = second;

    /* put packages w/o names at the end */
    if (!(*a)->name) return 1;
    if (!(*b)->name) return -1;

    return strcasecmp((*a)->name, (*b)->name);
}


/* Adds all files in the second file list which are not in the first
   file list to the hash table. */
static void compareFileList(int availFileCount, char **availBaseNames,
			    char ** availDirNames, int * availDirIndexes,
			    int instFileCount, char **instBaseNames,
			    char ** instDirNames, int * instDirIndexes,
			    struct hash_table *ht)
{
    int installedX, availX, rc;
    char * availDir, * availBase;
    char * instDir, * instBase;
    static int i = 0;
    
    availX = 0;
    installedX = 0;
    while (installedX < instFileCount) {
	instBase = instBaseNames[installedX];
	instDir = instDirNames[instDirIndexes[installedX]];

	if (availX == availFileCount) {
	    /* All the rest have moved */
	    DEBUG(("=> %d: %s%s\n", i++, instDir, instBase))
	    if (strncmp(instDir, "/etc/rc.d/", 10))
		htAddToTable(ht, instDir, instBase);
	    installedX++;
	} else {
	    availBase = availBaseNames[availX];
	    availDir = availDirNames[availDirIndexes[availX]];

	    rc = strcmp(availDir, instDir);
	    if (!rc) 
		rc = strcmp(availBase, instBase);

	    if (rc > 0) {
		/* Avail > Installed -- file has moved */
		DEBUG(("=> %d: %s%s\n", i++, instDir, instBase))
		if (strncmp(instDir, "/etc/rc.d/", 10))
		    htAddToTable(ht, instDir, instBase);
		installedX++;
	    } else if (rc < 0) {
		/* Avail < Installed -- avail has some new files */
		availX++;
	    } else {
		/* Files are equal -- file not moved */
		availX++;
		installedX++;
	    }
	}
    }
}

static void addLostFiles(rpmdb db, struct pkgSet *psp, struct hash_table *ht)
{
    char *name;
    struct packageInfo **pack;
    struct packageInfo key;
    struct packageInfo *keyaddr = &key;
    char **installedFiles;
    char **installedDirs;
    int_32 * installedDirIndexes;
    int installedFileCount;

  { Header h = NULL;

#ifdef	DYING
    int num;

    for (num = rpmdbFirstRecNum(db);
	 num > 0;
	 num = rpmdbNextRecNum(db, num))
    {
	if (h) {
	    headerFree(h);
	    h = NULL;
	}
	h = rpmdbGetRecord(db, num);
#else
    rpmdbMatchIterator mi;

    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, NULL, 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
#endif

	headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, NULL);
	if (name && !strcmp(name, "metroess")) {
	    /* metro was removed from 5.1, but leave it if it's already
	       installed */
	    continue;
	}
	key.name = name;
	
	pack = bsearch(&keyaddr, psp->packages, psp->numPackages,
		       sizeof(*psp->packages), (void *)pkgCompare);
	if (!pack) {
	    if (headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL,
			  (void **) &installedFiles, &installedFileCount)) {
		headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL,
			  (void **) &installedDirIndexes, NULL);
		headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL,
			  (void **) &installedDirs, NULL);

		compareFileList(0, NULL, NULL, NULL, installedFileCount,
				installedFiles, installedDirs,
				installedDirIndexes, ht);

		free(installedFiles);
		free(installedDirs);
	    }
	}
#ifndef	DYING
    }

    rpmdbFreeIterator(mi);
#else
    }
    if (h) {
	headerFree(h);
	h = NULL;
    }
#endif
  }
}

static int findPackagesWithObsoletes(rpmdb db, struct pkgSet *psp)
{
    int count, obsoletesCount;
    struct packageInfo **pip;
    char **obsoletes;

    count = psp->numPackages;
    pip = psp->packages;
    while (count--) {
	if ((*pip)->selected) {
	    pip++;
	    continue;
	}

	if (headerGetEntryMinMemory((*pip)->h, RPMTAG_OBSOLETES, NULL,
		       (void **) &obsoletes, &obsoletesCount)) {
	    while (obsoletesCount--) {
#ifdef	DYING
		dbiIndexSet matches;
		int rc;

		rc = rpmdbFindPackage(db, obsoletes[obsoletesCount], &matches);
		if (!rc) {
		    if (dbiIndexSetCount(matches)) {
			(*pip)->selected = 1;
			dbiFreeIndexSet(matches);
			break;
		    }

		    dbiFreeIndexSet(matches);
		}
#else
		if (rpmdbCountPackages(db, obsoletes[obsoletesCount]) > 0) {
		    (*pip)->selected = 1;
		    break;
		}
#endif
	    }

	    free(obsoletes);
	}

	pip++;
    }

    return 0;
}

static void errorFunction(void)
{
}

static int findUpgradePackages(rpmdb db, struct pkgSet *psp,
			       struct hash_table *ht)
{
    int skipThis;
    Header h, installedHeader;
    char *name;
#ifdef	DYING
    dbiIndexSet matches;
    int rc, i;
#endif
    int count;
    char **installedFiles, **availFiles;
    char **installedDirs, ** availDirs;
    int_32 * installedDirIndexes, * availDirIndexes;
    int installedFileCount, availFileCount;
    struct packageInfo **pip;

    count = psp->numPackages;
    pip = psp->packages;
    while (count--) {
	h = (*pip)->h;
	name = NULL;
	headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, NULL);
	if (!name) {
	    /* bum header */
	    /*logMessage("Failed with bad header");*/
	    return(-1);
	}
	
	DEBUG (("Avail: %s\n", name));
#ifdef	DYING
	rc = rpmdbFindPackage(db, name, &matches);

	if (rc == 0) {
	    skipThis = 0;
	    rpmErrorSetCallback(errorFunction);
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		installedHeader =
		    rpmdbGetRecord(db, dbiIndexRecordOffset(matches, i));
		if (rpmVersionCompare(installedHeader, h) >= 0) {
		    /* already have a newer version installed */
		    DEBUG (("Already have newer version\n"))
		    skipThis = 1;
		    headerFree(installedHeader);
		    break;
		}
		headerFree(installedHeader);
	    }
	    rpmErrorSetCallback(NULL);
	    if (! skipThis) {
		DEBUG (("No newer version installed\n"))
	    }
	} else {
	    skipThis = 1;
	    DEBUG (("Not installed\n"))
	}
#else
    {	rpmdbMatchIterator mi;

	mi = rpmdbInitIterator(db, RPMDBI_NAME, name, 0);
	skipThis = (mi ? 0 : 1);
	rpmErrorSetCallback(errorFunction);
	while((installedHeader = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmVersionCompare(installedHeader, h) >= 0) {
		/* already have a newer version installed */
		DEBUG (("Already have newer version\n"))
		skipThis = 1;
		break;
	    }
	}
	rpmdbFreeIterator(mi);
	rpmErrorSetCallback(NULL);
	if (! skipThis) {
	    DEBUG (("No newer version installed\n"))
	}
    }
#endif
	
	if (skipThis) {
	    DEBUG (("DO NOT INSTALL\n"))
	} else {
	    DEBUG (("UPGRADE\n"))
	    (*pip)->selected = 1;

	    if (!headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL,
			  (void **) &availFiles, &availFileCount)) {
		availFiles = NULL;
		availFileCount = 0;
	    } else {
		headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL,
			    (void **) &availDirs, NULL);
		headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL,
			    (void **) &availDirIndexes, NULL);
	    }

#ifdef	DYING
	    for (i = 0; i < dbiIndexSetCount(matches); i++) {
		/* Compare the file lists */
		installedHeader =
		    rpmdbGetRecord(db, dbiIndexRecordOffset(matches, i));
		if (headerGetEntryMinMemory(installedHeader, RPMTAG_BASENAMES, 
			      NULL, (void **) &installedFiles,
			      &installedFileCount)) {
		    headerGetEntryMinMemory(installedHeader, RPMTAG_DIRNAMES, 
				NULL, (void **) &installedDirs, NULL);
		    headerGetEntryMinMemory(installedHeader, RPMTAG_DIRINDEXES, 
				NULL, (void **) &installedDirIndexes, NULL);

		    compareFileList(availFileCount, availFiles,
				    availDirs, availDirIndexes,
				    installedFileCount, installedFiles, 
				    installedDirs, installedDirIndexes,
				    ht);

		    free(installedFiles);
		    free(installedDirs);
		}
		headerFree(installedHeader);
	    }
#else
	{   rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(db, RPMDBI_NAME, name, 0);
	    while((installedHeader = rpmdbNextIterator(mi)) != NULL) {
		if (headerGetEntryMinMemory(installedHeader, RPMTAG_BASENAMES, 
			      NULL, (void **) &installedFiles,
			      &installedFileCount)) {
		    headerGetEntryMinMemory(installedHeader, RPMTAG_DIRNAMES, 
				NULL, (void **) &installedDirs, NULL);
		    headerGetEntryMinMemory(installedHeader, RPMTAG_DIRINDEXES, 
				NULL, (void **) &installedDirIndexes, NULL);

		    compareFileList(availFileCount, availFiles,
				    availDirs, availDirIndexes,
				    installedFileCount, installedFiles, 
				    installedDirs, installedDirIndexes,
				    ht);

		    free(installedFiles);
		    free(installedDirs);
		}
	    }
	    rpmdbFreeIterator(mi);
	}
#endif

	    if (availFiles) {
		free(availFiles);
		free(availDirs);
	    }
	}

#ifdef	DYING
	if (rc == 0) {
	    dbiFreeIndexSet(matches);
	}
#endif

	DEBUG (("\n\n"))

	pip++;
    }

    return 0;
}

static int removeMovedFilesAlreadyHandled(struct pkgSet *psp,
					  struct hash_table *ht)
{
    char *name;
    int i, count;
    Header h;
    char **availFiles, ** availDirs;
    int_32 * availDirIndexes;
    int availFileCount;
    struct packageInfo **pip;

    count = psp->numPackages;
    pip = psp->packages;
    while (count--) {
	h = (*pip)->h;
	if ((*pip)->selected) {
	    name = NULL;
	    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, NULL);

	    if (headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL,
			  (void **) &availFiles, &availFileCount)) {

		headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, 
			       (void **) &availDirs, NULL);
		headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, 
			       (void **) &availDirIndexes, NULL);

		for (i = 0; i < availFileCount; i++) {
		    if (htInTable(ht, availDirs[availDirIndexes[i]],
					  availFiles[i])) {
			htRemoveFromTable(ht, availDirs[availDirIndexes[i]],
					  availFiles[i]);
			DEBUG (("File already in %s: %s%s\n", name, 
				availDirs[availDirIndexes[i]], availFiles[i]))
			break;
		    }
		}

		free(availFiles);
		free(availDirs);
	    }
	}

	pip++;
    }

    return 0;
}

static int findPackagesWithRelocatedFiles(struct pkgSet *psp,
					  struct hash_table *ht)
{
    char *name;
    int i, count;
    Header h;
    char **availFiles, **availDirs;
    int_32 * availDirIndexes;
    int availFileCount;
    struct packageInfo **pip;
    int_16 * availFileModes;

    count = psp->numPackages;
    pip = psp->packages;
    while (count--) {
	h = (*pip)->h;
	if (! (*pip)->selected) {
	    name = NULL;
	    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, NULL);

	    if (headerGetEntry(h, RPMTAG_BASENAMES, NULL,
			 (void **) &availFiles, &availFileCount)) {
		headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL,
			    (void **) &availDirs, NULL);
		headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL,
			    (void **) &availDirIndexes, NULL);
		headerGetEntryMinMemory(h, RPMTAG_FILEMODES, NULL,
			    (void **) &availFileModes, NULL);

		for (i = 0; i < availFileCount; i++) {
		    if (S_ISDIR(availFileModes[i])) continue;

		    if (htInTable(ht, availDirs[availDirIndexes[i]], 
				    availFiles[i])) {
			htRemoveFromTable(ht, availDirs[availDirIndexes[i]],
					  availFiles[i]);
			DEBUG (("Found file in %s: %s%s\n", name,
				availDirs[availDirIndexes[i]], availFiles[i]))
			(*pip)->selected = 1;
			break;
		    }
		}
		free(availFiles);
		free(availDirs);
	    }
	}

	pip++;
    }

    return 0;
}

/*
static void printCount(struct pkgSet *psp)
{
    int i, upgradeCount;
    struct packageInfo *pip;
    
    upgradeCount = 0;
    pip = psp->packages;
    i = psp->numPackages;
    while (i--) {
	if (pip->selected) {
	    upgradeCount++;
	}
	pip++;
    }
    logMessage("marked %d packages for upgrade", upgradeCount);
}
*/

static int unmarkPackagesAlreadyInstalled(rpmdb db, struct pkgSet *psp)
{
    Header h, installedHeader;
    char *name;
    struct packageInfo **pip;
    int count;

    count = psp->numPackages;
    pip = psp->packages;
    while (count--) {
	if ((*pip)->selected) {
	    h = (*pip)->h;
	    /* If this package is already installed, don't bother */
	    name = NULL;
	    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name, NULL);
	    if (!name) {
		/* bum header */
		/*logMessage("Failed with bad header");*/
		return(-1);
	    }
#ifdef	DYING
	{   dbiIndexSet matches;
	    int rc;
	    int i;

	    rc = rpmdbFindPackage(db, name, &matches);
	    if (rc == 0) {
		rpmErrorSetCallback(errorFunction);
		for (i = 0; i < dbiIndexSetCount(matches); i++) {
		    installedHeader =
			rpmdbGetRecord(db, dbiIndexRecordOffset(matches, i));
		    if (rpmVersionCompare(installedHeader, h) >= 0) {
			/* already have a newer version installed */
			DEBUG (("Already have newer version\n"))
			(*pip)->selected = 0;
			headerFree(installedHeader);
			break;
		    }
		    headerFree(installedHeader);
		}
		rpmErrorSetCallback(NULL);
		dbiFreeIndexSet(matches);
	    }
	}
#else
	{   rpmdbMatchIterator mi;

	    mi = rpmdbInitIterator(db, RPMDBI_NAME, name, 0);
	    rpmErrorSetCallback(errorFunction);
	    while((installedHeader = rpmdbNextIterator(mi)) != NULL) {
		if (rpmVersionCompare(installedHeader, h) >= 0) {
		    /* already have a newer version installed */
		    DEBUG (("Already have newer version\n"))
		    (*pip)->selected = 0;
		    break;
		}
	    }
	    rpmdbFreeIterator(mi);
	    rpmErrorSetCallback(NULL);
	}
#endif
	}

	pip++;
    }

    return 0;
}
	    
static void emptyErrorCallback(void) {
}

int ugFindUpgradePackages(struct pkgSet *psp, char *installRoot)
{
    rpmdb db;
    struct hash_table *hashTable;
    rpmErrorCallBackType old;    

    /*logDebugMessage(("ugFindUpgradePackages() ..."));*/

    rpmReadConfigFiles(NULL, NULL);

    rpmSetVerbosity(RPMMESS_FATALERROR);
    old = rpmErrorSetCallback(emptyErrorCallback);

    if (rpmdbOpenForTraversal(installRoot, &db)) {
	/*logMessage("failed opening %s/var/lib/rpm/packages.rpm",
		     installRoot);*/
	return(-1);
    }

    rpmErrorSetCallback(old);
    rpmSetVerbosity(RPMMESS_NORMAL);
    
    hashTable = htNewTable(1103);

    /* For all packages that are installed, if there is no package       */
    /* available by that name, add the package's files to the hash table */
    addLostFiles(db, psp, hashTable);
    /*logDebugMessage(("added lost files"));
    printCount(psp);*/
    
    /* Find packges that are new, and mark them in installThisPackage,  */
    /* updating availPkgs with the count.  Also add files to the hash   */
    /* table that do not exist in the new package - they may have moved */
    if (findUpgradePackages(db, psp, hashTable)) {
	rpmdbClose(db);
	return(-1);
    }
    /*logDebugMessage(("found basic packages to upgrade"));
    printCount(psp);
    hash_stats(hashTable);*/

    /* Remove any files that were added to the hash table that are in */
    /* some other package marked for upgrade.                         */
    removeMovedFilesAlreadyHandled(psp, hashTable);
    /*logDebugMessage(("removed extra files which have moved"));
    printCount(psp);*/

    findPackagesWithRelocatedFiles(psp, hashTable);
    /*logDebugMessage(("found packages with relocated files"));
    printCount(psp);*/

    findPackagesWithObsoletes(db, psp);
    /*logDebugMessage(("found packages that obsolete installed packages"));
    printCount(psp);*/
    
    unmarkPackagesAlreadyInstalled(db, psp);
    /*logDebugMessage(("unmarked packages already installed"));
    printCount(psp);*/
    
    htFreeHashTable(hashTable);
    
    /*printMemStats("Done");*/

    rpmdbClose(db);

    return 0;
}
