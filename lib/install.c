#include "config.h"
#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>		/* needed for mkdir(2) prototype! */
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <zlib.h>

#include "cpio.h"
#include "header.h"
#include "install.h"
#include "md5.h"
#include "messages.h"
#include "misc.h"
#include "rpmdb.h"
#include "rpmlib.h"

enum instActions { UNKNOWN, CREATE, BACKUP, KEEP, SAVE, SKIP, ALTNAME };
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

struct callbackInfo {
    unsigned long archiveSize;
    rpmNotifyFunction notify;
    char ** specFilePtr;
};

struct fileMemory {
    char ** md5s;
    char ** links;
    char ** names;
    char ** cpioNames;
    struct fileInfo * files;
};

struct fileInfo {
    char * cpioPath;
    char * relativePath;		/* relative to root */
    char * md5;
    char * link;
    uid_t uid;
    gid_t gid;
    uint_32 flags;
    uint_32 size;
    mode_t mode;
    char state;
    enum instActions action;
    int install;
} ;

struct replacedFile {
    int recOffset, fileNumber;
} ;

enum fileTypes whatis(short mode);
static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2);
static enum instActions decideFileFate(char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int instFlags, int brokenMd5);
static int installArchive(int fd, struct fileInfo * files,
			  int fileCount, rpmNotifyFunction notify, 
			  char ** specFile, int archiveSize);
static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int * recOffset, int flags);
static int instHandleSharedFiles(rpmdb db, int ignoreOffset, 
				 struct fileInfo * files,
				 int fileCount, int * notErrors,
				 struct replacedFile ** repListPtr, int flags);
static int installSources(Header h, char * rootdir, int fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  char * labelFormat);
static int markReplacedFiles(rpmdb db, struct replacedFile * replList);
static int archOkay(Header h);
static int osOkay(Header h);
static int ensureOlder(rpmdb db, Header new, int dbOffset);
static int assembleFileList(Header h, struct fileMemory * mem, 
			     int * fileCountPtr, struct fileInfo ** filesPtr, 
			     int stripPrefixLength,
			     struct rpmRelocation * rawRelocations,
			     int allowRandomRelocations);
static void setFileOwners(Header h, struct fileInfo * files, int fileCount);
static void freeFileMemory(struct fileMemory fileMem);
static void trimChangelog(Header h);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallSourcePackage(char * rootdir, int fd, char ** specFile,
			    rpmNotifyFunction notify, char * labelFormat,
			    char ** cookie) {
    int rc, isSource;
    Header h;
    int major, minor;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc) return rc;

    if (!isSource) {
	rpmError(RPMERR_NOTSRPM, "source package expected, binary found");
	return 2;
    }

    if (major == 1) {
	notify = NULL;
	labelFormat = NULL;
	h = NULL;
    }

    if (cookie) {
	*cookie = NULL;
	if (h && headerGetEntry(h, RPMTAG_COOKIE, NULL, (void *) cookie,
				NULL)) {
	    *cookie = strdup(*cookie);
	}
    }
    
    rc = installSources(h, rootdir, fd, specFile, notify, labelFormat);
    if (h) headerFree(h);
 
    return rc;
}

static void freeFileMemory(struct fileMemory fileMem) {
    free(fileMem.files);
    free(fileMem.md5s);
    free(fileMem.links);
    free(fileMem.names);
    free(fileMem.cpioNames);
}

/* files should not be preallocated */
static int assembleFileList(Header h, struct fileMemory * mem, 
			     int * fileCountPtr, struct fileInfo ** filesPtr, 
			     int stripPrefixLength,
			     struct rpmRelocation * rawRelocations,
			     int allowRandomRelocations) {
    uint_32 * fileFlags;
    uint_32 * fileSizes;
    uint_16 * fileModes;
    struct fileInfo * files;
    struct fileInfo * file;
    int fileCount;
    int i, j, numRelocations = 0, madeSwap, len, newLen;
    struct rpmRelocation * relocations = NULL;
    struct rpmRelocation tmpReloc;
    struct rpmRelocation * nextReloc;
    char ** validRelocations = NULL, ** actualRelocations;
    char * newName;
    int rc;
    int numValid;

    if (rawRelocations) {
	if (!headerGetEntry(h, RPMTAG_PREFIXES, NULL,
			    (void **) &validRelocations, &numValid)) {
	    numValid = 0;
	}

	for (i = 0; rawRelocations[i].newPath; i++) ;
	numRelocations = i;
	relocations = alloca(sizeof(*relocations) * numRelocations);

/* XXX this code assumes the validRelocations array won't
   have trailing /'s in it */

	for (i = 0; i < numRelocations; i++) {
	    if (!rawRelocations[i].oldPath) {
		if (!numValid) {
		    rpmError(RPMERR_NORELOCATE, 
			     "package is not relocatable");
		    return 1;
		} else if (numValid != 1){
		    rpmError(RPMERR_NORELOCATE, 
			     "package has multiple relocatable components");
		    return 1;
		}
		relocations[i].oldPath = 
		    alloca(strlen(validRelocations[0]) + 1);
		strcpy(relocations[i].oldPath, validRelocations[0]);
	    } else {
		relocations[i].oldPath = 
		    alloca(strlen(rawRelocations[i].oldPath) + 1);
		strcpy(relocations[i].oldPath, rawRelocations[i].oldPath);
		stripTrailingSlashes(relocations[i].oldPath);
	    }

	    relocations[i].newPath = 
		alloca(strlen(rawRelocations[i].newPath) + 1);
	    strcpy(relocations[i].newPath, rawRelocations[i].newPath);
	    stripTrailingSlashes(relocations[i].newPath);

	    if (!allowRandomRelocations) {
		for (j = 0; j < numValid; j++) 
		    if (!strcmp(validRelocations[j],
				relocations[i].oldPath)) break;
		if (j == numValid) {
		    rpmError(RPMERR_BADRELOCATE, "path %s is not relocatable",
			     relocations[i].oldPath);
		    return 1;
		}
	    }
	}

	/* stupid bubble sort, but it's probably faster here */
	for (i = 0; i < numRelocations; i++) {
	    madeSwap = 0;
	    for (j = 1; j < numRelocations; j++) {
		if (strcmp(relocations[j - 1].oldPath, 
			   relocations[j].oldPath) > 0) {
		    tmpReloc = relocations[j - 1];
		    relocations[j - 1] = relocations[j];
		    relocations[j] = tmpReloc;
		    madeSwap = 1;
		}
	    }
	    if (!madeSwap) break;
	}

	if (validRelocations) free(validRelocations);
    }

    if (headerGetEntry(h, RPMTAG_PREFIXES, NULL,
			(void **) &validRelocations, &numValid)) {
	actualRelocations = malloc(sizeof(*actualRelocations) * numValid);

	/* handle the special case of oldPath == NULL, which can only happen
	   when numValid == 1 (which we've tested for when we build the
	   relocation table above */
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (!strcmp(validRelocations[i], relocations[j].oldPath)) {
		    actualRelocations[i] = relocations[j].newPath;
		    break;
		}
	    }

	    if (j == numRelocations)
		actualRelocations[i] = validRelocations[i];
	}

	headerAddEntry(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numValid);

	free(validRelocations);
	free(actualRelocations);
    }

    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->names, 
		   fileCountPtr);
    /* get this before we start mucking with this info */
    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->cpioNames, 
		   fileCountPtr);
    fileCount = *fileCountPtr;

    if (relocations) {
	/* go through things backwards so that /usr/local relocations take
	   precedence over /usr ones */
	nextReloc = relocations + numRelocations - 1;
	len = strlen(nextReloc->oldPath);
	newLen = strlen(nextReloc->newPath);
	for (i = fileCount - 1; i >= 0 && nextReloc; i--) {
	    do {
		rc = strncmp(nextReloc->oldPath, mem->names[i], len);
		if (rc > 0) {
		    if (nextReloc == relocations) {
			nextReloc = 0;
		    } else {
			nextReloc--;
			len = strlen(nextReloc->oldPath);
			newLen = strlen(nextReloc->newPath);
		    }
		}
	    } while (rc > 0 && nextReloc);

	    if (!rc) {
		newName = alloca(newLen + strlen(mem->names[i]));
		strcpy(newName, nextReloc->newPath);
		strcat(newName, mem->names[i] + len);
		rpmMessage(RPMMESS_DEBUG, "relocating %s to %s\n",
			   mem->names[i], newName);
		mem->names[i] = newName;
	    } 
	}

	headerModifyEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, 
			  mem->names, fileCount);
	/* make mem->names point to the new data in the header rather
	   then the old (now modified) data */
	free(mem->names);
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->names, 
		       fileCountPtr);
    }

    files = *filesPtr = mem->files = malloc(sizeof(*mem->files) * fileCount);
    
    headerGetEntry(h, RPMTAG_FILEMD5S, NULL, (void **) &mem->md5s, NULL);
    headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL);
    headerGetEntry(h, RPMTAG_FILEMODES, NULL, (void **) &fileModes, NULL);
    headerGetEntry(h, RPMTAG_FILESIZES, NULL, (void **) &fileSizes, NULL);
    headerGetEntry(h, RPMTAG_FILELINKTOS, NULL, (void **) &mem->links, NULL);

    for (i = 0, file = files; i < fileCount; i++, file++) {
	file->state = RPMFILE_STATE_NORMAL;
	file->action = UNKNOWN;
	file->install = 1;

	file->relativePath = mem->names[i];
	file->cpioPath = mem->cpioNames[i] + stripPrefixLength;
	file->mode = fileModes[i];
	file->md5 = mem->md5s[i];
	file->link = mem->links[i];
	file->size = fileSizes[i];
	file->flags = fileFlags[i];
    }

    return 0;
}

static void setFileOwners(Header h, struct fileInfo * files, int fileCount) {
    char ** fileOwners;
    char ** fileGroups;
    int i;

    headerGetEntry(h, RPMTAG_FILEUSERNAME, NULL, (void **) &fileOwners, NULL);
    headerGetEntry(h, RPMTAG_FILEGROUPNAME, NULL, (void **) &fileGroups, NULL);

    for (i = 0; i < fileCount; i++) {
	if (unameToUid(fileOwners[i], &files[i].uid)) {
	    rpmError(RPMERR_NOUSER, "user %s does not exist - using root", 
			fileOwners[i]);
	    files[i].uid = 0;
	    /* turn off the suid bit */
	    files[i].mode &= ~S_ISUID;
	}

	if (gnameToGid(fileGroups[i], &files[i].gid)) {
	    rpmError(RPMERR_NOGROUP, "group %s does not exist - using root", 
			fileGroups[i]);
	    files[i].gid = 0;
	    /* turn off the sgid bit */
	    files[i].mode &= ~S_ISGID;
	}
    }

    free(fileOwners);
    free(fileGroups);
}

static void trimChangelog(Header h) {
    int * times;
    char ** names, ** texts;
    long numToKeep;
    char * buf, * end;
    int count;

    buf = rpmGetVar(RPMVAR_INSTCHANGELOG);
    if (!buf) return;

    numToKeep = strtol(buf, &end, 10);
    if (*end) {
	rpmError(RPMERR_RPMRC, "instchangelog value in rpmrc should be a "
		    "number, but isn't");
	return;
    }

    if (numToKeep < 0) return;

    if (!numToKeep) {
	headerRemoveEntry(h, RPMTAG_CHANGELOGTIME);
	headerRemoveEntry(h, RPMTAG_CHANGELOGNAME);
	headerRemoveEntry(h, RPMTAG_CHANGELOGTEXT);
	return;
    }

    if (!headerGetEntry(h, RPMTAG_CHANGELOGTIME, NULL, (void **) &times, 
			&count) ||
	count < numToKeep) return;
    headerGetEntry(h, RPMTAG_CHANGELOGNAME, NULL, (void **) &names, &count);
    headerGetEntry(h, RPMTAG_CHANGELOGTEXT, NULL, (void **) &texts, &count);

    headerModifyEntry(h, RPMTAG_CHANGELOGTIME, RPM_INT32_TYPE, times, 
		      numToKeep);
    headerModifyEntry(h, RPMTAG_CHANGELOGNAME, RPM_STRING_ARRAY_TYPE, names, 
		      numToKeep);
    headerModifyEntry(h, RPMTAG_CHANGELOGTEXT, RPM_STRING_ARRAY_TYPE, texts, 
		      numToKeep);
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallPackage(char * rootdir, rpmdb db, int fd,
		      struct rpmRelocation * relocations,
		      int flags, rpmNotifyFunction notify, char * labelFormat) {
    int rc, isSource, major, minor;
    char * name, * version, * release;
    Header h;
    int fileCount, type, count;
    struct fileInfo * files;
    int_32 installTime;
    char * fileStates;
    int i, j;
    int remFlags;
    int installFile = 0;
    int otherOffset = 0;
    char * ext = NULL, * newpath;
    int rootLength = strlen(rootdir);
    struct replacedFile * replacedList = NULL;
    char * defaultPrefix;
    dbiIndexSet matches;
    int * toRemove = NULL;
    int toRemoveAlloced = 1;
    int * intptr = NULL;
    char * tmpPath;
    int scriptArg;
    int hasOthers = 0;
    int stripSize = 1;		/* strip at least first / for cpio */
    uint_32 * archiveSizePtr;
    struct fileMemory fileMem;
    int freeFileMem = 0;
    char * currDir = NULL, * tmpptr;
    int currDirLen;
    char ** obsoletes;

    if (flags & RPMINSTALL_JUSTDB)
	flags |= RPMINSTALL_NOSCRIPTS;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc) return rc;

    if (isSource) {
	if (flags & RPMINSTALL_TEST) {
	    rpmMessage(RPMMESS_DEBUG, 
			"stopping install as we're running --test\n");
	    return 0;
	}

	if (major == 1) {
	    notify = NULL;
	    labelFormat = NULL;
	    h = NULL;
	}

	if (flags & RPMINSTALL_JUSTDB) return 0;

	rc = installSources(h, rootdir, fd, NULL, notify, labelFormat);
	if (h) headerFree(h);

	return rc;
    }

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);

    /* We don't use these entries (and never have) and they are pretty
       misleading. Let's just get rid of them so they don't confuse
       anyone. */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	headerRemoveEntry(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	headerRemoveEntry(h, RPMTAG_FILEGIDS);
    
    if (!(flags & RPMINSTALL_NOARCH) && !archOkay(h)) {
	rpmError(RPMERR_BADARCH, "package %s-%s-%s is for a different "
	      "architecture", name, version, release);
	headerFree(h);
	return 2;
    }

    if (!(flags & RPMINSTALL_NOOS) && !osOkay(h)) {
	rpmError(RPMERR_BADOS, "package %s-%s-%s is for a different "
	      "operating system", name, version, release);
	headerFree(h);
	return 2;
    }

    if (packageAlreadyInstalled(db, name, version, release, &otherOffset, 
				flags)) {
	headerFree(h);
	return 2;
    }

    rpmMessage(RPMMESS_DEBUG, "package: %s-%s-%s files test = %d\n", 
		name, version, release, flags & RPMINSTALL_TEST);

    /* This canonicalizes the root */
    if (rootdir && rootdir[rootLength] == '/') {
	char * newRootdir;

	newRootdir = alloca(rootLength + 2);
	strcpy(newRootdir, rootdir);
	newRootdir[rootLength++] = '/';
	newRootdir[rootLength] = '\0';
	rootdir = newRootdir;
    }

    rc = rpmdbFindPackage(db, name, &matches);
    if (rc == -1) return 2;
    if (rc) {
 	scriptArg = 1;
    } else {
	hasOthers = 1;
	scriptArg = matches.count + 1;
    }

    if (flags & RPMINSTALL_UPGRADE) {
	/* 
	   We need to get a list of all old version of this package. We let
	   this install procede normally then, but:

		1) we don't report conflicts between the new package and
		   the old versions installed
		2) when we're done, we uninstall the old versions

	   Note if the version being installed is already installed, we don't
	   put that in the list -- that situation is handled normally.

	   We also need to handle packages which were made oboslete.
	*/

	if (hasOthers) {
	    toRemoveAlloced = matches.count + 1;
	    intptr = toRemove = malloc(toRemoveAlloced * sizeof(int));
	    for (i = 0; i < matches.count; i++) {
		if (matches.recs[i].recOffset != otherOffset) {
		    if (!(flags & RPMINSTALL_UPGRADETOOLD)) 
			if (ensureOlder(db, h, matches.recs[i].recOffset)) {
			    headerFree(h);
			    dbiFreeIndexRecord(matches);
			    return 2;
			}
		    *intptr++ = matches.recs[i].recOffset;
		}
	    }

	    dbiFreeIndexRecord(matches);
	}

	if (!(flags & RPMINSTALL_KEEPOBSOLETE) &&
	    headerGetEntry(h, RPMTAG_OBSOLETES, NULL, (void **) &obsoletes, 
				&count)) {
	    for (i = 0; i < count; i++) {
		rc = rpmdbFindPackage(db, obsoletes[i], &matches);
		if (rc == -1) return 2;
		if (rc == 1) continue;		/* no matches */

		rpmMessage(RPMMESS_DEBUG, "package %s is now obsolete and will"
			   " be removed\n", obsoletes[i]);

		toRemoveAlloced += matches.count;
		j = toRemove ? intptr - toRemove : 0; 
		toRemove = realloc(toRemove, toRemoveAlloced * sizeof(int));
		intptr = toRemove + j;

		for (j = 0; j < matches.count; j++)
		    *intptr++ = matches.recs[j].recOffset;

		dbiFreeIndexRecord(matches);
	    }

	    free(obsoletes);
	}

	if (toRemove) {
	    *intptr++ = 0;

	    /* this means we don't have to free the list */
	    intptr = alloca(toRemoveAlloced * sizeof(int));
	    memcpy(intptr, toRemove, toRemoveAlloced * sizeof(int));
	    free(toRemove);
	    toRemove = intptr;
	}
    } else if (hasOthers) {
	dbiFreeIndexRecord(matches);
    }

    if (rootdir) {
	currDirLen = 50;
	currDir = malloc(currDirLen);
	while (!getcwd(currDir, currDirLen)) {
	    currDirLen += 50;
	    currDir = realloc(currDir, currDirLen);
	}

	tmpptr = currDir;
	currDir = alloca(strlen(tmpptr) + 1);
	strcpy(currDir, tmpptr);
	free(tmpptr);

	/* this loads all of the name services libraries, in case we
	   don't have access to them in the chroot() */
	getpwnam("root");
	endpwent();

	chdir("/");
	chroot(rootdir);
    }

    if (!(flags & RPMINSTALL_JUSTDB) && headerIsEntry(h, RPMTAG_FILENAMES)) {
	char ** netsharedPaths;
	char ** nsp;

	/* RPM used to allow only "fully relocateable packages", which
	   were done by making the paths in cpio archive relative to the
	   relocation point (as RPM just exec()d cpio then). If this is
	   such a package, undo this and build a proper relocation entry
	   for it. In any case, we still have to strip the leading / from
	   the filename to get the one in the cpio archive, as they are
	   always stored as relative paths */
	   
	if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void *)
				  &defaultPrefix, NULL)) {
	    /* a trailing '/' in the defaultPrefix would confuse us */
	    defaultPrefix = strcpy(alloca(strlen(defaultPrefix) + 1), 
				   defaultPrefix);
	    stripTrailingSlashes(defaultPrefix);
	    stripSize = strlen(defaultPrefix) + 1;

	    headerAddEntry(h, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
			   &defaultPrefix, 1); 
	} else {
	    stripSize = 1;
	    defaultPrefix = NULL;
	}

	if (assembleFileList(h, &fileMem, &fileCount, &files, stripSize,
			     relocations, flags & RPMINSTALL_FORCERELOCATE)) {
	    if (rootdir) {
		chroot(".");
		chdir(currDir);
	    }
	    if (replacedList) free(replacedList);
	    return 2;
	}

	freeFileMem = 1;

	if ((tmpPath = rpmGetVar(RPMVAR_NETSHAREDPATH)))
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	else
	    netsharedPaths = NULL;

	/* check for any config files that already exist. If they do, plan
	   on making a backup copy. If that's not the right thing to do
	   instHandleSharedFiles() below will take care of the problem */
	for (i = 0; i < fileCount; i++) {
	    /* netsharedPaths are not relative to the current root (though 
	       they do need to take package relocations into account) */
	    for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
		j = strlen(*nsp);
		if (!strncmp(files[i].relativePath, *nsp, j) &&
		    (files[i].relativePath[j] == '\0' ||
		     files[i].relativePath[j] == '/'))
		    break;
	    }

	    if (nsp && *nsp) {
		rpmMessage(RPMMESS_DEBUG, "file %s in netshared path\n", 
				files[i].relativePath);
		files[i].action = SKIP;
		files[i].state = RPMFILE_STATE_NETSHARED;
	    } else if ((files[i].flags & RPMFILE_DOC) && 
			(flags & RPMINSTALL_NODOCS)) {
		files[i].action = SKIP;
		files[i].state = RPMFILE_STATE_NOTINSTALLED;
	    } else {
		files[i].state = RPMFILE_STATE_NORMAL;

		files[i].action = CREATE;
		if ((files[i].flags & RPMFILE_CONFIG) &&
		    !S_ISDIR(files[i].mode)) {
		    if (exists(files[i].relativePath)) {
			if (files[i].flags & RPMFILE_NOREPLACE) {
			    rpmMessage(RPMMESS_DEBUG, 
				"%s exists - creating with alternate name\n", 
				files[i].relativePath);
			    files[i].action = ALTNAME;
			} else {
			    rpmMessage(RPMMESS_DEBUG, 
				"%s exists - backing up\n", 
				files[i].relativePath);
			    files[i].action = BACKUP;
			}
		    }
		}
	    }
	}

	if (netsharedPaths) free(netsharedPaths);

	rc = instHandleSharedFiles(db, otherOffset, files, fileCount, 
				   toRemove, &replacedList, flags);

	if (rc) {
	    if (rootdir) {
		chroot(".");
		chdir(currDir);
	    }
	    if (replacedList) free(replacedList);
	    if (freeFileMem) freeFileMemory(fileMem);
	    return 2;
	}
    } else {
	files = NULL;
    }
    
    if (flags & RPMINSTALL_TEST) {
	if (rootdir) {
	    chroot(".");
	    chdir(currDir);
	}

	rpmMessage(RPMMESS_DEBUG, "stopping install as we're running --test\n");
	if (replacedList) free(replacedList);
	if (freeFileMem) freeFileMemory(fileMem);
	return 0;
    }

    rpmMessage(RPMMESS_DEBUG, "running preinstall script (if any)\n");
    if (runScript("/", h, RPMTAG_PREIN, RPMTAG_PREINPROG, scriptArg, 
		  flags & RPMINSTALL_NOSCRIPTS, 0)) {
	if (replacedList) free(replacedList);
	if (freeFileMem) freeFileMemory(fileMem);

	if (rootdir) {
	    chroot(".");
	    chdir(currDir);
	}

	return 2;
    }

    if (files) {
	setFileOwners(h, files, fileCount);

	for (i = 0; i < fileCount; i++) {
	    switch (files[i].action) {
	      case BACKUP:
		ext = ".rpmorig";
		installFile = 1;
		break;

	      case ALTNAME:
		ext = NULL;
		installFile = 1;

		newpath = alloca(strlen(files[i].relativePath) + 20);
		strcpy(newpath, files[i].relativePath);
		strcat(newpath, ".rpmnew");
		rpmError(RPMMESS_ALTNAME, "warning: %s created as %s",
			files[i].relativePath, newpath);
		files[i].relativePath = newpath;
		
		break;

	      case SAVE:
		ext = ".rpmsave";
		installFile = 1;
		break;

	      case CREATE:
		installFile = 1;
		ext = NULL;
		break;

	      case KEEP:
		installFile = 0;
		ext = NULL;
		break;

	      case SKIP:
		installFile = 0;
		ext = NULL;
		break;

	      case UNKNOWN:
		break;
	    }

	    if (ext) {
		newpath = alloca(strlen(files[i].relativePath) + 20);
		strcpy(newpath, files[i].relativePath);
		strcat(newpath, ext);
		rpmError(RPMMESS_BACKUP, "warning: %s saved as %s", 
			files[i].relativePath, newpath);

		if (rename(files[i].relativePath, newpath)) {
		    rpmError(RPMERR_RENAME, "rename of %s to %s failed: %s",
			  files[i].relativePath, newpath, strerror(errno));
		    if (replacedList) free(replacedList);
		    if (freeFileMem) freeFileMemory(fileMem);

		    if (rootdir) {
			chroot(".");
			chdir(currDir);
		    }

		    return 2;
		}
	    }

	    if (!installFile) {
		files[i].install = 0;
	    }
	}

	if (rootdir) {
	    tmpPath = alloca(strlen(rootdir) + 
			     strlen(rpmGetVar(RPMVAR_TMPPATH)) + 20);
	    strcpy(tmpPath, rootdir);
	    strcat(tmpPath, rpmGetVar(RPMVAR_TMPPATH));
	} else
	    tmpPath = rpmGetVar(RPMVAR_TMPPATH);

	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, 
				(void *) &archiveSizePtr, &count))
	    archiveSizePtr = NULL;

	if (labelFormat) {
	    printf(labelFormat, name, version, release);
	    fflush(stdout);
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(fd, files, fileCount, notify, 
			   NULL, archiveSizePtr ? *archiveSizePtr : 0)) {
	    headerFree(h);
	    if (replacedList) free(replacedList);
	    if (freeFileMem) freeFileMemory(fileMem);

	    if (rootdir) {
		chroot(".");
		chdir(currDir);
	    }

	    return 2;
	}

	if (files) {
	    fileStates = malloc(sizeof(*fileStates) * fileCount);
	    for (i = 0; i < fileCount; i++)
		fileStates[i] = files[i].state;

	    headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fileStates, 
			    fileCount);

	    free(fileStates);
	    if (freeFileMem) freeFileMemory(fileMem);
	}

	installTime = time(NULL);
	headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1);
    }

    if (rootdir) {
	chroot(".");
	chdir(currDir);
    }

    if (replacedList) {
	rc = markReplacedFiles(db, replacedList);
	free(replacedList);

	if (rc) return rc;
    }

    trimChangelog(h);

    /* if this package has already been installed, remove it from the database
       before adding the new one */
    if (otherOffset) {
        rpmdbRemove(db, otherOffset, 1);
    }

    if (rpmdbAdd(db, h)) {
	headerFree(h);
	return 2;
    }

    rpmMessage(RPMMESS_DEBUG, "running postinstall script (if any)\n");

    if (runScript(rootdir, h, RPMTAG_POSTIN, RPMTAG_POSTINPROG, scriptArg,
		  flags & RPMINSTALL_NOSCRIPTS, 0)) {
	return 2;
    }

    if (toRemove && flags & RPMINSTALL_UPGRADE) {
	rpmMessage(RPMMESS_DEBUG, "removing old versions of package\n");
	intptr = toRemove;

	if (flags & RPMINSTALL_NOSCRIPTS)
	    remFlags = RPMUNINSTALL_NOSCRIPTS;
	else
	    remFlags = 0;

	while (*intptr) {
	    rpmRemovePackage(rootdir, db, *intptr, remFlags);
	    intptr++;
	}
    }

    headerFree(h);

    return 0;
}

static void callback(struct cpioCallbackInfo * cpioInfo, void * data) {
    struct callbackInfo * ourInfo = data;
    char * chptr;

    if (ourInfo->notify)
	ourInfo->notify(cpioInfo->bytesProcessed, ourInfo->archiveSize);

    if (ourInfo->specFilePtr) {
	chptr = cpioInfo->file + strlen(cpioInfo->file) - 5;
	if (!strcmp(chptr, ".spec")) 
	    *ourInfo->specFilePtr = strdup(cpioInfo->file);
    }
}

/* NULL files means install all files */
static int installArchive(int fd, struct fileInfo * files,
			  int fileCount, rpmNotifyFunction notify, 
			  char ** specFile, int archiveSize) {
    gzFile stream;
    int rc, i;
    struct cpioFileMapping * map = NULL;
    int mappedFiles = 0;
    char * failedFile;
    struct callbackInfo info;

    if (!files) {
	/* install all files */
	fileCount = 0;
    } else if (!fileCount) {
	/* no files to install */
	return 0;
    }

    info.archiveSize = archiveSize;
    info.notify = notify;
    info.specFilePtr = specFile;

    if (specFile) *specFile = NULL;

    if (files) {
	map = alloca(sizeof(*map) * fileCount);
	for (i = 0, mappedFiles = 0; i < fileCount; i++) {
	    if (!files[i].install) continue;

	    map[mappedFiles].archivePath = files[i].cpioPath;
	    map[mappedFiles].fsPath = files[i].relativePath;
	    map[mappedFiles].finalMode = files[i].mode;
	    map[mappedFiles].finalUid = files[i].uid;
	    map[mappedFiles].finalGid = files[i].gid;
	    map[mappedFiles].mapFlags = CPIO_MAP_PATH | CPIO_MAP_MODE | 
					CPIO_MAP_UID | CPIO_MAP_GID;
	    mappedFiles++;
	}

	qsort(map, mappedFiles, sizeof(*map), cpioFileMapCmp);
    }

    if (notify)
	notify(0, archiveSize);

    stream = gzdopen(fd, "r");
    rc = cpioInstallArchive(stream, map, mappedFiles, 
			    ((notify && archiveSize) || specFile) ? 
				callback : NULL, 
			    &info, &failedFile);

    if (rc) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	rpmError(RPMERR_CPIO, "unpacking of archive failed on file %s: %d: %s", 
		 failedFile, rc, strerror(errno));
	return 1;
    }

    if (notify && archiveSize)
	notify(archiveSize, archiveSize);
    else if (notify) {
	notify(100, 100);
    }

    return 0;
}

static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int * offset, int flags) {
    char * secVersion, * secRelease;
    Header sech;
    int i;
    dbiIndexSet matches;
    int type, count;

    if (!rpmdbFindPackage(db, name, &matches)) {
	for (i = 0; i < matches.count; i++) {
	    sech = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!sech) {
		return 1;
	    }

	    headerGetEntry(sech, RPMTAG_VERSION, &type, (void **) &secVersion, 
			&count);
	    headerGetEntry(sech, RPMTAG_RELEASE, &type, (void **) &secRelease, 
			&count);

	    if (!strcmp(secVersion, version) && !strcmp(secRelease, release)) {
		*offset = matches.recs[i].recOffset;
		if (!(flags & RPMINSTALL_REPLACEPKG)) {
		    rpmError(RPMERR_PKGINSTALLED, 
			  "package %s-%s-%s is already installed",
			  name, version, release);
		    headerFree(sech);
		    return 1;
		}
	    }

	    headerFree(sech);
	}

	dbiFreeIndexRecord(matches);
    }

    return 0;
}

static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2) {
    enum fileTypes what1, what2;

    what1 = whatis(mode1);
    what2 = whatis(mode2);

    if (what1 != what2) return 1;

    if (what1 == LINK)
	return strcmp(link1, link2);
    else if (what1 == REG)
	return strcmp(md51, md52);

    return 0;
}

static enum instActions decideFileFate(char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int newFlags,
				int instFlags, int brokenMd5) {
    char buffer[1024];
    char * dbAttr, * newAttr;
    enum fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int i, rc;

    if (lstat(filespec, &sb)) {
	/* the file doesn't exist on the disk create it unless the new
	   package has marked it as missingok */
	if (!(instFlags & RPMINSTALL_ALLFILES) && 
	      (newFlags & RPMFILE_MISSINGOK)) {
	    rpmMessage(RPMMESS_DEBUG, "%s skipped due to missingok flag\n",
			filespec);
	    return SKIP;
	} else
	    return CREATE;
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(dbMode);
    newWhat = whatis(newMode);

    /* RPM >= 2.3.10 shouldn't create config directories -- we'll ignore
       them in older packages as well */
    if (newWhat == XDIR)
	return CREATE;

    if (diskWhat != newWhat) {
	rpmMessage(RPMMESS_DEBUG, 
	    "\tfile type on disk is different then package - saving\n");
	return SAVE;
    } else if (newWhat != dbWhat && diskWhat != dbWhat) {
	rpmMessage(RPMMESS_DEBUG, "\tfile type in database is different then "
		   "disk and package file - saving\n");
	return SAVE;
    } else if (dbWhat != newWhat) {
	rpmMessage(RPMMESS_DEBUG, "\tfile type changed - replacing\n");
	return CREATE;
    } else if (dbWhat != LINK && dbWhat != REG) {
	rpmMessage(RPMMESS_DEBUG, 
		"\tcan't check file for changes - replacing\n");
	return CREATE;
    }

    if (dbWhat == REG) {
	if (brokenMd5)
	    rc = mdfileBroken(filespec, buffer);
	else
	    rc = mdfile(filespec, buffer);

	if (rc) {
	    /* assume the file has been removed, don't freak */
	    rpmMessage(RPMMESS_DEBUG, "	file not present - creating");
	    return CREATE;
	}
	dbAttr = dbMd5;
	newAttr = newMd5;
    } else /* dbWhat == LINK */ {
	memset(buffer, 0, sizeof(buffer));
	i = readlink(filespec, buffer, sizeof(buffer) - 1);
	if (i == -1) {
	    /* assume the file has been removed, don't freak */
	    rpmMessage(RPMMESS_DEBUG, "	file not present - creating");
	    return CREATE;
	}
	dbAttr = dbLink;
	newAttr = newLink;
     }

    /* this order matters - we'd prefer to CREATE the file if at all 
       possible in case something else (like the timestamp) has changed */

    if (!strcmp(dbAttr, buffer)) {
	/* this config file has never been modified, so 
	   just replace it */
	rpmMessage(RPMMESS_DEBUG, "	old == current, replacing "
		"with new version\n");
	return CREATE;
    }

    if (!strcmp(dbAttr, newAttr)) {
	/* this file is the same in all versions of this package */
	rpmMessage(RPMMESS_DEBUG, "	old == new, keeping\n");
	return KEEP;
    }

    /* the config file on the disk has been modified, but
       the ones in the two packages are different. It would
       be nice if RPM was smart enough to at least try and
       merge the difference ala CVS, but... */
    rpmMessage(RPMMESS_DEBUG, "	files changed too much - backing up\n");
	    
    return SAVE;
}

/* return 0: okay, continue install */
/* return 1: problem, halt install */

static int instHandleSharedFiles(rpmdb db, int ignoreOffset,
				 struct fileInfo * files,
				 int fileCount, int * notErrors,
				 struct replacedFile ** repListPtr, int flags) {
    struct sharedFile * sharedList;
    int secNum, mainNum;
    int sharedCount;
    int i, type;
    int * intptr;
    Header sech = NULL;
    int secOffset = 0;
    int secFileCount;
    char ** secFileMd5List, ** secFileList, ** secFileLinksList;
    char * secFileStatesList;
    int_16 * secFileModesList;
    uint_32 * secFileFlagsList;
    char * name, * version, * release;
    int rc = 0;
    struct replacedFile * replacedList;
    int numReplacedFiles, numReplacedAlloced;
    char state;
    char ** fileList;

    fileList = malloc(sizeof(*fileList) * fileCount);
    for (i = 0; i < fileCount; i++)
	fileList[i] = files[i].relativePath;

    if (findSharedFiles(db, 0, fileList, fileCount, &sharedList, 
			&sharedCount)) {
	free(fileList);
	return 1;
    }
    free(fileList);
    
    numReplacedAlloced = 10;
    numReplacedFiles = 0;
    replacedList = malloc(sizeof(*replacedList) * numReplacedAlloced);

    for (i = 0; i < sharedCount; i++) {
	/* if a file isn't going to be installed it doesn't matter who
	   it's shared with */
	if (!files[sharedList[i].mainFileNumber].action == SKIP) continue;

	if (secOffset != sharedList[i].secRecOffset) {
	    if (secOffset) {
		headerFree(sech);
		free(secFileMd5List);
		free(secFileLinksList);
		free(secFileList);
	    }

	    secOffset = sharedList[i].secRecOffset;
	    sech = rpmdbGetRecord(db, secOffset);
	    if (!sech) {
		rpmError(RPMERR_DBCORRUPT, "cannot read header at %d for "
		      "uninstall", secOffset);
		rc = 1;
		break;
	    }

	    headerGetEntry(sech, RPMTAG_NAME, &type, (void **) &name, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_VERSION, &type, (void **) &version, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_RELEASE, &type, (void **) &release, 
		     &secFileCount);

	    rpmMessage(RPMMESS_DEBUG, "package %s-%s-%s contain shared files\n", 
		    name, version, release);

	    if (!headerGetEntry(sech, RPMTAG_FILENAMES, &type, 
			  (void **) &secFileList, &secFileCount)) {
		rpmError(RPMERR_DBCORRUPT, "package %s contains no files",
		      name);
		headerFree(sech);
		rc = 1;
		break;
	    }

	    headerGetEntry(sech, RPMTAG_FILESTATES, &type, 
		     (void **) &secFileStatesList, &secFileCount);
	    headerGetEntry(sech, RPMTAG_FILEMD5S, &type, 
		     (void **) &secFileMd5List, &secFileCount);
	    headerGetEntry(sech, RPMTAG_FILEFLAGS, &type, 
		     (void **) &secFileFlagsList, &secFileCount);
	    headerGetEntry(sech, RPMTAG_FILELINKTOS, &type, 
		     (void **) &secFileLinksList, &secFileCount);
	    headerGetEntry(sech, RPMTAG_FILEMODES, &type, 
		     (void **) &secFileModesList, &secFileCount);
	}

 	secNum = sharedList[i].secFileNumber;
	mainNum = sharedList[i].mainFileNumber;

	rpmMessage(RPMMESS_DEBUG, "file %s is shared\n", secFileList[secNum]);

	if (notErrors) {
	    intptr = notErrors;
	    while (*intptr) {
		if (*intptr == sharedList[i].secRecOffset) break;
		intptr++;
	    }
	    if (!*intptr) intptr = NULL;
	} else 
	    intptr = NULL;

	/* if this instance of the shared file is already recorded as
	   replaced, just forget about it */
	state = secFileStatesList[sharedList[i].secFileNumber];
	if (state == RPMFILE_STATE_REPLACED) {
	    rpmMessage(RPMMESS_DEBUG, "	old version already replaced\n");
	    continue;
	} else if (state == RPMFILE_STATE_NOTINSTALLED) {
	    rpmMessage(RPMMESS_DEBUG, "	other version never installed\n");
	    continue;
	}

	if (filecmp(files[mainNum].mode, files[mainNum].md5, 
		    files[mainNum].link, secFileModesList[secNum],
		    secFileMd5List[secNum], secFileLinksList[secNum])) {
	    if (!(flags & RPMINSTALL_REPLACEFILES) && !intptr) {
		rpmError(RPMERR_PKGINSTALLED, "%s conflicts with file from "
		         "%s-%s-%s", 
			 files[sharedList[i].mainFileNumber].relativePath,
		         name, version, release);
		rc = 1;
	    } else {
		if (numReplacedFiles == numReplacedAlloced) {
		    numReplacedAlloced += 10;
		    replacedList = realloc(replacedList, 
					   sizeof(*replacedList) * 
					       numReplacedAlloced);
		}
	       
		replacedList[numReplacedFiles].recOffset = 
		    sharedList[i].secRecOffset;
		replacedList[numReplacedFiles].fileNumber = 	
		    sharedList[i].secFileNumber;
		numReplacedFiles++;

		rpmMessage(RPMMESS_DEBUG, "%s from %s-%s-%s will be replaced\n", 
			files[sharedList[i].mainFileNumber].relativePath,
			name, version, release);
	    }
	}

	/* if this is a config file, we need to be carefull here */
	if (files[sharedList[i].mainFileNumber].flags & RPMFILE_CONFIG ||
	    secFileFlagsList[sharedList[i].secFileNumber] & RPMFILE_CONFIG) {
	    files[sharedList[i].mainFileNumber].action = 
		decideFileFate(files[mainNum].relativePath, 
			       secFileModesList[secNum],
			       secFileMd5List[secNum], secFileLinksList[secNum],
			       files[mainNum].mode, files[mainNum].md5,
			       files[mainNum].link, 
			       files[sharedList[i].mainFileNumber].flags,
			       flags,
			       !headerIsEntry(sech, RPMTAG_RPMVERSION));
	}
    }

    if (secOffset) {
	headerFree(sech);
	free(secFileMd5List);
	free(secFileLinksList);
	free(secFileList);
    }

    if (sharedList) free(sharedList);
   
    if (!numReplacedFiles) 
	free(replacedList);
    else {
	replacedList[numReplacedFiles].recOffset = 0;  /* mark the end */
	*repListPtr = replacedList;
    }

    return rc;
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
static int installSources(Header h, char * rootdir, int fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  char * labelFormat) {
    char * specFile;
    char * sourceDir, * specDir;
    int specFileIndex = -1;
    char * realSourceDir, * realSpecDir;
    char * instSpecFile, * correctSpecFile;
    char * name, * release, * version;
    int fileCount = 0;
    uint_32 * archiveSizePtr = NULL;
    int type, count;
    struct fileMemory fileMem;
    struct fileInfo * files;
    int i;
    char * chptr;
    char * currDir;
    int currDirLen;
    uid_t currUid = getuid();
    gid_t currGid = getgid();

    rpmMessage(RPMMESS_DEBUG, "installing a source package\n");

    sourceDir = rpmGetVar(RPMVAR_SOURCEDIR);
    specDir = rpmGetVar(RPMVAR_SPECDIR);

    realSourceDir = alloca(strlen(rootdir) + strlen(sourceDir) + 2);
    strcpy(realSourceDir, rootdir);
    strcat(realSourceDir, "/");
    strcat(realSourceDir, sourceDir);

    realSpecDir = alloca(strlen(rootdir) + strlen(specDir) + 2);
    strcpy(realSpecDir, rootdir);
    strcat(realSpecDir, "/");
    strcat(realSpecDir, specDir);

    if (access(realSourceDir, W_OK)) {
	rpmError(RPMERR_CREATE, "cannot write to %s", realSourceDir);
	return 2;
    }

    if (access(realSpecDir, W_OK)) {
	rpmError(RPMERR_CREATE, "cannot write to %s", realSpecDir);
	return 2;
    }

    rpmMessage(RPMMESS_DEBUG, "sources in: %s\n", realSourceDir);
    rpmMessage(RPMMESS_DEBUG, "spec file in: %s\n", realSpecDir);

    if (h && headerIsEntry(h, RPMTAG_FILENAMES)) {
	/* we can't remap v1 packages */
	assembleFileList(h, &fileMem, &fileCount, &files, 0, NULL, 0);

	for (i = 0; i < fileCount; i++) {
	    files[i].relativePath = files[i].relativePath;
	    files[i].uid = currUid;
	    files[i].gid = currGid;
	}

	if (headerIsEntry(h, RPMTAG_COOKIE))
	    for (i = 0; i < fileCount; i++)
		if (files[i].flags & RPMFILE_SPECFILE) break;

	if (i == fileCount) {
	    /* find the spec file by name */
	    for (i = 0; i < fileCount; i++) {
		chptr = files[i].cpioPath + strlen(files[i].cpioPath) - 5;
		if (!strcmp(chptr, ".spec")) break;
	    }
	}

	if (i < fileCount) {
	    specFileIndex = i;

	    files[i].relativePath = alloca(strlen(realSpecDir) + 
					 strlen(files[i].cpioPath) + 5);
	    strcpy(files[i].relativePath, realSpecDir);
	    strcat(files[i].relativePath, "/");
	    strcat(files[i].relativePath, files[i].cpioPath);
	} else {
	    rpmError(RPMERR_NOSPEC, "source package contains no .spec file");
	    if (fileCount > 0) freeFileMemory(fileMem);
	    return 2;
	}
    }

    if (labelFormat && h) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void *) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void *) &release, &count);
	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, 
				(void *) &archiveSizePtr, &count))
	    archiveSizePtr = NULL;
	printf(labelFormat, name, version, release);
	fflush(stdout);
    }

    currDirLen = 50;
    currDir = malloc(currDirLen);
    while (!getcwd(currDir, currDirLen)) {
	currDirLen += 50;
	currDir = realloc(currDir, currDirLen);
    }

    chdir(realSourceDir);
    if (installArchive(fd, fileCount > 0 ? files : NULL,
			  fileCount, notify, 
			  specFileIndex >=0 ? NULL : &specFile, 
			  archiveSizePtr ? *archiveSizePtr : 0)) {
	if (fileCount > 0) freeFileMemory(fileMem);
	free(currDir);
	return 2;
    }

    chdir(currDir);
    free(currDir);

    if (specFileIndex == -1) {
	if (!specFile) {
	    rpmError(RPMERR_NOSPEC, "source package contains no .spec file");
	    return 1;
	}

	/* This logic doesn't work is realSpecDir and realSourceDir are on
	   different filesystems, but we only do this on v1 source packages
	   so I don't really care much. */
	instSpecFile = alloca(strlen(realSourceDir) + strlen(specFile) + 2);
	strcpy(instSpecFile, realSourceDir);
	strcat(instSpecFile, "/");
	strcat(instSpecFile, specFile);

	correctSpecFile = alloca(strlen(realSpecDir) + strlen(specFile) + 2);
	strcpy(correctSpecFile, realSpecDir);
	strcat(correctSpecFile, "/");
	strcat(correctSpecFile, specFile);

	free(specFile);

	rpmMessage(RPMMESS_DEBUG, 
		    "renaming %s to %s\n", instSpecFile, correctSpecFile);
	if (rename(instSpecFile, correctSpecFile)) {
	    rpmError(RPMERR_RENAME, "rename of %s to %s failed: %s",
		     instSpecFile, correctSpecFile, strerror(errno));
	    return 2;
	}

	if (specFilePtr)
	    *specFilePtr = strdup(correctSpecFile);
    } else {
	if (specFilePtr)
	    *specFilePtr = strdup(files[specFileIndex].relativePath);

	if (fileCount > 0) freeFileMemory(fileMem);
    }

    return 0;
}

static int markReplacedFiles(rpmdb db, struct replacedFile * replList) {
    struct replacedFile * fileInfo;
    Header secHeader = NULL, sh;
    char * secStates;
    int type, count;
    
    int secOffset = 0;

    for (fileInfo = replList; fileInfo->recOffset; fileInfo++) {
	if (secOffset != fileInfo->recOffset) {
	    if (secHeader) {
		/* ignore errors here - just do the best we can */

		rpmdbUpdateRecord(db, secOffset, secHeader);
		headerFree(secHeader);
	    }

	    secOffset = fileInfo->recOffset;
	    sh = rpmdbGetRecord(db, secOffset);
	    if (!sh) {
		secOffset = 0;
	    } else {
		secHeader = headerCopy(sh);	/* so we can modify it */
		headerFree(sh);
	    }

	    headerGetEntry(secHeader, RPMTAG_FILESTATES, &type, 
			   (void **) &secStates, &count);
	}

	/* by now, secHeader is the right header to modify, secStates is
	   the right states list to modify  */
	
	secStates[fileInfo->fileNumber] = RPMFILE_STATE_REPLACED;
    }

    if (secHeader) {
	/* ignore errors here - just do the best we can */

	rpmdbUpdateRecord(db, secOffset, secHeader);
	headerFree(secHeader);
    }

    return 0;
}

int rpmVersionCompare(Header first, Header second) {
    char * one, * two;
    int_32 * serialOne, * serialTwo;
    int rc;

    if (!headerGetEntry(first, RPMTAG_SERIAL, NULL, (void **) &serialOne, NULL))
	serialOne = NULL;
    if (!headerGetEntry(second, RPMTAG_SERIAL, NULL, (void **) &serialTwo, 
			NULL))
	serialTwo = NULL;

    if (serialOne && !serialTwo) 
	return 1;
    else if (!serialOne && serialTwo) 
	return -1;
    else if (serialOne && serialTwo) {
	if (*serialOne < *serialTwo)
	    return -1;
	else if (*serialOne > *serialTwo)
	    return 1;
	return 0;
    }
	
    headerGetEntry(first, RPMTAG_VERSION, NULL, (void **) &one, NULL);
    headerGetEntry(second, RPMTAG_VERSION, NULL, (void **) &two, NULL);

    rc = rpmvercmp(one, two);
    if (rc)
	return rc;

    headerGetEntry(first, RPMTAG_RELEASE, NULL, (void **) &one, NULL);
    headerGetEntry(second, RPMTAG_RELEASE, NULL, (void **) &two, NULL);
    
    return rpmvercmp(one, two);
}

static int ensureOlder(rpmdb db, Header new, int dbOffset) {
    Header old;
    char * name, * version, * release;
    int result, rc = 0;

    old = rpmdbGetRecord(db, dbOffset);
    if (!old) return 1;

    result = rpmVersionCompare(old, new);
    if (result < 0)
	rc = 0;
    else if (result > 0) {
	rc = 1;
	headerGetEntry(old, RPMTAG_NAME, NULL, (void **) &name, NULL);
	headerGetEntry(old, RPMTAG_VERSION, NULL, (void **) &version, NULL);
	headerGetEntry(old, RPMTAG_RELEASE, NULL, (void **) &release, NULL);
	rpmError(RPMERR_OLDPACKAGE, "package %s-%s-%s (which is newer) is "
		"already installed", name, version, release);
    }

    headerFree(old);

    return rc;
}

enum fileTypes whatis(short mode) {
    enum fileTypes result;

    if (S_ISDIR(mode))
	result = XDIR;
    else if (S_ISCHR(mode))
	result = CDEV;
    else if (S_ISBLK(mode))
	result = BDEV;
    else if (S_ISLNK(mode))
	result = LINK;
    else if (S_ISSOCK(mode))
	result = SOCK;
    else if (S_ISFIFO(mode))
	result = PIPE;
    else
	result = REG;
 
    return result;
}

static int archOkay(Header h) {
    int_8 * pkgArchNum;
    void * pkgArch;
    int type, count, archNum;

    /* make sure we're trying to install this on the proper architecture */
    headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);
    if (type == RPM_INT8_TYPE) {
	/* old arch handling */
	rpmGetArchInfo(NULL, &archNum);
	pkgArchNum = pkgArch;
	if (archNum != *pkgArchNum) {
	    return 0;
	}
    } else {
	/* new arch handling */
	if (!rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch)) {
	    return 0;
	}
    }

    return 1;
}

static int osOkay(Header h) {
    void * pkgOs;
    int type, count;

    /* make sure we're trying to install this on the proper os */
    headerGetEntry(h, RPMTAG_OS, &type, (void **) &pkgOs, &count);
    if (type == RPM_INT8_TYPE) {
	/* v1 packages and v2 packages both used improper OS numbers, so just
	   deal with it hope things work */
	return 1;
    } else {
	/* new os handling */
	if (!rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs)) {
	    return 0;
	}
    }

    return 1;
}
