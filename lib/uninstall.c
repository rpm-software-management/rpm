#include "config.h"
#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif 

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbindex.h"
#include "depends.h"
#include "install.h"
#include "intl.h"
#include "messages.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"
#include "rpmlib.h"

static char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:"
			                 "/usr/X11R6/bin";

enum fileActions { REMOVE, BACKUP, KEEP };

static int sharedFileCmp(const void * one, const void * two);
static int handleSharedFiles(rpmdb db, int offset, char ** fileList, 
			     char ** fileMd5List, int fileCount, 
			     enum fileActions * fileActions);
static int removeFile(char * file, char state, unsigned int flags, char * md5, 
		      short mode, enum fileActions action, 
		      int brokenMd5, int test);
static int runScript(Header h, char * root, int progArgc, char ** progArgv, 
		     char * script, int arg1, int arg2, int errfd);

static int sharedFileCmp(const void * one, const void * two) {
    if (((struct sharedFile *) one)->secRecOffset <
	((struct sharedFile *) two)->secRecOffset)
	return -1;
    else if (((struct sharedFile *) one)->secRecOffset ==
	((struct sharedFile *) two)->secRecOffset)
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
    dbiIndexSet matches;

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

	    dbiFreeIndexRecord(matches);
	}
    }

    if (itemsUsed) {
	qsort(list, itemsUsed, sizeof(struct sharedFile), sharedFileCmp);
	*listPtr = list;
	*listCountPtr = itemsUsed;
    } else {
	free(list);
	*listPtr = NULL;
	*listCountPtr = 0;
    }

    return 0;
}

static int handleSharedFiles(rpmdb db, int offset, char ** fileList, 
			     char ** fileMd5List, int fileCount, 
			     enum fileActions * fileActions) {
    Header sech = NULL;
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
		headerFree(sech);
		free(secFileMd5List);
		free(secFileList);
	    }

	    secOffset = sharedList[i].secRecOffset;
	    sech = rpmdbGetRecord(db, secOffset);
	    if (!sech) {
		rpmError(RPMERR_DBCORRUPT, 
			 _("cannot read header at %d for uninstall"), offset);
		rc = 1;
		break;
	    }

	    headerGetEntry(sech, RPMTAG_NAME, &type, (void **) &name, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_VERSION, &type, (void **) &version, 
		     &secFileCount);
	    headerGetEntry(sech, RPMTAG_RELEASE, &type, (void **) &release, 
		     &secFileCount);

	    rpmMessage(RPMMESS_DEBUG, 
			_("package %s-%s-%s contain shared files\n"), 
		    	name, version, release);

	    if (!headerGetEntry(sech, RPMTAG_FILENAMES, &type, 
			  (void **) &secFileList, &secFileCount)) {
		rpmError(RPMERR_DBCORRUPT, "package %s contains no files",
		      name);
		headerFree(sech);
		rc = 1;
		break;
	    }

	    if (!headerGetEntry(sech, RPMTAG_FILESTATES, &type, 
		                (void **) &secFileStatesList, NULL)) {
		/* This shouldn't happen, but some versions of RPM didn't
		   implement --justdb properly, and chose to leave this stuff
		   out. */
		rpmMessage(RPMMESS_DEBUG, 
			    "package is missing FILESTATES\n");
		secFileStatesList = alloca(secFileCount);
		memset(secFileStatesList, RPMFILE_STATE_NOTINSTALLED,
			secFileCount);
	    }

	    headerGetEntry(sech, RPMTAG_FILEMD5S, &type, 
		     (void **) &secFileMd5List, &secFileCount);
	}

	rpmMessage(RPMMESS_DEBUG, "file %s is shared\n",
		fileList[sharedList[i].mainFileNumber]);
	
	switch (secFileStatesList[sharedList[i].secFileNumber]) {
	  case RPMFILE_STATE_REPLACED:
	    rpmMessage(RPMMESS_DEBUG, "     file has already been replaced\n");
	    break;

	  case RPMFILE_STATE_NOTINSTALLED:
	    rpmMessage(RPMMESS_DEBUG, "     file was never installed\n");
	    break;
    
	  case RPMFILE_STATE_NETSHARED:
	    rpmMessage(RPMMESS_DEBUG, "     file is netshared (so don't touch it)\n");
	    fileActions[sharedList[i].mainFileNumber] = KEEP;
	    break;
    
	  case RPMFILE_STATE_NORMAL:
	    if (!strcmp(fileMd5List[sharedList[i].mainFileNumber],
			secFileMd5List[sharedList[i].secFileNumber])) {
		rpmMessage(RPMMESS_DEBUG, "    file is truely shared - saving\n");
	    }
	    fileActions[sharedList[i].mainFileNumber] = KEEP;
	    break;
	}
    }

    if (secOffset) {
	headerFree(sech);
	free(secFileMd5List);
	free(secFileList);
    }
    free(sharedList);

    return rc;
}

int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, int flags) {
    Header h;
    int i, j;
    int fileCount;
    char * name, * version, * release;
    char * fnbuffer = NULL;
    dbiIndexSet matches;
    int fnbuffersize = 0;
    int prefixLength = strlen(prefix);
    char ** fileList, ** fileMd5List;
    int type, count;
    uint_32 * fileFlagsList;
    int_16 * fileModesList;
    char * fileStatesList;
    enum { REMOVE, BACKUP, KEEP } * fileActions;
    int scriptArg;

    if (flags & RPMUNINSTALL_JUSTDB)
	flags |= RPMUNINSTALL_NOSCRIPTS;

    h = rpmdbGetRecord(db, offset);
    if (!h) {
	rpmError(RPMERR_DBCORRUPT, "cannot read header at %d for uninstall",
	      offset);
	return 1;
    }

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name,  &count);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version,  &count);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release,  &count);
    /* when we run scripts, we pass an argument which is the number of 
       versions of this package that will be installed when we are finished */
    if (rpmdbFindPackage(db, name, &matches)) {
	rpmError(RPMERR_DBCORRUPT, "cannot read packages named %s for uninstall",
	      name);
	return 1;
    }
 
    scriptArg = matches.count - 1;
    dbiFreeIndexRecord(matches);

    /* run triggers from this package which are keyed on installed packages */
    if (runImmedTriggers(prefix, db, RPMSENSE_TRIGGERUN, h, -1)) {
	return 2;
    }

    /* run triggers which are set off by the removal of this package */
    if (runTriggers(prefix, db, RPMSENSE_TRIGGERUN, h, -1))
	return 1;

    if (!(flags & RPMUNINSTALL_TEST)) {
	if (runInstScript(prefix, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, scriptArg, 
		          flags & RPMUNINSTALL_NOSCRIPTS, 0)) {
	    headerFree(h);
	    return 1;
	}
    }
    
    rpmMessage(RPMMESS_DEBUG, "will remove files test = %d\n", 
		flags & RPMUNINSTALL_TEST);
    if (!(flags & RPMUNINSTALL_JUSTDB) &&
	headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	               &fileCount)) {
	if (prefix[0]) {
	    fnbuffersize = 1024;
	    fnbuffer = alloca(fnbuffersize);
	}

	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5List, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);

	if (!headerGetEntry(h, RPMTAG_FILESTATES, &type, 
			    (void **) &fileStatesList, NULL)) {
	    /* This shouldn't happen, but some versions of RPM didn't
	       implement --justdb properly, and chose to leave this stuff
	       out. */
	    rpmMessage(RPMMESS_DEBUG, "package is missing FILESTATES\n");
	    fileStatesList = alloca(fileCount);
	    memset(fileStatesList, RPMFILE_STATE_NOTINSTALLED,
		    fileCount);
	}

	fileActions = alloca(sizeof(*fileActions) * fileCount);
	for (i = 0; i < fileCount; i++) 
	    if (fileStatesList[i] == RPMFILE_STATE_NOTINSTALLED ||
		fileStatesList[i] == RPMFILE_STATE_NETSHARED) 
		fileActions[i] = KEEP;
	    else
		fileActions[i] = REMOVE;

	if (rpmGetVar(RPMVAR_NETSHAREDPATH)) {
	    char ** netsharedPaths, ** nsp;

	    netsharedPaths = splitString(rpmGetVar(RPMVAR_NETSHAREDPATH),
			strlen(rpmGetVar(RPMVAR_NETSHAREDPATH)), ':');

	    for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
		j = strlen(*nsp);

		for (i = 0; i < fileCount; i++) 
		if (!strncmp(fileList[i], *nsp, j) &&
		    (fileList[i][j] == '\0' || fileList[i][j] == '/')) {
		    rpmMessage(RPMMESS_DEBUG, "%s has a netshared override\n",
				fileList[i]);
		    fileActions[i] = KEEP;
		}
	    }

	    freeSplitString(netsharedPaths);
	}

	handleSharedFiles(db, offset, fileList, fileMd5List, fileCount, 
			  fileActions);

	/* go through the filelist backwards to help insure that rmdir()
	   will work */
	for (i = fileCount - 1; i >= 0; i--) {
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
		       !headerIsEntry(h, RPMTAG_RPMVERSION),
		       flags & RPMUNINSTALL_TEST);
	}

	free(fileList);
	free(fileMd5List);
    }

    if (!(flags & RPMUNINSTALL_TEST)) {
	rpmMessage(RPMMESS_DEBUG, "running postuninstall script (if any)\n");
	runInstScript(prefix, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG, scriptArg, 
		      flags & RPMUNINSTALL_NOSCRIPTS, 0);
    }

    /* Run postun triggers which are set off by this package's removal */
    if (runTriggers(prefix, db, RPMSENSE_TRIGGERPOSTUN, h, 0)) {
	return 2;
    }

    headerFree(h);

    rpmMessage(RPMMESS_DEBUG, "removing database entry\n");
    if (!(flags & RPMUNINSTALL_TEST))
	rpmdbRemove(db, offset, 0);

    return 0;
}

static int runScript(Header h, char * root, int progArgc, char ** progArgv, 
		     char * script, int arg1, int arg2, int errfd) {
    char ** argv;
    int argc;
    char ** prefixes = NULL;
    int numPrefixes;
    char * oldPrefix;
    int maxPrefixLength;
    int len;
    char * prefixBuf;
    pid_t child;
    int status;
    char * fn;
    int fd;
    int i;
    int freePrefixes = 0;
    int pipes[2];
    int out = 1;

    if (!progArgv && !script)
	return 0;
    else if (!progArgv) {
	argv = alloca(5 * sizeof(char *));
	argv[0] = "/bin/sh";
	argc = 1;
    } else {
	argv = progArgv;
	argv = alloca((progArgc + 4) * sizeof(char *));
	memcpy(argv, progArgv, progArgc * sizeof(char *));
	argc = progArgc;
    }

    if (headerGetEntry(h, RPMTAG_INSTPREFIXES, NULL, (void *) &prefixes,
		       &numPrefixes)) {
	freePrefixes = 1;
    } else if (headerGetEntry(h, RPMTAG_INSTALLPREFIX, NULL, 
			      (void **) &oldPrefix,
	    	       NULL)) {
	prefixes = &oldPrefix;
	numPrefixes = 1;
    } else {
	numPrefixes = 0;
    }

    maxPrefixLength = 0;
    for (i = 0; i < numPrefixes; i++) {
	len = strlen(prefixes[i]);
	if (len > maxPrefixLength) maxPrefixLength = len;
    }
    prefixBuf = alloca(maxPrefixLength + 50);

    if (script) {
	if (makeTempFile(root, &fn, &fd)) {
	    free(argv);
	    if (freePrefixes) free(prefixes);
	    return 1;
	}

	if (rpmIsDebug() &&
	    (!strcmp(argv[0], "/bin/sh") || !strcmp(argv[0], "/bin/bash")))
	    write(fd, "set -x\n", 7);

	write(fd, script, strlen(script));
	close(fd);

	argv[argc++] = fn + strlen(root);
	if (arg1 >= 0) {
	    argv[argc] = alloca(20);
	    sprintf(argv[argc++], "%d", arg1);
	}
	if (arg2 >= 0) {
	    argv[argc] = alloca(20);
	    sprintf(argv[argc++], "%d", arg2);
	}
    }

    argv[argc] = NULL;

    if (errfd) {
	if (rpmIsVerbose()) {
	    out = errfd;
	} else {
	    out = open("/dev/null", O_WRONLY);
	    if (out < 0) {
		out = errfd;
	    }
	}
    }
    
    if (!(child = fork())) {
	/* make stdin inaccessible */
	pipe(pipes);
	close(pipes[1]);
	dup2(pipes[0], 0);
	close(pipes[0]);

	if (errfd) {
	    if (errfd != 2) dup2(errfd, 2);
	    if (out != 1) dup2(out, 1);
	    /* make sure we don't close stdin/stderr/stdout by mistake! */
	    if (errfd > 2) close (errfd);
	    if (out > 2 && out != errfd) close (out); 
	}

	doputenv(SCRIPT_PATH);

	for (i = 0; i < numPrefixes; i++) {
	    sprintf(prefixBuf, "RPM_INSTALL_PREFIX%d=%s", i, prefixes[i]);
	    doputenv(prefixBuf);

	    /* backwards compatibility */
	    if (!i) {
		sprintf(prefixBuf, "RPM_INSTALL_PREFIX=%s", prefixes[i]);
		doputenv(prefixBuf);
	    }
	}

	if (strcmp(root, "/")) {
	    chroot(root);
	}

	chdir("/");

	execv(argv[0], argv);
 	_exit(-1);
    }

    waitpid(child, &status, 0);

    if (freePrefixes) free(prefixes);

    if (errfd) {
	if (out > 2) close(out);
	if (errfd > 2) close(errfd);
    }
    
    if (script) {
	if (!rpmIsDebug()) unlink(fn);
	free(fn);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, _("execution of script failed"));
	return 1;
    }

    return 0;
}

int runInstScript(char * root, Header h, int scriptTag, int progTag,
	          int arg, int norunScripts, int err) {
    void ** programArgv;
    int programArgc;
    char ** argv;
    int programType;
    char * script;
    int rc;

    if (norunScripts) return 0;

    /* headerGetEntry() sets the data pointer to NULL if the entry does
       not exist */
    headerGetEntry(h, progTag, &programType, (void **) &programArgv,
		   &programArgc);
    headerGetEntry(h, scriptTag, NULL, (void **) &script, NULL);

    if (programArgv && programType == RPM_STRING_TYPE) {
	argv = alloca(sizeof(char *));
	*argv = (char *) programArgv;
    } else {
	argv = (char **) programArgv;
    }

    rc = runScript(h, root, programArgc, argv, script, arg, -1, err);
    if (programType == RPM_STRING_ARRAY_TYPE) free(programArgv);
    return rc;
}

static int removeFile(char * file, char state, unsigned int flags, char * md5, 
		      short mode, enum fileActions action, 
		      int brokenMd5, int test) {
    char currentMd5[40];
    int rc = 0;
    char * newfile;
	
    switch (state) {
      case RPMFILE_STATE_REPLACED:
	rpmMessage(RPMMESS_DEBUG, "%s has already been replaced\n", file);
	break;

      case RPMFILE_STATE_NORMAL:
	if ((action == REMOVE) && (flags & RPMFILE_CONFIG)) {
	    /* if it's a config file, we may not want to remove it */
	    rpmMessage(RPMMESS_DEBUG, "finding md5sum of %s\n", file);
	    if (brokenMd5)
		rc = mdfileBroken(file, currentMd5);
	    else
		rc = mdfile(file, currentMd5);

	    if (mdfile(file, currentMd5)) 
		rpmMessage(RPMMESS_DEBUG, 
				"    failed - assuming file removed\n");
	    else {
		if (strcmp(currentMd5, md5)) {
		    rpmMessage(RPMMESS_DEBUG, "    file changed - will save\n");
		    action = BACKUP;
		} else {
		    rpmMessage(RPMMESS_DEBUG, 
				"    file unchanged - will remove\n");
		}
	    }
	}

	switch (action) {

	  case KEEP:
	    rpmMessage(RPMMESS_DEBUG, "keeping %s\n", file);
	    break;

	  case BACKUP:
	    rpmMessage(RPMMESS_DEBUG, "saving %s as %s.rpmsave\n", file, file);
	    if (!test) {
		newfile = alloca(strlen(file) + 20);
		strcpy(newfile, file);
		strcat(newfile, ".rpmsave");
		if (rename(file, newfile)) {
		    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
				file, newfile, strerror(errno));
		    rc = 1;
		}
	    }
	    break;

	  case REMOVE:
	    rpmMessage(RPMMESS_DEBUG, "%s - removing\n", file);
	    if (S_ISDIR(mode)) {
		if (!test) {
		    if (rmdir(file)) {
			if (errno == ENOTEMPTY)
			    rpmError(RPMERR_RMDIR, 
				_("cannot remove %s - directory not empty"), 
				file);
			else
			    rpmError(RPMERR_RMDIR, _("rmdir of %s failed: %s"),
					file, strerror(errno));
			rc = 1;
		    }
		}
	    } else {
		if (!test) {
		    if (unlink(file)) {
			if (errno != ENOENT || !(flags & RPMFILE_MISSINGOK)) {
			    rpmError(RPMERR_UNLINK, 
				      _("removal of %s failed: %s"),
					file, strerror(errno));
			}
			rc = 1;
		    }
		}
	    }
	    break;
	}
    }
 
    return 0;
}

static int handleOneTrigger(char * root, rpmdb db, int sense, Header sourceH,
			    Header triggeredH, int arg1correction, int arg2,
			    char * triggersAlreadyRun) {
    char ** triggerNames, ** triggerVersions;
    char ** triggerScripts, ** triggerProgs;
    int_32 * triggerFlags, * triggerIndices;
    char * triggerPackageName, * sourceName;
    int numTriggers;
    int rc = 0;
    int i;
    int index;
    dbiIndexSet matches;

    if (!headerGetEntry(triggeredH, RPMTAG_TRIGGERNAME, NULL, 
			(void **) &triggerNames, &numTriggers)) {
	headerFree(triggeredH);
	return 0;
    }

    headerGetEntry(sourceH, RPMTAG_NAME, NULL, (void **) &sourceName, NULL);

    headerGetEntry(triggeredH, RPMTAG_TRIGGERFLAGS, NULL, 
		   (void **) &triggerFlags, NULL);
    headerGetEntry(triggeredH, RPMTAG_TRIGGERVERSION, NULL, 
		   (void **) &triggerVersions, NULL);

    for (i = 0; i < numTriggers; i++) {
	if (!(triggerFlags[i] & sense)) continue;
	if (strcmp(triggerNames[i], sourceName)) continue;
	if (!headerMatchesDepFlags(sourceH, triggerVersions[i], 
				   triggerFlags[i])) continue;

	headerGetEntry(triggeredH, RPMTAG_TRIGGERINDEX, NULL,
		       (void **) &triggerIndices, NULL);
	headerGetEntry(triggeredH, RPMTAG_TRIGGERSCRIPTS, NULL,
		       (void **) &triggerScripts, NULL);
	headerGetEntry(triggeredH, RPMTAG_TRIGGERSCRIPTPROG, NULL,
		       (void **) &triggerProgs, NULL);

	headerGetEntry(triggeredH, RPMTAG_NAME, NULL, 
		       (void **) &triggerPackageName, NULL);
	rpmdbFindPackage(db, triggerPackageName, &matches);
	dbiFreeIndexRecord(matches);

	index = triggerIndices[i];
	if (!triggersAlreadyRun || !triggersAlreadyRun[index]) {
	    rc = runScript(triggeredH, root, 1, triggerProgs + index,
			    triggerScripts[index], 
			    matches.count + arg1correction, arg2, 0);
	    if (triggersAlreadyRun) triggersAlreadyRun[index] = 1;
	}

	free(triggerScripts);
	free(triggerProgs);

	/* each target/source header pair can only result in a single
	   script being run */
	break;
    }

    free(triggerNames);

    return rc;
}

int runTriggers(char * root, rpmdb db, int sense, Header h,
		int countCorrection) {
    char * packageName;
    dbiIndexSet matches, otherMatches;
    Header triggeredH;
    int numPackage;
    int rc;
    int i, j;

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &packageName, NULL);

    if ((rc = rpmdbFindByTriggeredBy(db, packageName, &matches)) < 0)
	return 1;
    else if (rc)
	return 0;

    rpmdbFindPackage(db, packageName, &otherMatches);
    numPackage = otherMatches.count + countCorrection;
    dbiFreeIndexRecord(otherMatches);

    rc = 0;
    for (i = 0; i < matches.count; i++) {
	if (!(triggeredH = rpmdbGetRecord(db, matches.recs[i].recOffset))) 
	    return 1;

	rc |= handleOneTrigger(root, db, sense, h, triggeredH, 0, numPackage, 
			       NULL);
	
	headerFree(triggeredH);
    }

    dbiFreeIndexRecord(matches);

    return rc;
    
}

int runImmedTriggers(char * root, rpmdb db, int sense, Header h,
		     int countCorrection) {
    int rc = 0;
    dbiIndexSet matches;
    char ** triggerNames;
    int numTriggers;
    int i, j;
    int_32 * triggerIndices;
    char * triggersRun;
    Header sourceH;

    if (!headerGetEntry(h, RPMTAG_TRIGGERNAME, NULL, (void **) &triggerNames, 
			&numTriggers))
	return 0;
    headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &triggerIndices, 
		   &i);
    triggersRun = alloca(sizeof(*triggersRun) * i);
    memset(triggersRun, 0, sizeof(*triggersRun) * i);

    for (i = 0; i < numTriggers; i++) {
	if (triggersRun[triggerIndices[i]]) continue;
	
        if ((j = rpmdbFindPackage(db, triggerNames[i], &matches))) {
	    if (j < 0) rc |= 1;	
	    continue;
	} 

	for (j = 0; j < matches.count; j++) {
	    if (!(sourceH = rpmdbGetRecord(db, matches.recs[j].recOffset))) 
		return 1;
	    rc |= handleOneTrigger(root, db, sense, sourceH, h, 
				   countCorrection, matches.count, triggersRun);
	    headerFree(sourceH);
	    if (triggersRun[triggerIndices[i]]) break;
	}

	dbiFreeIndexRecord(matches);
    }

    return rc;
}
