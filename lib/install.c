#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>		/* needed for mkdir(2) prototype! */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "header.h"
#include "install.h"
#include "package.h"
#include "rpmerr.h"
#include "rpmlib.h"

static int installArchive(char * prefix, int fd);
static int packageAlreadyInstalled(rpmdb db, char * name, char * version, 
				   char * release, int flags);
static int setFileOwnerships(char * prefix, char ** fileList, 
			     char ** fileOwners, char ** fileGroups, 
			     int fileCount);
static int setFileOwner(char * prefix, char * file, char * owner, 
			char * group);
static int createDirectories(char * prefix, char ** fileList, int fileCount);
static int mkdirIfNone(char * directory, mode_t perms);

/* 0 success */
/* 1 bad magic */
/* 2 error */
int rpmInstallPackage(char * prefix, rpmdb db, int fd, int flags, int test) {
    int rc, isSource;
    char * name, * version, * release;
    Header h;
    int fileCount, type;
    char ** fileList;
    char ** fileOwners, ** fileGroups;

    rc = pkgReadHeader(fd, &h, &isSource);
    if (rc) return rc;

    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &fileCount);
    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &fileCount);
    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &fileCount);

    message(MESS_DEBUG, "package: %s-%s-%s files test = %d\n", 
		name, version, release, test);

    if (packageAlreadyInstalled(db, name, version, release, flags)) {
	freeHeader(h);
	return 2;
    }
    
    if (test) {
	message(MESS_DEBUG, "stopping install as we're running --test\n");
	return 0;
    }

    message(MESS_DEBUG, "running preinstall script (if any)\n");
    if (runScript(prefix, h, RPMTAG_PREIN)) {
	return 2;
    }

    if (getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &fileCount)) {

	if (createDirectories(prefix, fileList, fileCount)) {
	    freeHeader(h);
	    free(fileList);
	    return 2;
	}

	/* the file pointer for fd is pointing at the cpio archive */
	if (installArchive(prefix, fd)) {
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
    }

    message(MESS_DEBUG, "running postinstall script (if any)\n");

    if (runScript(prefix, h, RPMTAG_POSTIN)) {
	return 1;
    }

    if (rpmdbAdd(db, h)) {
	freeHeader(h);
	return 2;
    }

    return 0;
}

#define BLOCKSIZE 1024

static int installArchive(char * prefix, int fd) {
    gzFile stream;
    char buf[BLOCKSIZE];
    pid_t child;
    int p[2];
    int bytesRead;
    int status;
    int cpioFailed = 0;
    void * oldhandler;

    /* fd should be a gzipped cpio archive */
    
    stream = gzdopen(fd, "r");
    pipe(p);

    if (access("/bin/cpio",  X_OK))  {
	error(RPMERR_CPIO, "/bin/cpio does not exist");
	return 1;
    }

    oldhandler = signal(SIGPIPE, SIG_IGN);

    child = fork();
    if (!child) {
	close(p[1]);   /* we don't need to write to it */
	close(0);      /* stdin will come from the pipe instead */
	dup2(p[0], 0);
	close(p[0]);

	chdir(prefix);
	execl("/bin/cpio", "/bin/cpio", "--extract", "--verbose",
	      "--unconditional", "--preserve-modification-time",
	      "--make-directories", "--quiet", /*"-t",*/ NULL);
	exit(-1);
    }

    close(p[0]);

    do {
	bytesRead = gzread(stream, buf, sizeof(buf));
	if (write(p[1], buf, bytesRead) != bytesRead) {
	     cpioFailed = 1;
	     kill(SIGTERM, child);
	}
    } while (bytesRead);

    gzclose(stream);
    close(p[1]);
    signal(SIGPIPE, oldhandler);
    waitpid(child, &status, 0);

    if (cpioFailed || !WIFEXITED(status) || WEXITSTATUS(status)) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	error(RPMERR_CPIO, "unpacking of archive failed");
	return 1;
    }

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
	return 1;
    }

    return 0;
}

/* This could be more efficient. A brute force tokenization and mkdir's
   seems like horrible overkill. I did make it know better then trying 
   to create the same direc though. That should make it perform adequatally 
   thanks to the sorted filelist.

   This could create directories that should be symlinks :-( RPM building
   should probably resolve symlinks in paths.

   This creates directories whose permissions are modified by the current
   umask... I'm not sure how this should act */

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
		*chptr = '\0';
		if (mkdirIfNone(buffer, 0755)) {
		    free(lastDirectory);
		    free(buffer);
		    return 1;
		}
		*chptr = '/';
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

    error(RPMERR_MKDIR, "failed to create %s - %s\n", directory, 
	  strerror(errno));

    return errno;
}
