#include "system.h"

#include <signal.h>

#include "rpmlib.h"

#include "cpio.h"
#include "install.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"

enum instActions { UNKNOWN, CREATE, BACKUP, KEEP, SAVE, SKIP, ALTNAME };
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

struct callbackInfo {
    unsigned long archiveSize;
    rpmNotifyFunction notify;
    char ** specFilePtr;
    Header h;
    void * notifyData;
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
static int installArchive(FD_t fd, struct fileInfo * files,
			  int fileCount, rpmNotifyFunction notify, 
			  void * notifydb, Header h,
			  char ** specFile, int archiveSize);
static int instHandleSharedFiles(rpmdb db, int ignoreOffset, 
				 struct fileInfo * files,
				 int fileCount, int * notErrors,
				 struct replacedFile ** repListPtr, int flags);
static int installSources(Header h, char * rootdir, FD_t fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  void * notifyData, char * labelFormat);
static int markReplacedFiles(rpmdb db, struct replacedFile * replList);
static int ensureOlder(rpmdb db, Header new, int dbOffset);
static int assembleFileList(Header h, struct fileMemory * mem, 
			     int * fileCountPtr, struct fileInfo ** filesPtr, 
			     int stripPrefixLength);
static void setFileOwners(Header h, struct fileInfo * files, int fileCount);
static void freeFileMemory(struct fileMemory fileMem);
static void trimChangelog(Header h);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallSourcePackage(char * rootdir, FD_t fd, char ** specFile,
			    rpmNotifyFunction notify, void * notifyData,
			    char * labelFormat, char ** cookie) {
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
    
    rc = installSources(h, rootdir, fd, specFile, notify, notifyData,
			labelFormat);
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
			     int stripPrefixLength) {
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

    if (!headerGetEntry(h, RPMTAG_ORIGFILENAMES, NULL, (void **) &mem->names, 
		        fileCountPtr))
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->names, 
		           fileCountPtr);
    if (!headerGetEntry(h, RPMTAG_ORIGFILENAMES, NULL, 
			(void **) &mem->cpioNames, NULL))
	headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &mem->cpioNames, 
		           fileCountPtr);

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
	file->action = UNKNOWN;
	file->install = 1;

	file->relativePath = mem->names[i];
	file->cpioPath = mem->cpioNames[i] + stripPrefixLength;
	file->mode = fileModes[i];
	file->md5 = mem->md5s[i];
	file->link = mem->links[i];
	file->size = fileSizes[i];
	file->flags = fileFlags[i];

	if (fileLangs && languages && *fileLangs[i]) {
	    for (lang = languages; *lang; lang++)
		if (!strcmp(*lang, fileLangs[i])) break;
	    if (!*lang) {
		file->install = 0;
		rpmMessage(RPMMESS_DEBUG, _("not installing %s -- linguas\n"),
			   file->relativePath);
	    }
	}
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
int installBinaryPackage(char * rootdir, rpmdb db, FD_t fd, Header h,
		         rpmRelocation * relocations,
		         int flags, rpmNotifyFunction notify, 
			 void * notifyData) {
    int rc;
    char * name, * version, * release;
    int fileCount, type, count;
    struct fileInfo * files;
    int_32 installTime;
    char * fileStates;
    int i, j;
    int installFile = 0;
    int otherOffset = 0;
    char * ext = NULL, * newpath;
    int rootLength = strlen(rootdir);
    struct replacedFile * replacedList = NULL;
    char * defaultPrefix;
    dbiIndexSet matches;
    char * tmpPath;
    int scriptArg;
    int stripSize = 1;		/* strip at least first / for cpio */
    uint_32 * archiveSizePtr;
    struct fileMemory fileMem;
    int freeFileMem = 0;
    char * currDir = NULL, * tmpptr;
    int currDirLen;

    if (flags & RPMINSTALL_JUSTDB)
	flags |= RPMINSTALL_NOSCRIPTS;

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);


    rpmMessage(RPMMESS_DEBUG, _("package: %s-%s-%s files test = %d\n"), 
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
	scriptArg = dbiIndexSetCount(matches) + 1;
    }

    if (rootdir) {
	currDirLen = 50;
	currDir = malloc(currDirLen);
	while (!getcwd(currDir, currDirLen) && errno == ERANGE) {
	    currDirLen += 50;
	    currDir = realloc(currDir, currDirLen);
	}

	tmpptr = currDir;
	currDir = alloca(strlen(tmpptr) + 1);
	strcpy(currDir, tmpptr);
	free(tmpptr);

	/* this loads all of the name services libraries, in case we
	   don't have access to them in the chroot() */
	(void)getpwnam("root");
	endpwent();

	chdir("/");
	chroot(rootdir);
    }

    if (!(flags & RPMINSTALL_JUSTDB) && headerIsEntry(h, RPMTAG_FILENAMES)) {
	char ** netsharedPaths;
	char ** nsp;

	/* old format relocateable packages need the entire default
	   prefix stripped to form the cpio list, while all other packages 
	   need the leading / stripped */
	if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void *)
				  &defaultPrefix, NULL)) {
	    stripSize = strlen(defaultPrefix) + 1;
	} else {
	    stripSize = 1;
	}

	if (assembleFileList(h, &fileMem, &fileCount, &files, stripSize)) {
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
		rpmMessage(RPMMESS_DEBUG, _("file %s in netshared path\n"), 
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
		    if (rpmfileexists(files[i].relativePath)) {
			if (files[i].flags & RPMFILE_NOREPLACE) {
			    rpmMessage(RPMMESS_DEBUG, 
				_("%s exists - creating with alternate name\n"), 
				files[i].relativePath);
			    files[i].action = ALTNAME;
			} else {
			    rpmMessage(RPMMESS_DEBUG, 
				_("%s exists - backing up\n"), 
				files[i].relativePath);
			    files[i].action = BACKUP;
			}
		    }
		}
	    }
	}

	if (netsharedPaths) freeSplitString(netsharedPaths);

	rc = instHandleSharedFiles(db, otherOffset, files, fileCount, 
				   NULL, &replacedList, flags);

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

	rpmMessage(RPMMESS_DEBUG, _("stopping install as we're running --test\n"));
	if (replacedList) free(replacedList);
	if (freeFileMem) freeFileMemory(fileMem);
	return 0;
    }

    rpmMessage(RPMMESS_DEBUG, _("running preinstall script (if any)\n"));
    if (runInstScript("/", h, RPMTAG_PREIN, RPMTAG_PREINPROG, scriptArg, 
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
		rpmError(RPMMESS_BACKUP, _("warning: %s saved as %s"), 
			files[i].relativePath, newpath);

		if (rename(files[i].relativePath, newpath)) {
		    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
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

	if (notify) {
	    notify(h, RPMNOTIFY_INST_START, 0, 0, notifyData);
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(fd, files, fileCount, notify, notifyData, h,
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

    headerFree(h);

    return 0;
}

static void callback(struct cpioCallbackInfo * cpioInfo, void * data) {
    struct callbackInfo * ourInfo = data;
    char * chptr;

    if (ourInfo->notify)
	ourInfo->notify(ourInfo->h, RPMNOTIFY_INST_PROGRESS,
			cpioInfo->bytesProcessed, 
			ourInfo->archiveSize, ourInfo->notifyData);

    if (ourInfo->specFilePtr) {
	chptr = cpioInfo->file + strlen(cpioInfo->file) - 5;
	if (!strcmp(chptr, ".spec")) 
	    *ourInfo->specFilePtr = strdup(cpioInfo->file);
    }
}

/* NULL files means install all files */
static int installArchive(FD_t fd, struct fileInfo * files,
			  int fileCount, rpmNotifyFunction notify, 
			  void * notifyData, Header h,
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
	notify(h, RPMNOTIFY_INST_PROGRESS, 0, archiveSize, notifyData);

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
	notify(h, RPMNOTIFY_INST_PROGRESS, archiveSize, archiveSize, 
	       notifyData);
    else if (notify) {
	notify(h, RPMNOTIFY_INST_PROGRESS, 100, 100, notifyData);
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
	    rpmMessage(RPMMESS_DEBUG, _("%s skipped due to missingok flag\n"),
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
	    _("\tfile type on disk is different then package - saving\n"));
	return SAVE;
    } else if (newWhat != dbWhat && diskWhat != dbWhat) {
	rpmMessage(RPMMESS_DEBUG, _("\tfile type in database is different then "
		   "disk and package file - saving\n"));
	return SAVE;
    } else if (dbWhat != newWhat) {
	rpmMessage(RPMMESS_DEBUG, _("\tfile type changed - replacing\n"));
	return CREATE;
    } else if (dbWhat != LINK && dbWhat != REG) {
	rpmMessage(RPMMESS_DEBUG, 
		_("\tcan't check file for changes - replacing\n"));
	return CREATE;
    }

    if (dbWhat == REG) {
	if (brokenMd5)
	    rc = mdfileBroken(filespec, buffer);
	else
	    rc = mdfile(filespec, buffer);

	if (rc) {
	    /* assume the file has been removed, don't freak */
	    rpmMessage(RPMMESS_DEBUG, _("\tfile not present - creating"));
	    return CREATE;
	}
	dbAttr = dbMd5;
	newAttr = newMd5;
    } else /* dbWhat == LINK */ {
	memset(buffer, 0, sizeof(buffer));
	i = readlink(filespec, buffer, sizeof(buffer) - 1);
	if (i == -1) {
	    /* assume the file has been removed, don't freak */
	    rpmMessage(RPMMESS_DEBUG, _("\tfile not present - creating"));
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
	rpmMessage(RPMMESS_DEBUG, _("\told == current, replacing "
		"with new version\n"));
	return CREATE;
    }

    if (!strcmp(dbAttr, newAttr)) {
	/* this file is the same in all versions of this package */
	rpmMessage(RPMMESS_DEBUG, _("\told == new, keeping\n"));
	return KEEP;
    }

    /* the config file on the disk has been modified, but
       the ones in the two packages are different. It would
       be nice if RPM was smart enough to at least try and
       merge the difference ala CVS, but... */
    rpmMessage(RPMMESS_DEBUG, _("\tfiles changed too much - backing up\n"));
	    
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
	if (files[sharedList[i].mainFileNumber].action == SKIP) continue;

	if (secOffset != sharedList[i].secRecOffset) {
	    if (secOffset) {
		headerFree(sech);
		free(secFileMd5List);
		free(secFileLinksList);
		free(secFileList);
	    }

	    secOffset = sharedList[i].secRecOffset;
	    sech = rpmdbGetRecord(db, secOffset);
	    if (sech == NULL) {
		rpmError(RPMERR_DBCORRUPT, _("cannot read header at %d for "
		      "uninstall"), secOffset);
		rc = 1;
		break;
	    }

	    headerGetEntry(sech, RPMTAG_NAME, &type, (void **) &name, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_VERSION, &type, (void **) &version, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_RELEASE, &type, (void **) &release, 
		     &secFileCount);

	    rpmMessage(RPMMESS_DEBUG, _("package %s-%s-%s contain shared files\n"), 
		    name, version, release);

	    if (!headerGetEntry(sech, RPMTAG_FILENAMES, &type, 
			  (void **) &secFileList, &secFileCount)) {
		rpmError(RPMERR_DBCORRUPT, _("package %s contains no files"),
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

	rpmMessage(RPMMESS_DEBUG, _("file %s is shared\n"), secFileList[secNum]);

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
	    rpmMessage(RPMMESS_DEBUG, _("\told version already replaced\n"));
	    continue;
	} else if (state == RPMFILE_STATE_NOTINSTALLED) {
	    rpmMessage(RPMMESS_DEBUG, _("\tother version never installed\n"));
	    continue;
	}

	if (filecmp(files[mainNum].mode, files[mainNum].md5, 
		    files[mainNum].link, secFileModesList[secNum],
		    secFileMd5List[secNum], secFileLinksList[secNum])) {
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

	    rpmMessage(RPMMESS_DEBUG, _("%s from %s-%s-%s will be replaced\n"), 
		    files[sharedList[i].mainFileNumber].relativePath,
		    name, version, release);
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
static int installSources(Header h, char * rootdir, FD_t fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  void * notifyData,
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

    rpmMessage(RPMMESS_DEBUG, _("installing a source package\n"));

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
	rpmError(RPMERR_CREATE, _("cannot write to %s"), realSourceDir);
	return 2;
    }

    if (access(realSpecDir, W_OK)) {
	rpmError(RPMERR_CREATE, _("cannot write to %s"), realSpecDir);
	return 2;
    }

    rpmMessage(RPMMESS_DEBUG, _("sources in: %s\n"), realSourceDir);
    rpmMessage(RPMMESS_DEBUG, _("spec file in: %s\n"), realSpecDir);

    if (h && headerIsEntry(h, RPMTAG_FILENAMES)) {
	/* we can't remap v1 packages */
	assembleFileList(h, &fileMem, &fileCount, &files, 0);

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
	    return 2;
	}
    }

    if (labelFormat && h != NULL) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void *) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void *) &release, &count);
	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, 
				(void *) &archiveSizePtr, &count))
	    archiveSizePtr = NULL;
	fprintf(stdout, labelFormat, name, version, release);
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
			  fileCount, notify, notifyData, h,
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
	    rpmError(RPMERR_NOSPEC, _("source package contains no .spec file"));
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
		    _("renaming %s to %s\n"), instSpecFile, correctSpecFile);
	if (rename(instSpecFile, correctSpecFile)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
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
	    if (secHeader != NULL) {
		/* ignore errors here - just do the best we can */

		rpmdbUpdateRecord(db, secOffset, secHeader);
		headerFree(secHeader);
	    }

	    secOffset = fileInfo->recOffset;
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
	
	secStates[fileInfo->fileNumber] = RPMFILE_STATE_REPLACED;
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

