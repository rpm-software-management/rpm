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
 * Private data for cpio callback.
 */
typedef struct callbackInfo_s {
    unsigned long archiveSize;
    rpmCallbackFunction notify;
    const char ** specFilePtr;
    Header h;
    rpmCallbackData notifyData;
    const void * pkgKey;
} * cbInfo;

/* XXX add more tags */
/**
 * Macros to be defined from per-header tag values.
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

/* files should not be preallocated */
/**
 * Build file information array.
 * @param fi		transaction file info (NULL for source package)
 * @param h		header
 * @retval memPtr	address of allocated memory from header access
 * @retval files	address of install file information
 * @param actions	array of file dispositions
 * @return		0 always
 */
static int assembleFileList(TFI_t fi, Header h)
{
    int i;

    if (fi->fc == 0)
	return 0;

    if (headerIsEntry(h, RPMTAG_ORIGBASENAMES)) {
	buildOrigFileList(h, &fi->apath, NULL);
    } else {
	rpmBuildFileList(h, &fi->apath, NULL);
    }

    for (i = 0; i < fi->fc; i++) {
	rpmMessage(RPMMESS_DEBUG, _("   file: %s%s action: %s\n"),
			fi->dnl[fi->dil[i]], fi->bnl[i],
		fileActionString((fi->actions ? fi->actions[i] : FA_UNKNOWN)) );
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
    enum fileActions * actions = fi->actions;
    int i, j, k, fc;
    int_32 type, count, dirNamesCount, dirCount;
    void * data, * newdata;
    int_32 * dirIndexes, * newDirIndexes;
    uint_32 * fileSizes, fileSize;
    char ** dirNames, ** newDirNames;
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
	    free (data);
	    break;
	default:
	    rpmError(RPMERR_DATATYPE, _("Data type %d not supported\n"),
			(int) type);
	    return 1;
	    /*@notreached@*/ break;
	}
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
	char **Names, **EVR, **newNames, **newEVR;
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
 */
static void callback(struct cpioCallbackInfo * cpioInfo, void * data)
{
    cbInfo cbi = data;

    if (cbi->notify)
	(void)cbi->notify(cbi->h, RPMCALLBACK_INST_PROGRESS,
			cpioInfo->bytesProcessed,
			cbi->archiveSize, cbi->pkgKey, cbi->notifyData);

    if (cbi->specFilePtr) {
	const char * t = cpioInfo->file + strlen(cpioInfo->file) - 5;
	if (!strcmp(t, ".spec"))
	    *cbi->specFilePtr = xstrdup(cpioInfo->file);
    }
}

/**
 * Setup payload map and install payload archive.
 *
 * @todo Add endian tag so that srpm MD5 sums can ber verified when installed.
 *
 * @param ts		transaction set
 * @param fi		transaction element file info (NULL means all files)
 * @param fd		file handle of package (positioned at payload)
 * @param notify	callback function
 * @param notifyData	callback private data
 * @param pkgKey	package private data (e.g. file name)
 * @param h		header
 * @retval specFile	address of spec file name
 * @param archiveSize	@todo Document.
 * @return		0 on success
 */
static int installArchive(const rpmTransactionSet ts, TFI_t fi, FD_t fd,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			const void * pkgKey, Header h,
			/*@out@*/ const char ** specFile, int archiveSize)
{
    const char * failedFile = NULL;
    cbInfo cbi = alloca(sizeof(*cbi));
    char * rpmio_flags;
    int saveerrno;
    int rc;

    if (fi == NULL) {
	/* install all files */
    } else if (fi->fc == 0) {
	/* no files to install */
	return 0;
    }

    cbi->archiveSize = archiveSize;	/* arg9 */
    cbi->notify = notify;		/* arg4 */
    cbi->notifyData = notifyData;	/* arg5 */
    cbi->specFilePtr = specFile;	/* arg8 */
    cbi->h = headerLink(h);		/* arg7 */
    cbi->pkgKey = pkgKey;		/* arg6 */

    if (specFile) *specFile = NULL;

    if (notify)
	(void)notify(h, RPMCALLBACK_INST_PROGRESS, 0, archiveSize, pkgKey,
	       notifyData);

    /* Retrieve type of payload compression. */
    {	const char * payload_compressor = NULL;
	char * t;

	if (!headerGetEntry(h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	rpmio_flags = t = alloca(sizeof("r.gzdio"));
	*t++ = 'r';
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
    }

    {	FD_t cfd;
	(void) Fflush(fd);
	cfd = Fdopen(fdDup(Fileno(fd)), rpmio_flags);
	rc = cpioInstallArchive(cfd, fi,
		    ((notify && archiveSize) || specFile) ? callback : NULL,
		    cbi, &failedFile);
	saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	Fclose(cfd);
    }
    headerFree(cbi->h);

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
    } else if (notify) {
	if (archiveSize == 0)
	    archiveSize = 100;
	(void)notify(h, RPMCALLBACK_INST_PROGRESS, archiveSize, archiveSize,
		pkgKey, notifyData);
	rc = 0;
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
 * @param h		header
 * @param rootDir	path to top of install tree
 * @param fd		file handle of package (positioned at payload)
 * @retval specFilePtr	address of spec file name
 * @param notify	callback function
 * @param notifyData	callback private data
 * @return		0 on success, 1 on bad magic, 2 on error
 */
static int installSources(Header h, const char * rootDir, FD_t fd,
			const char ** specFilePtr,
			rpmCallbackFunction notify, rpmCallbackData notifyData)
{
    TFI_t fi = xcalloc(sizeof(*fi), 1);
    const char * specFile = NULL;
    int specFileIndex = -1;
    const char * _sourcedir = rpmGenPath(rootDir, "%{_sourcedir}", "");
    const char * _specdir = rpmGenPath(rootDir, "%{_specdir}", "");
    uint_32 * archiveSizePtr = NULL;
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

    fi->type = TR_ADDED;
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

    assembleFileList(fi, h);

    i = fi->fc;
    if (headerIsEntry(h, RPMTAG_COOKIE))
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

    if (i < fi->fc) {
	char *t = xmalloc(strlen(_specdir) + strlen(fi->apath[i]) + 5);
	(void)stpcpy(stpcpy(t, _specdir), "/");
	fi->dnl[fi->dil[i]] = t;
	fi->bnl[i] = xstrdup(fi->apath[i]);
	specFileIndex = i;
    } else {
	rpmError(RPMERR_NOSPEC, _("source package contains no .spec file\n"));
	rc = 2;
	goto exit;
    }

    if (notify)
	(void)notify(h, RPMCALLBACK_INST_START, 0, 0, NULL, notifyData);

    if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, NULL,
			    (void **) &archiveSizePtr, NULL))
	archiveSizePtr = NULL;

    {	const char * currDir = currentDirectory();
	Chdir(_sourcedir);
	rc = installArchive(NULL, NULL, fd,
			  notify, notifyData, NULL, h,
			  specFileIndex >= 0 ? NULL : &specFile,
			  archiveSizePtr ? *archiveSizePtr : 0);

	Chdir(currDir);
	free((void *)currDir);
	if (rc) {
	    rc = 2;
	    goto exit;
	}
    }

    if (specFileIndex == -1) {
	char * cSpecFile;
	char * iSpecFile;

	if (specFile == NULL) {
	    rpmError(RPMERR_NOSPEC,
		_("source package contains no .spec file\n"));
	    rc = 1;
	    goto exit;
	}

	/*
	 * This logic doesn't work if _specdir and _sourcedir are on
	 * different filesystems, but we only do this on v1 source packages
	 * so I don't really care much.
	 */
	iSpecFile = alloca(strlen(_sourcedir) + strlen(specFile) + 2);
	(void)stpcpy(stpcpy(stpcpy(iSpecFile, _sourcedir), "/"), specFile);

	cSpecFile = alloca(strlen(_specdir) + strlen(specFile) + 2);
	(void)stpcpy(stpcpy(stpcpy(cSpecFile, _specdir), "/"), specFile);

	free((void *)specFile);

	if (strcmp(iSpecFile, cSpecFile)) {
	    rpmMessage(RPMMESS_DEBUG,
		    _("renaming %s to %s\n"), iSpecFile, cSpecFile);
	    if ((rc = Rename(iSpecFile, cSpecFile))) {
		rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s\n"),
			iSpecFile, cSpecFile, strerror(errno));
		rc = 2;
		goto exit;
	    }
	}

	if (specFilePtr)
	    *specFilePtr = xstrdup(cSpecFile);
    } else {
	if (specFilePtr) {
	    const char * dn = fi->dnl[fi->dil[specFileIndex]];
	    const char * bn = fi->bnl[specFileIndex];
	    char * t = xmalloc(strlen(dn) + strlen(bn) + 1);
	    (void)stpcpy( stpcpy(t, dn), bn);
	    *specFilePtr = t;
	}
    }
    rc = 0;

exit:
    if (fi) {
	freeFi(fi);
	free(fi);
    }
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

/*@obserever@*/ const char *const fileActionString(enum fileActions a)
{
    switch (a) {
    case FA_UNKNOWN: return "unknown";
    case FA_CREATE: return "create";
    case FA_BACKUP: return "backup";
    case FA_SAVE: return "save";
    case FA_SKIP: return "skip";
    case FA_ALTNAME: return "altname";
    case FA_REMOVE: return "remove";
    case FA_SKIPNSTATE: return "skipnstate";
    case FA_SKIPNETSHARED: return "skipnetshared";
    case FA_SKIPMULTILIB: return "skipmultilib";
    }
    /*@notreached@*/
    return "???";
}

int rpmInstallSourcePackage(const char * rootDir, FD_t fd,
			const char ** specFile,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			char ** cookie)
{
    int rc, isSource;
    Header h;
    int major, minor;

    rc = rpmReadPackageHeader(fd, &h, &isSource, &major, &minor);
    if (rc) return rc;

    if (!isSource) {
	rpmError(RPMERR_NOTSRPM, _("source package expected, binary found\n"));
	return 2;
    }

    if (cookie) {
	*cookie = NULL;
	if (h != NULL &&
	   headerGetEntry(h, RPMTAG_COOKIE, NULL, (void **) cookie, NULL)) {
	    *cookie = xstrdup(*cookie);
	}
    }

    rpmInstallLoadMacros(h);

    rc = installSources(h, rootDir, fd, specFile, notify, notifyData);
    if (h)
 	headerFree(h);

    return rc;
}

/**
 * @todo Packages w/o files never get a callback, hence don't get displayed
 * on install with -v.
 */
int installBinaryPackage(const rpmTransactionSet ts, TFI_t fi)
{
    static char * stepName = "install";
    struct availablePackage * alp = fi->ap;
    char * fstates = alloca(sizeof(*fstates) * fi->fc);
    Header oldH = NULL;
    int otherOffset = 0;
    int ec = 2;		/* assume error return */
    int rc;
    int i;

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

    memset(fstates, RPMFILE_STATE_NORMAL, fi->fc);

    if (!(ts->transFlags & RPMTRANS_FLAG_JUSTDB) && fi->fc > 0) {
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
	if (assembleFileList(fi, fi->h))
	    goto exit;
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
	/* this loads all of the name services libraries, in case we
	   don't have access to them in the chroot() */
	(void)getpwnam("root");
	endpwent();

	chdir("/");
	/*@-unrecog@*/ chroot(ts->rootDir); /*@=unrecog@*/
	ts->chrootDone = 1;
    }

    if (!(ts->transFlags & RPMTRANS_FLAG_JUSTDB) && fi->fc > 0) {
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

	setFileOwners(fi);

      if (fi->actions) {
	char * opath = alloca(fi->dnlmax + fi->bnlmax + 64);
	char * npath = alloca(fi->dnlmax + fi->bnlmax + 64);

	for (i = 0; i < fi->fc; i++) {
	    char * ext, * t;

	    ext = NULL;

	    switch (fi->actions[i]) {
	    case FA_BACKUP:
		ext = ".rpmorig";
		break;

	    case FA_ALTNAME:
		ext = ".rpmnew";
		t = xmalloc(strlen(fi->bnl[i]) + strlen(ext) + 1);
		(void)stpcpy(stpcpy(t, fi->bnl[i]), ext);
		rpmMessage(RPMMESS_WARNING, _("%s%s created as %s\n"),
			fi->dnl[fi->dil[i]], fi->bnl[i], t);
		fi->bnl[i] = t;		/* XXX memory leak iff i = 0 */
		ext = NULL;
		break;

	    case FA_SAVE:
		ext = ".rpmsave";
		break;

	    case FA_CREATE:
		break;

	    case FA_SKIP:
	    case FA_SKIPMULTILIB:
		break;

	    case FA_SKIPNSTATE:
		fstates[i] = RPMFILE_STATE_NOTINSTALLED;
		break;

	    case FA_SKIPNETSHARED:
		fstates[i] = RPMFILE_STATE_NETSHARED;
		break;

	    case FA_UNKNOWN:
	    case FA_REMOVE:
		break;
	    }

	    if (ext == NULL)
		continue;
	    (void) stpcpy( stpcpy(opath, fi->dnl[fi->dil[i]]), fi->bnl[i]);
	    if (access(opath, F_OK) != 0)
		continue;
	    (void) stpcpy( stpcpy(npath, opath), ext);
	    rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"), opath, npath);

	    if (!rename(opath, npath))
		continue;

	    rpmError(RPMERR_RENAME, _("%s rename of %s to %s failed: %s\n"),
			stepName, opath, npath, strerror(errno));
	    goto exit;
	}
      }

	{   uint_32 archiveSize, * asp;

	    rc = headerGetEntry(fi->h, RPMTAG_ARCHIVESIZE, NULL,
		(void **) &asp, NULL);
	    archiveSize = (rc ? *asp : 0);

	    if (ts->notify) {
		(void)ts->notify(fi->h, RPMCALLBACK_INST_START, 0, 0,
		    alp->key, ts->notifyData);
	    }

	    /* the file pointer for fd is pointing at the cpio archive */
	    rc = installArchive(ts, fi, alp->fd,
			ts->notify, ts->notifyData, alp->key,
			fi->h, NULL, archiveSize);
	    if (rc)
		goto exit;
	}

	headerAddEntry(fi->h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fstates, fi->fc);

    } else if (fi->fc > 0 && ts->transFlags & RPMTRANS_FLAG_JUSTDB) {
	headerAddEntry(fi->h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fstates, fi->fc);
    }

    {	int_32 installTime = time(NULL);
	headerAddEntry(fi->h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1);
    }

    if (ts->rootDir) {
	/*@-unrecog@*/ chroot("."); /*@=unrecog@*/
	ts->chrootDone = 0;
	chdir(ts->currDir);
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
