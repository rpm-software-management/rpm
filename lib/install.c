#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <misc.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>		/* needed for mkdir(2) prototype! */
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "header.h"
#include "install.h"
#include "md5.h"
#include "package.h"
#include "rpmerr.h"
#include "rpmlib.h"

enum instActions { CREATE, BACKUP, KEEP, SAVE };

struct fileToInstall {
    char * fileName;
    int size;
} ;

static int installArchive(char * prefix, int fd, struct fileToInstall * files,
			  int fileCount, notifyFunction notify);
static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int flags);
static int setFileOwnerships(char * prefix, char ** fileList, 
			     char ** fileOwners, char ** fileGroups, 
			     int fileCount);
static int setFileOwner(char * prefix, char * file, char * owner, 
			char * group);
static int createDirectories(char * prefix, char ** fileList, int fileCount);
static int mkdirIfNone(char * directory, mode_t perms);
static int instHandleSharedFiles(rpmdb db, int ignoreOffset, char ** fileList, 
			         char ** fileMd5List, int fileCount, 
				 enum instActions * instActions, 
				 char ** prefixedFileList, int flags);
static int fileCompare(const void * one, const void * two);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallPackage(char * prefix, rpmdb db, int fd, int flags, 
		      notifyFunction notify) {
    int rc, isSource;
    char * name, * version, * release;
    Header h, h2;
    int fileCount, type;
    char ** fileList;
    char ** fileOwners, ** fileGroups, ** fileMd5s;
    uint_32 * fileFlagsList;
    uint_32 * fileSizesList;
    int_32 installTime;
    char * fileStatesList;
    struct fileToInstall * files;
    enum instActions * instActions = NULL;
    int i;
    int archiveFileCount = 0;
    int installFile = 0;
    char * ext = NULL, * newpath;
    int prefixLength = strlen(prefix);
    char ** prefixedFileList = NULL;

    rc = pkgReadHeader(fd, &h, &isSource);
    if (rc) return rc;

    /* we make a copy of the header here so we have one which we can add
       entries to */
    h2 = copyHeader(h);
    freeHeader(h);
    h = h2;

    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);

    message(MESS_DEBUG, "package: %s-%s-%s files test = %d\n", 
		name, version, release, flags & INSTALL_TEST);

    if (packageAlreadyInstalled(db, name, version, release, flags)) {
	freeHeader(h);
	return 2;
    }

    fileList = NULL;
    if (getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &fileCount)) {

	instActions = alloca(sizeof(enum instActions) * fileCount);
	prefixedFileList = alloca(sizeof(char *) * fileCount);

	getEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5s, &fileCount);
	getEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);

	/* check for any config files that already exist. If they do, plan
	   on making a backup copy. If that's not the right thing to do
	   instHandleSharedFiles() below will take care of the problem */
	for (i = 0; i < fileCount; i++) {
	    if (prefixLength > 1) {
		prefixedFileList[i] = alloca(strlen(fileList[i]) + 
				prefixLength + 3);
		strcpy(prefixedFileList[i], prefix);
		strcat(prefixedFileList[i], "/");
		strcat(prefixedFileList[i], fileList[i]);
	    } else 
		prefixedFileList[i] = fileList[i];

	    instActions[i] = CREATE;
	    if (fileFlagsList[i] & RPMFILE_CONFIG) {
		if (exists(prefixedFileList[i])) {
		    message(MESS_DEBUG, "%s exists - backing up", 
				prefixedFileList[i]);
		    instActions[i] = BACKUP;
		}
	    }
	}

	rc = instHandleSharedFiles(db, 0, fileList, fileMd5s, fileCount, 
				   instActions, prefixedFileList, flags);

	free(fileMd5s);
	if (rc) {
	    free(fileList);
	    return 1;
	}
    }
    
    if (flags & INSTALL_TEST) {
	message(MESS_DEBUG, "stopping install as we're running --test\n");
	free(fileList);
	return 0;
    }

    message(MESS_DEBUG, "running preinstall script (if any)\n");
    if (runScript(prefix, h, RPMTAG_PREIN)) {
	free(fileList);
	return 2;
    }

    if (fileList) {

	if (createDirectories(prefix, fileList, fileCount)) {
	    freeHeader(h);
	    free(fileList);
	    return 2;
	}

	getEntry(h, RPMTAG_FILESIZES, &type, (void **) &fileSizesList, 
		 &fileCount);

	files = alloca(sizeof(struct fileToInstall) * fileCount);
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
	    }

	    if (ext) {
		newpath = malloc(strlen(prefixedFileList[i]) + 20);
		strcpy(newpath, prefixedFileList[i]);
		strcat(newpath, ext);
		message(MESS_WARNING, "%s saved as %s\n", prefixedFileList[i], 
			newpath);
		/* XXX this message is a bad idea - it'll make glint more
		   difficult */

		if (rename(prefixedFileList[i], newpath)) {
		    error(RPMERR_RENAME, "rename of %s to %s failed: %s\n",
			  prefixedFileList[i], newpath, strerror(errno));
		    return 1;
		}

		free(newpath);
	    }

	    if (installFile) {
 		/* +1 cuts off leading / */

		files[archiveFileCount].fileName = fileList[i] + 1;
		files[archiveFileCount].size = fileSizesList[i];
		archiveFileCount++;
	    }
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(prefix, fd, files, archiveFileCount, notify)) {
	    freeHeader(h);
	    free(fileList);
	    return 2;
	}

	if (getEntry(h, RPMTAG_FILEUSERNAME, &type, (void **) &fileOwners, 
		     &fileCount)) {
	    if (getEntry(h, RPMTAG_FILEGROUPNAME, &type, (void **) &fileGroups, 
			 &fileCount)) {
		if (setFileOwnerships(prefix, fileList, fileOwners, fileGroups, 
				fileCount)) {
		    free(fileOwners);
		    free(fileGroups);
		    free(fileList);
		    return 2;
		}
		free(fileGroups);
	    }
	    free(fileOwners);
	}
	free(fileList);

	fileStatesList = malloc(sizeof(char) * fileCount);
	memset(fileStatesList, RPMFILE_STATE_NORMAL, fileCount);
	addEntry(h, RPMTAG_FILESTATES, CHAR_TYPE, fileStatesList, fileCount);

	installTime = time(NULL);
	addEntry(h, RPMTAG_INSTALLTIME, INT32_TYPE, &installTime, 1);
    }

    if (rpmdbAdd(db, h)) {
	freeHeader(h);
	return 2;
    }

    /* we need to figure out which files have been replaced XXX */

    message(MESS_DEBUG, "running postinstall script (if any)\n");

    if (runScript(prefix, h, RPMTAG_POSTIN)) {
	return 1;
    }

    return 0;
}

#define BLOCKSIZE 1024

static int installArchive(char * prefix, int fd, struct fileToInstall * files,
			  int fileCount, notifyFunction notify) {
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
    char * cpioBinary;
    int needSecondPipe;
    char line[1024];
    int j;
    int i = 0;
    unsigned long totalSize = 0;
    unsigned long sizeInstalled = 0;
    struct fileToInstall fileInstalled;
    struct fileToInstall * file;
    char * chptr;
    char ** args;

    /* fd should be a gzipped cpio archive */

    needSecondPipe = (notify != NULL);
    
    if (access("/bin/cpio",  X_OK))  {
	if (access("/usr/bin/cpio",  X_OK))  {
	    error(RPMERR_CPIO, "cpio cannot be found in /bin or /usr/sbin");
	    return 1;
	} else 
	    cpioBinary = "/usr/bin/cpio";
    } else
	cpioBinary = "/bin/cpio";

    args = alloca(sizeof(char *) * (fileCount + 10));

    args[i++] = cpioBinary;
    args[i++] = "--extract";
    args[i++] = "--unconditional";
    args[i++] = "--preserve-modification-time";
    args[i++] = "--make-directories";
    args[i++] = "--quiet";

    if (needSecondPipe)
	args[i++] = "--verbose";

    for (j = 0; j < fileCount; j++)
	args[i++] = files[j].fileName;

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

	execv(args[0], args);

	exit(-1);
    }

    close(p[0]);
    if (needSecondPipe) {
	close(statusPipe[1]);
	fcntl(statusPipe[0], F_SETFL, O_NONBLOCK);
    }

    do {
	bytesRead = gzread(stream, buf, sizeof(buf));
	if (write(p[1], buf, bytesRead) != bytesRead) {
	     cpioFailed = 1;
	     kill(SIGTERM, child);
	}

	if (needSecondPipe) {
	    bytes = read(statusPipe[0], line, sizeof(line));
	    while (bytes > 0) {
		fileInstalled.fileName = line;

		while ((chptr = (strchr(fileInstalled.fileName, '\n')))) {
		    *chptr = '\0';

		    message(MESS_DEBUG, "file \"%s\" complete\n", line);

		    file = bsearch(&fileInstalled, files, fileCount, 
				   sizeof(struct fileToInstall), fileCompare);
		    if (file) {
			sizeInstalled += file->size;
			notify(sizeInstalled, totalSize);
		    }
		 
		    fileInstalled.fileName = chptr + 1;
		}

		bytes = read(statusPipe[0], line, sizeof(line));
	    }
	}
    } while (bytesRead);

    gzclose(stream);
    close(p[1]);
    if (needSecondPipe) close(statusPipe[0]);
    signal(SIGPIPE, oldhandler);
    waitpid(child, &status, 0);

    if (cpioFailed || !WIFEXITED(status) || WEXITSTATUS(status)) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	error(RPMERR_CPIO, "unpacking of archive failed");
	return 1;
    }

    if (notify)
	notify(totalSize, totalSize);

    return 0;
}

static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int flags) {
    char * secVersion, * secRelease;
    Header sech;
    int i;
    dbIndexSet matches;
    int type, count;

    if (!rpmdbFindPackage(db, name, &matches)) {
	for (i = 0; i < matches.count; i++) {
	    sech = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!sech) {
		return 1;
	    }

	    getEntry(sech, RPMTAG_VERSION, &type, (void **) &secVersion, 
			&count);
	    getEntry(sech, RPMTAG_RELEASE, &type, (void **) &secRelease, 
			&count);

	    freeHeader(sech);

	    if (!strcmp(secVersion, version) && !strcmp(secRelease, release)) {
		if (!(flags & INSTALL_REPLACEPKG)) {
		    error(RPMERR_PKGINSTALLED, 
			  "package %s-%s-%s is already installed",
			  name, version, release);
		    return 1;
		}
	    }
	}
    }

    return 0;
}

static int setFileOwnerships(char * prefix, char ** fileList, 
			     char ** fileOwners, char ** fileGroups, 
			     int fileCount) {
    int i;

    message(MESS_DEBUG, "setting file owners and groups by name (not id)");

    for (i = 0; i < fileCount; i++) {
	/* ignore errors here - setFileOwner handles them reasonable
	   and we want to keep running */
	setFileOwner(prefix, fileList[i], fileOwners[i], fileGroups[i]);
    }

    return 0;
}

/* setFileOwner() is really poorly implemented. It really ought to use 
   hash tables. I just made the guess that most files would be owned by 
   root or the same person/group who owned the last file. Those two values 
   are cached, everything else is looked up via getpw() and getgr() functions. 
   If this performs too poorly I'll have to implement it properly :-( */

static int setFileOwner(char * prefix, char * file, char * owner, 
			char * group) {
    static char * lastOwner = NULL, * lastGroup = NULL;
    static uid_t lastUID;
    static gid_t lastGID;
    uid_t uid = 0;
    gid_t gid = 0;
    struct passwd * pwent;
    struct group * grent;
    char * filespec;

    filespec = alloca(strlen(prefix) + strlen(file) + 5);
    strcpy(filespec, prefix);
    strcat(filespec, "/");
    strcat(filespec, file);

    if (!strcmp(owner, "root"))
	uid = 0;
    else if (lastOwner && !strcmp(lastOwner, owner))
	uid = lastUID;
    else {
	pwent = getpwnam(owner);
	if (!pwent) {
	    error(RPMERR_NOUSER, "user %s does not exist - using root", owner);
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
	    error(RPMERR_NOGROUP, "group %s does not exist - using root", 
			group);
	    gid = 0;
	} else {
	    gid = grent->gr_gid;
	    if (lastGroup) free(lastGroup);
	    lastGroup = strdup(owner);
	    lastGID = gid;
	}
    }
	
    message(MESS_DEBUG, "%s owned by %s (%d), group %s (%d)\n", filespec,
	    owner, uid, group, gid);
    if (chown(filespec, uid, gid)) {
	error(RPMERR_CHOWN, "cannot set owner and group for %s - %s\n",
		filespec, strerror(errno));
	/* screw with the permissions so it's not SUID and 0.0 */
	chmod(filespec, 0644);
	return 1;
    }

    return 0;
}

/* This could be more efficient. A brute force tokenization and mkdir's
   seems like horrible overkill. I did make it know better then trying to 
   create the same directory sintrg twice in a row though. That should make it 
   perform adequatally thanks to the sorted filelist.

   This could create directories that should be symlinks :-( RPM building
   should probably resolve symlinks in paths.

   This creates directories which are always 0755, despite the current umask */

static int createDirectories(char * prefix, char ** fileList, int fileCount) {
    int i;
    char * lastDirectory;
    char * buffer;
    int bufferLength;
    int prefixLength = strlen(prefix);
    int neededLength;
    char * chptr;

    lastDirectory = malloc(1);
    lastDirectory[0] = '\0';

    bufferLength = 1000;		/* should be more then adequate */
    buffer = malloc(bufferLength);

    for (i = 0; i < fileCount; i++) {
	neededLength = prefixLength + 5 + strlen(fileList[i]);
	if (neededLength > bufferLength) { 
	    free(buffer);
	    bufferLength = neededLength * 2;
	    buffer = malloc(bufferLength);
	}
	strcpy(buffer, prefix);
	strcat(buffer, "/");
	strcat(buffer, fileList[i]);
	
	for (chptr = buffer + strlen(buffer) - 1; *chptr; chptr--) {
	    if (*chptr == '/') break;
	}

	if (! *chptr) continue;		/* no path, just filename */
	if (chptr == buffer) continue;  /* /filename - no directories */

	*chptr = '\0';			/* buffer is now just directories */

	if (!strcmp(buffer, lastDirectory)) continue;
	
	for (chptr = buffer + 1; *chptr; chptr++) {
	    if (*chptr == '/') {
		if (*(chptr -1) != '/') {
		    *chptr = '\0';
		    if (mkdirIfNone(buffer, 0755)) {
			free(lastDirectory);
			free(buffer);
			return 1;
		    }
		    *chptr = '/';
		}
	    }
	}

	if (mkdirIfNone(buffer, 0755)) {
	    free(lastDirectory);
	    free(buffer);
	    return 1;
	}

	free(lastDirectory);
	lastDirectory = strdup(buffer);
    }

    free(lastDirectory);
    free(buffer);

    return 0;
}

static int mkdirIfNone(char * directory, mode_t perms) {
    int rc;
    char * chptr;

    /* if the path is '/' we get ENOFILE not found" from mkdir, rather
       then EEXIST which is weird */
    for (chptr = directory; *chptr; chptr++)
	if (*chptr != '/') break;
    if (!*chptr) return 0;

    message(MESS_DEBUG, "trying to make %s\n", directory);

    rc = mkdir(directory, perms);
    if (!rc || errno == EEXIST) return 0;

    chmod(directory, perms);  /* this should not be modified by the umask */

    error(RPMERR_MKDIR, "failed to create %s - %s\n", directory, 
	  strerror(errno));

    return errno;
}

/* return 0: okay, continue install */
/* return 1: problem, halt install */

static int instHandleSharedFiles(rpmdb db, int ignoreOffset, char ** fileList, 
			         char ** fileMd5List, int fileCount, 
			         enum instActions * instActions, 
			 	 char ** prefixedFileList, int flags) {
    struct sharedFile * sharedList;
    int sharedCount;
    int i, type;
    Header sech;
    int secOffset = 0;
    int secFileCount;
    char ** secFileMd5List, ** secFileList;
    char * secFileStatesList;
    uint_32 * secFileFlagsList;
    char * name, * version, * release;
    char currentMd5[33];
    int rc = 0;

    if (findSharedFiles(db, 0, fileList, fileCount, &sharedList, 
			&sharedCount)) {
	return 1;
    }
    
    for (i = 0; i < sharedCount; i++) {
	if (secOffset == ignoreOffset) continue;

	if (secOffset != sharedList[i].secRecOffset) {
	    if (secOffset) {
		free(secFileMd5List);
		free(secFileList);
	    }

	    secOffset = sharedList[i].secRecOffset;
	    sech = rpmdbGetRecord(db, secOffset);
	    if (!sech) {
		error(RPMERR_DBCORRUPT, "cannot read header at %d for "
		      "uninstall", secOffset);
		rc = 1;
		break;
	    }

	    getEntry(sech, RPMTAG_NAME, &type, (void **) &name, 
		     &secFileCount);
	    getEntry(sech, RPMTAG_VERSION, &type, (void **) &version, 
		     &secFileCount);
	    getEntry(sech, RPMTAG_RELEASE, &type, (void **) &release, 
		     &secFileCount);

	    message(MESS_DEBUG, "package %s-%s-%s contain shared files\n", 
		    name, version, release);

	    if (!getEntry(sech, RPMTAG_FILENAMES, &type, 
			  (void **) &secFileList, &secFileCount)) {
		error(RPMERR_DBCORRUPT, "package %s contains no files\n",
		      name);
		freeHeader(sech);
		rc = 1;
		break;
	    }

	    getEntry(sech, RPMTAG_FILESTATES, &type, 
		     (void **) &secFileStatesList, &secFileCount);
	    getEntry(sech, RPMTAG_FILEMD5S, &type, 
		     (void **) &secFileMd5List, &secFileCount);
	    getEntry(sech, RPMTAG_FILEFLAGS, &type, 
		     (void **) &secFileFlagsList, &secFileCount);

	    freeHeader(sech);
	}

	message(MESS_DEBUG, "file %s is shared\n", 
		secFileList[sharedList[i].secFileNumber]);

	if (!(flags & INSTALL_REPLACEFILES)) {
	    error(RPMERR_PKGINSTALLED, "%s conflicts with file from %s",
		  fileList[sharedList[i].mainFileNumber],
		  name, version, release);
	    rc = 1;
	}

	/* if this instance of the shared file is already recorded as
	   replaced, just forget about it */
	if (secFileStatesList[sharedList[i].secFileNumber] == 
		RPMFILE_STATE_REPLACED) {
	    message(MESS_DEBUG, "	old version already replaced\n");
	    continue;
	}

	/* All of this md5 stuff is nice, but it needs to know about
	   symlink comparisons as well :-( */

	/* if this is a config file, we need to be carefull here */
	if (secFileFlagsList[sharedList[i].secFileNumber] & RPMFILE_CONFIG) {
	    if (!strcmp(fileMd5List[sharedList[i].mainFileNumber], 
		        secFileMd5List[sharedList[i].secFileNumber])) {

		/* the file is the same in both the old package and the
		   new one, save the one that is currently installed */

		message(MESS_DEBUG, "	old == new, keeping installed "
			"version\n");
		instActions[sharedList[i].mainFileNumber] = KEEP;
	    } else {
		if (mdfile(prefixedFileList[sharedList[i].mainFileNumber], 
				currentMd5)) {
		    /* assume the file has been removed, don't freak */
		    message(MESS_DEBUG, "	file not present - creating");
		    instActions[sharedList[i].mainFileNumber] = CREATE;
		} else if (!strcmp(secFileMd5List[sharedList[i].secFileNumber],
			           currentMd5)) {
		    /* this config file has never been modified, so 
		       just replace it */
		    message(MESS_DEBUG, "	old == current, replacing "
			    "with new version\n");
		    instActions[sharedList[i].mainFileNumber] = CREATE;
		} else {
		    /* the config file on the disk has been modified, but
		       the ones in the two packages are different. It would
		       be nice if RPM was smart enough to at least try and
		       merge the difference ala CVS, but... */
		    message(MESS_DEBUG, "	files changed to much - "
			    "backing up");
		    
		    message(MESS_WARNING, "%s will be saved as %s.rpmsave\n",
			prefixedFileList[sharedList[i].mainFileNumber], 
			prefixedFileList[sharedList[i].mainFileNumber]);

		    instActions[sharedList[i].mainFileNumber] = SAVE;
		}
	    }
	}
    }

    if (secOffset) {
	free(secFileMd5List);
	free(secFileList);
    }

    free(sharedList);

    return rc;
}

static int fileCompare(const void * one, const void * two) {
    return strcmp(((struct fileToInstall *) one)->fileName,
		  ((struct fileToInstall *) two)->fileName);
}
