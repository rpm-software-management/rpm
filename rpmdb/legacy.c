/**
 * \file rpmdb/legacy.c
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmlib.h>
#include "misc.h"
#include "legacy.h"
#include "debug.h"

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

int domd5(const char * fn, unsigned char * digest, int asAscii)
{
    const char * path;
    unsigned char * md5sum = NULL;
    size_t md5len;
    int rc = 0;

    switch(urlPath(fn, &path)) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
#if HAVE_MMAP
    {	struct stat sb, * st = &sb;
	DIGEST_CTX ctx;
	void * mapped;
	int fdno;
	int xx;

	if (stat(path, st) < 0
	|| (fdno = open(path, O_RDONLY)) < 0)
	    return 1;
	mapped = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, fdno, 0);
	if (mapped == (void *)-1) {
	    xx = close(fdno);
	    return 1;
	}

        xx = madvise(mapped, st->st_size, MADV_SEQUENTIAL);

	ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
	xx = rpmDigestUpdate(ctx, mapped, st->st_size);
	xx = rpmDigestFinal(ctx, (void **)&md5sum, &md5len, asAscii);
	xx = munmap(mapped, st->st_size);
	xx = close(fdno);

    }	break;
#endif
    case URL_IS_FTP:
    case URL_IS_HTTP:
    case URL_IS_DASH:
    default:
    {	FD_t fd;
	unsigned char buf[32*BUFSIZ];

	fd = Fopen(fn, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    if (fd != NULL)
		(void) Fclose(fd);
	    return 1;
	}

	fdInitDigest(fd, PGPHASHALGO_MD5, 0);

	while ((rc = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    {};
	fdFiniDigest(fd, PGPHASHALGO_MD5, (void **)&md5sum, &md5len, asAscii);

	if (Ferror(fd))
	    rc = 1;
	(void) Fclose(fd);
    }	break;
    }

    if (!rc)
	memcpy(digest, md5sum, md5len);
    md5sum = _free(md5sum);

    return rc;
}

/*@-exportheadervar@*/
/*@unchecked@*/
int _noDirTokens = 0;
/*@=exportheadervar@*/

static int dncmp(const void * a, const void * b)
	/*@*/
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}

void compressFilelist(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    HFD_t hfd = headerFreeData;
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    rpmTagType fnt;
    int count;
    int i, xx;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	xx = hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!hge(h, RPMTAG_OLDFILENAMES, &fnt, (void **) &fileNames, &count))
	return;		/* no file list */
    if (fileNames == NULL || count <= 0)
	return;

    dirNames = alloca(sizeof(*dirNames) * count);	/* worst case */
    baseNames = alloca(sizeof(*dirNames) * count);
    dirIndexes = alloca(sizeof(*dirIndexes) * count);

    if (fileNames[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = "";
	for (i = 0; i < count; i++) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = fileNames[i];
	}
	goto exit;
    }

    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	const char ** needle;
	char savechar;
	char * baseName;
	int len;

	if (fileNames[i] == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(fileNames[i], '/') + 1;
	len = baseName - fileNames[i];
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
	if (dirIndex < 0 ||
	    (needle = bsearch(&fileNames[i], dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = alloca(len + 1);
	    memcpy(s, fileNames[i], len + 1);
	    s[len] = '\0';
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;

	*baseName = savechar;
	baseNames[i] = baseName;
    }
    /*@=branchstate@*/

exit:
    if (count > 0) {
	xx = hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	xx = hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	xx = hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    }

    fileNames = hfd(fileNames, fnt);

    xx = hre(h, RPMTAG_OLDFILENAMES);
}

/*
 * This is pretty straight-forward. The only thing that even resembles a trick
 * is getting all of this into a single xmalloc'd block.
 */
static void doBuildFileList(Header h, /*@out@*/ const char *** fileListPtr,
			    /*@out@*/ int * fileCountPtr, rpmTag baseNameTag,
			    rpmTag dirNameTag, rpmTag dirIndexesTag)
	/*@modifies *fileListPtr, *fileCountPtr @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    rpmTagType bnt, dnt;
    char * data;
    int i, xx;

    if (!hge(h, baseNameTag, &bnt, (void **) &baseNames, &count)) {
	if (fileListPtr) *fileListPtr = NULL;
	if (fileCountPtr) *fileCountPtr = 0;
	return;		/* no file list */
    }

    xx = hge(h, dirNameTag, &dnt, (void **) &dirNames, NULL);
    xx = hge(h, dirIndexesTag, NULL, (void **) &dirIndexes, &count);

    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++)
	size += strlen(baseNames[i]) + strlen(dirNames[dirIndexes[i]]) + 1;

    fileNames = xmalloc(size);
    data = ((char *) fileNames) + (sizeof(*fileNames) * count);
    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	fileNames[i] = data;
	data = stpcpy( stpcpy(data, dirNames[dirIndexes[i]]), baseNames[i]);
	*data++ = '\0';
    }
    /*@=branchstate@*/
    baseNames = hfd(baseNames, bnt);
    dirNames = hfd(dirNames, dnt);

    /*@-branchstate@*/
    if (fileListPtr)
	*fileListPtr = fileNames;
    else
	fileNames = _free(fileNames);
    /*@=branchstate@*/
    if (fileCountPtr) *fileCountPtr = count;
}

void expandFilelist(Header h)
{
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    const char ** fileNames = NULL;
    int count = 0;
    int xx;

    /*@-branchstate@*/
    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	doBuildFileList(h, &fileNames, &count, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
	if (fileNames == NULL || count <= 0)
	    return;
	xx = hae(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);
	fileNames = _free(fileNames);
    }
    /*@=branchstate@*/

    xx = hre(h, RPMTAG_DIRNAMES);
    xx = hre(h, RPMTAG_BASENAMES);
    xx = hre(h, RPMTAG_DIRINDEXES);
}


void rpmBuildFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
}

void buildOrigFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_ORIGBASENAMES,
			RPMTAG_ORIGDIRNAMES, RPMTAG_ORIGDIRINDEXES);
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i, xx;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    xx = headerNVR(h, &name, &version, &release);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (void **) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    xx = hge(h, RPMTAG_PROVIDEFLAGS, NULL, (void **) &provideFlags, NULL);

    /*@-nullderef@*/	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }
    /*@=nullderef@*/

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}

void legacyRetrofit(Header h, const struct rpmlead * lead)
{
    const char * prefix;

    /*
     * We don't use these entries (and rpm >= 2 never has) and they are
     * pretty misleading. Let's just get rid of them so they don't confuse
     * anyone.
     */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEGIDS);

    /*
     * We switched the way we do relocateable packages. We fix some of
     * it up here, though the install code still has to be a bit 
     * careful. This fixup makes queries give the new values though,
     * which is quite handy.
     */
    /*@=branchstate@*/
    if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void **) &prefix, NULL))
    {
	const char * nprefix = stripTrailingChar(alloca_strdup(prefix), '/');
	(void) headerAddEntry(h, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
		&nprefix, 1); 
    }
    /*@=branchstate@*/

    /*
     * The file list was moved to a more compressed format which not
     * only saves memory (nice), but gives fingerprinting a nice, fat
     * speed boost (very nice). Go ahead and convert old headers to
     * the new style (this is a noop for new headers).
     */
    if (lead->major < 4)
	compressFilelist(h);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (lead->type == RPMLEAD_SOURCE) {
	int_32 one = 1;
	if (!headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	    (void) headerAddEntry(h, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&one, 1);
    } else if (lead->major < 4) {
	/* Retrofit "Provide: name = EVR" for binary packages. */
	providePackageNVR(h);
    }
}
