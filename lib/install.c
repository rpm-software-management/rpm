#include "config.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>		/* needed for mkdir(2) prototype! */
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <zlib.h>

#include "header.h"
#include "install.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"
#include "rpmlib.h"
#include "messages.h"

enum instActions { CREATE, BACKUP, KEEP, SAVE, SKIP };
enum fileTypes { XDIR, BDEV, CDEV, SOCK, PIPE, REG, LINK } ;

struct fileToInstall {
    char * fileName;
    int size;
} ;

struct replacedFile {
    int recOffset, fileNumber;
} ;

enum fileTypes whatis(short mode);
static int filecmp(short mode1, char * md51, char * link1, 
	           short mode2, char * md52, char * link2);
static enum instActions decideFileFate(char * filespec, short dbMode, 
				char * dbMd5, char * dbLink, short newMode, 
				char * newMd5, char * newLink, int brokenMd5);
static int installArchive(char * prefix, int fd, struct fileToInstall * files,
			  int fileCount, rpmNotifyFunction notify,
			  char ** installArchive, char * tmpPath,
			  int archiveSize);
static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int * recOffset, int flags);
static int setFileOwnerships(char * rootdir, char ** fileList, 
			     char ** fileOwners, char ** fileGroups, 
			     int_16 * fileModes, 
			     enum instActions * instActions, int fileCount);
static int setFileOwner(char * file, char * owner, char * group, int_16 mode);
static int createDirectories(char ** fileList, uint_32 * modesList, 
			     uint_32 * mtimesList, int fileCount);
static int mkdirIfNone(char * directory, mode_t perms, uint_32 stamp);
static int instHandleSharedFiles(rpmdb db, int ignoreOffset, char ** fileList, 
			         char ** fileMd5List, int_16 * fileModeList,
				 char ** fileLinkList, uint_32 * fileFlagsList,
				 int fileCount, enum instActions * instActions, 
			 	 char ** prootdirootdir, int * notErrors,
				 struct replacedFile ** repListPtr, int flags);
static int fileCompare(const void * one, const void * two);
static int installSources(Header h, char * rootdir, int fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  char * labelFormat);
static int markReplacedFiles(rpmdb db, struct replacedFile * replList);
static int relocateFilelist(Header * hp, char * defaultPrefix, 
			char * newPrefix, int * relocationSize);
static int archOkay(Header h);
static int osOkay(Header h);
static int moveFile(char * sourceName, char * destName);
static int copyFile(char * sourceName, char * destName);
static void unglobFilename(char * dptr, char * sptr);
static int ensureOlder(rpmdb db, Header new, int dbOffset);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallSourcePackage(char * rootdir, int fd, char ** specFile,
			    rpmNotifyFunction notify, char * labelFormat) {
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

    rc = installSources(h, rootdir, fd, specFile, notify, labelFormat);
    if (h) headerFree(h);
 
    return rc;
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallPackage(char * rootdir, rpmdb db, int fd, char * location, 
		      int flags, rpmNotifyFunction notify, char * labelFormat,
		      char * netsharedPath) {
    int rc, isSource, major, minor;
    char * name, * version, * release;
    Header h;
    int fileCount, type, count;
    char ** fileList;
    char ** fileOwners, ** fileGroups, ** fileMd5s, ** fileLinkList;
    uint_32 * fileFlagsList;
    uint_32 * fileSizesList, * fileMtimesList;
    int_16 * fileModesList;
    int_32 installTime;
    char * fileStatesList = NULL;
    struct fileToInstall * files;
    enum instActions * instActions = NULL;
    int i;
    int archiveFileCount = 0;
    int installFile = 0;
    int otherOffset = 0;
    char * ext = NULL, * newpath;
    int rootLength = strlen(rootdir);
    char ** rootedFileList = NULL;
    char ** finalFileList = NULL;
    char ** mkdirFileList = NULL;
    int mkdirFileCount = 0;
    uint_32 * mkdirModesList, * mkdirMtimesList;
    struct replacedFile * replacedList = NULL;
    char * defaultPrefix;
    dbiIndexSet matches;
    int * oldVersions;
    int * intptr;
    char * archivePrefix, * tmpPath;
    int scriptArg;
    int hasOthers = 0;
    int relocationSize = 1;		/* strip at least first / for cpio */
    uint_32 * archiveSizePtr;

    oldVersions = alloca(sizeof(int));
    *oldVersions = 0;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc) return rc;

    if (isSource) {
	if (flags & RPMINSTALL_TEST) {
	    rpmMessage(RPMMESS_DEBUG, "stopping install as we're running --test\n");
	    return 0;
	}

	if (major == 1) {
	    notify = NULL;
	    labelFormat = NULL;
	    h = NULL;
	}

	rc = installSources(h, rootdir, fd, NULL, notify, labelFormat);
	if (h) headerFree(h);

	return rc;
    }

    umask(0);		/* we know what we're doing */

    /* Do this now so we can give error messages, even though we'll just
       do it again after relocating everything */
    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);

    if (!headerGetEntry(h, RPMTAG_DEFAULTPREFIX, &type, (void *)
			      &defaultPrefix, &fileCount)) {
	defaultPrefix = NULL;
    }

    if (location && !defaultPrefix) {
	rpmError(RPMERR_NORELOCATE, "package %s-%s-%s is not relocatable",
		      name, version, release);
	headerFree(h);
	return 2;
    } else if (!location && defaultPrefix)
	location = defaultPrefix;
    
    if (location) {
	relocateFilelist(&h, defaultPrefix, location, &relocationSize);
        headerGetEntry(h, RPMTAG_DEFAULTPREFIX, &type, (void *) &defaultPrefix, 
		&fileCount);
	archivePrefix = alloca(strlen(rootdir) + strlen(location) + 2);
	sprintf(archivePrefix, "%s/%s", rootdir, location);
    } else {
	archivePrefix = rootdir;
	relocationSize = 1;
    }

    /* You're probably upset because name already points to the name of
       this package, right? Almost... it points to the name in the original
       header, which could have been trashed by relocateFileList() */
    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);

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

    if (labelFormat) {
	printf(labelFormat, name, version, release);
	fflush(stdout);
    }

    rpmMessage(RPMMESS_DEBUG, "package: %s-%s-%s files test = %d\n", 
		name, version, release, flags & RPMINSTALL_TEST);

    rc = rpmdbFindPackage(db, name, &matches);
    if (rc == -1) return 2;
    if (rc) {
 	scriptArg = 1;
    } else {
	hasOthers = 1;
	scriptArg = matches.count + 1;
    }

    /* This canonicalizes the root */
    if (rootdir && rootdir[rootLength] == '/') {
	char * newRootdir;

	newRootdir = alloca(rootLength + 2);
	strcpy(newRootdir, rootdir);
	newRootdir[rootLength++] = '/';
	newRootdir[rootLength] = '\0';
	rootdir = newRootdir;
    }

    if (flags & RPMINSTALL_UPGRADE && hasOthers) {
	/* 
	   We need to get a list of all old version of this package. We let
	   this install procede normally then, but:

		1) we don't report conflicts between the new package and
		   the old versions installed
		2) when we're done, we uninstall the old versions

	   Note if the version being installed is already installed, we don't
	   put that in the list -- that situation is handled normally.
	*/

	if (!rc) {
	    intptr = oldVersions = alloca((matches.count + 1) * sizeof(int));
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
	    *intptr++ = 0;
	}
    }

    if (hasOthers) 
	dbiFreeIndexRecord(matches);

    fileList = NULL;
    if (headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &fileCount)) {
	char ** netsharedPaths;
	char ** nsp;

	if (netsharedPath) 
	    netsharedPaths = splitString(netsharedPath, 
					 strlen(netsharedPath), ':');
	else
	    netsharedPaths = NULL;

	instActions = alloca(sizeof(enum instActions) * fileCount);
	rootedFileList = alloca(sizeof(char *) * fileCount);
	fileStatesList = alloca(sizeof(*fileStatesList) * fileCount);

	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5s, &fileCount);
	headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILELINKTOS, &type, (void **) &fileLinkList, 
		 &fileCount);

	/* check for any config files that already exist. If they do, plan
	   on making a backup copy. If that's not the right thing to do
	   instHandleSharedFiles() below will take care of the problem */
	for (i = 0; i < fileCount; i++) {
	    if (rootLength > 1) {
		rootedFileList[i] = alloca(strlen(fileList[i]) + 
				rootLength + 3);
		strcpy(rootedFileList[i], rootdir);
		strcat(rootedFileList[i], fileList[i]);
	    } else 
		rootedFileList[i] = fileList[i];

	    /* netsharedPaths are not relative to the current root (though 
	       they do need to take the package prefix into account */
	    for (nsp = netsharedPaths; nsp && *nsp; nsp++) 
		if (!strncmp(fileList[i], *nsp, strlen(*nsp))) break;

	    if (nsp && *nsp) {
		rpmMessage(RPMMESS_DEBUG, "file %s in netshared path\n", 
			rootedFileList[i]);
		instActions[i] = SKIP;
		fileStatesList[i] = RPMFILE_STATE_NETSHARED;
	    } else if ((fileFlagsList[i] & RPMFILE_DOC) && 
			(flags & RPMINSTALL_NODOCS)) {
		instActions[i] = SKIP;
		fileStatesList[i] = RPMFILE_STATE_NOTINSTALLED;
	    } else {
		fileStatesList[i] = RPMFILE_STATE_NORMAL;

		instActions[i] = CREATE;
		if ((fileFlagsList[i] & RPMFILE_CONFIG) &&
		    !S_ISDIR(fileModesList[i])) {
		    if (exists(rootedFileList[i])) {
			rpmMessage(RPMMESS_DEBUG, "%s exists - backing up\n", 
				    rootedFileList[i]);
			instActions[i] = BACKUP;
		    }
		}
	    }
	}

	rc = instHandleSharedFiles(db, 0, fileList, fileMd5s, fileModesList,
				   fileLinkList, fileFlagsList, fileCount, 
				   instActions, rootedFileList, oldVersions, 
				   &replacedList, flags);

	free(fileMd5s);
	free(fileLinkList);
	if (rc) {
	    if (replacedList) free(replacedList);
	    free(fileList);
	    return 2;
	}
    }
    
    if (flags & RPMINSTALL_TEST) {
	rpmMessage(RPMMESS_DEBUG, "stopping install as we're running --test\n");
	free(fileList);
	if (replacedList) free(replacedList);
	return 0;
    }

    rpmMessage(RPMMESS_DEBUG, "running preinstall script (if any)\n");
    if (runScript(rootdir, h, RPMTAG_PREIN, scriptArg, 
		  flags & RPMINSTALL_NOSCRIPTS)) {
	free(fileList);
	if (replacedList) free(replacedList);
	return 2;
    }

    if (fileList) {
	headerGetEntry(h, RPMTAG_FILESIZES, &type, (void **) &fileSizesList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMTIMES, &type, (void **) &fileMtimesList, 
		 &fileCount);

	files = alloca(sizeof(struct fileToInstall) * fileCount);
	finalFileList = alloca(sizeof(char *) * fileCount);
	mkdirFileList = alloca(sizeof(char *) * fileCount);
	mkdirModesList = alloca(sizeof(*mkdirModesList) * fileCount);
	mkdirMtimesList = alloca(sizeof(*mkdirMtimesList) * fileCount);
	for (i = 0; i < fileCount; i++) {
	    switch (instActions[i]) {
	      case BACKUP:
		ext = ".rpmorig";
		installFile = 1;
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
	    }

	    if (ext) {
		newpath = alloca(strlen(rootedFileList[i]) + 20);
		strcpy(newpath, rootedFileList[i]);
		strcat(newpath, ext);
		rpmError(RPMMESS_BACKUP, "warning: %s saved as %s", 
			rootedFileList[i], newpath);

		if (rename(rootedFileList[i], newpath)) {
		    rpmError(RPMERR_RENAME, "rename of %s to %s failed: %s",
			  rootedFileList[i], newpath, strerror(errno));
		    if (replacedList) free(replacedList);
		    return 2;
		}
	    }

	    if (installFile) {
		/* if we are using a relocateable package, we need to strip
		   off whatever part of the (already relocated!) filelist

		   we also need to strip globbing characters, as cpio
		   globs the patterns we pass to it against the filenames
		   in the archive */

		files[archiveFileCount].fileName = 
			alloca((strlen(fileList[i]) * 2) + 1);

		unglobFilename(files[archiveFileCount].fileName, 
				fileList[i] + relocationSize);
		files[archiveFileCount].size = fileSizesList[i];


		/* both of these list what files need to be installed and 
		   where they end up *after* any relocations have been done 
		   the mkdir list includes direcories though, while the final
		   list does *not -- we don't let cpio expand directories as
		   it has a bad habit of replacing symlinks with directories
		   when we'd rather it didn't */
		mkdirFileList[mkdirFileCount] = rootedFileList[i];
		mkdirModesList[mkdirFileCount] = fileModesList[i];
		mkdirMtimesList[mkdirFileCount] = fileMtimesList[i];
		mkdirFileCount++;

		if (!S_ISDIR(fileModesList[i])) {
		    finalFileList[archiveFileCount] = rootedFileList[i];
		    archiveFileCount++;
		}
	    }
	}

	if (rootdir) {
	    tmpPath = alloca(strlen(rootdir) + 
			     strlen(rpmGetVar(RPMVAR_TMPPATH)) + 20);
	    strcpy(tmpPath, rootdir);
	    strcat(tmpPath, rpmGetVar(RPMVAR_TMPPATH));
	} else
	    tmpPath = rpmGetVar(RPMVAR_TMPPATH);

	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, (void *) &archiveSizePtr, 
		      &count))
	    archiveSizePtr = NULL;

	if (createDirectories(mkdirFileList, mkdirModesList, mkdirMtimesList,
			      mkdirFileCount)) {
	    headerFree(h);
	    free(fileList);
	    if (replacedList) free(replacedList);
	    return 2;
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(archivePrefix, fd, files, archiveFileCount, notify, 
			   NULL, tmpPath, 
			   archiveSizePtr ? *archiveSizePtr : 0)) {
	    headerFree(h);
	    free(fileList);
	    if (replacedList) free(replacedList);
	    return 2;
	}

	if (headerGetEntry(h, RPMTAG_FILEUSERNAME, &type, (void **) &fileOwners, 
		     &fileCount)) {
	    if (headerGetEntry(h, RPMTAG_FILEGROUPNAME, &type, (void **) &fileGroups, 
			 &fileCount)) {
		if (setFileOwnerships(rootdir, fileList, fileOwners, fileGroups, 
				fileModesList, instActions, fileCount)) {
		    free(fileOwners);
		    free(fileGroups);
		    free(fileList);
		    if (replacedList) free(replacedList);

		    return 2;
		}
		free(fileGroups);
	    }
	    free(fileOwners);
	}
	free(fileList);

	headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fileStatesList, fileCount);

	installTime = time(NULL);
	headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1);
    }

    if (replacedList) {
	rc = markReplacedFiles(db, replacedList);
	free(replacedList);

	if (rc) return rc;
    }

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

    if (runScript(rootdir, h, RPMTAG_POSTIN, scriptArg,
		  flags & RPMINSTALL_NOSCRIPTS)) {
	return 2;
    }

    if (flags & RPMINSTALL_UPGRADE) {
	rpmMessage(RPMMESS_DEBUG, "removing old versions of package\n");
	intptr = oldVersions;
	while (*intptr) {
	    rpmRemovePackage(rootdir, db, *intptr, 0);
	    intptr++;
	}
    }

    headerFree(h);

    return 0;
}

#define BLOCKSIZE 1024

/* -1 fileCount means install all files */
static int installArchive(char * prefix, int fd, struct fileToInstall * files,
			  int fileCount, rpmNotifyFunction notify, 
			  char ** specFile, char * tmpPath, int archiveSize) {
    gzFile stream;
    char buf[BLOCKSIZE];
    pid_t child;
    int p[2];
    int statusPipe[2];
    int bytesRead;
    int bytes;
    int status;
    int cpioFailed = 0;
    void * oldhandler;
    int needSecondPipe;
    char line[1024];
    int j;
    int i = 0;
    int childGotStatus = 0;
    unsigned long totalSize = 0;
    unsigned long sizeInstalled = 0;
    struct fileToInstall fileInstalled;
    struct fileToInstall * file;
    char * chptr, * filelist;
    char ** args;
    FILE * f;
    int len;
    int childDead = 0;

    /* no files to install */
    if (!fileCount) return 0;

    /* install all files, so don't pass a filelist to cpio */
    if (fileCount == -1) fileCount = 0;

    /* fd should be a gzipped cpio archive */

    rpmMessage(RPMMESS_DEBUG, "installing archive into %s\n", prefix);

    needSecondPipe = (notify != NULL && !archiveSize) || specFile;

    if (specFile) *specFile = NULL;
    
    args = alloca(sizeof(char *) * (fileCount + 10));

    args[i++] = rpmGetVar(RPMVAR_CPIOBIN);
    args[i++] = "--extract";
    args[i++] = "--unconditional";
    args[i++] = "--preserve-modification-time";
    args[i++] = "--make-directories";
    args[i++] = "--quiet";

    if (!args[0]) args[0] = "cpio";

    if (needSecondPipe)
	args[i++] = "--verbose";

    /* note - if fileCount == 0, all files get installed */
    /* if fileCount > 500, we use a temporary file to pass the file
       list to cpio rather then args because we're in danger of passing
       too much argv/env stuff */

    if (fileCount > 500) {
	filelist = alloca(strlen(tmpPath) + 40);

	rpmMessage(RPMMESS_DEBUG, "using a %s filelist\n", tmpPath);
	sprintf(filelist, "%s/rpm-cpiofilelist.%d.tmp", tmpPath, 
		(int) getpid());
	f = fopen(filelist, "w");
	if (!f) {
	    rpmError(RPMERR_CREATE, "failed to create %s: %s", filelist,
		  strerror(errno));
	    return 1;
	}
	
	for (j = 0; j < fileCount; j++) {
	    if ((fputs(files[j].fileName, f) == EOF) || 
		(fputs("\n", f) == EOF)) {
		if (errno == ENOSPC) {
		    rpmError(RPMERR_NOSPACE, "out of space on device");
		} else {
		    rpmError(RPMERR_CREATE, "failed to create %s: %s", filelist,
			  strerror(errno));
		}

		fclose(f);
		unlink(filelist);
		return 1;
	    }
	}

	fclose(f);

	args[i++] = "--pattern-file";
	args[i++] = filelist;
    } else {
	filelist = NULL;
	for (j = 0; j < fileCount; j++)
	    args[i++] = files[j].fileName;
    }

    args[i++] = NULL;
    
    stream = gzdopen(fd, "r");
    pipe(p);

    if (needSecondPipe) {
	pipe(statusPipe);
	for (i = 0; i < fileCount; i++)
	    totalSize += files[i].size;
	qsort(files, fileCount, sizeof(struct fileToInstall), fileCompare);
    }

    oldhandler = signal(SIGPIPE, SIG_IGN);

    child = fork();
    if (!child) {
	chdir(prefix);

	close(p[1]);   /* we don't need to write to it */
	close(0);      /* stdin will come from the pipe instead */
	dup2(p[0], 0);
	close(p[0]);

	if (needSecondPipe) {
	    close(statusPipe[0]);   /* we don't need to read from it*/
	    close(2);      	    /* stderr will go to a pipe instead */
	    dup2(statusPipe[1], 2);
	    close(statusPipe[1]);
	}

	execvp(args[0], args);

	_exit(-1);
    }

    close(p[0]);
    if (needSecondPipe) {
	close(statusPipe[1]);
	fcntl(statusPipe[0], F_SETFL, O_NONBLOCK);
    }

    do {
	if (waitpid(child, &status, WNOHANG)) childDead = 1;
	
	bytesRead = gzread(stream, buf, sizeof(buf));
	if (bytesRead < 0) {
	     cpioFailed = 1;
	     childDead = 1;
	     kill(child, SIGTERM);
	} else if (bytesRead == 0) {
	     /* if it's not dead yet, it will be when we close the pipe */
	     waitpid(child, &status, 0);
	     childDead = 1;
	     childGotStatus = 1;
	} else if (bytesRead && write(p[1], buf, bytesRead) != bytesRead) {
	     cpioFailed = 1;
	     childDead = 1;
	     kill(child, SIGTERM);
	}

	if (needSecondPipe) {
	    bytes = read(statusPipe[0], line, sizeof(line));

	    while (bytes > 0) {
		/* the sooner we erase this, the better. less chance
		   of leaving it sitting around after a SIGINT
		   (or SIGSEGV!) */
		if (filelist) {
		    unlink(filelist);
		    filelist = NULL;
		}

		fileInstalled.fileName = line;

		while ((chptr = (strchr(fileInstalled.fileName, '\n')))) {
		    *chptr = '\0';

		    rpmMessage(RPMMESS_DEBUG, "file \"%s\" complete\n", 
				fileInstalled.fileName);

		    if (notify && !archiveSize) {
			file = bsearch(&fileInstalled, files, fileCount, 
				       sizeof(struct fileToInstall), 
				       fileCompare);
			if (file) {
			    sizeInstalled += file->size;
			    notify(sizeInstalled, totalSize);
			}
		    }

		    if (specFile) {
			len = strlen(fileInstalled.fileName);
			if (fileInstalled.fileName[len - 1] == 'c' &&
			    fileInstalled.fileName[len - 2] == 'e' &&
			    fileInstalled.fileName[len - 3] == 'p' &&
			    fileInstalled.fileName[len - 4] == 's' &&
			    fileInstalled.fileName[len - 5] == '.') {

			    if (*specFile) free(*specFile);
			    *specFile = strdup(fileInstalled.fileName);
			}
		    }
		 
		    fileInstalled.fileName = chptr + 1;
		}

		bytes = read(statusPipe[0], line, sizeof(line));
	    }
	} 

	if (notify && archiveSize) {
	    sizeInstalled += bytesRead;
	    notify(sizeInstalled, archiveSize);
	}
    } while (!childDead);

    gzclose(stream);
    close(p[1]);
    if (needSecondPipe) close(statusPipe[0]);
    signal(SIGPIPE, oldhandler);

    if (!childGotStatus)
	waitpid(child, &status, 0);

    if (filelist) {
	unlink(filelist);
    }

    if (cpioFailed || !WIFEXITED(status) || WEXITSTATUS(status)) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	rpmError(RPMERR_CPIO, "unpacking of archive failed");
	return 1;
    }

    if (notify)
	notify(totalSize, totalSize);

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

static int setFileOwnerships(char * rootdir, char ** fileList, 
			     char ** fileOwners, char ** fileGroups, 
			     int_16 * fileModesList,
			     enum instActions * instActions, int fileCount) {
    int i;
    char * chptr;
    int doFork = 0;
    pid_t child;
    int status;

    rpmMessage(RPMMESS_DEBUG, "setting file owners and groups by name (not id)\n");

    chptr = rootdir;
    while (*chptr && *chptr == '/') 
	chptr++;

    if (*chptr) {
	rpmMessage(RPMMESS_DEBUG, "forking child to setid's in chroot() "
		"environment\n");
	doFork = 1;

	if ((child = fork())) {
	    waitpid(child, &status, 0);
	    return 0;
	} else {
	    chroot(rootdir);
	}
    }

    for (i = 0; i < fileCount; i++) {
	if (instActions[i] != SKIP) {
	    /* ignore errors here - setFileOwner handles them reasonable
	       and we want to keep running */
	    setFileOwner(fileList[i], fileOwners[i], fileGroups[i],
			 fileModesList[i]);
	}
    }

    if (doFork)
	_exit(0);

    return 0;
}

/* setFileOwner() is really poorly implemented. It really ought to use 
   hash tables. I just made the guess that most files would be owned by 
   root or the same person/group who owned the last file. Those two values 
   are cached, everything else is looked up via getpw() and getgr() functions. 
   If this performs too poorly I'll have to implement it properly :-( */

static int setFileOwner(char * file, char * owner, char * group, 
			int_16 mode ) {
    static char * lastOwner = NULL, * lastGroup = NULL;
    static uid_t lastUID;
    static gid_t lastGID;
    uid_t uid = 0;
    gid_t gid = 0;
    struct passwd * pwent;
    struct group * grent;

    if (!strcmp(owner, "root"))
	uid = 0;
    else if (lastOwner && !strcmp(lastOwner, owner))
	uid = lastUID;
    else {
	pwent = getpwnam(owner);
	if (!pwent) {
	    rpmError(RPMERR_NOUSER, "user %s does not exist - using root", owner);
	    uid = 0;
	} else {
	    uid = pwent->pw_uid;
	    if (lastOwner) free(lastOwner);
	    lastOwner = strdup(owner);
	    lastUID = uid;
	}
    }

    if (!strcmp(group, "root"))
	gid = 0;
    else if (lastGroup && !strcmp(lastGroup, group))
	gid = lastGID;
    else {
	grent = getgrnam(group);
	if (!grent) {
	    rpmError(RPMERR_NOGROUP, "group %s does not exist - using root", 
			group);
	    gid = 0;
	} else {
	    gid = grent->gr_gid;
	    if (lastGroup) free(lastGroup);
	    lastGroup = strdup(group);
	    lastGID = gid;
	}
    }
	
    rpmMessage(RPMMESS_DEBUG, "%s owned by %s (%d), group %s (%d) mode %o\n",
		file, owner, uid, group, gid, mode & 07777);
    if (chown(file, uid, gid)) {
	rpmError(RPMERR_CHOWN, "cannot set owner and group for %s - %s",
		file, strerror(errno));
	/* screw with the permissions so it's not SUID and 0.0 */
	chmod(file, 0644);
	return 1;
    }
    /* Also set the mode according to what is stored in the header */
    if (! S_ISLNK(mode)) {
	if (chmod(file, mode & 07777)) {
	    rpmError(RPMERR_CHOWN, "cannot change mode for %s - %s",
		  file, strerror(errno));
	    /* screw with the permissions so it's not SUID and 0.0 */
	    chmod(file, 0644);
	    return 1;
	}
    }

    return 0;
}

/* This could be more efficient. A brute force tokenization and mkdir's
   seems like horrible overkill. I did make it know better then trying to 
   create the same directory string twice in a row though. That should make it 
   perform adequatally thanks to the sorted filelist.

   This could create directories that should be symlinks :-( RPM building
   should probably resolve symlinks in paths. 

   This creates directories which are always 0755, despite the current umask */

static int createDirectories(char ** fileList, uint_32 * modesList, 
			     uint_32 * fileMtimesList, int fileCount) {
    int i;
    char * lastDirectory;
    char * buffer;
    int bufferLength;
    int neededLength;
    char * chptr;

    lastDirectory = malloc(1);
    lastDirectory[0] = '\0';

    bufferLength = 1000;		/* should be more then adequate */
    buffer = malloc(bufferLength);

    for (i = 0; i < fileCount; i++) {
	neededLength = strlen(fileList[i]) + 1;
	if (neededLength > bufferLength) { 
	    free(buffer);
	    bufferLength = neededLength * 2;
	    buffer = malloc(bufferLength);
	}
	strcpy(buffer, fileList[i]);
	
	for (chptr = buffer + strlen(buffer) - 1; *chptr; chptr--) {
	    if (*chptr == '/') break;
	}

	if (! *chptr) continue;		/* no path, just filename */
	if (chptr == buffer) continue;  /* /filename - no directories */

	*chptr = '\0';			/* buffer is now just directories */

	if (strcmp(buffer, lastDirectory)) {
	    for (chptr = buffer + 1; *chptr; chptr++) {
		if (*chptr == '/') {
		    if (*(chptr -1) != '/') {
			*chptr = '\0';
			if (mkdirIfNone(buffer, 0755, 0)) {
			    free(lastDirectory);
			    free(buffer);
			    return 1;
			}
			*chptr = '/';
		    }
		}
	    }

	    if (mkdirIfNone(buffer, 0755, 0)) {
		free(lastDirectory);
		free(buffer);
		return 1;
	    }

	    free(lastDirectory);
	    lastDirectory = strdup(buffer);
	}

	if (S_ISDIR(modesList[i])) {
	    if (mkdirIfNone(fileList[i], 0755, fileMtimesList[i])) {
		free(lastDirectory);
		free(buffer);
		return 1;
	    }
	}
    }

    free(lastDirectory);
    free(buffer);

    return 0;
}

static int mkdirIfNone(char * directory, mode_t perms, uint_32 mtime) {
    int rc;
    char * chptr;
    struct utimbuf stamp;

    for (chptr = directory; *chptr; chptr++)
	if (*chptr != '/') break;
    if (!*chptr) return 0;

    if (exists(directory)) {
	return 0;
    }

    rpmMessage(RPMMESS_DEBUG, "trying to make %s\n", directory);

    rc = mkdir(directory, perms);
    if (!rc) {
	if (mtime) {
	    stamp.actime = mtime;
	    stamp.modtime = mtime;
	    utime(directory, &stamp);
	}
	return 0;
    }

    rpmError(RPMERR_MKDIR, "failed to create %s - %s", directory, 
	  strerror(errno));

    return errno;
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
				char * newMd5, char * newLink, int brokenMd5) {
    char buffer[1024];
    char * dbAttr, * newAttr;
    enum fileTypes dbWhat, newWhat, diskWhat;
    struct stat sb;
    int i, rc;

    if (lstat(filespec, &sb)) {
	/* the file doesn't exist on the disk - might as well make it */
	return CREATE;
    }

    diskWhat = whatis(sb.st_mode);
    dbWhat = whatis(dbMode);
    newWhat = whatis(newMode);

    if (diskWhat != newWhat) {
	rpmMessage(RPMMESS_DEBUG, "	file type on disk is different then package - "
			"saving\n");
	return SAVE;
    } else if (newWhat != dbWhat && diskWhat != dbWhat) {
	rpmMessage(RPMMESS_DEBUG, "	file type in database is different then disk"
			" and package file - saving\n");
	return SAVE;
    } else if (dbWhat != newWhat) {
	rpmMessage(RPMMESS_DEBUG, "	file type changed - replacing\n");
	return CREATE;
    } else if (dbWhat != LINK && dbWhat != REG) {
	rpmMessage(RPMMESS_DEBUG, "	can't check file for changes - replacing\n");
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

static int instHandleSharedFiles(rpmdb db, int ignoreOffset, char ** fileList, 
			         char ** fileMd5List, int_16 * fileModesList,
				 char ** fileLinkList, uint_32 * fileFlagsList,
				 int fileCount, enum instActions * instActions, 
			 	 char ** prefixedFileList, int * notErrors,
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

    if (findSharedFiles(db, 0, fileList, fileCount, &sharedList, 
			&sharedCount)) {
	return 1;
    }
    
    numReplacedAlloced = 10;
    numReplacedFiles = 0;
    replacedList = malloc(sizeof(*replacedList) * numReplacedAlloced);

    for (i = 0; i < sharedCount; i++) {
	if (sharedList[i].secRecOffset == ignoreOffset) continue;

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

	intptr = notErrors;
	while (*intptr) {
	    if (*intptr == sharedList[i].secRecOffset) break;
	    intptr++;
	}

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

	if (filecmp(fileModesList[mainNum], fileMd5List[mainNum], 
		    fileLinkList[mainNum], secFileModesList[secNum],
		    secFileMd5List[secNum], secFileLinksList[secNum])) {
	    if (!(flags & RPMINSTALL_REPLACEFILES) && !(*intptr)) {
		rpmError(RPMERR_PKGINSTALLED, "%s conflicts with file from "
		      "%s-%s-%s", fileList[sharedList[i].mainFileNumber],
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
			fileList[sharedList[i].mainFileNumber],
			name, version, release);
	    }
	}

	/* if this is a config file, we need to be carefull here */
	if (fileFlagsList[sharedList[i].mainFileNumber] & RPMFILE_CONFIG ||
	    secFileFlagsList[sharedList[i].secFileNumber] & RPMFILE_CONFIG) {
	    instActions[sharedList[i].mainFileNumber] = 
		decideFileFate(prefixedFileList[mainNum], 
			       secFileModesList[secNum],
			       secFileMd5List[secNum], secFileLinksList[secNum],
			       fileModesList[mainNum], fileMd5List[mainNum],
			       fileLinkList[mainNum], 
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

static int fileCompare(const void * one, const void * two) {
    return strcmp(((struct fileToInstall *) one)->fileName,
		  ((struct fileToInstall *) two)->fileName);
}

/* 0 success */
/* 1 bad magic */
/* 2 error */
static int installSources(Header h, char * rootdir, int fd, 
			  char ** specFilePtr, rpmNotifyFunction notify,
			  char * labelFormat) {
    char * specFile;
    char * sourceDir, * specDir;
    char * realSourceDir, * realSpecDir;
    char * instSpecFile, * correctSpecFile;
    char * tmpPath, * name, * release, * version;
    uint_32 * archiveSizePtr = NULL;
    int type, count;

    rpmMessage(RPMMESS_DEBUG, "installing a source package\n");

    sourceDir = rpmGetVar(RPMVAR_SOURCEDIR);
    specDir = rpmGetVar(RPMVAR_SPECDIR);

    if (access(sourceDir, W_OK)) {
	rpmError(RPMERR_CREATE, "cannot write to %s", sourceDir);
	return 2;
    }

    if (access(specDir, W_OK)) {
	rpmError(RPMERR_CREATE, "cannot write to %s", specDir);
	return 2;
    }

    realSourceDir = alloca(strlen(rootdir) + strlen(sourceDir) + 2);
    strcpy(realSourceDir, rootdir);
    strcat(realSourceDir, "/");
    strcat(realSourceDir, sourceDir);

    realSpecDir = alloca(strlen(rootdir) + strlen(specDir) + 2);
    strcpy(realSpecDir, rootdir);
    strcat(realSpecDir, "/");
    strcat(realSpecDir, specDir);

    rpmMessage(RPMMESS_DEBUG, "sources in: %s\n", realSourceDir);
    rpmMessage(RPMMESS_DEBUG, "spec file in: %s\n", realSpecDir);

    if (rootdir) {
	tmpPath = alloca(strlen(rootdir) + 
			 strlen(rpmGetVar(RPMVAR_TMPPATH)) + 20);
	strcpy(tmpPath, rootdir);
	strcat(tmpPath, rpmGetVar(RPMVAR_TMPPATH));
    } else
	tmpPath = rpmGetVar(RPMVAR_TMPPATH);

    if (labelFormat && h) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void *) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void *) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void *) &release, &count);
	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, &type, (void *) &archiveSizePtr, 
		      &count))
	    archiveSizePtr = NULL;
	printf(labelFormat, name, version, release);
	fflush(stdout);
    }

    if (installArchive(realSourceDir, fd, NULL, -1, notify, &specFile, 
		       tmpPath, archiveSizePtr ? *archiveSizePtr : 0)) {
	return 2;
    }

    if (!specFile) {
	rpmError(RPMERR_NOSPEC, "source package contains no .spec file");
	return 1;
    }

    /* This logic doesn't work is realSpecDir and realSourceDir are on
       different filesystems XXX */
    instSpecFile = alloca(strlen(realSourceDir) + strlen(specFile) + 2);
    strcpy(instSpecFile, realSourceDir);
    strcat(instSpecFile, "/");
    strcat(instSpecFile, specFile);

    correctSpecFile = alloca(strlen(realSpecDir) + strlen(specFile) + 2);
    strcpy(correctSpecFile, realSpecDir);
    strcat(correctSpecFile, "/");
    strcat(correctSpecFile, specFile);

    rpmMessage(RPMMESS_DEBUG, "renaming %s to %s\n", instSpecFile, correctSpecFile);
    if (rename(instSpecFile, correctSpecFile)) {
	/* try copying the file */
	if (moveFile(instSpecFile, correctSpecFile))
	    return 2;
    }

    if (specFilePtr)
	*specFilePtr = strdup(correctSpecFile);

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

	    headerGetEntry(secHeader, RPMTAG_FILESTATES, &type, (void **) &secStates, 
		     &count);
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
    int rc;

    if (!headerGetEntry(first, RPMTAG_SERIAL, NULL, (void **) &one, NULL))
	one = NULL;
    if (!headerGetEntry(second, RPMTAG_SERIAL, NULL, (void **) &two, NULL))
	two = NULL;

    if (one && !two) 
	return -1;
    else if (!one && two) 
	return 1;
    else if (one && two)
	return rpmvercmp(one, two);

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

/* This is *much* more difficult then it should be. Rather then just
   modifying an entry with a single call to modifyEntry(), we have to
   clone most of the header. Once the internal data structures of
   header.c get cleaned up, this will be *much* easier */
static int relocateFilelist(Header * hp, char * defaultPrefix, 
			    char * newPrefix, int * relocationLength) {
    Header newh, h = *hp;
    HeaderIterator it;
    char ** newFileList, ** fileList;
    int type, count, tag, fileCount, i;
    void * data;
    int defaultPrefixLength;
    int newPrefixLength;

    /* a trailing '/' in the defaultPrefix or in the newPrefix would really
       confuse us */
    defaultPrefix = strcpy(alloca(strlen(defaultPrefix) + 1), defaultPrefix);
    stripTrailingSlashes(defaultPrefix);
    newPrefix = strcpy(alloca(strlen(newPrefix) + 1), newPrefix);
    stripTrailingSlashes(newPrefix);

    rpmMessage(RPMMESS_DEBUG, "relocating files from %s to %s\n", defaultPrefix,
			 newPrefix);

    if (!strcmp(newPrefix, defaultPrefix)) {
	headerAddEntry(h, RPMTAG_INSTALLPREFIX, RPM_STRING_TYPE, defaultPrefix, 1);
	*relocationLength = strlen(defaultPrefix) + 1;
	return 0;
    }

    defaultPrefixLength = strlen(defaultPrefix);
    newPrefixLength = strlen(newPrefix);

    /* packages can have empty filelists */
    if (!headerGetEntry(h, RPMTAG_FILENAMES, &type, (void *) &fileList, &fileCount))
	return 0;
    if (!count)
	return 0;

    newh = headerNew();
    it = headerInitIterator(h);
    while (headerNextIterator(it, &tag, &type, &data, &count))
	if (tag != RPMTAG_FILENAMES) {
	    headerAddEntry(newh, tag, type, data, count);
	if (type == RPM_STRING_ARRAY_TYPE) free(data);
    }

    headerFreeIterator(it);

    newFileList = alloca(sizeof(char *) * fileCount);
    for (i = 0; i < fileCount; i++) {
	if (!strcmp(fileList[i], defaultPrefix)) {
	    /* special case as there is no '/' on this */
	    newFileList[i] = newPrefix;
	} else if (!strncmp(fileList[i], defaultPrefix, defaultPrefixLength)) {
	    newFileList[i] = alloca(strlen(fileList[i]) + newPrefixLength -
				 defaultPrefixLength + 2);
	    sprintf(newFileList[i], "%s/%s", newPrefix, 
		    fileList[i] + defaultPrefixLength + 1);
	} else {
	    rpmMessage(RPMMESS_DEBUG, "BAD - unprefixed file in relocatable "
			"package");
	    newFileList[i] = alloca(strlen(fileList[i]) - 
					defaultPrefixLength + 2);
	    sprintf(newFileList[i], "/%s", fileList[i] + 
			defaultPrefixLength + 1);
	}
    }

    headerAddEntry(newh, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, newFileList, 
		   fileCount);
    headerAddEntry(newh, RPMTAG_INSTALLPREFIX, RPM_STRING_TYPE, newPrefix, 1);

    *relocationLength = newPrefixLength + 1;
    *hp = newh;

    return 0;
}

static int archOkay(Header h) {
    int_8 * pkgArchNum;
    void * pkgArch;
    int type, count;

    /* make sure we're trying to install this on the proper architecture */
    headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count);
    if (type == RPM_INT8_TYPE) {
	/* old arch handling */
	pkgArchNum = pkgArch;
	if (rpmGetArchNum() != *pkgArchNum) {
	    return 0;
	}
    } else {
	/* new arch handling */
	if (!rpmArchScore(pkgArch)) {
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
	if (!rpmOsScore(pkgOs)) {
	    return 0;
	}
    }

    return 1;
}

static int moveFile(char * sourceName, char * destName) {
    if (copyFile(sourceName, destName)) return 1;

    unlink(sourceName);
    return 0;
}

static int copyFile(char * sourceName, char * destName) {
    int source, dest, i;
    char buf[16384];

    rpmMessage(RPMMESS_DEBUG, "coping %s to %s\n", sourceName, destName);

    source = open(sourceName, O_RDONLY);
    if (source < 0) {
	rpmError(RPMERR_INTERNAL, "file %s missing from source directory",
		    sourceName);
	return 1;
    }

    dest = creat(destName, 0644);
    if (dest < 0) {
	rpmError(RPMERR_CREATE, "failed to create file %s", destName);
	close(source);
	return 1;
    }

    while ((i = read(source, buf, sizeof(buf))) > 0) {
	if (write(dest, buf, i) != i) {
	    if (errno == ENOSPC) {
		rpmError(RPMERR_NOSPACE, "out of disk space writing file %s",
			destName);
	    } else {
		rpmError(RPMERR_CREATE, "error writing to file %s: %s",
			destName, strerror(errno));
	    }
	    close(source);
	    close(dest);
	    unlink(destName);
	    return 1;
	}
    }

    if (i < 0) {
	rpmError(RPMERR_CREATE, "error reading from file %s: %s",
		sourceName, strerror(errno));
    }

    close(source);
    close(dest);
    
    if (i < 0)
	return 1;

    return 0;
}

static void unglobFilename(char * dptr, char * sptr) {
    while (*sptr) {
	switch (*sptr) {
	  case '*': case '[': case ']': case '?': case '\\':
	    *dptr++ = '\\';
	    /*fallthrough*/
	  default:
	    *dptr++ = *sptr++;
	}
    }
    *dptr++ = *sptr;
}
