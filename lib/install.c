/** \ingroup rpmtrans payload
 * \file lib/install.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "rollback.h"
#include "misc.h"
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */

/**
 * Macros to be defined from per-header tag values.
 * @todo Should other macros be added from header when installing a package?
 */
static struct tagMacro {
	const char *	macroname;	/*!< Macro name to define. */
	int		tag;		/*!< Header tag to use for value. */
} tagMacros[] = {
	{ "name",	RPMTAG_NAME },
	{ "version",	RPMTAG_VERSION },
	{ "release",	RPMTAG_RELEASE },
#if 0
	{ "epoch",	RPMTAG_EPOCH },
#endif
	{ NULL, 0 }
};

/**
 * Define per-header macros.
 * @param h		header
 * @return		0 always
 */
static int rpmInstallLoadMacros(Header h)
{
    struct tagMacro *tagm;
    union {
	const char * ptr;
	int_32 * i32p;
    } body;
    char numbuf[32];
    int_32 type;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	if (!headerGetEntry(h, tagm->tag, &type, (void **) &body, NULL))
	    continue;
	switch (type) {
	case RPM_INT32_TYPE:
	    sprintf(numbuf, "%d", *body.i32p);
	    addMacro(NULL, tagm->macroname, NULL, numbuf, -1);
	    break;
	case RPM_STRING_TYPE:
	    addMacro(NULL, tagm->macroname, NULL, body.ptr, -1);
	    break;
	}
    }
    return 0;
}

/**
 * Localize user/group id's.
 * @param fi		transaction element file info
 */
static void setFileOwners(TFI_t fi)
{
    uid_t uid;
    gid_t gid;
    int i;

    for (i = 0; i < fi->fc; i++) {
	if (unameToUid(fi->fuser[i], &uid)) {
	    rpmMessage(RPMMESS_WARNING,
		_("user %s does not exist - using root\n"), fi->fuser[i]);
	    uid = 0;
	    /* XXX this diddles header memory. */
	    fi->fmodes[i] &= ~S_ISUID;	/* turn off the suid bit */
	}

	if (gnameToGid(fi->fgroup[i], &gid)) {
	    rpmMessage(RPMMESS_WARNING,
		_("group %s does not exist - using root\n"), fi->fgroup[i]);
	    gid = 0;
	    /* XXX this diddles header memory. */
	    fi->fmodes[i] &= ~S_ISGID;	/* turn off the sgid bit */
	}
	fi->fuids[i] = uid;
	fi->fgids[i] = gid;
    }
}

#ifdef DYING
/**
 * Truncate header changelog tag to configurable limit before installing.
 * @param h		header
 * @return		none
 */
static void trimChangelog(Header h)
{
    int * times;
    char ** names, ** texts;
    long numToKeep = rpmExpandNumeric(
		"%{?_instchangelog:%{_instchagelog}}%{!?_instchangelog:5}");
    char * end;
    int count;

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
#endif	/* DYING */

/**
 * Copy file data from h to newH.
 * @param h		header from
 * @param newH		header to
 * @param actions	array of file dispositions
 * @return		0 on success, 1 on failure
 */
static int mergeFiles(Header h, Header newH, TFI_t fi)
{
    fileAction * actions = fi->actions;
    int i, j, k, fc;
    int_32 type = 0;
    int_32 count = 0;
    int_32 dirNamesCount, dirCount;
    void * data, * newdata;
    int_32 * dirIndexes, * newDirIndexes;
    uint_32 * fileSizes, fileSize;
    const char ** dirNames;
    const char ** newDirNames;
    static int_32 mergeTags[] = {
	RPMTAG_FILESIZES,
	RPMTAG_FILESTATES,
	RPMTAG_FILEMODES,
	RPMTAG_FILERDEVS,
	RPMTAG_FILEMTIMES,
	RPMTAG_FILEMD5S,
	RPMTAG_FILELINKTOS,
	RPMTAG_FILEFLAGS,
	RPMTAG_FILEUSERNAME,
	RPMTAG_FILEGROUPNAME,
	RPMTAG_FILEVERIFYFLAGS,
	RPMTAG_FILEDEVICES,
	RPMTAG_FILEINODES,
	RPMTAG_FILELANGS,
	RPMTAG_BASENAMES,
	0,
    };
    static int_32 requireTags[] = {
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	RPMTAG_PROVIDENAME, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS,
	RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS
    };

    headerGetEntry(h, RPMTAG_SIZE, NULL, (void **) &fileSizes, NULL);
    fileSize = *fileSizes;
    headerGetEntry(newH, RPMTAG_FILESIZES, NULL, (void **) &fileSizes, &count);
    for (i = 0, fc = 0; i < count; i++)
	if (actions[i] != FA_SKIPMULTILIB) {
	    fc++;
	    fileSize += fileSizes[i];
	}
    headerModifyEntry(h, RPMTAG_SIZE, RPM_INT32_TYPE, &fileSize, 1);
    for (i = 0; mergeTags[i]; i++) {
        if (!headerGetEntryMinMemory(newH, mergeTags[i], &type,
				    (const void **) &data, &count))
	    continue;
	switch (type) {
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	    newdata = xmalloc(fc * sizeof(int_8));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
			((int_8 *) newdata)[k++] = ((int_8 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    break;
	case RPM_INT16_TYPE:
	    newdata = xmalloc(fc * sizeof(int_16));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_16 *) newdata)[k++] = ((int_16 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    break;
	case RPM_INT32_TYPE:
	    newdata = xmalloc(fc * sizeof(int_32));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_32 *) newdata)[k++] = ((int_32 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    newdata = xmalloc(fc * sizeof(char *));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((char **) newdata)[k++] = ((char **) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    break;
	default:
	    rpmError(RPMERR_DATATYPE, _("Data type %d not supported\n"),
			(int) type);
	    return 1;
	    /*@notreached@*/ break;
	}
	data = headerFreeData(data, type);
    }
    headerGetEntry(newH, RPMTAG_DIRINDEXES, NULL, (void **) &newDirIndexes,
		   &count);
    headerGetEntryMinMemory(newH, RPMTAG_DIRNAMES, NULL,
			    (const void **) &newDirNames, NULL);
    headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
    headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, (const void **) &data,
			    &dirNamesCount);

    dirNames = xcalloc(dirNamesCount + fc, sizeof(char *));
    for (i = 0; i < dirNamesCount; i++)
	dirNames[i] = ((char **) data)[i];
    dirCount = dirNamesCount;
    newdata = xmalloc(fc * sizeof(int_32));
    for (i = 0, k = 0; i < count; i++) {
	if (actions[i] == FA_SKIPMULTILIB)
	    continue;
	for (j = 0; j < dirCount; j++)
	    if (!strcmp(dirNames[j], newDirNames[newDirIndexes[i]]))
		break;
	if (j == dirCount)
	    dirNames[dirCount++] = newDirNames[newDirIndexes[i]];
	((int_32 *) newdata)[k++] = j;
    }
    headerAddOrAppendEntry(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, newdata, fc);
    if (dirCount > dirNamesCount)
	headerAddOrAppendEntry(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			       dirNames + dirNamesCount,
			       dirCount - dirNamesCount);
    if (data) free (data);
    if (newDirNames) free (newDirNames);
    free (newdata);
    free (dirNames);

    for (i = 0; i < 9; i += 3) {
	const char **Names, **EVR, **newNames, **newEVR;
	uint_32 *Flags, *newFlags;
	int Count = 0, newCount = 0;

	if (!headerGetEntryMinMemory(newH, requireTags[i], NULL,
				    (const void **) &newNames, &newCount))
	    continue;

	headerGetEntryMinMemory(newH, requireTags[i+1], NULL,
				(const void **) &newEVR, NULL);
	headerGetEntry(newH, requireTags[i+2], NULL, (void **) &newFlags, NULL);
	if (headerGetEntryMinMemory(h, requireTags[i], NULL,
				    (const void **) &Names, &Count))
	{
	    headerGetEntryMinMemory(h, requireTags[i+1], NULL,
				    (const void **) &EVR, NULL);
	    headerGetEntry(h, requireTags[i+2], NULL, (void **) &Flags, NULL);
	    for (j = 0; j < newCount; j++)
		for (k = 0; k < Count; k++)
		    if (!strcmp (newNames[j], Names[k])
			&& !strcmp (newEVR[j], EVR[k])
			&& (newFlags[j] & RPMSENSE_SENSEMASK) ==
			   (Flags[k] & RPMSENSE_SENSEMASK))
		    {
			newNames[j] = NULL;
			break;
		    }
	}
	for (j = 0, k = 0; j < newCount; j++) {
	    if (!newNames[j] || !isDependsMULTILIB(newFlags[j]))
		continue;
	    if (j != k) {
		newNames[k] = newNames[j];
		newEVR[k] = newEVR[j];
		newFlags[k] = newFlags[j];
	    }
	    k++;
	}
	if (k) {
	    headerAddOrAppendEntry(h, requireTags[i],
				       RPM_STRING_ARRAY_TYPE, newNames, k);
	    headerAddOrAppendEntry(h, requireTags[i+1],
				       RPM_STRING_ARRAY_TYPE, newEVR, k);
	    headerAddOrAppendEntry(h, requireTags[i+2], RPM_INT32_TYPE,
				       newFlags, k);
	}
    }
    return 0;
}

/**
 * Mark files in database shared with this package as "replaced".
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 always
 */
static int markReplacedFiles(const rpmTransactionSet ts, const TFI_t fi)
{
    rpmdb rpmdb = ts->rpmdb;
    const struct sharedFileInfo * replaced = fi->replaced;
    const struct sharedFileInfo * sfi;
    rpmdbMatchIterator mi;
    Header h;
    unsigned int * offsets;
    unsigned int prev;
    int num;

    if (!(fi->fc > 0 && fi->replaced))
	return 0;

    num = prev = 0;
    for (sfi = replaced; sfi->otherPkg; sfi++) {
	if (prev && prev == sfi->otherPkg)
	    continue;
	prev = sfi->otherPkg;
	num++;
    }
    if (num == 0)
	return 0;

    offsets = alloca(num * sizeof(*offsets));
    num = prev = 0;
    for (sfi = replaced; sfi->otherPkg; sfi++) {
	if (prev && prev == sfi->otherPkg)
	    continue;
	prev = sfi->otherPkg;
	offsets[num++] = sfi->otherPkg;
    }

    mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, NULL, 0);
    rpmdbAppendIterator(mi, offsets, num);

    sfi = replaced;
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * secStates;
	int modified;
	int count;

	modified = 0;

	if (!headerGetEntry(h, RPMTAG_FILESTATES, NULL, (void **)&secStates, &count))
	    continue;
	
	prev = rpmdbGetIteratorOffset(mi);
	num = 0;
	while (sfi->otherPkg && sfi->otherPkg == prev) {
	    assert(sfi->otherFileNum < count);
	    if (secStates[sfi->otherFileNum] != RPMFILE_STATE_REPLACED) {
		secStates[sfi->otherFileNum] = RPMFILE_STATE_REPLACED;
		if (modified == 0) {
		    /* Modified header will be rewritten. */
		    modified = 1;
		    rpmdbSetIteratorModified(mi, modified);
		}
		num++;
	    }
	    sfi++;
	}
    }
    rpmdbFreeIterator(mi);

    return 0;
}

/**
 * Setup payload map and install payload archive.
 *
 * @todo Add endian tag so that srpm MD5 sums can be verified when installed.
 *
 * @param ts		transaction set
 * @param fi		transaction element file info (NULL means all files)
 * @param allFiles	install all files?
 * @return		0 on success
 */
static int installArchive(const rpmTransactionSet ts, TFI_t fi, int allFiles)
{
    struct availablePackage * alp = fi->ap;
    const char * failedFile = NULL;
    char * rpmio_flags;
    int saveerrno;
    int rc;

    if (allFiles) {
	/* install all files */
    } else if (fi->fc == 0) {
	/* no files to install */
	return 0;
    }

    /* Retrieve type of payload compression. */
    {	const char * payload_compressor = NULL;
	char * t;

	if (!headerGetEntry(fi->h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	rpmio_flags = t = alloca(sizeof("r.gzdio"));
	*t++ = 'r';
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
    }

    {
	FD_t cfd;
	(void) Fflush(alp->fd);

	cfd = Fdopen(fdDup(Fileno(alp->fd)), rpmio_flags);
	cfd = fdLink(cfd, "persist (installArchive");

	rc = fsmSetup(fi->fsm, ts, fi, cfd, &failedFile);
	rc = cpioInstallArchive(fi->fsm);
	saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	Fclose(cfd);
	(void) fsmTeardown(fi->fsm);
    }

    if (rc) {
	/*
	 * This would probably be a good place to check if disk space
	 * was used up - if so, we should return a different error.
	 */
	errno = saveerrno; /* XXX FIXME: Fclose with libio destroys errno */
	rpmError(RPMERR_CPIO, _("unpacking of archive failed%s%s: %s\n"),
		(failedFile != NULL ? _(" on file ") : ""),
		(failedFile != NULL ? failedFile : ""),
		cpioStrerror(rc));
	rc = 1;
    } else {
	if (ts && ts->notify) {
	    unsigned int archiveSize = (fi->archiveSize ? fi->archiveSize : 100);
	    (void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS,
			archiveSize, archiveSize,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
    }

    if (failedFile)
	free((void *)failedFile);

    return rc;
}

static int chkdir (const char * dpath, const char * dname)
{
    struct stat st;
    int rc;

    if ((rc = Stat(dpath, &st)) < 0) {
	int ut = urlPath(dpath, NULL);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    if (errno != ENOENT)
		break;
	    /*@fallthrough@*/
	case URL_IS_FTP:
	case URL_IS_HTTP:
	    /* XXX this will only create last component of directory path */
	    rc = Mkdir(dpath, 0755);
	    break;
	case URL_IS_DASH:
	    break;
	}
	if (rc < 0) {
	    rpmError(RPMERR_CREATE, _("cannot create %s %s\n"),
			dname, dpath);
	    return 2;
	}
    }
    if ((rc = Access(dpath, W_OK))) {
	rpmError(RPMERR_CREATE, _("cannot write to %s\n"), dpath);
	return 2;
    }
    return 0;
}

/**
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @retval specFilePtr	address of spec file name
 * @return		0 on success, 1 on bad magic, 2 on error
 */
static int installSources(const rpmTransactionSet ts, TFI_t fi,
			/*@out@*/ const char ** specFilePtr)
{
    const char * _sourcedir = rpmGenPath(ts->rootDir, "%{_sourcedir}", "");
    const char * _specdir = rpmGenPath(ts->rootDir, "%{_specdir}", "");
    const char * specFile = NULL;
    int rc = 0;
    int i;

    rpmMessage(RPMMESS_DEBUG, _("installing a source package\n"));

    rc = chkdir(_sourcedir, "sourcedir");
    if (rc) {
	rc = 2;
	goto exit;
    }

    rc = chkdir(_specdir, "specdir");
    if (rc) {
	rc = 2;
	goto exit;
    }

    i = fi->fc;
    if (headerIsEntry(fi->h, RPMTAG_COOKIE))
	for (i = 0; i < fi->fc; i++)
		if (fi->fflags[i] & RPMFILE_SPECFILE) break;

    if (i == fi->fc) {
	/* find the spec file by name */
	for (i = 0; i < fi->fc; i++) {
	    const char * t = fi->apath[i];
	    t += strlen(fi->apath[i]) - 5;
	    if (!strcmp(t, ".spec")) break;
	}
    }

    /* Build dnl/dil with {_sourcedir, _specdir} as values. */
    if (i < fi->fc) {
	int speclen = strlen(_specdir) + 2;
	int sourcelen = strlen(_sourcedir) + 2;
	char * t;

	if (fi->dnl) {
	    free((void *)fi->dnl); fi->dnl = NULL;
	}

	fi->dc = 2;
	fi->dnl = xmalloc(fi->dc * sizeof(*fi->dnl) + fi->fc * sizeof(*fi->dil) +
			speclen + sourcelen);
	fi->dil = (int *)(fi->dnl + fi->dc);
	memset(fi->dil, 0, fi->fc * sizeof(*fi->dil));
	fi->dil[i] = 1;
	fi->dnl[0] = t = (char *)(fi->dil + fi->fc);
	fi->dnl[1] = t = stpcpy( stpcpy(t, _sourcedir), "/") + 1;
	(void) stpcpy( stpcpy(t, _specdir), "/");

	t = xmalloc(speclen + strlen(fi->bnl[i]) + 1);
	(void) stpcpy( stpcpy( stpcpy(t, _specdir), "/"), fi->bnl[i]);
	specFile = t;
    } else {
	rpmError(RPMERR_NOSPEC, _("source package contains no .spec file\n"));
	rc = 2;
	goto exit;
    }

    rc = installArchive(ts, fi, 1);

    if (rc) {
	rc = 2;
	goto exit;
    }

exit:
    if (rc == 0 && specFile && specFilePtr)
	*specFilePtr = specFile;
    else
	free((void *)specFile);
    if (_specdir)	free((void *)_specdir);
    if (_sourcedir)	free((void *)_sourcedir);
    return rc;
}

int rpmVersionCompare(Header first, Header second)
{
    const char * one, * two;
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

int rpmInstallSourcePackage(const char * rootDir, FD_t fd,
			const char ** specFile,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			char ** cookie)
{
    rpmdb rpmdb = NULL;
    rpmTransactionSet ts = rpmtransCreateSet(rpmdb, rootDir);
    TFI_t fi = xcalloc(sizeof(*fi), 1);
    int isSource;
    Header h;
    int major, minor;
    int rc;
    int i;

    ts->notify = notify;
    ts->notifyData = notifyData;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc)
	goto exit;

    if (!isSource) {
	rpmError(RPMERR_NOTSRPM, _("source package expected, binary found\n"));
	rc = 2;
	goto exit;
    }

    if (cookie) {
	*cookie = NULL;
	if (headerGetEntry(h, RPMTAG_COOKIE, NULL, (void **) cookie, NULL))
	    *cookie = xstrdup(*cookie);
    }

    rc = rpmtransAddPackage(ts, h, fd, NULL, 0, NULL);
    headerFree(h);	/* XXX reference held by transaction set */

    fi->type = TR_ADDED;
    fi->ap = ts->addedPackages.list;
    loadFi(h, fi);
    if (fi->fmd5s) {		/* DYING */
	free((void **)fi->fmd5s); fi->fmd5s = NULL;
    }
    if (fi->fmapflags) {	/* DYING */
	free((void **)fi->fmapflags); fi->fmapflags = NULL;
    }
    fi->uid = getuid();
    fi->gid = getgid();
    fi->striplen = 0;
    fi->mapflags = CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

    fi->fuids = xcalloc(sizeof(*fi->fuids), fi->fc);
    fi->fgids = xcalloc(sizeof(*fi->fgids), fi->fc);
    for (i = 0; i < fi->fc; i++) {
	fi->fuids[i] = fi->uid;
	fi->fgids[i] = fi->gid;
    }

    rpmBuildFileList(fi->h, &fi->apath, NULL);

    rpmInstallLoadMacros(fi->h);

    rc = installSources(ts, fi, specFile);

exit:
    if (fi) {
	freeFi(fi);
	free(fi);
    }
    if (ts)
	rpmtransFree(ts);
    return rc;
}

/**
 * @todo Packages w/o files never get a callback, hence don't get displayed
 * on install with -v.
 */
int installBinaryPackage(const rpmTransactionSet ts, TFI_t fi)
{
/*@observer@*/ static char * stepName = "install";
    Header oldH = NULL;
    int otherOffset = 0;
    int ec = 2;		/* assume error return */
    int rc;

    rpmMessage(RPMMESS_DEBUG, _("%s: %s-%s-%s has %d files, test = %d\n"),
		stepName, fi->name, fi->version, fi->release,
		fi->fc, (ts->transFlags & RPMTRANS_FLAG_TEST));

    /*
     * When we run scripts, we pass an argument which is the number of 
     * versions of this package that will be installed when we are finished.
     */
    fi->scriptArg = rpmdbCountPackages(ts->rpmdb, fi->name) + 1;
    if (fi->scriptArg < 1)
	goto exit;

    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, fi->name, 0);
	rpmdbSetIteratorVersion(mi, fi->version);
	rpmdbSetIteratorRelease(mi, fi->release);
	while ((oldH = rpmdbNextIterator(mi))) {
	    otherOffset = rpmdbGetIteratorOffset(mi);
	    oldH = (ts->transFlags & RPMTRANS_FLAG_MULTILIB)
		? headerCopy(oldH) : NULL;
	    break;
	}
	rpmdbFreeIterator(mi);
    }

    if (fi->fc > 0 && fi->fstates == NULL) {
	fi->fstates = xmalloc(sizeof(*fi->fstates) * fi->fc);
	memset(fi->fstates, RPMFILE_STATE_NORMAL, fi->fc);
    }

    if (fi->fc > 0 && !(ts->transFlags & RPMTRANS_FLAG_JUSTDB)) {
	const char * p;

	/*
	 * Old format relocateable packages need the entire default
	 * prefix stripped to form the cpio list, while all other packages
	 * need the leading / stripped.
	 */
	rc = headerGetEntry(fi->h, RPMTAG_DEFAULTPREFIX, NULL,
				(void **) &p, NULL);
	fi->striplen = (rc ? strlen(p) + 1 : 1); 
	fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

	if (headerIsEntry(fi->h, RPMTAG_ORIGBASENAMES))
	    buildOrigFileList(fi->h, &fi->apath, NULL);
	else
	    rpmBuildFileList(fi->h, &fi->apath, NULL);

	if (fi->fuser == NULL)
	    headerGetEntryMinMemory(fi->h, RPMTAG_FILEUSERNAME, NULL,
				(const void **) &fi->fuser, NULL);
	if (fi->fgroup == NULL)
	    headerGetEntryMinMemory(fi->h, RPMTAG_FILEGROUPNAME, NULL,
				(const void **) &fi->fgroup, NULL);
	if (fi->fuids == NULL)
	    fi->fuids = xcalloc(sizeof(*fi->fuids), fi->fc);
	if (fi->fgids == NULL)
	    fi->fgids = xcalloc(sizeof(*fi->fgids), fi->fc);

    }

    if (ts->transFlags & RPMTRANS_FLAG_TEST) {
	ec = 0;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("%s: running %s script(s) (if any)\n"),
	stepName, "pre-install");

    rc = runInstScript(ts, fi->h, RPMTAG_PREIN, RPMTAG_PREINPROG, fi->scriptArg,
		      ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS);
    if (rc) {
	rpmError(RPMERR_SCRIPT,
		_("skipping %s-%s-%s install, %%pre scriptlet failed rc %d\n"),
		fi->name, fi->version, fi->release, rc);
	goto exit;
    }

    if (ts->rootDir) {
	static int _loaded = 0;

	/*
	 * This loads all of the name services libraries, in case we
	 * don't have access to them in the chroot().
	 */
	if (!_loaded) {
	    (void)getpwnam("root");
	    endpwent();
	    _loaded++;
	}

	chdir("/");
	/*@-unrecog@*/ chroot(ts->rootDir); /*@=unrecog@*/
	ts->chrootDone = 1;
    }

    if (fi->fc > 0 && !(ts->transFlags & RPMTRANS_FLAG_JUSTDB)) {

	setFileOwners(fi);

	rc = pkgActions(ts, fi, FSM_COMMIT);
	if (rc)
	    goto exit;

	rc = installArchive(ts, fi, 0);

	if (rc)
	    goto exit;
    }

    if (ts->rootDir) {
	/*@-unrecog@*/ chroot("."); /*@=unrecog@*/
	ts->chrootDone = 0;
	chdir(ts->currDir);
    }

    if (fi->fc > 0 && fi->fstates) {
	headerAddEntry(fi->h, RPMTAG_FILESTATES, RPM_CHAR_TYPE,
			fi->fstates, fi->fc);
    }

    {	int_32 installTime = time(NULL);
	headerAddEntry(fi->h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE,
			&installTime, 1);
    }

#ifdef	DYING
    trimChangelog(h);
#endif

    /* if this package has already been installed, remove it from the database
       before adding the new one */
    if (otherOffset)
        rpmdbRemove(ts->rpmdb, ts->id, otherOffset);

    if (ts->transFlags & RPMTRANS_FLAG_MULTILIB) {
	uint_32 multiLib, * newMultiLib, * p;

	if (headerGetEntry(fi->h, RPMTAG_MULTILIBS, NULL,
				(void **) &newMultiLib, NULL)
	    && headerGetEntry(oldH, RPMTAG_MULTILIBS, NULL,
			      (void **) &p, NULL)) {
	    multiLib = *p;
	    multiLib |= *newMultiLib;
	    headerModifyEntry(oldH, RPMTAG_MULTILIBS, RPM_INT32_TYPE,
			      &multiLib, 1);
	}
	if (mergeFiles(oldH, fi->h, fi))
	    goto exit;
    }

    if (rpmdbAdd(ts->rpmdb, ts->id, fi->h))
	goto exit;

    rpmMessage(RPMMESS_DEBUG, _("%s: running %s script(s) (if any)\n"),
	stepName, "post-install");

    rc = runInstScript(ts, fi->h, RPMTAG_POSTIN, RPMTAG_POSTINPROG,
		fi->scriptArg, (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS));
    if (rc)
	goto exit;

    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run triggers this package sets off */
	if (runTriggers(ts, RPMSENSE_TRIGGERIN, fi->h, 0))
	    goto exit;

	/*
	 * Run triggers in this package which are set off by other packages in
	 * the database.
	 */
	if (runImmedTriggers(ts, RPMSENSE_TRIGGERIN, fi->h, 0))
	    goto exit;
    }

    markReplacedFiles(ts, fi);

    ec = 0;

exit:
    if (ts->chrootDone) {
	/*@-unrecog@*/ chroot("."); /*@=unrecog@*/
	chdir(ts->currDir);
	ts->chrootDone = 0;
    }
    if (oldH)
	headerFree(oldH);
    return ec;
}
