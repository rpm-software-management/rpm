/** \ingroup rpmtrans payload
 * \file lib/install.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "cpio.h"
#include "depends.h"
#include "install.h"
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

/**
 * Header file info, gathered per-file, rather than per-tag.
 */
typedef struct fileInfo_s {
/*@dependent@*/ const char * cpioPath;
/*@dependent@*/ const char * dn;		/* relative to root */
/*@dependent@*/ const char * bn;
/*@observer@*/	const char * bnsuffix;
/*@observer@*/	const char * oext;
/*@dependent@*/ const char * md5sum;
    uid_t uid;
    gid_t gid;
    uint_32 flags;
    uint_32 size;
    mode_t mode;
    char state;
    enum fileActions action;
    int install;
} * XFI_t ;

/**
 * Keeps track of memory allocated while accessing header tags.
 */
typedef struct fileMemory_s {
/*@owned@*/ const char ** dnl;
/*@owned@*/ const char ** bnl;
/*@owned@*/ const char ** cpioNames;
/*@owned@*/ const char ** md5sums;
/*@owned@*/ XFI_t files;
} * fileMemory;

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

/**
 * Create memory used to access header.
 * @return		pointer to memory
 */
static /*@only@*/ fileMemory newFileMemory(void)
{
    fileMemory fileMem = xmalloc(sizeof(*fileMem));
    fileMem->files = NULL;
    fileMem->dnl = NULL;
    fileMem->bnl = NULL;
    fileMem->cpioNames = NULL;
    fileMem->md5sums = NULL;
    return fileMem;
}

/**
 * Destroy memory used to access header.
 * @param fileMem	pointer to memory
 */
static void freeFileMemory( /*@only@*/ fileMemory fileMem)
{
    if (fileMem->files) free(fileMem->files);
    if (fileMem->dnl) free(fileMem->dnl);
    if (fileMem->bnl) free(fileMem->bnl);
    if (fileMem->cpioNames) free(fileMem->cpioNames);
    if (fileMem->md5sums) free(fileMem->md5sums);
    free(fileMem);
}

/* files should not be preallocated */
/**
 * Build file information array.
 * @param fi		transaction file info (NULL for source package)
 * @param h		header
 * @retval memPtr	address of allocated memory from header access
 * @retval fileCountPtr	address of install file count
 * @retval files	address of install file information
 * @param stripPrefixLength no. bytes of file prefix to skip
 * @param actions	array of file dispositions
 * @return		0 always
 */
static int assembleFileList(TFI_t fi, Header h,
	/*@out@*/ fileMemory * memPtr,
	/*@out@*/ int * fileCountPtr,
	/*@out@*/ XFI_t * filesPtr,
	int stripPrefixLength)
{
    enum fileActions * actions;
    const char ** dnl, ** bnl, ** fmd5s;
    const int_32 * dil;
    uint_32 * fflags;
    uint_32 * fsizes;
    uint_16 * fmodes;
    fileMemory mem = newFileMemory();
    XFI_t files;
    XFI_t file;
    int fileCount;
    int i;

    *memPtr = mem;

    if (!headerIsEntry(h, RPMTAG_BASENAMES)) return 0;

    if (headerIsEntry(h, RPMTAG_ORIGBASENAMES)) {
	buildOrigFileList(h, &mem->cpioNames, fileCountPtr);
    } else {
	rpmBuildFileList(h, &mem->cpioNames, fileCountPtr);
    }

    fileCount = *fileCountPtr;

    files = *filesPtr = mem->files = xcalloc(fileCount, sizeof(*mem->files));

    if (fi) {
	dnl = fi->dnl;
	dil = fi->dil;
	bnl = fi->bnl;
	fmd5s = fi->fmd5s;
	fflags = fi->fflags;
	fmodes = fi->fmodes;
	fsizes = fi->fsizes;
	actions = fi->actions;
    } else {
	headerGetEntry(h, RPMTAG_DIRNAMES, NULL, (void **) &dnl, NULL);
	mem->dnl = dnl;
	headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, (void **) &dil, NULL);
	mem->bnl = bnl;
	headerGetEntry(h, RPMTAG_BASENAMES, NULL, (void **) &bnl, NULL);
	if (!headerGetEntry(h, RPMTAG_FILEMD5S, NULL, (void **) &fmd5s, NULL))
	    fmd5s = NULL;
	mem->md5sums = fmd5s;
	headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fflags, NULL);
	headerGetEntry(h, RPMTAG_FILEMODES, NULL, (void **) &fmodes, NULL);
	headerGetEntry(h, RPMTAG_FILESIZES, NULL, (void **) &fsizes, NULL);
	actions = NULL;
    }

    for (i = 0, file = files; i < fileCount; i++, file++) {
	file->state = RPMFILE_STATE_NORMAL;
	file->action = (actions ? actions[i] : FA_UNKNOWN);
	file->install = 1;

	file->dn = dnl[dil[i]];
	file->bn = bnl[i];
	file->cpioPath = mem->cpioNames[i] + stripPrefixLength;
	file->md5sum = (fmd5s ? fmd5s[i] : NULL);
	file->mode = fmodes[i];
	file->size = fsizes[i];
	file->flags = fflags[i];

	rpmMessage(RPMMESS_DEBUG, _("   file: %s%s action: %s\n"),
		    file->dn, file->bn, fileActionString(file->action));
    }

    return 0;
}

/**
 * Localize user/group id's.
 * @param h		header
 * @param files		install file information
 * @param fileCount	install file count
 */
static void setFileOwners(Header h, XFI_t files, int fileCount)
{
    const char ** fileOwners;
    const char ** fileGroups;
    XFI_t file;
    int i;

    headerGetEntry(h, RPMTAG_FILEUSERNAME, NULL, (void **) &fileOwners, NULL);
    headerGetEntry(h, RPMTAG_FILEGROUPNAME, NULL, (void **) &fileGroups, NULL);

    for (i = 0, file = files; i < fileCount; i++, file++) {
	if (unameToUid(fileOwners[i], &file->uid)) {
	    rpmMessage(RPMMESS_WARNING,
		_("user %s does not exist - using root\n"), fileOwners[i]);
	    file->uid = 0;
	    file->mode &= ~S_ISUID;	/* turn off the suid bit */
	}

	if (gnameToGid(fileGroups[i], &file->gid)) {
	    rpmMessage(RPMMESS_WARNING,
		_("group %s does not exist - using root\n"), fileGroups[i]);
	    file->gid = 0;
	    file->mode &= ~S_ISGID;	/* turn off the sgid bit */
	}
    }

    free(fileOwners);
    free(fileGroups);
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
    int i, j, k, fileCount;
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
    for (i = 0, fileCount = 0; i < count; i++)
	if (actions[i] != FA_SKIPMULTILIB) {
	    fileCount++;
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
	    newdata = xmalloc(fileCount * sizeof(int_8));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
			((int_8 *) newdata)[k++] = ((int_8 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata,
				       fileCount);
	    free (newdata);
	    break;
	case RPM_INT16_TYPE:
	    newdata = xmalloc(fileCount * sizeof(int_16));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_16 *) newdata)[k++] = ((int_16 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fileCount);
	    free (newdata);
	    break;
	case RPM_INT32_TYPE:
	    newdata = xmalloc(fileCount * sizeof(int_32));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_32 *) newdata)[k++] = ((int_32 *) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fileCount);
	    free (newdata);
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    newdata = xmalloc(fileCount * sizeof(char *));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((char **) newdata)[k++] = ((char **) data)[j];
	    headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fileCount);
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

    dirNames = xcalloc(dirNamesCount + fileCount, sizeof(char *));
    for (i = 0; i < dirNamesCount; i++)
	dirNames[i] = ((char **) data)[i];
    dirCount = dirNamesCount;
    newdata = xmalloc(fileCount * sizeof(int_32));
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
    headerAddOrAppendEntry(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, newdata,
			   fileCount);
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
 * @param fd		file handle of package (positioned at payload)
 * @param files		files to install (NULL means "all files")
 * @param fileCount	no. files to install
 * @param notify	callback function
 * @param notifyData	callback private data
 * @param pkgKey	package private data (e.g. file name)
 * @param h		header
 * @retval specFile	address of spec file name
 * @param archiveSize	@todo Document.
 * @return		0 on success
 */
static int installArchive(FD_t fd, XFI_t files, int fileCount,
			rpmCallbackFunction notify, rpmCallbackData notifyData,
			const void * pkgKey, Header h,
			/*@out@*/ const char ** specFile, int archiveSize)
{
    struct cpioFileMapping * map = NULL;
    int mappedFiles = 0;
    const char * failedFile = NULL;
    cbInfo cbi = alloca(sizeof(*cbi));
    char * rpmio_flags;
    FD_t cfd;
    int saveerrno;
    int rc;

    if (!files) {
	/* install all files */
	fileCount = 0;
    } else if (!fileCount) {
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

    if (files) {
	XFI_t file;
	int i;

	map = alloca(sizeof(*map) * fileCount);
	for (i = 0, mappedFiles = 0, file = files; i < fileCount; i++, file++) {
	    if (!file->install) continue;

	    map[mappedFiles].archivePath = file->cpioPath;
	    (void) urlPath(file->dn, &map[mappedFiles].dirName);
	    map[mappedFiles].baseName = file->bn;

	    /* XXX Can't do src rpm MD5 sum verification (yet). */
    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
	    map[mappedFiles].md5sum = headerIsEntry(h, RPMTAG_SOURCERPM)
			? file->md5sum : NULL;
	    map[mappedFiles].finalMode = file->mode;
	    map[mappedFiles].finalUid = file->uid;
	    map[mappedFiles].finalGid = file->gid;
	    map[mappedFiles].mapFlags = CPIO_MAP_PATH | CPIO_MAP_MODE |
					CPIO_MAP_UID | CPIO_MAP_GID;
	    mappedFiles++;
	}

	qsort(map, mappedFiles, sizeof(*map), cpioFileMapCmp);
    }

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

    (void) Fflush(fd);
    cfd = Fdopen(fdDup(Fileno(fd)), rpmio_flags);
    rc = cpioInstallArchive(cfd, map, mappedFiles,
		    ((notify && archiveSize) || specFile) ? callback : NULL,
		    cbi, &failedFile);
    saveerrno = errno;	/* XXX FIXME: Fclose with libio destroys errno */
    Fclose(cfd);
    headerFree(cbi->h);

    if (rc) {
	/* this would probably be a good place to check if disk space
	   was used up - if so, we should return a different error */
	errno = saveerrno; /* XXX FIXME: Fclose with libio destroys errno */
	rpmError(RPMERR_CPIO, _("unpacking of archive failed%s%s: %s\n"),
		(failedFile != NULL ? _(" on file ") : ""),
		(failedFile != NULL ? failedFile : ""),
		cpioStrerror(rc));
	rc = 1;
    } else if (notify) {
	if (archiveSize)
	    (void)notify(h, RPMCALLBACK_INST_PROGRESS, archiveSize, archiveSize,
		pkgKey, notifyData);
	else
	    (void)notify(h, RPMCALLBACK_INST_PROGRESS, 100, 100,
		pkgKey, notifyData);
	rc = 0;
    }

    if (failedFile)
	free((void *)failedFile);

    return rc;
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
    const char * specFile = NULL;
    int specFileIndex = -1;
    const char * _sourcedir = NULL;
    const char * _specdir = NULL;
    int fileCount = 0;
    uint_32 * archiveSizePtr = NULL;
    fileMemory fileMem = NULL;
    XFI_t files = NULL;
    int i;
    const char * currDir = NULL;
    uid_t currUid = getuid();
    gid_t currGid = getgid();
    struct stat st;
    int rc = 0;

    rpmMessage(RPMMESS_DEBUG, _("installing a source package\n"));

    _sourcedir = rpmGenPath(rootDir, "%{_sourcedir}", "");
    if ((rc = Stat(_sourcedir, &st)) < 0) {
	int ut = urlPath(_sourcedir, NULL);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    if (errno != ENOENT)
		break;
	    /*@fallthrough@*/
	case URL_IS_FTP:
	case URL_IS_HTTP:
	    /* XXX this will only create last component of directory path */
	    rc = Mkdir(_sourcedir, 0755);
	    break;
	case URL_IS_DASH:
	    break;
	}
	if (rc < 0) {
	    rpmError(RPMERR_CREATE, _("cannot create sourcedir %s\n"),
			_sourcedir);
	    rc = 2;
	    goto exit;
	}
    }
    if ((rc = Access(_sourcedir, W_OK))) {
	rpmError(RPMERR_CREATE, _("cannot write to %s\n"), _sourcedir);
	rc = 2;
	goto exit;
    }
    rpmMessage(RPMMESS_DEBUG, _("sources in: %s\n"), _sourcedir);

    _specdir = rpmGenPath(rootDir, "%{_specdir}", "");
    if ((rc = Stat(_specdir, &st)) < 0) {
	int ut = urlPath(_specdir, NULL);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    if (errno != ENOENT)
		break;
	    /*@fallthrough@*/
	case URL_IS_FTP:
	case URL_IS_HTTP:
	    /* XXX this will only create last component of directory path */
	    rc = Mkdir(_specdir, 0755);
	    break;
	case URL_IS_DASH:
	    break;
	}
	if (rc < 0) {
	    rpmError(RPMERR_CREATE, _("cannot create specdir %s\n"), _specdir);
	    rc = 2;
	    goto exit;
	}
    }
    if ((rc = Access(_specdir, W_OK))) {
	rpmError(RPMERR_CREATE, _("cannot write to %s\n"), _specdir);
	rc = 2;
	goto exit;
    }
    rpmMessage(RPMMESS_DEBUG, _("spec file in: %s\n"), _specdir);

    if (h != NULL && headerIsEntry(h, RPMTAG_BASENAMES)) {
	/* we can't remap v1 packages */
	assembleFileList(NULL, h, &fileMem, &fileCount, &files, 0);

	for (i = 0; i < fileCount; i++) {
	    files[i].uid = currUid;
	    files[i].gid = currGid;
	}

	if (headerIsEntry(h, RPMTAG_COOKIE))
	    for (i = 0; i < fileCount; i++)
		if (files[i].flags & RPMFILE_SPECFILE) break;

	if (i == fileCount) {
	    /* find the spec file by name */
	    for (i = 0; i < fileCount; i++) {
		const char * t = files[i].cpioPath;
		t += strlen(files[i].cpioPath) - 5;
		if (!strcmp(t, ".spec")) break;
	    }
	}

	if (i < fileCount) {
	    char *t = alloca(strlen(_specdir) + strlen(files[i].cpioPath) + 5);
	    (void)stpcpy(stpcpy(t, _specdir), "/");
	    rpmCleanPath(t);
	    files[i].dn = t;
	    files[i].bn = files[i].cpioPath;
	    specFileIndex = i;
	} else {
	    rpmError(RPMERR_NOSPEC,
		_("source package contains no .spec file\n"));
	    rc = 2;
	    goto exit;
	}
    }

    if (notify)
	(void)notify(h, RPMCALLBACK_INST_START, 0, 0, NULL, notifyData);

    if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, NULL,
			    (void **) &archiveSizePtr, NULL))
	archiveSizePtr = NULL;

    currDir = currentDirectory();
    Chdir(_sourcedir);
    if (installArchive(fd, fileCount > 0 ? files : NULL,
			  fileCount, notify, notifyData, NULL, h,
			  specFileIndex >= 0 ? NULL : &specFile,
			  archiveSizePtr ? *archiveSizePtr : 0)) {
	rc = 2;
	goto exit;
    }
    Chdir(currDir);

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
	    const char * dn = files[specFileIndex].dn;
	    const char * bn = files[specFileIndex].bn;
	    char * t = xmalloc(strlen(dn) + strlen(bn) + 1);
	    (void)stpcpy( stpcpy(t, dn), bn);
	    *specFilePtr = t;
	}
    }
    rc = 0;

exit:
    if (fileMem)	freeFileMemory(fileMem);
    if (currDir)	free((void *)currDir);
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

int installBinaryPackage(const rpmTransactionSet ts, Header h, TFI_t fi)
{
    rpmtransFlags transFlags = ts->transFlags;
    struct availablePackage * alp = fi->ap;
    int fileCount = 0;
    XFI_t files = NULL;
    Header oldH = NULL;
    int otherOffset = 0;
    int scriptArg;
    fileMemory fileMem = NULL;
    int ec = 2;		/* assume error return */
    int rc;
    int i;

    /* XXX MULTILIB is broken, as packages can and do execute /sbin/ldconfig. */
    if (transFlags & (RPMTRANS_FLAG_JUSTDB | RPMTRANS_FLAG_MULTILIB))
	transFlags |= RPMTRANS_FLAG_NOSCRIPTS;

    rpmMessage(RPMMESS_DEBUG, _("package: %s-%s-%s files test = %d\n"),
		alp->name, alp->version, alp->release,
		transFlags & RPMTRANS_FLAG_TEST);

    if ((scriptArg = rpmdbCountPackages(ts->rpmdb, alp->name)) < 0)
	goto exit;
    scriptArg += 1;

    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, alp->name, 0);
	rpmdbSetIteratorVersion(mi, alp->version);
	rpmdbSetIteratorRelease(mi, alp->release);
	while ((oldH = rpmdbNextIterator(mi))) {
	    otherOffset = rpmdbGetIteratorOffset(mi);
	    oldH = (transFlags & RPMTRANS_FLAG_MULTILIB)
		? headerCopy(oldH) : NULL;
	    break;
	}
	rpmdbFreeIterator(mi);
    }

    if (!(transFlags & RPMTRANS_FLAG_JUSTDB) &&
		headerIsEntry(h, RPMTAG_BASENAMES))
    {
	const char * p;
	int stripSize;

	/*
	 * Old format relocateable packages need the entire default
	 * prefix stripped to form the cpio list, while all other packages
	 * need the leading / stripped.
	 */
	rc = headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void **) &p, NULL);
	stripSize = (rc ? strlen(p) + 1 : 1); 
	if (assembleFileList(fi, h, &fileMem, &fileCount, &files, stripSize))
	    goto exit;
    } else {
	files = NULL;
    }

    if (transFlags & RPMTRANS_FLAG_TEST) {
	rpmMessage(RPMMESS_DEBUG, _("stopping install as we're running --test\n"));
	ec = 0;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("running preinstall script (if any)\n"));

    rc = runInstScript(ts, h, RPMTAG_PREIN, RPMTAG_PREINPROG, scriptArg,
		      transFlags & RPMTRANS_FLAG_NOSCRIPTS);

    if (rc) {
	rpmError(RPMERR_SCRIPT,
		_("skipping %s-%s-%s install, %%pre scriptlet failed rc %d\n"),
		alp->name, alp->version, alp->release, rc);
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

    if (files) {
	XFI_t file;

	setFileOwners(h, files, fileCount);

	for (i = 0, file = files; i < fileCount; i++, file++) {
	    char opath[BUFSIZ];
	    char * npath;
	    char * ext;

	    file->bnsuffix = file->oext = ext = NULL;

	    switch (file->action) {
	    case FA_BACKUP:
		file->oext = ext = ".rpmorig";
		break;

	    case FA_ALTNAME:
		file->bnsuffix = ".rpmnew";
		npath = alloca(strlen(file->bn) + strlen(file->bnsuffix) + 1);
		(void)stpcpy(stpcpy(npath, file->bn), file->bnsuffix);
		rpmMessage(RPMMESS_WARNING, _("%s%s created as %s\n"),
			file->dn, file->bn, npath);
		file->bn = npath;
		break;

	    case FA_SAVE:
		file->oext = ext = ".rpmsave";
		break;

	    case FA_CREATE:
		break;

	    case FA_SKIP:
	    case FA_SKIPMULTILIB:
		file->install = 0;
		break;

	    case FA_SKIPNSTATE:
		file->state = RPMFILE_STATE_NOTINSTALLED;
		file->install = 0;
		break;

	    case FA_SKIPNETSHARED:
		file->state = RPMFILE_STATE_NETSHARED;
		file->install = 0;
		break;

	    case FA_UNKNOWN:
	    case FA_REMOVE:
		file->install = 0;
		break;
	    }

	    if (ext == NULL)
		continue;
	    (void)stpcpy(stpcpy(opath, file->dn), file->bn);
	    if (access(opath, F_OK) != 0)
		continue;
	    npath = alloca(strlen(file->dn) + strlen(file->bn)
			+ strlen(ext) + 1);
	    (void)stpcpy(stpcpy(npath, opath), ext);
	    rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"), opath, npath);

	    if (!rename(opath, npath))
		continue;

	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s\n"),
			opath, npath, strerror(errno));
	    goto exit;
	}

	{   uint_32 archiveSize, * asp;

	    rc = headerGetEntry(h, RPMTAG_ARCHIVESIZE, NULL,
		(void **) &asp, NULL);
	    archiveSize = (rc ? *asp : 0);

	    if (ts->notify) {
		(void)ts->notify(h, RPMCALLBACK_INST_START, 0, 0,
		    alp->key, ts->notifyData);
	    }

	    /* the file pointer for fd is pointing at the cpio archive */
	    rc = installArchive(alp->fd, files, fileCount,
			ts->notify, ts->notifyData, alp->key,
			h, NULL, archiveSize);
	    if (rc)
		goto exit;
	}

	{  char *fstates = alloca(sizeof(*fstates) * fileCount);
	    for (i = 0, file = files; i < fileCount; i++, file++)
		fstates[i] = file->state;
	    headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fstates,
			fileCount);
	}

	if (fileMem) freeFileMemory(fileMem);
	fileMem = NULL;
    } else if (transFlags & RPMTRANS_FLAG_JUSTDB) {
	if (headerGetEntry(h, RPMTAG_BASENAMES, NULL, NULL, &fileCount)) {
	    char * fstates = alloca(sizeof(*fstates) * fileCount);
	    memset(fstates, RPMFILE_STATE_NORMAL, fileCount);
	    headerAddEntry(h, RPMTAG_FILESTATES, RPM_CHAR_TYPE, fstates,
			fileCount);
	}
    }

    {	int_32 installTime = time(NULL);
	headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1);
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

    if (transFlags & RPMTRANS_FLAG_MULTILIB) {
	uint_32 multiLib, * newMultiLib, * p;

	if (headerGetEntry(h, RPMTAG_MULTILIBS, NULL, (void **) &newMultiLib,
			   NULL)
	    && headerGetEntry(oldH, RPMTAG_MULTILIBS, NULL,
			      (void **) &p, NULL)) {
	    multiLib = *p;
	    multiLib |= *newMultiLib;
	    headerModifyEntry(oldH, RPMTAG_MULTILIBS, RPM_INT32_TYPE,
			      &multiLib, 1);
	}
	if (mergeFiles(oldH, h, fi))
	    goto exit;
    }

    if (rpmdbAdd(ts->rpmdb, ts->id, h))
	goto exit;

    rpmMessage(RPMMESS_DEBUG, _("running postinstall scripts (if any)\n"));

    rc = runInstScript(ts, h, RPMTAG_POSTIN, RPMTAG_POSTINPROG, scriptArg,
			(transFlags & RPMTRANS_FLAG_NOSCRIPTS));
    if (rc)
	goto exit;

    if (!(transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run triggers this package sets off */
	if (runTriggers(ts, RPMSENSE_TRIGGERIN, h, 0))
	    goto exit;

	/*
	 * Run triggers in this package which are set off by other packages in
	 * the database.
	 */
	if (runImmedTriggers(ts, RPMSENSE_TRIGGERIN, h, 0))
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
    if (fileMem)
	freeFileMemory(fileMem);
    if (oldH)
	headerFree(oldH);
    return ec;
}
