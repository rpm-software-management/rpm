/**
 * \file lib/legacy.c
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmds.h>

#include "debug.h"

static int dncmp(const void * a, const void * b)
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}

static void compressFilelist(Header h)
{
    struct rpmtd_s fileNames;
    char ** dirNames;
    const char ** baseNames;
    uint32_t * dirIndexes;
    rpm_count_t count;
    int xx, i;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	xx = headerDel(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!headerGet(h, RPMTAG_OLDFILENAMES, &fileNames, HEADERGET_MINMEM)) 
	return;
    count = rpmtdCount(&fileNames);
    if (count < 1) 
	return;

    dirNames = xmalloc(sizeof(*dirNames) * count);	/* worst case */
    baseNames = xmalloc(sizeof(*dirNames) * count);
    dirIndexes = xmalloc(sizeof(*dirIndexes) * count);

    /* HACK. Source RPM, so just do things differently */
    {	const char *fn = rpmtdGetString(&fileNames);
	if (fn && *fn != '/') {
	    dirIndex = 0;
	    dirNames[dirIndex] = xstrdup("");
	    while ((i = rpmtdNext(&fileNames)) >= 0) {
		dirIndexes[i] = dirIndex;
		baseNames[i] = rpmtdGetString(&fileNames);
	    }
	    goto exit;
	}
    }

    while ((i = rpmtdNext(&fileNames)) >= 0) {
	char ** needle;
	char savechar;
	char * baseName;
	size_t len;
	const char *filename = rpmtdGetString(&fileNames);

	if (filename == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(filename, '/') + 1;
	len = baseName - filename;
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
	if (dirIndex < 0 ||
	    (needle = bsearch(&filename, dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = xmalloc(len + 1);
	    rstrlcpy(s, filename, len + 1);
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;

	*baseName = savechar;
	baseNames[i] = baseName;
    }

exit:
    if (count > 0) {
	headerPutUint32(h, RPMTAG_DIRINDEXES, dirIndexes, count);
	headerPutStringArray(h, RPMTAG_BASENAMES, baseNames, count);
	headerPutStringArray(h, RPMTAG_DIRNAMES, 
			     (const char **) dirNames, dirIndex + 1);
    }

    rpmtdFreeData(&fileNames);
    for (i = 0; i <= dirIndex; i++) {
	free(dirNames[i]);
    }
    free(dirNames);
    free(baseNames);
    free(dirIndexes);

    xx = headerDel(h, RPMTAG_OLDFILENAMES);
}

static void expandFilelist(Header h)
{
    struct rpmtd_s filenames;

    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	(void) headerGet(h, RPMTAG_FILENAMES, &filenames, HEADERGET_EXT);
	if (rpmtdCount(&filenames) < 1)
	    return;
	rpmtdSetTag(&filenames, RPMTAG_OLDFILENAMES);
	headerPut(h, &filenames, HEADERPUT_DEFAULT);
	rpmtdFreeData(&filenames);
    }

    (void) headerDel(h, RPMTAG_DIRNAMES);
    (void) headerDel(h, RPMTAG_BASENAMES);
    (void) headerDel(h, RPMTAG_DIRINDEXES);
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
static void providePackageNVR(Header h)
{
    const char *name = headerGetString(h, RPMTAG_NAME);
    char *pEVR = headerGetAsString(h, RPMTAG_EVR);
    rpmsenseFlags pFlags = RPMSENSE_EQUAL;
    int bingo = 1;
    struct rpmtd_s pnames;
    rpmds hds, nvrds;

    /* Generate provides for this package name-version-release. */
    if (!(name && pEVR))
	return;

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!headerGet(h, RPMTAG_PROVIDENAME, &pnames, HEADERGET_MINMEM)) {
	goto exit;
    }

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!headerIsEntry(h, RPMTAG_PROVIDEVERSION)) {
	while (rpmtdNext(&pnames) >= 0) {
	    rpmsenseFlags fdummy = RPMSENSE_ANY;

	    headerPutString(h, RPMTAG_PROVIDEVERSION, "");
	    headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &fdummy, 1);
	}
	goto exit;
    }

    /* see if we already have this provide */
    hds = rpmdsNew(h, RPMTAG_PROVIDENAME, 0);
    nvrds = rpmdsSingle(RPMTAG_PROVIDENAME, name, pEVR, pFlags);
    if (rpmdsFind(hds, nvrds) >= 0) {
	bingo = 0;
    }
    rpmdsFree(hds);
    rpmdsFree(nvrds);
    

exit:
    if (bingo) {
	headerPutString(h, RPMTAG_PROVIDENAME, name);
	headerPutString(h, RPMTAG_PROVIDEVERSION, pEVR);
	headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pFlags, 1);
    }
    rpmtdFreeData(&pnames);
    free(pEVR);
}

static void legacyRetrofit(Header h)
{
    struct rpmtd_s dprefix;

    /*
     * We don't use these entries (and rpm >= 2 never has) and they are
     * pretty misleading. Let's just get rid of them so they don't confuse
     * anyone.
     */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	(void) headerDel(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	(void) headerDel(h, RPMTAG_FILEGIDS);

    /*
     * We switched the way we do relocatable packages. We fix some of
     * it up here, though the install code still has to be a bit 
     * careful. This fixup makes queries give the new values though,
     * which is quite handy.
     */
    if (headerGet(h, RPMTAG_DEFAULTPREFIX, &dprefix, HEADERGET_MINMEM)) {
	const char *prefix = rpmtdGetString(&dprefix);
	char * nprefix = stripTrailingChar(xstrdup(prefix), '/');
	
	headerPutString(h, RPMTAG_PREFIXES, nprefix);
	free(nprefix);
	rpmtdFreeData(&dprefix);
    }

    /*
     * The file list was moved to a more compressed format which not
     * only saves memory (nice), but gives fingerprinting a nice, fat
     * speed boost (very nice). Go ahead and convert old headers to
     * the new style (this is a noop for new headers).
     */
     compressFilelist(h);

    /* Retrofit "Provide: name = EVR" for binary packages. */
    if (!headerIsSource(h)) {
	providePackageNVR(h);
    }
}

int headerConvert(Header h, headerConvOps op)
{
    int rc = 1;

    if (h == NULL)
	return 0;

    switch (op) {
    case HEADERCONV_EXPANDFILELIST:
	expandFilelist(h);
	break;
    case HEADERCONV_COMPRESSFILELIST:
	compressFilelist(h);
	break;
    case HEADERCONV_RETROFIT_V3:
	legacyRetrofit(h);
	break;
    default:
	rc = 0;
	break;
    }
    return rc;
};
