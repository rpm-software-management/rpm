#include "system.h"

#include <signal.h>

#include "rpmlib.h"

#include "cpio.h"
#include "install.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"
#include "rpmmacro.h"

struct callbackInfo {
    unsigned long archiveSize;
    rpmCallbackFunction notify;
    char ** specFilePtr;
    Header h;
    void * notifyData;
    const void * pkgKey;
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
    enum fileActions action;
    int install;
} ;

static int installArchive(FD_t fd, struct fileInfo * files,
			  int fileCount, rpmCallbackFunction notify, 
			  void * notifydb, const void * pkgKey, Header h,
			  char ** specFile, int archiveSize);
static int installSources(Header h, const char * rootdir, FD_t fd, 
			  const char ** specFilePtr, rpmCallbackFunction notify,
			  void * notifyData);
static int assembleFileList(Header h, struct fileMemory * mem, 
			     int * fileCountPtr, struct fileInfo ** filesPtr, 
			     int stripPrefixLength, enum fileActions * actions);
static void setFileOwners(Header h, struct fileInfo * files, int fileCount);
static void freeFileMemory(struct fileMemory fileMem);
static void trimChangelog(Header h);
static int markReplacedFiles(rpmdb db, struct sharedFileInfo * replList);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallSourcePackage(const char * rootdir, FD_t fd, 
			    const char ** specFile, rpmCallbackFunction notify, 
			    void * notifyData, char ** cookie) {
    int rc, isSource;
    Header h;
    int major, minor;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc) return rc;

    if (!isSource) {
	rpmError(RPMERR_NOTSRPM, _("source package expected, binary found"));
	return 2;
    }

    if (major == 1) {
	notify = NULL;
	h = NULL;
    }

    if (cookie) {
	*cookie = NULL;
	if (h && headerGetEntry(h, RPMTAG_COOKIE, NULL, (void *) cookie,
				NULL)) {
	    *cookie = strdup(*cookie);
	}
    }
    
    rc = installSources(h, rootdir, fd, specFile, notify, notifyData);
    if (h != NULL) headerFree(h);
 
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
			     enum fileActions * actions) {
    uint_32 * fileFlags;
    uint_32 * fileSizes;
    uint_16 * fileModes;
    struct fileInfo * files;
    struct fileInfo * file;
    int fileCount;
    int i;
    char ** fileLangs;
    char * chptr;
    char ** languages, ** lang;

    if (!headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->names, 
		        fileCountPtr))
	return 0;

    if (!headerGetEntry(h, RPMTAG_ORIGFILENAMES, NULL, 
			(void **) &mem->cpioNames, NULL))
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->cpioNames, 
		           fileCountPtr);
    headerRemoveEntry(h, RPMTAG_ORIGFILENAMES);

    fileCount = *fileCountPtr;

    files = *filesPtr = mem->files = malloc(sizeof(*mem->files) * fileCount);
    
    headerGetEntry(h, RPMTAG_FILEMD5S, NULL, (void **) &mem->md5s, NULL);
    headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL);
    headerGetEntry(h, RPMTAG_FILEMODES, NULL, (void **) &fileModes, NULL);
    headerGetEntry(h, RPMTAG_FILESIZES, NULL, (void **) &fileSizes, NULL);
    headerGetEntry(h, RPMTAG_FILELINKTOS, NULL, (void **) &mem->links, NULL);
    if (!headerGetEntry(h, RPMTAG_FILELANGS, NULL, (void **) &fileLangs, NULL))
	fileLangs = NULL;

    if ((chptr = getenv("LINGUAS"))) {
	languages = splitString(chptr, strlen(chptr), ':');
    } else
	languages = NULL;

    for (i = 0, file = files; i < fileCount; i++, file++) {
	file->state = RPMFILE_STATE_NORMAL;
	if (actions)
	    file->action = actions[i];
	else
	    file->action = UNKNOWN;
	file->install = 1;

	file->relativePath = mem->names[i];
	file->cpioPath = mem->cpioNames[i] + stripPrefixLength;
	file->mode = fileModes[i];
	file->md5 = mem->md5s[i];
	file->link = mem->links[i];
	file->size = fileSizes[i];
	file->flags = fileFlags[i];

	/* FIXME: move this logic someplace else */
	if (fileLangs && languages && *fileLangs[i]) {
	    for (lang = languages; *lang; lang++)
		if (!strcmp(*lang, fileLangs[i])) break;
	    if (!*lang) {
		file->install = 0;
		rpmMessage(RPMMESS_DEBUG, _("not installing %s -- linguas\n"),
			   file->relativePath);
	    }
	}

	rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
		    file->relativePath, fileActionString(file->action));
    }

    if (fileLangs) free(fileLangs);
    if (languages) freeSplitString(languages);

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
	    rpmError(RPMERR_NOUSER, _("user %s does not exist - using root"), 
			fileOwners[i]);
	    files[i].uid = 0;
	    /* turn off the suid bit */
	    files[i].mode &= ~S_ISUID;
	}

	if (gnameToGid(fileGroups[i], &files[i].gid)) {
	    rpmError(RPMERR_NOGROUP, _("group %s does not exist - using root"), 
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
	rpmError(RPMERR_RPMRC, _("instchangelog value in rpmrc should be a "
		    "number, but isn't"));
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

    free(names);
    free(texts);
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
int installBinaryPackage(const char * rootdir, rpmdb db, FD_t fd, Header h,
		         int flags, rpmCallbackFunction notify, 
			 void * notifyData, const void * pkgKey, 
			 enum fileActions * actions, 
			 struct sharedFileInfo * sharedList) {
    int rc;
    char * name, * version, * release;
    int fileCount, type, count;
    struct fileInfo * files;
    int_32 installTime;
    char * fileStates;
    int i;
    int installFile = 0;
    int otherOffset = 0;
    char * ext = NULL, * newpath;
    char * defaultPrefix;
    dbiIndexSet matches;
    int scriptArg;
    int stripSize = 1;		/* strip at least first / for cpio */
    uint_32 * archiveSizePtr;
    struct fileMemory fileMem;
    int freeFileMem = 0;
    char * currDir = NULL, * tmpptr;

    if (flags & RPMINSTALL_JUSTDB)
	flags |= RPMINSTALL_NOSCRIPTS;

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);


    rpmMessage(RPMMESS_DEBUG, _("package: %s-%s-%s files test = %d\n"), 
		name, version, release, flags & RPMINSTALL_TEST);

    rc = rpmdbFindPackage(db, name, &matches);
    if (rc == -1) return 2;
    if (rc) {
 	scriptArg = 1;
    } else {
	scriptArg = dbiIndexSetCount(matches) + 1;
    }

    if (rootdir) {
	/* this loads all of the name services libraries, in case we
	   don't have access to them in the chroot() */
	(void)getpwnam("root");
	endpwent();

	tmpptr = currentDirectory();
	currDir = alloca(strlen(tmpptr) + 1);
	strcpy(currDir, tmpptr);
	free(tmpptr);

	chdir("/");
	chroot(rootdir);
    }

    if (!(flags & RPMINSTALL_JUSTDB) && headerIsEntry(h, RPMTAG_FILENAMES)) {
	/* old format relocateable packages need the entire default
	   prefix stripped to form the cpio list, while all other packages 
	   need the leading / stripped */
	if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void *)
				  &defaultPrefix, NULL)) {
	    stripSize = strlen(defaultPrefix) + 1;
	} else {
	    stripSize = 1;
	}

	if (assembleFileList(h, &fileMem, &fileCount, &files, stripSize,
			     actions)) {
	    if (rootdir) {
		chroot(".");
		chdir(currDir);
	    }
	    return 2;
	}

	freeFileMem = 1;
    } else {
	files = NULL;
    }
    
    if (flags & RPMINSTALL_TEST) {
	if (rootdir) {
	    chroot(".");
	    chdir(currDir);
	}

	rpmMessage(RPMMESS_DEBUG, _("stopping install as we're running --test\n"));
	if (freeFileMem) freeFileMemory(fileMem);
	return 0;
    }

    rpmMessage(RPMMESS_DEBUG, _("running preinstall script (if any)\n"));
    if (runInstScript("/", h, RPMTAG_PREIN, RPMTAG_PREINPROG, scriptArg, 
		      flags & RPMINSTALL_NOSCRIPTS, 0)) {
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
		rpmError(RPMMESS_ALTNAME, _("warning: %s created as %s"),
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

	      case SKIP:
		installFile = 0;
		ext = NULL;
		break;

	      case SKIPNSTATE:
		installFile = 0;
		ext = NULL;
		files[i].state = RPMFILE_STATE_NOTINSTALLED;
		break;

	      case UNKNOWN:
	      case REMOVE:
		break;
	    }

	    if (ext) {
		newpath = alloca(strlen(files[i].relativePath) + 20);
		strcpy(newpath, files[i].relativePath);
		strcat(newpath, ext);
		rpmError(RPMMESS_BACKUP, _("warning: %s saved as %s"), 
			files[i].relativePath, newpath);

		if (rename(files[i].relativePath, newpath)) {
		    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
			  files[i].relativePath, newpath, strerror(errno));
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

	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, 
				(void *) &archiveSizePtr, &count))
	    archiveSizePtr = NULL;

	if (notify) {
	    notify(h, RPMCALLBACK_INST_START, 0, 0, pkgKey, notifyData);
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(fd, files, fileCount, notify, notifyData, pkgKey, h,
			   NULL, archiveSizePtr ? *archiveSizePtr : 0)) {
	    headerFree(h);
	    if (freeFileMem) freeFileMemory(fileMem);

	    if (rootdir) {
		chroot(".");
		chdir(currDir);
	    }

	    return 2;
	}

	fileStates = malloc(sizeof(*fileStates) * fileCount);
	for (i = 0; i < fileCount; i++)
	    fileStates[i] = files[i].state;

	headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fileStates, 
			fileCount);

	free(fileStates);
	if (freeFileMem) freeFileMemory(fileMem);
    } else if (flags & RPMINSTALL_JUSTDB) {
	char ** fileNames;

	if (headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &fileNames,
		           &fileCount)) {
	    fileStates = malloc(sizeof(*fileStates) * fileCount);
	    memset(fileStates, RPMFILE_STATE_NORMAL, fileCount);
	    headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fileStates, 
			    fileCount);
	    free(fileStates);
	}
    }

    installTime = time(NULL);
    headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1);

    if (rootdir) {
	chroot(".");
	chdir(currDir);
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

    rpmMessage(RPMMESS_DEBUG, _("running postinstall script (if any)\n"));

    if (runInstScript(rootdir, h, RPMTAG_POSTIN, RPMTAG_POSTINPROG, scriptArg,
		      flags & RPMINSTALL_NOSCRIPTS, 0)) {
	return 2;
    }

    if (!(flags & RPMINSTALL_NOTRIGGERS)) {
	/* Run triggers this package sets off */
	if (runTriggers(rootdir, db, RPMSENSE_TRIGGERIN, h, 0)) {
	    return 2;
	}

	/* Run triggers in this package which are set off by other things in
	   the database. */
	if (runImmedTriggers(rootdir, db, RPMSENSE_TRIGGERIN, h, 0)) {
	    return 2;
	}
    }

    if (sharedList) 
	markReplacedFiles(db, sharedList);

    headerFree(h);

    return 0;
}

static void callback(struct cpioCallbackInfo * cpioInfo, void * data) {
    struct callbackInfo * ourInfo = data;
    char * chptr;

    if (ourInfo->notify)
	ourInfo->notify(ourInfo->h, RPMCALLBACK_INST_PROGRESS,
			cpioInfo->bytesProcessed, 
			ourInfo->archiveSize, ourInfo->pkgKey, 
			ourInfo->notifyData);

    if (ourInfo->specFilePtr) {
	chptr = cpioInfo->file + strlen(cpioInfo->file) - 5;
	if (!strcmp(chptr, ".spec")) 
	    *ourInfo->specFilePtr = strdup(cpioInfo->file);
    }
}

/* NULL files means install all files */
static int installArchive(FD_t fd, struct fileInfo * files,
			  int fileCount, rpmCallbackFunction notify, 
			  void * notifyData, const void * pkgKey, Header h,
			  char ** specFile, int archiveSize) {
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
    info.notifyData = notifyData;
    info.specFilePtr = specFile;
    info.h = h;
    info.pkgKey = pkgKey;

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
	notify(h, RPMCALLBACK_INST_PROGRESS, 0, archiveSize, pkgKey, 
	       notifyData);

  { CFD_t cfdbuf, *cfd = &cfdbuf;
    cfd->cpioIoType = cpioIoTypeGzFd;
    cfd->cpioGzFd = gzdFdopen(fdDup(fdFileno(fd)), "r");
    rc = cpioInstallArchive(cfd, map, mappedFiles, 
			    ((notify && archiveSize) || specFile) ? 
				callback : NULL, 
			    &info, &failedFile);
    gzdClose(cfd->cpioGzFd);
  }

    if (rc) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	rpmError(RPMERR_CPIO, _("unpacking of archive failed on file %s: %s"), 
		 failedFile, cpioStrerror(rc));
	return 1;
    }

    if (notify && archiveSize)
	notify(h, RPMCALLBACK_INST_PROGRESS, archiveSize, archiveSize, 
	       pkgKey, notifyData);
    else if (notify) {
	notify(h, RPMCALLBACK_INST_PROGRESS, 100, 100, pkgKey, notifyData);
    }

    return 0;
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
static int installSources(Header h, const char * rootdir, FD_t fd, 
			  const char ** specFilePtr, rpmCallbackFunction notify,
			  void * notifyData) {
    char * specFile;
    int specFileIndex = -1;
    const char * realSourceDir = NULL;
    const char * realSpecDir = NULL;
    char * instSpecFile, * correctSpecFile;
    int fileCount = 0;
    uint_32 * archiveSizePtr = NULL;
    struct fileMemory fileMem;
    struct fileInfo * files;
    int i;
    char * chptr;
    char * currDir;
    int currDirLen;
    uid_t currUid = getuid();
    gid_t currGid = getgid();
    int rc = 0;

    rpmMessage(RPMMESS_DEBUG, _("installing a source package\n"));

    realSourceDir = rpmGetPath(rootdir, "/%{_sourcedir}", NULL);
    realSpecDir = rpmGetPath(rootdir, "/%{_specdir}", NULL);

    if (access(realSourceDir, W_OK)) {
	rpmError(RPMERR_CREATE, _("cannot write to %s"), realSourceDir);
	rc = 2;
	goto exit;
    }

    if (access(realSpecDir, W_OK)) {
	rpmError(RPMERR_CREATE, _("cannot write to %s"), realSpecDir);
	rc = 2;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("sources in: %s\n"), realSourceDir);
    rpmMessage(RPMMESS_DEBUG, _("spec file in: %s\n"), realSpecDir);

    if (h && headerIsEntry(h, RPMTAG_FILENAMES)) {
	/* we can't remap v1 packages */
	assembleFileList(h, &fileMem, &fileCount, &files, 0, NULL);

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
	    rpmError(RPMERR_NOSPEC, _("source package contains no .spec file"));
	    if (fileCount > 0) freeFileMemory(fileMem);
	    rc = 2;
	    goto exit;
	}
    }

    if (notify) {
	notify(h, RPMCALLBACK_INST_START, 0, 0, NULL, notifyData);
    }

    currDirLen = 50;
    currDir = malloc(currDirLen);
    while (!getcwd(currDir, currDirLen)) {
	currDirLen += 50;
	currDir = realloc(currDir, currDirLen);
    }

    chdir(realSourceDir);
    if (installArchive(fd, fileCount > 0 ? files : NULL,
			  fileCount, notify, notifyData, NULL, h,
			  specFileIndex >=0 ? NULL : &specFile, 
			  archiveSizePtr ? *archiveSizePtr : 0)) {
	if (fileCount > 0) freeFileMemory(fileMem);
	rc = 2;
	goto exit;
    }

    chdir(currDir);

    if (specFileIndex == -1) {
	if (!specFile) {
	    rpmError(RPMERR_NOSPEC, _("source package contains no .spec file"));
	    rc = 1;
	    goto exit;
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
		    _("renaming %s to %s\n"), instSpecFile, correctSpecFile);
	if (rename(instSpecFile, correctSpecFile)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
		     instSpecFile, correctSpecFile, strerror(errno));
	    rc = 2;
	    goto exit;
	}

	if (specFilePtr)
	    *specFilePtr = strdup(correctSpecFile);
    } else {
	if (specFilePtr)
	    *specFilePtr = strdup(files[specFileIndex].relativePath);

	if (fileCount > 0) freeFileMemory(fileMem);
    }
    rc = 0;

exit:
    if (realSpecDir)	xfree(realSpecDir);
    if (realSourceDir)	xfree(realSourceDir);
    return rc;
}

static int markReplacedFiles(rpmdb db, struct sharedFileInfo * replList) {
    struct sharedFileInfo * fileInfo;
    Header secHeader = NULL, sh;
    char * secStates;
    int type, count;
    
    int secOffset = 0;

    for (fileInfo = replList; fileInfo->otherPkg; fileInfo++) {
	if (secOffset != fileInfo->otherPkg) {
	    if (secHeader != NULL) {
		/* ignore errors here - just do the best we can */

		rpmdbUpdateRecord(db, secOffset, secHeader);
		headerFree(secHeader);
	    }

	    secOffset = fileInfo->otherPkg;
	    sh = rpmdbGetRecord(db, secOffset);
	    if (sh == NULL) {
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
	
	secStates[fileInfo->otherFileNum] = RPMFILE_STATE_REPLACED;
    }

    if (secHeader != NULL) {
	/* ignore errors here - just do the best we can */

	rpmdbUpdateRecord(db, secOffset, secHeader);
	headerFree(secHeader);
    }

    return 0;
}

int rpmVersionCompare(Header first, Header second) {
    char * one, * two;
    int_32 * epochOne, * epochTwo;
    int rc;

    if (!headerGetEntry(first, RPMTAG_EPOCH, NULL, (void **) &epochOne, NULL))
	epochOne = NULL;
    if (!headerGetEntry(second, RPMTAG_EPOCH, NULL, (void **) &epochTwo, 
			NULL))
	epochTwo = NULL;

    if (epochOne && !epochTwo) 
	return 1;
    else if (!epochOne && epochTwo) 
	return -1;
    else if (epochOne && epochTwo) {
	if (*epochOne < *epochTwo)
	    return -1;
	else if (*epochOne > *epochTwo)
	    return 1;
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

#ifdef	UNUSED
static int ensureOlder(rpmdb db, Header new, int dbOffset);
static int ensureOlder(rpmdb db, Header new, int dbOffset) {
    Header old;
    char * name, * version, * release;
    int result, rc = 0;

    old = rpmdbGetRecord(db, dbOffset);
    if (old == NULL) return 1;

    result = rpmVersionCompare(old, new);
    if (result < 0)
	rc = 0;
    else if (result > 0) {
	rc = 1;
	headerGetEntry(old, RPMTAG_NAME, NULL, (void **) &name, NULL);
	headerGetEntry(old, RPMTAG_VERSION, NULL, (void **) &version, NULL);
	headerGetEntry(old, RPMTAG_RELEASE, NULL, (void **) &release, NULL);
	rpmError(RPMERR_OLDPACKAGE, _("package %s-%s-%s (which is newer) is "
		"already installed"), name, version, release);
    }

    headerFree(old);

    return rc;
}
#endif

const char * fileActionString(enum fileActions a) {
    switch (a) {
      case UNKNOWN: return "unknown";
      case CREATE: return "create";
      case BACKUP: return "backup";
      case SAVE: return "save";
      case SKIP: return "skip";
      case SKIPNSTATE: return "skipnstate";
      case ALTNAME: return "altname";
      case REMOVE: return "remove";
    }

    return "???";
}
