#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "install.h"
#include "messages.h"
#include "md5.h"
#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

enum fileActions { REMOVE, BACKUP, KEEP };

static int sharedFileCmp(const void * one, const void * two);
static int handleSharedFiles(rpmdb db, int offset, char ** fileList, 
			     char ** fileMd5List, int fileCount, 
			     enum fileActions * fileActions);
static int removeFile(char * file, char state, unsigned int flags, char * md5, 
		      short mode, enum fileActions action, char * rmmess, 
		      int test);

static int sharedFileCmp(const void * one, const void * two) {
    if (((struct sharedFile *) one)->mainFileNumber <
	((struct sharedFile *) one)->mainFileNumber)
	return -1;
    else if (((struct sharedFile *) one)->mainFileNumber ==
	((struct sharedFile *) one)->mainFileNumber)
	return 0;
    else 
	return 1;
}

int findSharedFiles(rpmdb db, int offset, char ** fileList, int fileCount,
		    struct sharedFile ** listPtr, int * listCountPtr) {
    int i, j;
    struct sharedFile * list = NULL;
    int itemsUsed = 0;
    int itemsAllocated = 0;
    dbIndexSet matches;

    itemsAllocated = 5;
    list = malloc(sizeof(struct sharedFile) * itemsAllocated);

    for (i = 0; i < fileCount; i++) {
	if (!rpmdbFindByFile(db, fileList[i], &matches)) {
	    for (j = 0; j < matches.count; j++) {
		if (matches.recs[j].recOffset != offset) {
		    if (itemsUsed == itemsAllocated) {
			itemsAllocated += 10;
			list = realloc(list, sizeof(struct sharedFile) * 
					    itemsAllocated);
		    }
		    list[itemsUsed].mainFileNumber = i;
		    list[itemsUsed].secRecOffset = matches.recs[j].recOffset;
		    list[itemsUsed].secFileNumber = matches.recs[j].fileNumber;
		    itemsUsed++;
		}
	    }
	}
    }

    qsort(list, itemsUsed, sizeof(struct sharedFile), sharedFileCmp);

    *listPtr = list;
    *listCountPtr = itemsUsed;

    return 0;
}

static int handleSharedFiles(rpmdb db, int offset, char ** fileList, 
			     char ** fileMd5List, int fileCount, 
			     enum fileActions * fileActions) {
    Header sech;
    int secOffset = 0;
    struct sharedFile * sharedList;
    int sharedCount;
    char * name, * version, * release;
    int secFileCount;
    char ** secFileMd5List, ** secFileList;
    char * secFileStatesList;
    int type;
    int i;
    int rc = 0;

    if (findSharedFiles(db, offset, fileList, fileCount, &sharedList, 
			&sharedCount)) {
	return 1;
    }

    if (!sharedCount) {
	return 0;
    }

    for (i = 0; i < sharedCount; i++) {
	if (secOffset != sharedList[i].secRecOffset) {
	    if (secOffset) {
		free(secFileMd5List);
		free(secFileList);
	    }

	    secOffset = sharedList[i].secRecOffset;
	    sech = rpmdbGetRecord(db, secOffset);
	    if (!sech) {
		error(RPMERR_DBCORRUPT, "cannot read header at %d for "
		      "uninstall", offset);
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

	    freeHeader(sech);
	}

	message(MESS_DEBUG, "file %s is shared\n",
		fileList[sharedList[i].mainFileNumber]);
	
	switch (secFileStatesList[sharedList[i].secFileNumber]) {
	  case RPMFILE_STATE_REPLACED:
	    message(MESS_DEBUG, "     file has already been replaced\n");
	    break;
    
	  case RPMFILE_STATE_NORMAL:
	    if (!strcmp(fileMd5List[sharedList[i].mainFileNumber],
			secFileMd5List[sharedList[i].secFileNumber])) {
		message(MESS_DEBUG, "    file is truely shared - saving\n");
	    }
	    fileActions[sharedList[i].mainFileNumber] = KEEP;
	    break;
	}
    }

    free(secFileMd5List);
    free(secFileList);
    free(sharedList);

    return rc;
}

int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, int test) {
    Header h;
    int i;
    int fileCount;
    char * rmmess;
    char * fnbuffer = NULL;
    int fnbuffersize = 0;
    int prefixLength = strlen(prefix);
    char ** fileList, ** fileMd5List;
    int type;
    uint_32 * fileFlagsList;
    int_16 * fileModesList;
    char * fileStatesList;
    enum { REMOVE, BACKUP, KEEP } * fileActions;

    h = rpmdbGetRecord(db, offset);
    if (!h) {
	error(RPMERR_DBCORRUPT, "cannot read header at %d for uninstall",
	      offset);
	return 1;
    }

    /* dependency checking should go in here */

    if (test) {
	rmmess = "would remove";
    } else {
	rmmess = "removing";
    }

    message(MESS_DEBUG, "running preuninstall script (if any)\n");
    runScript(prefix, h, RPMTAG_PREUN);
    
    message(MESS_DEBUG, "%s files test = %d\n", rmmess, test);
    if (!getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &fileCount)) {
	puts("(contains no files)");
    } else {
	if (prefix[0]) {
	    fnbuffersize = 1024;
	    fnbuffer = alloca(fnbuffersize);
	}

	fileActions = alloca(sizeof(*fileActions) * fileCount);
	for (i = 0; i < fileCount; i++) fileActions[i] = REMOVE;

	getEntry(h, RPMTAG_FILESTATES, &type, (void **) &fileStatesList, 
		 &fileCount);
	getEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5List, 
		 &fileCount);
	getEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	getEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);

	handleSharedFiles(db, offset, fileList, fileMd5List, fileCount, fileActions);

	for (i = 0; i < fileCount; i++) {
	    if (strcmp(prefix, "/")) {
		if ((strlen(fileList[i]) + prefixLength + 1) > fnbuffersize) {
		    fnbuffersize = (strlen(fileList[i]) + prefixLength) * 2;
		    fnbuffer = alloca(fnbuffersize);
		}
		strcpy(fnbuffer, prefix);
		strcat(fnbuffer, "/");
		strcat(fnbuffer, fileList[i]);
	    } else {
		fnbuffer = fileList[i];
	    }

	    removeFile(fnbuffer, fileStatesList[i], fileFlagsList[i],
		       fileMd5List[i], fileModesList[i], fileActions[i], 
		       rmmess, test);
	}

	free(fileList);
	free(fileMd5List);
    }

    message(MESS_DEBUG, "running postuninstall script (if any)\n");
    runScript(prefix, h, RPMTAG_POSTUN);

    freeHeader(h);

    message(MESS_DEBUG, "%s database entry\n", rmmess);
    if (!test)
	rpmdbRemove(db, offset, 0);

    return 0;
}

int runScript(char * prefix, Header h, int tag) {
    int count, type;
    char * script;
    char * fn;
    int fd;
    int isdebug = isDebug();
    int child;
    int status;

    if (getEntry(h, tag, &type, (void **) &script, &count)) {
	fn = tmpnam(NULL);
	message(MESS_DEBUG, "script found - running from file %s\n", fn);
	fd = open(fn, O_CREAT | O_RDWR);
	unlink(fn);
	if (fd < 0) {
            error(RPMERR_SCRIPT, "error creating file for (un)install script");
	    return 1;
	}
	write(fd, script, strlen(script));
	
	/* run the script via /bin/sh - just feed the commands to the
	   shell as stdin */
	child = fork();
	if (!child) {
	    lseek(fd, 0, SEEK_SET);
	    close(0);
	    dup2(fd, 0);
	    close(fd);

	    if (strcmp(prefix, "/")) {
		message(MESS_DEBUG, "performing chroot(%s)\n", prefix);
		chroot(prefix);
		chdir("/");
	    }

	    if (isdebug)
		execl("/bin/sh", "/bin/sh", "-x", NULL);
	    else
		execl("/bin/sh", "/bin/sh", NULL);
	    exit(-1);
	}
	close(fd);
	waitpid(child, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	    error(RPMERR_SCRIPT, "execution of script failed\n");
	    return 1;
	}
    }

    return 0;
}

static int removeFile(char * file, char state, unsigned int flags, char * md5, 
		      short mode, enum fileActions action, char * rmmess, 
		      int test) {
    char currentMd5[40];
    int rc = 0;
    char * newfile;
	
    switch (state) {
      case RPMFILE_STATE_REPLACED:
	message(MESS_DEBUG, "%s has already been replaced\n", file);
	break;

      case RPMFILE_STATE_NORMAL:
	if ((action == REMOVE) && (flags & RPMFILE_CONFIG)) {
	    /* if it's a config file, we may not want to remove it */
	    message(MESS_DEBUG, "finding md5sum of %s\n", file);
	    if (mdfile(file, currentMd5)) 
		message(MESS_DEBUG, "    failed - assuming file removed\n");
	    else {
		if (strcmp(currentMd5, md5)) {
		    message(MESS_DEBUG, "    file changed - will save\n");
		    action = BACKUP;
		} else {
		    message(MESS_DEBUG, "    file unchanged - will remove\n");
		}
	    }
	}

	switch (action) {

	  case KEEP:
	    message(MESS_DEBUG, "keeping %s\n", file);

	  case BACKUP:
	    message(MESS_DEBUG, "saving %s as %s.rpmsave\n", file, file);
	    if (!test) {
		newfile = alloca(strlen(file) + 20);
		strcpy(newfile, file);
		strcat(newfile, ".rpmsave");
		if (rename(file, newfile)) {
		    error(RPMERR_RENAME, "rename of %s to %s failed: %s",
				file, newfile, strerror(errno));
		    rc = 1;
		}
	    }
	    break;

	  case REMOVE:
	    message(MESS_DEBUG, "%s - %s\n", file, rmmess);
	    if (S_ISDIR(mode)) {
		if (!test) {
		    if (rmdir(file)) {
			if (errno == ENOTEMPTY)
			    error(RPMERR_RMDIR, "cannot remove %s - directory "
				  "not empty", file);
			else
			    error(RPMERR_RMDIR, "rmdir of %s failed: %s",
					file, strerror(errno));
			rc = 1;
		    }
		}
	    } else {
		if (!test) {
		    if (unlink(file)) {
			error(RPMERR_UNLINK, "removal of %s failed: %s",
				    file, strerror(errno));
			rc = 1;
		    }
		}
	    }
	    break;
	}
   }
 
   return 0;
}
