#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "install.h"
#include "md5.h"
#include "misc.h"
#include "rpmdb.h"

static char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

static int removeFile(const char * file, unsigned int flags, short mode, 
		      enum fileActions action)
{
    int rc = 0;
    char * newfile;
	
    switch (action) {

      case FA_BACKUP:
	newfile = alloca(strlen(file) + 20);
	strcpy(newfile, file);
	strcat(newfile, ".rpmsave");
	if (rename(file, newfile)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
			file, newfile, strerror(errno));
	    rc = 1;
	}
	break;

      case FA_REMOVE:
	if (S_ISDIR(mode)) {
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
	} else {
	    if (unlink(file)) {
		if (errno != ENOENT || !(flags & RPMFILE_MISSINGOK)) {
		    rpmError(RPMERR_UNLINK, 
			      _("removal of %s failed: %s"),
				file, strerror(errno));
		}
		rc = 1;
	    }
	}
	break;
      case FA_UNKNOWN:
      case FA_CREATE:
      case FA_SAVE:
      case FA_ALTNAME:
      case FA_SKIP:
      case FA_SKIPNSTATE:
      case FA_SKIPNETSHARED:
	break;
    }
 
    return 0;
}

int removeBinaryPackage(const char * prefix, rpmdb db, unsigned int offset, 
			int flags, enum fileActions * actions, FD_t scriptFd)
{
    Header h = NULL;
    int i;
    int fileCount;
    const char * name, * version, * release;
    const char ** baseNameList;
    int type;
    int scriptArg;
    int rc = 0;

    if (flags & RPMTRANS_FLAG_JUSTDB)
	flags |= RPMTRANS_FLAG_NOSCRIPTS;

    h = rpmdbGetRecord(db, offset);
    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("cannot read header at %d for uninstall"),
	      offset);
	rc = 1;
	goto exit;
    }

    headerNVR(h, &name, &version, &release);

    /* when we run scripts, we pass an argument which is the number of 
       versions of this package that will be installed when we are finished */
    {	dbiIndexSet matches;
	if (rpmdbFindPackage(db, name, &matches)) {
	    rpmError(RPMERR_DBCORRUPT, _("cannot read packages named %s for uninstall"),
		name);
	    dbiFreeIndexRecord(matches);
	    rc = 1;
	    goto exit;
	}
	scriptArg = dbiIndexSetCount(matches) - 1;
	dbiFreeIndexRecord(matches);
    }

    if (!(flags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* run triggers from this package which are keyed on installed 
	   packages */
	if (runImmedTriggers(prefix, db, RPMSENSE_TRIGGERUN, h, -1, scriptFd)) {
	    rc = 2;
	    goto exit;
	}

	/* run triggers which are set off by the removal of this package */
	if (runTriggers(prefix, db, RPMSENSE_TRIGGERUN, h, -1, scriptFd)) {
	    rc = 1;
	    goto exit;
	}
    }

    if (!(flags & RPMTRANS_FLAG_TEST)) {
	if (runInstScript(prefix, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, scriptArg, 
		          flags & RPMTRANS_FLAG_NOSCRIPTS, scriptFd)) {
	    rc = 1;
	    goto exit;
	}
    }
    
    rpmMessage(RPMMESS_DEBUG, _("will remove files test = %d\n"), 
		flags & RPMTRANS_FLAG_TEST);

    if (!(flags & RPMTRANS_FLAG_JUSTDB) &&
	headerGetEntry(h, RPMTAG_COMPFILELIST, NULL, (void **) &baseNameList, 
	               &fileCount)) {
	const char ** fileMd5List;
	uint_32 * fileFlagsList;
	int_16 * fileModesList;
	const char ** dirNames;
	int_32 * dirIndexes;
	char * fnbuffer = NULL;
	int prefixlen = 0;

	headerGetEntry(h, RPMTAG_COMPFILEDIRS, NULL, (void **) &dirIndexes,
	               NULL);
	headerGetEntry(h, RPMTAG_COMPDIRLIST, NULL, (void **) &dirNames,
	               NULL);

	/* Get alloca buffer for largest possible prefix + filename. */
	if (prefix && prefix[0] != '\0') {
	    int fnbuffersize = 0;
	    size_t fnlen;

	    for (i = 0; i < fileCount; i++) {
		fnlen = strlen(baseNameList[i]) + 
			strlen(dirNames[dirIndexes[i]]);
		if (fnlen > fnbuffersize)
		    fnbuffersize = fnlen;
	    }

	    prefixlen = strlen(prefix);
	    fnbuffersize += prefixlen + sizeof("/");
	    fnbuffer = alloca(fnbuffersize + 1);

	    strcpy(fnbuffer, prefix);
	    if (fnbuffer[prefixlen-1] != '/') {
		fnbuffer[prefixlen++] = '/';
		fnbuffer[prefixlen] = '\0';
	    }
	}

	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5List, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);

	/* Traverse filelist backwards to help insure that rmdir() will work. */
	for (i = fileCount - 1; i >= 0; i--) {
	    const char * dirName;

	    dirName = dirNames[dirIndexes[i]];

	    if (prefixlen) {
		if (*dirName == '/') dirName++;
		strcpy(fnbuffer + prefixlen, dirName);
	    } else {
		strcpy(fnbuffer, dirName);
	    }
	    strcat(fnbuffer, baseNameList[i]);

	    rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
			fnbuffer, fileActionString(actions[i]));

	    if (!(flags & RPMTRANS_FLAG_TEST))
		removeFile(fnbuffer, fileFlagsList[i], fileModesList[i], 
			   actions[i]);
	}

	free(baseNameList);
	free(dirNames);
	free(fileMd5List);
    }

    if (!(flags & RPMTRANS_FLAG_TEST)) {
	rpmMessage(RPMMESS_DEBUG, _("running postuninstall script (if any)\n"));
	runInstScript(prefix, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG, scriptArg, 
		      flags & RPMTRANS_FLAG_NOSCRIPTS, scriptFd);
    }

    if (!(flags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run postun triggers which are set off by this package's removal. */
	if (runTriggers(prefix, db, RPMSENSE_TRIGGERPOSTUN, h, -1, scriptFd)) {
	    rc = 2;
	    goto exit;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("removing database entry\n"));
    if (!(flags & RPMTRANS_FLAG_TEST))
	rpmdbRemove(db, offset, 0);

    rc = 0;

 exit:
    if (h) {
	headerFree(h);
    }
    return rc;
}

static int runScript(Header h, const char * root, int progArgc, const char ** progArgv, 
		     const char * script, int arg1, int arg2, FD_t errfd)
{
    const char ** argv = NULL;
    int argc = 0;
    const char ** prefixes = NULL;
    int numPrefixes;
    const char * oldPrefix;
    int maxPrefixLength;
    int len;
    char * prefixBuf = NULL;
    pid_t child;
    int status;
    const char * fn = NULL;
    int i;
    int freePrefixes = 0;
    FD_t out;

    if (!progArgv && !script)
	return 0;
    else if (!progArgv) {
	argv = alloca(5 * sizeof(char *));
	argv[0] = "/bin/sh";
	argc = 1;
    } else {
	argv = alloca((progArgc + 4) * sizeof(char *));
	memcpy(argv, progArgv, progArgc * sizeof(char *));
	argc = progArgc;
    }

    if (headerGetEntry(h, RPMTAG_INSTPREFIXES, NULL, (void **) &prefixes,
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
	FD_t fd;
	if (makeTempFile(root, &fn, &fd)) {
	    if (freePrefixes) free(prefixes);
	    return 1;
	}

	if (rpmIsDebug() &&
	    (!strcmp(argv[0], "/bin/sh") || !strcmp(argv[0], "/bin/bash")))
	    (void)Fwrite("set -x\n", 7, 1, fd);

	(void)Fwrite(script, strlen(script), 1, fd);
	Fclose(fd);

	argv[argc++] = fn + strlen(root);
	if (arg1 >= 0) {
	    char *av = alloca(20);
	    sprintf(av, "%d", arg1);
	    argv[argc++] = av;
	}
	if (arg2 >= 0) {
	    char *av = alloca(20);
	    sprintf(av, "%d", arg2);
	    argv[argc++] = av;
	}
    }

    argv[argc] = NULL;

    if (errfd != NULL) {
	if (rpmIsVerbose()) {
	    out = fdDup(Fileno(errfd));
	} else {
	    out = fdOpen("/dev/null", O_WRONLY, 0);
	    if (Ferror(out))
		out = fdDup(Fileno(errfd));
	}
    } else {
	out = fdDup(STDOUT_FILENO);
    }
    
    if (!(child = fork())) {
	int pipes[2];

	pipes[0] = pipes[1] = 0;
	/* make stdin inaccessible */
	pipe(pipes);
	close(pipes[1]);
	dup2(pipes[0], STDIN_FILENO);
	close(pipes[0]);

	if (errfd != NULL) {
	    if (Fileno(errfd) != STDERR_FILENO)
		dup2(Fileno(errfd), STDERR_FILENO);
	    if (Fileno(out) != STDOUT_FILENO)
		dup2(Fileno(out), STDOUT_FILENO);
	    /* make sure we don't close stdin/stderr/stdout by mistake! */
	    if (Fileno(out) > STDERR_FILENO && Fileno(out) != Fileno(errfd))
		Fclose (out);
	    if (Fileno(errfd) > STDERR_FILENO)
		Fclose (errfd);
	}

	doputenv(SCRIPT_PATH);

	for (i = 0; i < numPrefixes; i++) {
	    sprintf(prefixBuf, "RPM_INSTALL_PREFIX%d=%s", i, prefixes[i]);
	    doputenv(prefixBuf);

	    /* backwards compatibility */
	    if (i == 0) {
		sprintf(prefixBuf, "RPM_INSTALL_PREFIX=%s", prefixes[i]);
		doputenv(prefixBuf);
	    }
	}

	if (strcmp(root, "/")) {
	    /*@-unrecog@*/ chroot(root); /*@=unrecog@*/
	}

	chdir("/");

	execv(argv[0], (char *const *)argv);
 	_exit(-1);
	/*@notreached@*/
    }

    (void)waitpid(child, &status, 0);

    if (freePrefixes) free(prefixes);

    Fclose(out);	/* XXX dup'd STDOUT_FILENO */
    
    if (script) {
	if (!rpmIsDebug()) unlink(fn);
	xfree(fn);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, _("execution of script failed"));
	return 1;
    }

    return 0;
}

int runInstScript(const char * root, Header h, int scriptTag, int progTag,
	          int arg, int norunScripts, FD_t err)
{
    void ** programArgv;
    int programArgc;
    const char ** argv;
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
	*argv = (const char *) programArgv;
    } else {
	argv = (const char **) programArgv;
    }

    rc = runScript(h, root, programArgc, argv, script, arg, -1, err);
    if (programArgv && programType == RPM_STRING_ARRAY_TYPE) free(programArgv);
    return rc;
}

static int handleOneTrigger(const char * root, rpmdb db, int sense, Header sourceH,
			    Header triggeredH, int arg1correction, int arg2,
			    char * triggersAlreadyRun, FD_t scriptFd)
{
    const char ** triggerNames;
    const char ** triggerEVR;
    const char ** triggerScripts;
    const char ** triggerProgs;
    int_32 * triggerFlags, * triggerIndices;
    char * triggerPackageName, * sourceName;
    int numTriggers;
    int rc = 0;
    int i;
    int index;
    dbiIndexSet matches;
    int skip;

    if (!headerGetEntry(triggeredH, RPMTAG_TRIGGERNAME, NULL, 
			(void **) &triggerNames, &numTriggers)) {
	headerFree(triggeredH);
	return 0;
    }

    headerGetEntry(sourceH, RPMTAG_NAME, NULL, (void **) &sourceName, NULL);

    headerGetEntry(triggeredH, RPMTAG_TRIGGERFLAGS, NULL, 
		   (void **) &triggerFlags, NULL);
    headerGetEntry(triggeredH, RPMTAG_TRIGGERVERSION, NULL, 
		   (void **) &triggerEVR, NULL);

    for (i = 0; i < numTriggers; i++) {
	if (!(triggerFlags[i] & sense)) continue;
	if (strcmp(triggerNames[i], sourceName)) continue;

	/* For some reason, the TRIGGERVERSION stuff includes the name
	   of the package which the trigger is based on. We need to skip
	   over that here. I suspect that we'll change our minds on this
	   and remove that, so I'm going to just 'do the right thing'. */
	skip = strlen(triggerNames[i]);
	if (!strncmp(triggerEVR[i], triggerNames[i], skip) &&
	    (triggerEVR[i][skip] == '-'))
	    skip++;
	else
	    skip = 0;

	if (!headerMatchesDepFlags(sourceH, triggerNames[i],
		triggerEVR[i] + skip, triggerFlags[i]))
	    continue;

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
			    dbiIndexSetCount(matches) + arg1correction, arg2, 
			    scriptFd);
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

int runTriggers(const char * root, rpmdb db, int sense, Header h,
		int countCorrection, FD_t scriptFd)
{
    char * packageName;
    dbiIndexSet matches, otherMatches;
    Header triggeredH;
    int numPackage;
    int rc;
    int i;

    headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &packageName, NULL);

    if ((rc = rpmdbFindByTriggeredBy(db, packageName, &matches)) < 0)
	return 1;
    else if (rc)
	return 0;

    rpmdbFindPackage(db, packageName, &otherMatches);
    numPackage = dbiIndexSetCount(otherMatches) + countCorrection;
    dbiFreeIndexRecord(otherMatches);

    rc = 0;
    for (i = 0; i < dbiIndexSetCount(matches); i++) {
	unsigned int recOffset = dbiIndexRecordOffset(matches, i);
	if ((triggeredH = rpmdbGetRecord(db, recOffset)) == NULL) 
	    return 1;

	rc |= handleOneTrigger(root, db, sense, h, triggeredH, 0, numPackage, 
			       NULL, scriptFd);
	
	headerFree(triggeredH);
    }

    dbiFreeIndexRecord(matches);

    return rc;
    
}

int runImmedTriggers(const char * root, rpmdb db, int sense, Header h,
		     int countCorrection, FD_t scriptFd)
{
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

	for (j = 0; j < dbiIndexSetCount(matches); j++) {
	    unsigned int recOffset = dbiIndexRecordOffset(matches, j);
	    if ((sourceH = rpmdbGetRecord(db, recOffset)) == NULL) 
		return 1;
	    rc |= handleOneTrigger(root, db, sense, sourceH, h, 
				   countCorrection, dbiIndexSetCount(matches), 
				   triggersRun, scriptFd);
	    headerFree(sourceH);
	    if (triggersRun[triggerIndices[i]]) break;
	}

	dbiFreeIndexRecord(matches);
    }

    return rc;
}
