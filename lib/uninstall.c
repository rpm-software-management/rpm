/** \ingroup rpmtrans payload
 * \file lib/uninstall.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "depends.h"	/* XXX for headerMatchesDepFlags */
#include "install.h"
#include "misc.h"	/* XXX for makeTempFile, doputenv */

static char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

#define	SUFFIX_RPMSAVE	".rpmsave"

/**
 * Remove (or rename) file according to file disposition.
 * @param file		file
 * @param flags
 * @param mode		file type
 * @param action	file disposition
 * @return
 */
static int removeFile(const char * file, unsigned int flags, short mode, 
		      enum fileActions action)
{
    int rc = 0;
    char * newfile;
	
    switch (action) {

      case FA_BACKUP:
	newfile = alloca(strlen(file) + sizeof(SUFFIX_RPMSAVE));
	(void)stpcpy(stpcpy(newfile, file), SUFFIX_RPMSAVE);

	if (rename(file, newfile)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s"),
			file, newfile, strerror(errno));
	    rc = 1;
	}
	break;

      case FA_REMOVE:
	if (S_ISDIR(mode)) {
	    /* XXX should permit %missingok for directories as well */
	    if (rmdir(file)) {
		switch (errno) {
		case ENOENT:	/* XXX rmdir("/") linux 2.2.x kernel hack */
		case ENOTEMPTY:
		    rpmError(RPMERR_RMDIR, 
			_("cannot remove %s - directory not empty"), 
			file);
		    break;
		default:
		    rpmError(RPMERR_RMDIR, _("rmdir of %s failed: %s"),
				file, strerror(errno));
		    break;
		}
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
      case FA_SKIPMULTILIB:
	break;
    }
 
    return 0;
}

int removeBinaryPackage(const char * rootdir, rpmdb rpmdb, unsigned int offset,
			Header h,
			int flags, rpmCallbackFunction notify,
			void * notifyData, const void * pkgKey,
			enum fileActions * actions, FD_t scriptFd)
{
    const char * name, * version, * release;
    const char ** baseNames;
    int scriptArg;
    int rc = 0;
    int fileCount;
    int i;

    if (flags & RPMTRANS_FLAG_JUSTDB)
	flags |= RPMTRANS_FLAG_NOSCRIPTS;

    headerNVR(h, &name, &version, &release);

    /*
     * When we run scripts, we pass an argument which is the number of 
     * versions of this package that will be installed when we are finished.
     */
    if ((scriptArg = rpmdbCountPackages(rpmdb, name)) < 0)
	return 1;
    scriptArg -= 1;

    if (!(flags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* run triggers from this package which are keyed on installed 
	   packages */
	if (runImmedTriggers(rootdir, rpmdb, RPMSENSE_TRIGGERUN, h, -1, scriptFd))
	    return 2;

	/* run triggers which are set off by the removal of this package */
	if (runTriggers(rootdir, rpmdb, RPMSENSE_TRIGGERUN, h, -1, scriptFd))
	    return 1;
    }

    if (!(flags & RPMTRANS_FLAG_TEST)) {
	rc = runInstScript(rootdir, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, scriptArg,
		          (flags & RPMTRANS_FLAG_NOSCRIPTS), scriptFd);
	if (rc)
	    return 1;
    }
    
    rpmMessage(RPMMESS_DEBUG, _("will remove files test = %d\n"), 
		flags & RPMTRANS_FLAG_TEST);

    if (!(flags & RPMTRANS_FLAG_JUSTDB) &&
	headerGetEntry(h, RPMTAG_BASENAMES, NULL, (void **) &baseNames, 
	               &fileCount)) {
	const char ** fileMd5List;
	uint_32 * fileFlagsList;
	int_16 * fileModesList;
	const char ** dirNames;
	int_32 * dirIndexes;
	int type;
	char * fileName;
	int fnmaxlen;
	int rdlen = (rootdir && !(rootdir[0] == '/' && rootdir[1] == '\0'))
			? strlen(rootdir) : 0;

	headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes,
	               NULL);
	headerGetEntry(h, RPMTAG_DIRNAMES, NULL, (void **) &dirNames,
	               NULL);

	/* Get buffer for largest possible rootdir + dirname + filename. */
	fnmaxlen = 0;
	for (i = 0; i < fileCount; i++) {
		size_t fnlen;
		fnlen = strlen(baseNames[i]) + 
			strlen(dirNames[dirIndexes[i]]);
		if (fnlen > fnmaxlen)
		    fnmaxlen = fnlen;
	}
	fnmaxlen += rdlen + sizeof("/");	/* XXX one byte too many */

	fileName = alloca(fnmaxlen);

	if (rdlen) {
	    strcpy(fileName, rootdir);
	    (void)rpmCleanPath(fileName);
	    rdlen = strlen(fileName);
	} else
	    *fileName = '\0';

	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5List, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);

	if (notify) {
	    (void)notify(h, RPMCALLBACK_UNINST_START, fileCount, fileCount,
		pkgKey, notifyData);
	}

	/* Traverse filelist backwards to help insure that rmdir() will work. */
	for (i = fileCount - 1; i >= 0; i--) {

	    /* XXX this assumes that dirNames always starts/ends with '/' */
	    (void)stpcpy(stpcpy(fileName+rdlen, dirNames[dirIndexes[i]]), baseNames[i]);

	    rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
			fileName, fileActionString(actions[i]));

	    if (!(flags & RPMTRANS_FLAG_TEST)) {
		if (notify) {
		    (void)notify(h, RPMCALLBACK_UNINST_PROGRESS,
			i, actions[i], fileName, notifyData);
		}
		removeFile(fileName, fileFlagsList[i], fileModesList[i], 
			   actions[i]);
	    }
	}

	if (notify) {
	    (void)notify(h, RPMCALLBACK_UNINST_STOP, 0, fileCount,
			pkgKey, notifyData);
	}

	free(baseNames);
	free(dirNames);
	free(fileMd5List);
    }

    if (!(flags & RPMTRANS_FLAG_TEST)) {
	rpmMessage(RPMMESS_DEBUG, _("running postuninstall script (if any)\n"));
	rc = runInstScript(rootdir, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG,
			scriptArg, (flags & RPMTRANS_FLAG_NOSCRIPTS), scriptFd);
	/* XXX postun failures are not cause for erasure failure. */
    }

    if (!(flags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run postun triggers which are set off by this package's removal. */
	rc = runTriggers(rootdir, rpmdb, RPMSENSE_TRIGGERPOSTUN, h,
			-1, scriptFd);
	if (rc)
	    return 2;
    }

    if (!(flags & RPMTRANS_FLAG_TEST))
	rpmdbRemove(rpmdb, offset);

    return 0;
}

/**
 * @param h		header
 * @param root		path to top of install tree
 * @param progArgc
 * @param progArgv
 * @param script
 * @param arg1
 * @param arg2
 * @param errfd
 * @return
 */
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
	    (void)Fwrite("set -x\n", sizeof(char), 7, fd);

	(void)Fwrite(script, sizeof(script[0]), strlen(script), fd);
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
	    out = Fopen("/dev/null", "w.fdio");
	    if (Ferror(out)) {
		out = fdDup(Fileno(errfd));
	    }
	}
    } else {
	out = fdDup(STDOUT_FILENO);
	out = fdLink(out, "runScript persist");
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
	    if (Fileno(out) > STDERR_FILENO && Fileno(out) != Fileno(errfd)) {
		Fclose (out);
	    }
	    if (Fileno(errfd) > STDERR_FILENO) {
		Fclose (errfd);
	    }
	}

	{   const char *ipath = rpmExpand("PATH=%{_install_script_path}", NULL);
	    const char *path = SCRIPT_PATH;

	    if (ipath && ipath[5] != '%')
		path = ipath;
	    doputenv(path);
	    if (ipath)	xfree(ipath);
	}

	for (i = 0; i < numPrefixes; i++) {
	    sprintf(prefixBuf, "RPM_INSTALL_PREFIX%d=%s", i, prefixes[i]);
	    doputenv(prefixBuf);

	    /* backwards compatibility */
	    if (i == 0) {
		sprintf(prefixBuf, "RPM_INSTALL_PREFIX=%s", prefixes[i]);
		doputenv(prefixBuf);
	    }
	}

	switch(urlIsURL(root)) {
	case URL_IS_PATH:
	    root += sizeof("file://") - 1;
	    root = strchr(root, '/');
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    if (strcmp(root, "/"))
		/*@-unrecog@*/ chroot(root); /*@=unrecog@*/
	    chdir("/");
	    execv(argv[0], (char *const *)argv);
	    break;
	default:
	    break;
	}

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
	const char *n, *v, *r;
	/* XXX FIXME: need to identify what script failed as well. */
	headerNVR(h, &n, &v, &r);
	rpmError(RPMERR_SCRIPT,
		_("execution of %s-%s-%s script failed, exit status %d"),
		n, v, r, WEXITSTATUS(status));
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

/**
 * @param root		path to top of install tree
 * @param db		rpm database
 * @param sense
 * @param sourceH
 * @param triggeredH
 * @param arg1correction
 * @param arg2
 * @param triggersAlreadyRun
 * @param scriptFd
 * @return
 */
static int handleOneTrigger(const char * root, rpmdb db, int sense,
			Header sourceH, Header triggeredH,
			int arg1correction, int arg2,
			char * triggersAlreadyRun, FD_t scriptFd)
{
    const char ** triggerNames;
    const char ** triggerEVR;
    const char ** triggerScripts;
    const char ** triggerProgs;
    int_32 * triggerFlags;
    int_32 * triggerIndices;
    const char * triggerPackageName;
    const char * sourceName;
    int numTriggers;
    int rc = 0;
    int i;
    int skip;

    if (!headerGetEntry(triggeredH, RPMTAG_TRIGGERNAME, NULL, 
			(void **) &triggerNames, &numTriggers)) {
	return 0;
    }

    headerNVR(sourceH, &sourceName, NULL, NULL);

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

	headerNVR(triggeredH, &triggerPackageName, NULL, NULL);

	{   int arg1;
	    int index;

	    if ((arg1 = rpmdbCountPackages(db, triggerPackageName)) < 0) {
		rc = 1;	/* XXX W2DO? same as "execution of script failed" */
	    } else {
		arg1 += arg1correction;
		index = triggerIndices[i];
		if (!triggersAlreadyRun || !triggersAlreadyRun[index]) {
		    rc = runScript(triggeredH, root, 1, triggerProgs + index,
			    triggerScripts[index], 
			    arg1, arg2, scriptFd);
		    if (triggersAlreadyRun) triggersAlreadyRun[index] = 1;
		}
	    }
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
    const char * name;
    int numPackage;
    int rc = 0;

    headerNVR(h, &name, NULL, NULL);

    numPackage = rpmdbCountPackages(db, name);
    if (numPackage < 0)
	return 1;

    {	Header triggeredH;
	rpmdbMatchIterator mi;

	mi = rpmdbInitIterator(db, RPMTAG_TRIGGERNAME, name, 0);
	while((triggeredH = rpmdbNextIterator(mi)) != NULL) {
	    rc |= handleOneTrigger(root, db, sense, h, triggeredH, 0, numPackage, 
			       NULL, scriptFd);
	}

	rpmdbFreeIterator(mi);
    }

    return rc;
}

int runImmedTriggers(const char * root, rpmdb db, int sense, Header h,
		     int countCorrection, FD_t scriptFd)
{
    const char ** triggerNames;
    int numTriggers;
    int_32 * triggerIndices;
    int numTriggerIndices;
    char * triggersRun;
    int rc = 0;

    if (!headerGetEntry(h, RPMTAG_TRIGGERNAME, NULL, (void **) &triggerNames, 
			&numTriggers))
	return 0;
    headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &triggerIndices, 
		   &numTriggerIndices);
    triggersRun = alloca(sizeof(*triggersRun) * numTriggerIndices);
    memset(triggersRun, 0, sizeof(*triggersRun) * numTriggerIndices);

    {	Header sourceH = NULL;
	int i;

	for (i = 0; i < numTriggers; i++) {
	    rpmdbMatchIterator mi;
	    const char * name = triggerNames[i];

	    if (triggersRun[triggerIndices[i]]) continue;
	
	    mi = rpmdbInitIterator(db, RPMTAG_NAME, name, 0);

	    while((sourceH = rpmdbNextIterator(mi)) != NULL) {
		rc |= handleOneTrigger(root, db, sense, sourceH, h, 
				   countCorrection, rpmdbGetIteratorCount(mi),
				   triggersRun, scriptFd);
	    }

	    rpmdbFreeIterator(mi);
	}
    }
    return rc;
}
