/** \ingroup rpmtrans payload
 * \file lib/psm.c
 * Package state machine to handle a package from a transaction set.
 */

#include "system.h"

#include "psm.h"
#include <rpmmacro.h>
#include <rpmurl.h>

#include "rpmlead.h"		/* writeLead proto */
#include "signature.h"		/* signature constants */
#include "legacy.h"		/* XXX buildOrigFileList() */
#include "ugid.h"		/* XXX unameToUid() and gnameToGid() */
#include "misc.h"		/* XXX stripTrailingChar() */
#include "rpmdb.h"		/* XXX for db_chrootDone */
#include "debug.h"

/*@access Header @*/		/* compared with NULL */
/*@access rpmdbMatchIterator @*/ /* compared with NULL */
/*@access FSM_t @*/		/* compared with NULL */
/*@access FD_t @*/		/* compared with NULL */
/*@access rpmdb @*/		/* compared with NULL */

/*@access rpmTransactionSet @*/
/*@access TFI_t @*/
/*@access rpmProblemSet@*/
/*@access rpmProblem@*/
/*@access PSM_t @*/

/*@-redecl -declundef -exportheadervar@*/
/*@unchecked@*/
extern const char * chroot_prefix;
/*@=redecl =declundef =exportheadervar@*/

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

    rc = headerGetEntry(first, RPMTAG_VERSION, NULL, (void **) &one, NULL);
    rc = headerGetEntry(second, RPMTAG_VERSION, NULL, (void **) &two, NULL);

    rc = rpmvercmp(one, two);
    if (rc)
	return rc;

    rc = headerGetEntry(first, RPMTAG_RELEASE, NULL, (void **) &one, NULL);
    rc = headerGetEntry(second, RPMTAG_RELEASE, NULL, (void **) &two, NULL);

    return rpmvercmp(one, two);
}

rpmProblemSet psCreate(void)
{
    rpmProblemSet probs;

    probs = xcalloc(1, sizeof(*probs));	/* XXX memory leak */
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

void psAppend(rpmProblemSet probs, rpmProblemType type,
		const struct availablePackage * alp,
		const char * dn, const char * bn,
		Header altH, unsigned long ulong1)
{
    rpmProblem p;
    char *t;

    if (probs->numProblems == probs->numProblemsAlloced) {
	if (probs->numProblemsAlloced)
	    probs->numProblemsAlloced *= 2;
	else
	    probs->numProblemsAlloced = 2;
	probs->probs = xrealloc(probs->probs,
			probs->numProblemsAlloced * sizeof(*probs->probs));
    }

    p = probs->probs + probs->numProblems;
    probs->numProblems++;
    memset(p, 0, sizeof(*p));
    p->type = type;
    /*@-assignexpose@*/
    p->key = alp->key;
    /*@=assignexpose@*/
    p->ulong1 = ulong1;
    p->ignoreProblem = 0;
    p->str1 = NULL;
    p->h = NULL;
    p->pkgNEVR = NULL;
    p->altNEVR = NULL;

    if (dn || bn) {
	p->str1 =
	    t = xcalloc(1, (dn ? strlen(dn) : 0) + (bn ? strlen(bn) : 0) + 1);
	if (dn) t = stpcpy(t, dn);
	if (bn) t = stpcpy(t, bn);
    }

    if (alp) {
	p->h = headerLink(alp->h);
	p->pkgNEVR =
	    t = xcalloc(1, strlen(alp->name) +
			   strlen(alp->version) +
			   strlen(alp->release) + sizeof("--"));
	t = stpcpy(t, alp->name);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->version);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->release);
    }

    if (altH) {
	const char * n, * v, * r;
	(void) headerNVR(altH, &n, &v, &r);
	p->altNEVR =
	    t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
	t = stpcpy(t, n);
	t = stpcpy(t, "-");
	t = stpcpy(t, v);
	t = stpcpy(t, "-");
	t = stpcpy(t, r);
    }
}
/**
 */
static /*@observer@*/ const char *const ftstring (fileTypes ft)
	/*@*/
{
    switch (ft) {
    case XDIR:	return "directory";
    case CDEV:	return "char dev";
    case BDEV:	return "block dev";
    case LINK:	return "link";
    case SOCK:	return "sock";
    case PIPE:	return "fifo/pipe";
    case REG:	return "file";
    default:	return "unknown file type";
    }
    /*@notreached@*/
}

fileTypes whatis(uint_16 mode)
{
    if (S_ISDIR(mode))	return XDIR;
    if (S_ISCHR(mode))	return CDEV;
    if (S_ISBLK(mode))	return BDEV;
    if (S_ISLNK(mode))	return LINK;
    if (S_ISSOCK(mode))	return SOCK;
    if (S_ISFIFO(mode))	return PIPE;
    return REG;
}

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

Header relocateFileList(const rpmTransactionSet ts, TFI_t fi,
		struct availablePackage * alp,
		Header origH, fileAction * actions)
{
    HGE_t hge = fi->hge;
    HAE_t hae = fi->hae;
    HME_t hme = fi->hme;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    static int _printed = 0;
    int allowBadRelocate = (ts->ignoreSet & RPMPROB_FILTER_FORCERELOCATE);
    rpmRelocation * rawRelocations = alp->relocs;
    rpmRelocation * relocations = NULL;
    int numRelocations;
    const char ** validRelocations;
    rpmTagType validType;
    int numValid;
    const char ** baseNames;
    const char ** dirNames;
    int_32 * dirIndexes;
    int_32 * newDirIndexes;
    int_32 fileCount;
    int_32 dirCount;
    uint_32 * fFlags = NULL;
    uint_16 * fModes = NULL;
    char * skipDirList;
    Header h;
    int nrelocated = 0;
    int fileAlloced = 0;
    char * fn = NULL;
    int haveRelocatedFile = 0;
    int reldel = 0;
    int len;
    int i, j, xx;

    if (!hge(origH, RPMTAG_PREFIXES, &validType,
			(void **) &validRelocations, &numValid))
	numValid = 0;

    numRelocations = 0;
    if (rawRelocations)
	while (rawRelocations[numRelocations].newPath ||
	       rawRelocations[numRelocations].oldPath)
	    numRelocations++;

    /*
     * If no relocations are specified (usually the case), then return the
     * original header. If there are prefixes, however, then INSTPREFIXES
     * should be added, but, since relocateFileList() can be called more
     * than once for the same header, don't bother if already present.
     */
    if (rawRelocations == NULL || numRelocations == 0) {
	if (numValid) {
	    if (!headerIsEntry(origH, RPMTAG_INSTPREFIXES))
		xx = hae(origH, RPMTAG_INSTPREFIXES,
			validType, validRelocations, numValid);
	    validRelocations = hfd(validRelocations, validType);
	}
	/* XXX FIXME multilib file actions need to be checked. */
	return headerLink(origH);
    }

    h = headerLink(origH);

    relocations = alloca(sizeof(*relocations) * numRelocations);

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < numRelocations; i++) {
	char * t;

	/*
	 * Default relocations (oldPath == NULL) are handled in the UI,
	 * not rpmlib.
	 */
	if (rawRelocations[i].oldPath == NULL) continue; /* XXX can't happen */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	t = alloca_strdup(rawRelocations[i].oldPath);
	/*@-branchstate@*/
	relocations[i].oldPath = (t[0] == '/' && t[1] == '\0')
	    ? t
	    : stripTrailingChar(t, '/');
	/*@=branchstate@*/

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (rawRelocations[i].newPath) {
	    int del;

	    t = alloca_strdup(rawRelocations[i].newPath);
	    /*@-branchstate@*/
	    relocations[i].newPath = (t[0] == '/' && t[1] == '\0')
		? t
		: stripTrailingChar(t, '/');
	    /*@=branchstate@*/

	    /*@-nullpass@*/	/* FIX:  relocations[i].oldPath == NULL */
	    /* Verify that the relocation's old path is in the header. */
	    for (j = 0; j < numValid; j++)
		if (!strcmp(validRelocations[j], relocations[i].oldPath))
		    /*@innerbreak@*/ break;
	    /* XXX actions check prevents problem from being appended twice. */
	    if (j == numValid && !allowBadRelocate && actions)
		psAppend(ts->probs, RPMPROB_BADRELOCATE, alp,
			 relocations[i].oldPath, NULL, NULL, 0);
	    del =
		strlen(relocations[i].newPath) - strlen(relocations[i].oldPath);
	    /*@=nullpass@*/

	    if (del > reldel)
		reldel = del;
	} else {
	    relocations[i].newPath = NULL;
	}
    }

    /* stupid bubble sort, but it's probably faster here */
    for (i = 0; i < numRelocations; i++) {
	int madeSwap;
	madeSwap = 0;
	for (j = 1; j < numRelocations; j++) {
	    rpmRelocation tmpReloc;
	    if (relocations[j - 1].oldPath == NULL || /* XXX can't happen */
		relocations[j    ].oldPath == NULL || /* XXX can't happen */
	strcmp(relocations[j - 1].oldPath, relocations[j].oldPath) <= 0)
		/*@innercontinue@*/ continue;
	    /*@-usereleased@*/ /* LCL: ??? */
	    tmpReloc = relocations[j - 1];
	    relocations[j - 1] = relocations[j];
	    relocations[j] = tmpReloc;
	    /*@=usereleased@*/
	    madeSwap = 1;
	}
	if (!madeSwap) break;
    }

    if (!_printed) {
	_printed = 1;
	rpmMessage(RPMMESS_DEBUG, _("========== relocations\n"));
	for (i = 0; i < numRelocations; i++) {
	    if (relocations[i].oldPath == NULL) continue; /* XXX can't happen */
	    if (relocations[i].newPath == NULL)
		rpmMessage(RPMMESS_DEBUG, _("%5d exclude  %s\n"),
			i, relocations[i].oldPath);
	    else
		rpmMessage(RPMMESS_DEBUG, _("%5d relocate %s -> %s\n"),
			i, relocations[i].oldPath, relocations[i].newPath);
	}
    }

    /* Add relocation values to the header */
    if (numValid) {
	const char ** actualRelocations;
	int numActual;

	actualRelocations = xmalloc(numValid * sizeof(*actualRelocations));
	numActual = 0;
	for (i = 0; i < numValid; i++) {
	    for (j = 0; j < numRelocations; j++) {
		if (relocations[j].oldPath == NULL || /* XXX can't happen */
		    strcmp(validRelocations[i], relocations[j].oldPath))
		    /*@innercontinue@*/ continue;
		/* On install, a relocate to NULL means skip the path. */
		if (relocations[j].newPath) {
		    actualRelocations[numActual] = relocations[j].newPath;
		    numActual++;
		}
		/*@innerbreak@*/ break;
	    }
	    if (j == numRelocations) {
		actualRelocations[numActual] = validRelocations[i];
		numActual++;
	    }
	}

	if (numActual)
	    xx = hae(h, RPMTAG_INSTPREFIXES, RPM_STRING_ARRAY_TYPE,
		       (void **) actualRelocations, numActual);

	actualRelocations = _free(actualRelocations);
	validRelocations = hfd(validRelocations, validType);
    }

    xx = hge(h, RPMTAG_BASENAMES, NULL, (void **) &baseNames, &fileCount);
    xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
    xx = hge(h, RPMTAG_DIRNAMES, NULL, (void **) &dirNames, &dirCount);
    xx = hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fFlags, NULL);
    xx = hge(h, RPMTAG_FILEMODES, NULL, (void **) &fModes, NULL);

    skipDirList = alloca(dirCount * sizeof(*skipDirList));
    memset(skipDirList, 0, dirCount * sizeof(*skipDirList));

    newDirIndexes = alloca(sizeof(*newDirIndexes) * fileCount);
    memcpy(newDirIndexes, dirIndexes, sizeof(*newDirIndexes) * fileCount);
    dirIndexes = newDirIndexes;

    /*
     * For all relocations, we go through sorted file/relocation lists 
     * backwards so that /usr/local relocations take precedence over /usr 
     * ones.
     */

    /* Relocate individual paths. */

    for (i = fileCount - 1; i >= 0; i--) {
	fileTypes ft;
	int fnlen;

	/*
	 * If only adding libraries of different arch into an already
	 * installed package, skip all other files.
	 */
	if (alp->multiLib && !isFileMULTILIB((fFlags[i]))) {
	    if (actions) {
		actions[i] = FA_SKIPMULTILIB;
		rpmMessage(RPMMESS_DEBUG, _("excluding multilib path %s%s\n"), 
			dirNames[dirIndexes[i]], baseNames[i]);
	    }
	    continue;
	}

	len = reldel +
		strlen(dirNames[dirIndexes[i]]) + strlen(baseNames[i]) + 1;
	/*@-branchstate@*/
	if (len >= fileAlloced) {
	    fileAlloced = len * 2;
	    fn = xrealloc(fn, fileAlloced);
	}
	/*@=branchstate@*/
	*fn = '\0';
	fnlen = stpcpy( stpcpy(fn, dirNames[dirIndexes[i]]), baseNames[i]) - fn;

	/*
	 * See if this file path needs relocating.
	 */
	/*
	 * XXX FIXME: Would a bsearch of the (already sorted) 
	 * relocation list be a good idea?
	 */
	for (j = numRelocations - 1; j >= 0; j--) {
	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (fnlen < len)
		/*@innercontinue@*/ continue;
	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (!(fn[len] == '/' || fnlen == len))
		/*@innercontinue@*/ continue;

	    if (strncmp(relocations[j].oldPath, fn, len))
		/*@innercontinue@*/ continue;
	    /*@innerbreak@*/ break;
	}
	if (j < 0) continue;

	ft = whatis(fModes[i]);

	/* On install, a relocate to NULL means skip the path. */
	if (relocations[j].newPath == NULL) {
	    if (ft == XDIR) {
		/* Start with the parent, looking for directory to exclude. */
		for (j = dirIndexes[i]; j < dirCount; j++) {
		    len = strlen(dirNames[j]) - 1;
		    while (len > 0 && dirNames[j][len-1] == '/') len--;
		    if (fnlen != len)
			/*@innercontinue@*/ continue;
		    if (strncmp(fn, dirNames[j], fnlen))
			/*@innercontinue@*/ continue;
		    /*@innerbreak@*/ break;
		}
		if (j < dirCount)
		    skipDirList[j] = 1;
	    }
	    if (actions) {
		actions[i] = FA_SKIPNSTATE;
		rpmMessage(RPMMESS_DEBUG, _("excluding %s %s\n"),
			ftstring(ft), fn);
	    }
	    continue;
	}

	/* Relocation on full paths only, please. */
	if (fnlen != len) continue;

	if (actions)
	    rpmMessage(RPMMESS_DEBUG, _("relocating %s to %s\n"),
		    fn, relocations[j].newPath);
	nrelocated++;

	strcpy(fn, relocations[j].newPath);
	{   char * te = strrchr(fn, '/');
	    if (te) {
		if (te > fn) te++;	/* root is special */
		fnlen = te - fn;
	    } else
		te = fn + strlen(fn);
	    /*@-nullpass -nullderef@*/	/* LCL: te != NULL here. */
	    if (strcmp(baseNames[i], te)) /* basename changed too? */
		baseNames[i] = alloca_strdup(te);
	    *te = '\0';			/* terminate new directory name */
	    /*@=nullpass =nullderef@*/
	}

	/* Does this directory already exist in the directory list? */
	for (j = 0; j < dirCount; j++) {
	    if (fnlen != strlen(dirNames[j]))
		/*@innercontinue@*/ continue;
	    if (strncmp(fn, dirNames[j], fnlen))
		/*@innercontinue@*/ continue;
	    /*@innerbreak@*/ break;
	}
	
	if (j < dirCount) {
	    dirIndexes[i] = j;
	    continue;
	}

	/* Creating new paths is a pita */
	if (!haveRelocatedFile) {
	    const char ** newDirList;

	    haveRelocatedFile = 1;
	    newDirList = xmalloc((dirCount + 1) * sizeof(*newDirList));
	    for (j = 0; j < dirCount; j++)
		newDirList[j] = alloca_strdup(dirNames[j]);
	    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
	    dirNames = newDirList;
	} else {
	    dirNames = xrealloc(dirNames, 
			       sizeof(*dirNames) * (dirCount + 1));
	}

	dirNames[dirCount] = alloca_strdup(fn);
	dirIndexes[i] = dirCount;
	dirCount++;
    }

    /* Finish off by relocating directories. */
    for (i = dirCount - 1; i >= 0; i--) {
	for (j = numRelocations - 1; j >= 0; j--) {

	    if (relocations[j].oldPath == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;
	    len = strcmp(relocations[j].oldPath, "/")
		? strlen(relocations[j].oldPath)
		: 0;

	    if (len && strncmp(relocations[j].oldPath, dirNames[i], len))
		/*@innercontinue@*/ continue;

	    /*
	     * Only subdirectories or complete file paths may be relocated. We
	     * don't check for '\0' as our directory names all end in '/'.
	     */
	    if (dirNames[i][len] != '/')
		/*@innercontinue@*/ continue;

	    if (relocations[j].newPath) { /* Relocate the path */
		const char * s = relocations[j].newPath;
		char * t = alloca(strlen(s) + strlen(dirNames[i]) - len + 1);

		(void) stpcpy( stpcpy(t, s) , dirNames[i] + len);
		if (actions)
		    rpmMessage(RPMMESS_DEBUG,
			_("relocating directory %s to %s\n"), dirNames[i], t);
		dirNames[i] = t;
		nrelocated++;
	    }
	}
    }

    /* Save original filenames in header and replace (relocated) filenames. */
    if (nrelocated) {
	int c;
	void * p;
	rpmTagType t;

	p = NULL;
	xx = hge(h, RPMTAG_BASENAMES, &t, &p, &c);
	xx = hae(h, RPMTAG_ORIGBASENAMES, t, p, c);
	p = hfd(p, t);

	p = NULL;
	xx = hge(h, RPMTAG_DIRNAMES, &t, &p, &c);
	xx = hae(h, RPMTAG_ORIGDIRNAMES, t, p, c);
	p = hfd(p, t);

	p = NULL;
	xx = hge(h, RPMTAG_DIRINDEXES, &t, &p, &c);
	xx = hae(h, RPMTAG_ORIGDIRINDEXES, t, p, c);
	p = hfd(p, t);

	xx = hme(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			  baseNames, fileCount);
	fi->bnl = hfd(fi->bnl, RPM_STRING_ARRAY_TYPE);
	xx = hge(h, RPMTAG_BASENAMES, NULL, (void **) &fi->bnl, &fi->fc);

	xx = hme(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			  dirNames, dirCount);
	fi->dnl = hfd(fi->dnl, RPM_STRING_ARRAY_TYPE);
	xx = hge(h, RPMTAG_DIRNAMES, NULL, (void **) &fi->dnl, &fi->dc);

	xx = hme(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			  dirIndexes, fileCount);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &fi->dil, NULL);
    }

    baseNames = hfd(baseNames, RPM_STRING_ARRAY_TYPE);
    dirNames = hfd(dirNames, RPM_STRING_ARRAY_TYPE);
    fn = _free(fn);

    return h;
}

void loadFi(const rpmTransactionSet ts, TFI_t fi, Header h, int scareMem)
{
    HGE_t hge;
    HFD_t hfd;
    uint_32 * uip;
    int len;
    int rc;
    int i;
    
    if (fi->fsm == NULL)
	fi->fsm = newFSM();

    /* XXX avoid gcc noise on pointer (4th arg) cast(s) */
    hge = (scareMem && fi->type == TR_ADDED)
	? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry;
    fi->hge = hge;
    fi->hae = (HAE_t) headerAddEntry;
    fi->hme = (HME_t) headerModifyEntry;
    fi->hre = (HRE_t) headerRemoveEntry;
    fi->hfd = hfd = headerFreeData;

    /*@-branchstate@*/
    if (h && fi->h == NULL)	fi->h = headerLink(h);
    /*@=branchstate@*/

    /* Duplicate name-version-release so that headers can be free'd. */
    rc = hge(fi->h, RPMTAG_NAME, NULL, (void **) &fi->name, NULL);
    fi->name = xstrdup(fi->name);
    rc = hge(fi->h, RPMTAG_VERSION, NULL, (void **) &fi->version, NULL);
    fi->version = xstrdup(fi->version);
    rc = hge(fi->h, RPMTAG_RELEASE, NULL, (void **) &fi->release, NULL);
    fi->release = xstrdup(fi->release);

    /* -1 means not found */
    rc = hge(fi->h, RPMTAG_EPOCH, NULL, (void **) &uip, NULL);
    fi->epoch = (rc ? *uip : -1);

    /* 0 means unknown */
    rc = hge(fi->h, RPMTAG_ARCHIVESIZE, NULL, (void **) &uip, NULL);
    fi->archiveSize = (rc ? *uip : 0);

    if (!hge(fi->h, RPMTAG_BASENAMES, NULL, (void **) &fi->bnl, &fi->fc)) {
	fi->dc = 0;
	fi->fc = 0;
	return;
    }

    rc = hge(fi->h, RPMTAG_DIRNAMES, NULL, (void **) &fi->dnl, &fi->dc);

    rc = hge(fi->h, RPMTAG_DIRINDEXES, NULL, (void **) &fi->dil, NULL);
    rc = hge(fi->h, RPMTAG_FILEMODES, NULL, (void **) &fi->fmodes, NULL);
    rc = hge(fi->h, RPMTAG_FILEFLAGS, NULL, (void **) &fi->fflags, NULL);
    rc = hge(fi->h, RPMTAG_FILESIZES, NULL, (void **) &fi->fsizes, NULL);

    /* XXX initialized to NULL for TR_ADDED? */
    rc = hge(fi->h, RPMTAG_FILESTATES, NULL, (void **) &fi->fstates, NULL);

    fi->action = FA_UNKNOWN;
    fi->flags = 0;

    /* actions is initialized earlier for added packages */
    if (fi->actions == NULL)
	fi->actions = xcalloc(fi->fc, sizeof(*fi->actions));

    switch (fi->type) {
    case TR_ADDED:
	fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;
	rc = hge(fi->h, RPMTAG_FILELINKTOS, NULL, (void **) &fi->flinks, NULL);
	rc = hge(fi->h, RPMTAG_FILELANGS, NULL, (void **) &fi->flangs, NULL);

	rc = hge(fi->h, RPMTAG_FILEMD5S, NULL, (void **) &fi->fmd5s, NULL);

	rc = hge(fi->h, RPMTAG_FILEMTIMES, NULL, (void **) &fi->fmtimes, NULL);
	rc = hge(fi->h, RPMTAG_FILERDEVS, NULL, (void **) &fi->frdevs, NULL);

	/* 0 makes for noops */
	fi->replacedSizes = xcalloc(fi->fc, sizeof(*fi->replacedSizes));

	if (ts != NULL && fi->ap != NULL && fi->h != NULL)
	{   Header foo = relocateFileList(ts, fi, fi->ap, fi->h, fi->actions);
	    foo = headerFree(foo);
	}

    if (!scareMem) {

fprintf(stderr, "*** %s-%s-%s scareMem\n", fi->name, fi->version, fi->release);
	fi->fmtimes = memcpy(xmalloc(fi->fc * sizeof(*fi->fmtimes)),
				fi->fmtimes, fi->fc * sizeof(*fi->fmtimes));
	fi->frdevs = memcpy(xmalloc(fi->fc * sizeof(*fi->frdevs)),
				fi->frdevs, fi->fc * sizeof(*fi->frdevs));

	fi->fsizes = memcpy(xmalloc(fi->fc * sizeof(*fi->fsizes)),
				fi->fsizes, fi->fc * sizeof(*fi->fsizes));
	fi->fflags = memcpy(xmalloc(fi->fc * sizeof(*fi->fflags)),
				fi->fflags, fi->fc * sizeof(*fi->fflags));
	fi->fmodes = memcpy(xmalloc(fi->fc * sizeof(*fi->fmodes)),
				fi->fmodes, fi->fc * sizeof(*fi->fmodes));
	/* XXX there's a tedious segfault here for some version(s) of rpm */
	if (fi->fstates)
	    fi->fstates = memcpy(xmalloc(fi->fc * sizeof(*fi->fstates)),
				fi->fstates, fi->fc * sizeof(*fi->fstates));
	else
	    fi->fstates = xcalloc(1, fi->fc * sizeof(*fi->fstates));
	fi->dil = memcpy(xmalloc(fi->fc * sizeof(*fi->dil)),
				fi->dil, fi->fc * sizeof(*fi->dil));
	fi->h = headerFree(fi->h);
    }

	break;
    case TR_REMOVED:
	fi->mapflags = 
		CPIO_MAP_ABSOLUTE | CPIO_MAP_ADDDOT | CPIO_ALL_HARDLINKS |
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;
	rc = hge(fi->h, RPMTAG_FILEMD5S, NULL, (void **) &fi->fmd5s, NULL);
	rc = hge(fi->h, RPMTAG_FILELINKTOS, NULL, (void **) &fi->flinks, NULL);
	fi->fsizes = memcpy(xmalloc(fi->fc * sizeof(*fi->fsizes)),
				fi->fsizes, fi->fc * sizeof(*fi->fsizes));
	fi->fflags = memcpy(xmalloc(fi->fc * sizeof(*fi->fflags)),
				fi->fflags, fi->fc * sizeof(*fi->fflags));
	fi->fmodes = memcpy(xmalloc(fi->fc * sizeof(*fi->fmodes)),
				fi->fmodes, fi->fc * sizeof(*fi->fmodes));
	/* XXX there's a tedious segfault here for some version(s) of rpm */
	if (fi->fstates)
	    fi->fstates = memcpy(xmalloc(fi->fc * sizeof(*fi->fstates)),
				fi->fstates, fi->fc * sizeof(*fi->fstates));
	else
	    fi->fstates = xcalloc(1, fi->fc * sizeof(*fi->fstates));
	fi->dil = memcpy(xmalloc(fi->fc * sizeof(*fi->dil)),
				fi->dil, fi->fc * sizeof(*fi->dil));
	fi->h = headerFree(fi->h);
	break;
    }

    fi->dnlmax = -1;
    for (i = 0; i < fi->dc; i++) {
	if ((len = strlen(fi->dnl[i])) > fi->dnlmax)
	    fi->dnlmax = len;
    }

    fi->bnlmax = -1;
    for (i = 0; i < fi->fc; i++) {
	if ((len = strlen(fi->bnl[i])) > fi->bnlmax)
	    fi->bnlmax = len;
    }

    fi->dperms = 0755;
    fi->fperms = 0644;

    return;
}

void freeFi(TFI_t fi)
{
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);

    fi->name = _free(fi->name);
    fi->version = _free(fi->version);
    fi->release = _free(fi->release);
    fi->actions = _free(fi->actions);
    fi->replacedSizes = _free(fi->replacedSizes);
    fi->replaced = _free(fi->replaced);

    fi->bnl = hfd(fi->bnl, -1);
    fi->dnl = hfd(fi->dnl, -1);
    fi->obnl = hfd(fi->obnl, -1);
    fi->odnl = hfd(fi->odnl, -1);
    fi->flinks = hfd(fi->flinks, -1);
    fi->fmd5s = hfd(fi->fmd5s, -1);
    fi->fuser = hfd(fi->fuser, -1);
    fi->fgroup = hfd(fi->fgroup, -1);
    fi->flangs = hfd(fi->flangs, -1);

    fi->apath = _free(fi->apath);
    fi->fuids = _free(fi->fuids);
    fi->fgids = _free(fi->fgids);
    fi->fmapflags = _free(fi->fmapflags);

    fi->fsm = freeFSM(fi->fsm);

    switch (fi->type) {
    case TR_ADDED:
	if (fi->h == NULL) {
	    fi->fmtimes = hfd(fi->fmtimes, -1);
	    fi->frdevs = hfd(fi->frdevs, -1);
	    fi->fsizes = hfd(fi->fsizes, -1);
	    fi->fflags = hfd(fi->fflags, -1);
	    fi->fmodes = hfd(fi->fmodes, -1);
	    fi->fstates = hfd(fi->fstates, -1);
	    fi->dil = hfd(fi->dil, -1);
	}
	break;
    case TR_REMOVED:
	fi->fsizes = hfd(fi->fsizes, -1);
	fi->fflags = hfd(fi->fflags, -1);
	fi->fmodes = hfd(fi->fmodes, -1);
	fi->fstates = hfd(fi->fstates, -1);
	fi->dil = hfd(fi->dil, -1);
	break;
    }

    fi->h = headerFree(fi->h);

    /*@-nullstate@*/ /* FIX: fi->{name,version,release,actions,...,h} NULL */
    return;
    /*@=nullstate@*/
}

/*@observer@*/ const char *const fiTypeString(TFI_t fi)
{
    switch(fi->type) {
    case TR_ADDED:	return " install";
    case TR_REMOVED:	return "   erase";
    default:		return "???";
    }
    /*@noteached@*/
}

/**
 * Macros to be defined from per-header tag values.
 * @todo Should other macros be added from header when installing a package?
 */
/*@observer@*/ /*@unchecked@*/
static struct tagMacro {
/*@observer@*/ /*@null@*/ const char *	macroname; /*!< Macro name to define. */
    rpmTag	tag;		/*!< Header tag to use for value. */
} tagMacros[] = {
    { "name",		RPMTAG_NAME },
    { "version",	RPMTAG_VERSION },
    { "release",	RPMTAG_RELEASE },
    { "epoch",		RPMTAG_EPOCH },
    { NULL, 0 }
};

/**
 * Define per-header macros.
 * @param fi		transaction element file info
 * @param h		header
 * @return		0 always
 */
static int rpmInstallLoadMacros(TFI_t fi, Header h)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    HGE_t hge = (HGE_t) fi->hge;
    struct tagMacro * tagm;
    union {
/*@unused@*/ void * ptr;
/*@unused@*/ const char ** argv;
	const char * str;
	int_32 * i32p;
    } body;
    char numbuf[32];
    rpmTagType type;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	if (!hge(h, tagm->tag, &type, (void **) &body, NULL))
	    continue;
	switch (type) {
	case RPM_INT32_TYPE:
	    sprintf(numbuf, "%d", *body.i32p);
	    addMacro(NULL, tagm->macroname, NULL, numbuf, -1);
	    /*@switchbreak@*/ break;
	case RPM_STRING_TYPE:
	    addMacro(NULL, tagm->macroname, NULL, body.str, -1);
	    /*@switchbreak@*/ break;
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	case RPM_INT16_TYPE:
	case RPM_BIN_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	default:
	    /*@switchbreak@*/ break;
	}
    }
    return 0;
}

/**
 * Copy file data from h to newH.
 * @param h		header from
 * @param newH		header to
 * @param actions	array of file dispositions
 * @return		0 on success, 1 on failure
 */
static int mergeFiles(TFI_t fi, Header h, Header newH)
	/*@modifies h @*/
{
    HGE_t hge = (HGE_t)fi->hge;
    HME_t hme = (HME_t)fi->hme;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    fileAction * actions = fi->actions;
    int i, j, k, fc, xx;
    rpmTagType type = 0;
    int_32 count = 0;
    int_32 dirNamesCount, dirCount;
    void * data, * newdata;
    int_32 * dirIndexes, * newDirIndexes;
    uint_32 * fileSizes, fileSize;
    const char ** dirNames;
    const char ** newDirNames;
    static rpmTag mergeTags[] = {
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
    static rpmTag requireTags[] = {
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	RPMTAG_PROVIDENAME, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS,
	RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS
    };

    xx = hge(h, RPMTAG_SIZE, NULL, (void **) &fileSizes, NULL);
    fileSize = *fileSizes;
    xx = hge(newH, RPMTAG_FILESIZES, NULL, (void **) &fileSizes, &count);
    for (i = 0, fc = 0; i < count; i++)
	if (actions[i] != FA_SKIPMULTILIB) {
	    fc++;
	    fileSize += fileSizes[i];
	}
    xx = hme(h, RPMTAG_SIZE, RPM_INT32_TYPE, &fileSize, 1);

    /*@-sizeoftype@*/
    for (i = 0; mergeTags[i]; i++) {
        if (!hge(newH, mergeTags[i], &type, (void **) &data, &count))
	    continue;
	switch (type) {
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	    newdata = xcalloc(fc, sizeof(int_8));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
			((int_8 *) newdata)[k++] = ((int_8 *) data)[j];
	    xx = headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    /*@switchbreak@*/ break;
	case RPM_INT16_TYPE:
	    newdata = xcalloc(fc, sizeof(int_16));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_16 *) newdata)[k++] = ((int_16 *) data)[j];
	    xx = headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    /*@switchbreak@*/ break;
	case RPM_INT32_TYPE:
	    newdata = xcalloc(fc, sizeof(int_32));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((int_32 *) newdata)[k++] = ((int_32 *) data)[j];
	    xx = headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    /*@switchbreak@*/ break;
	case RPM_STRING_ARRAY_TYPE:
	    newdata = xcalloc(fc, sizeof(char *));
	    for (j = 0, k = 0; j < count; j++)
		if (actions[j] != FA_SKIPMULTILIB)
		    ((char **) newdata)[k++] = ((char **) data)[j];
	    xx = headerAddOrAppendEntry(h, mergeTags[i], type, newdata, fc);
	    free (newdata);
	    /*@switchbreak@*/ break;
	default:
	    rpmError(RPMERR_DATATYPE, _("Data type %d not supported\n"),
			(int) type);
	    return 1;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}
	data = hfd(data, type);
    }
    /*@=sizeoftype@*/
    xx = hge(newH, RPMTAG_DIRINDEXES, NULL, (void **) &newDirIndexes, &count);
    xx = hge(newH, RPMTAG_DIRNAMES, NULL, (void **) &newDirNames, NULL);
    xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
    xx = hge(h, RPMTAG_DIRNAMES, NULL, (void **) &data, &dirNamesCount);

    dirNames = xcalloc(dirNamesCount + fc, sizeof(*dirNames));
    for (i = 0; i < dirNamesCount; i++)
	dirNames[i] = ((char **) data)[i];
    dirCount = dirNamesCount;
    newdata = xcalloc(fc, sizeof(*newDirIndexes));
    for (i = 0, k = 0; i < count; i++) {
	if (actions[i] == FA_SKIPMULTILIB)
	    continue;
	for (j = 0; j < dirCount; j++)
	    if (!strcmp(dirNames[j], newDirNames[newDirIndexes[i]]))
		/*@innerbreak@*/ break;
	if (j == dirCount)
	    dirNames[dirCount++] = newDirNames[newDirIndexes[i]];
	((int_32 *) newdata)[k++] = j;
    }
    xx = headerAddOrAppendEntry(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, newdata, fc);
    if (dirCount > dirNamesCount)
	xx = headerAddOrAppendEntry(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			       dirNames + dirNamesCount,
			       dirCount - dirNamesCount);
    data = hfd(data, -1);
    newDirNames = hfd(newDirNames, -1);
    free (newdata);
    free (dirNames);

    for (i = 0; i < 9; i += 3) {
	const char **Names, **EVR, **newNames, **newEVR;
	rpmTagType nnt, nvt, rnt;
	uint_32 *Flags, *newFlags;
	int Count = 0, newCount = 0;

	if (!hge(newH, requireTags[i], &nnt, (void **) &newNames, &newCount))
	    continue;

	xx = hge(newH, requireTags[i+1], &nvt, (void **) &newEVR, NULL);
	xx = hge(newH, requireTags[i+2], NULL, (void **) &newFlags, NULL);
	if (hge(h, requireTags[i], &rnt, (void **) &Names, &Count))
	{
	    xx = hge(h, requireTags[i+1], NULL, (void **) &EVR, NULL);
	    xx = hge(h, requireTags[i+2], NULL, (void **) &Flags, NULL);
	    for (j = 0; j < newCount; j++)
		for (k = 0; k < Count; k++)
		    if (!strcmp (newNames[j], Names[k])
			&& !strcmp (newEVR[j], EVR[k])
			&& (newFlags[j] & RPMSENSE_SENSEMASK) ==
			   (Flags[k] & RPMSENSE_SENSEMASK))
		    {
			newNames[j] = NULL;
			/*@innerbreak@*/ break;
		    }
	}
	for (j = 0, k = 0; j < newCount; j++) {
	    if (!newNames[j] || !isDependsMULTILIB(newFlags[j]))
		/*@innercontinue@*/ continue;
	    if (j != k) {
		newNames[k] = newNames[j];
		newEVR[k] = newEVR[j];
		newFlags[k] = newFlags[j];
	    }
	    k++;
	}
	if (k) {
	    xx = headerAddOrAppendEntry(h, requireTags[i],
				       RPM_STRING_ARRAY_TYPE, newNames, k);
	    xx = headerAddOrAppendEntry(h, requireTags[i+1],
				       RPM_STRING_ARRAY_TYPE, newEVR, k);
	    xx = headerAddOrAppendEntry(h, requireTags[i+2], RPM_INT32_TYPE,
				       newFlags, k);
	}
	newNames = hfd(newNames, nnt);
	newEVR = hfd(newEVR, nvt);
	Names = hfd(Names, rnt);
    }
    return 0;
}

/**
 * Mark files in database shared with this package as "replaced".
 * @param psm		package state machine data
 * @return		0 always
 */
static int markReplacedFiles(const PSM_t psm)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    HGE_t hge = (HGE_t)fi->hge;
    const struct sharedFileInfo * replaced = fi->replaced;
    const struct sharedFileInfo * sfi;
    rpmdbMatchIterator mi;
    Header h;
    unsigned int * offsets;
    unsigned int prev;
    int num, xx;

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

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
    xx = rpmdbAppendIterator(mi, offsets, num);
    xx = rpmdbSetIteratorRewrite(mi, 1);

    sfi = replaced;
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * secStates;
	int modified;
	int count;

	modified = 0;

	if (!hge(h, RPMTAG_FILESTATES, NULL, (void **)&secStates, &count))
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
		    xx = rpmdbSetIteratorModified(mi, modified);
		}
		num++;
	    }
	    sfi++;
	}
    }
    mi = rpmdbFreeIterator(mi);

    return 0;
}

/**
 * Create directory if it does not exist, make sure path is writable.
 * @note This will only create last component of directory path.
 * @param dpath		directory path
 * @param dname		directory use
 * @return		rpmRC return code
 */
static rpmRC chkdir (const char * dpath, const char * dname)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/
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
	    rc = Mkdir(dpath, 0755);
	    break;
	case URL_IS_DASH:
	    break;
	}
	if (rc < 0) {
	    rpmError(RPMERR_CREATE, _("cannot create %%%s %s\n"),
			dname, dpath);
	    return RPMRC_FAIL;
	}
    }
    if ((rc = Access(dpath, W_OK))) {
	rpmError(RPMERR_CREATE, _("cannot write to %%%s %s\n"), dname, dpath);
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

rpmRC rpmInstallSourcePackage(rpmTransactionSet ts,
		FD_t fd,
		const char ** specFilePtr,
		rpmCallbackFunction notify, rpmCallbackData notifyData,
		const char ** cookie)
{
    TFI_t fi = xcalloc(sizeof(*fi), 1);
    const char * _sourcedir = NULL;
    const char * _specdir = NULL;
    const char * specFile = NULL;
    HGE_t hge;
    HFD_t hfd;
    Header h = NULL;
    struct psm_s psmbuf;
    PSM_t psm = &psmbuf;
    int isSource;
    rpmRC rc;
    int i;

    /*@-mods -temptrans -assignexpose@*/
    ts->notify = notify;
    ts->notifyData = notifyData;
    /*@=mods =temptrans =assignexpose@*/

    /*@-mustmod@*/	/* LCL: segfault */
    rc = rpmReadPackageFile(ts, fd, "InstallSourcePackage", &h);
    /*@=mustmod@*/
    if (!(rc == RPMRC_OK || rc == RPMRC_BADSIZE) || h == NULL) {
	goto exit;
    }
    rc = RPMRC_OK;				/* XXX HACK */
    isSource = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);

    if (!isSource) {
	rpmError(RPMERR_NOTSRPM, _("source package expected, binary found\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    (void) rpmtransAddPackage(ts, h, fd, NULL, 0, NULL);
    if (ts->addedPackages.list == NULL) {	/* XXX can't happen */
	rc = RPMRC_FAIL;
	goto exit;
    }

    fi->type = TR_ADDED;
    fi->ap = ts->addedPackages.list;
    loadFi(ts, fi, h, 1);
    hge = fi->hge;
    hfd = (fi->hfd ? fi->hfd : headerFreeData);
    h = headerFree(h);	/* XXX reference held by transaction set */

    if (fi->h != NULL)	/* XXX can't happen */
    (void) rpmInstallLoadMacros(fi, fi->h);

    memset(psm, 0, sizeof(*psm));
    /*@-assignexpose@*/
    psm->ts = ts;
    psm->fi = fi;
    /*@=assignexpose@*/

    if (cookie) {
	*cookie = NULL;
	if (hge(fi->h, RPMTAG_COOKIE, NULL, (void **) cookie, NULL))
	    *cookie = xstrdup(*cookie);
    }

    /* XXX FIXME: can't do endian neutral MD5 verification yet. */
    fi->fmd5s = hfd(fi->fmd5s, -1);

    /* XXX FIXME: don't do per-file mapping, force global flags. */
    fi->fmapflags = _free(fi->fmapflags);
    fi->mapflags = CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

    fi->uid = getuid();
    fi->gid = getgid();
    fi->astriplen = 0;
    fi->striplen = 0;

    fi->fuids = xcalloc(sizeof(*fi->fuids), fi->fc);
    fi->fgids = xcalloc(sizeof(*fi->fgids), fi->fc);
    for (i = 0; i < fi->fc; i++) {
	fi->fuids[i] = fi->uid;
	fi->fgids[i] = fi->gid;
    }

    for (i = 0; i < fi->fc; i++) {
	fi->actions[i] = FA_CREATE;
    }

    i = fi->fc;

    if (fi->h != NULL) {	/* XXX can't happen */
	rpmBuildFileList(fi->h, &fi->apath, NULL);

	if (headerIsEntry(fi->h, RPMTAG_COOKIE))
	    for (i = 0; i < fi->fc; i++)
		if (fi->fflags[i] & RPMFILE_SPECFILE) break;
    }

    if (i == fi->fc) {
	/* Find the spec file by name. */
	for (i = 0; i < fi->fc; i++) {
	    const char * t = fi->apath[i];
	    t += strlen(fi->apath[i]) - 5;
	    if (!strcmp(t, ".spec")) break;
	}
    }

    _sourcedir = rpmGenPath(ts->rootDir, "%{_sourcedir}", "");
    rc = chkdir(_sourcedir, "sourcedir");
    if (rc) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    _specdir = rpmGenPath(ts->rootDir, "%{_specdir}", "");
    rc = chkdir(_specdir, "specdir");
    if (rc) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Build dnl/dil with {_sourcedir, _specdir} as values. */
    if (i < fi->fc) {
	int speclen = strlen(_specdir) + 2;
	int sourcelen = strlen(_sourcedir) + 2;
	char * t;

	fi->dnl = hfd(fi->dnl, -1);

	fi->dc = 2;
	fi->dnl = xmalloc(fi->dc * sizeof(*fi->dnl) + fi->fc * sizeof(*fi->dil) +
			speclen + sourcelen);
	fi->dil = (int *)(fi->dnl + fi->dc);
	memset(fi->dil, 0, fi->fc * sizeof(*fi->dil));
	fi->dil[i] = 1;
	/*@-dependenttrans@*/
	fi->dnl[0] = t = (char *)(fi->dil + fi->fc);
	fi->dnl[1] = t = stpcpy( stpcpy(t, _sourcedir), "/") + 1;
	/*@=dependenttrans@*/
	(void) stpcpy( stpcpy(t, _specdir), "/");

	t = xmalloc(speclen + strlen(fi->bnl[i]) + 1);
	(void) stpcpy( stpcpy( stpcpy(t, _specdir), "/"), fi->bnl[i]);
	specFile = t;
    } else {
	rpmError(RPMERR_NOSPEC, _("source package contains no .spec file\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    psm->goal = PSM_PKGINSTALL;

    /*@-compmempass@*/	/* FIX: psm->fi->dnl should be owned. */
    rc = psmStage(psm, PSM_PROCESS);

    (void) psmStage(psm, PSM_FINI);
    /*@=compmempass@*/

    if (rc) rc = RPMRC_FAIL;

exit:
    if (specFilePtr && specFile && rc == RPMRC_OK)
	*specFilePtr = specFile;
    else
	specFile = _free(specFile);

    _specdir = _free(_specdir);
    _sourcedir = _free(_sourcedir);

    if (h) h = headerFree(h);

    if (fi) {
	freeFi(fi);
	fi = _free(fi);
    }

    return rc;
}

/*@observer@*/ /*@unchecked@*/
static char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

/**
 * Return scriptlet name from tag.
 * @param tag		scriptlet tag
 * @return		name of scriptlet
 */
static /*@observer@*/ const char * const tag2sln(int tag)
	/*@*/
{
    switch (tag) {
    case RPMTAG_PREIN:		return "%pre";
    case RPMTAG_POSTIN:		return "%post";
    case RPMTAG_PREUN:		return "%preun";
    case RPMTAG_POSTUN:		return "%postun";
    case RPMTAG_VERIFYSCRIPT:	return "%verify";
    }
    return "%unknownscript";
}

/**
 * Run scriptlet with args.
 *
 * Run a script with an interpreter. If the interpreter is not specified,
 * /bin/sh will be used. If the interpreter is /bin/sh, then the args from
 * the header will be ignored, passing instead arg1 and arg2.
 * 
 * @param psm		package state machine data
 * @param h		header
 * @param sln		name of scriptlet section
 * @param progArgc	no. of args from header
 * @param progArgv	args from header, progArgv[0] is the interpreter to use
 * @param script	scriptlet from header
 * @param arg1		no. instances of package installed after scriptlet exec
 *			(-1 is no arg)
 * @param arg2		ditto, but for the target package
 * @return		0 on success, 1 on error
 */
static int runScript(PSM_t psm, Header h,
		const char * sln,
		int progArgc, const char ** progArgv, 
		const char * script, int arg1, int arg2)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState@*/
	/*@modifies psm, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    const char ** argv = NULL;
    int argc = 0;
    const char ** prefixes = NULL;
    int numPrefixes;
    rpmTagType ipt;
    const char * oldPrefix;
    int maxPrefixLength;
    int len;
    char * prefixBuf = NULL;
    pid_t child;
    int status = 0;
    const char * fn = NULL;
    int i, xx;
    int freePrefixes = 0;
    FD_t out;
    rpmRC rc = RPMRC_OK;
    const char *n, *v, *r;

    if (progArgv == NULL && script == NULL)
	return 0;

    rpmMessage(RPMMESS_DEBUG, _("%s: running %s scriptlet\n"),
		psm->stepName, tag2sln(psm->scriptTag));

    if (!progArgv) {
	argv = alloca(5 * sizeof(*argv));
	argv[0] = "/bin/sh";
	argc = 1;
    } else {
	argv = alloca((progArgc + 4) * sizeof(*argv));
	memcpy(argv, progArgv, progArgc * sizeof(*argv));
	argc = progArgc;
    }

    xx = headerNVR(h, &n, &v, &r);
    if (hge(h, RPMTAG_INSTPREFIXES, &ipt, (void **) &prefixes, &numPrefixes)) {
	freePrefixes = 1;
    } else if (hge(h, RPMTAG_INSTALLPREFIX, NULL, (void **) &oldPrefix, NULL)) {
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
	/*@-branchstate@*/
	if (makeTempFile((!ts->chrootDone ? ts->rootDir : "/"), &fn, &fd)) {
	    if (freePrefixes) free(prefixes);
	    return 1;
	}
	/*@=branchstate@*/

	if (rpmIsDebug() &&
	    (!strcmp(argv[0], "/bin/sh") || !strcmp(argv[0], "/bin/bash")))
	{
	    static const char set_x[] = "set -x\n";
	    xx = Fwrite(set_x, sizeof(set_x[0]), sizeof(set_x)-1, fd);
	}

	xx = Fwrite(script, sizeof(script[0]), strlen(script), fd);
	xx = Fclose(fd);

	{   const char * sn = fn;
	    if (!ts->chrootDone &&
		!(ts->rootDir[0] == '/' && ts->rootDir[1] == '\0'))
	    {
		sn += strlen(ts->rootDir)-1;
	    }
	    argv[argc++] = sn;
	}

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

    if (ts->scriptFd != NULL) {
	if (rpmIsVerbose()) {
	    out = fdDup(Fileno(ts->scriptFd));
	} else {
	    out = Fopen("/dev/null", "w.fdio");
	    if (Ferror(out)) {
		out = fdDup(Fileno(ts->scriptFd));
	    }
	}
    } else {
	out = fdDup(STDOUT_FILENO);
#ifdef	DYING
	out = fdLink(out, "runScript persist");
#endif
    }
    if (out == NULL) return 1;	/* XXX can't happen */
    
    if (!(child = fork())) {
	const char * rootDir;
	int pipes[2];

	pipes[0] = pipes[1] = 0;
	/* make stdin inaccessible */
	xx = pipe(pipes);
	xx = close(pipes[1]);
	xx = dup2(pipes[0], STDIN_FILENO);
	xx = close(pipes[0]);

	/*@-branchstate@*/
	if (ts->scriptFd != NULL) {
	    int sfdno = Fileno(ts->scriptFd);
	    int ofdno = Fileno(out);
	    if (sfdno != STDERR_FILENO)
		xx = dup2(sfdno, STDERR_FILENO);
	    if (ofdno != STDOUT_FILENO)
		xx = dup2(ofdno, STDOUT_FILENO);
	    /* make sure we don't close stdin/stderr/stdout by mistake! */
	    if (ofdno > STDERR_FILENO && ofdno != sfdno) {
		xx = Fclose (out);
	    }
	    if (sfdno > STDERR_FILENO) {
		xx = Fclose (ts->scriptFd);
	    }
	}
	/*@=branchstate@*/

	/*@-branchstate@*/
	{   const char *ipath = rpmExpand("PATH=%{_install_script_path}", NULL);
	    const char *path = SCRIPT_PATH;

	    if (ipath && ipath[5] != '%')
		path = ipath;

	    xx = doputenv(path);
	    /*@-modobserver@*/
	    ipath = _free(ipath);
	    /*@=modobserver@*/
	}
	/*@=branchstate@*/

	for (i = 0; i < numPrefixes; i++) {
	    sprintf(prefixBuf, "RPM_INSTALL_PREFIX%d=%s", i, prefixes[i]);
	    xx = doputenv(prefixBuf);

	    /* backwards compatibility */
	    if (i == 0) {
		sprintf(prefixBuf, "RPM_INSTALL_PREFIX=%s", prefixes[i]);
		xx = doputenv(prefixBuf);
	    }
	}

	if ((rootDir = ts->rootDir) != NULL)	/* XXX can't happen */
	switch(urlIsURL(rootDir)) {
	case URL_IS_PATH:
	    rootDir += sizeof("file://") - 1;
	    rootDir = strchr(rootDir, '/');
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    if (!ts->chrootDone && !(rootDir[0] == '/' && rootDir[1] == '\0')) {
		/*@-superuser -noeffect @*/
		xx = chroot(rootDir);
		/*@=superuser =noeffect @*/
	    }
	    xx = chdir("/");
	    xx = execv(argv[0], (char *const *)argv);
	    break;
	default:
	    break;
	}

 	_exit(-1);
	/*@notreached@*/
    }

    if (waitpid(child, &status, 0) < 0) {
	rpmError(RPMERR_SCRIPT,
     _("execution of %s scriptlet from %s-%s-%s failed, waitpid returned %s\n"),
		 sln, n, v, r, strerror (errno));
	/* XXX what to do here? */
	rc = RPMRC_OK;
    } else {
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	    rpmError(RPMERR_SCRIPT,
     _("execution of %s scriptlet from %s-%s-%s failed, exit status %d\n"),
		     sln, n, v, r, WEXITSTATUS(status));
	    rc = RPMRC_FAIL;
	}
    }

    if (freePrefixes) prefixes = hfd(prefixes, ipt);

    xx = Fclose(out);	/* XXX dup'd STDOUT_FILENO */
    
    /*@-branchstate@*/
    if (script) {
	if (!rpmIsDebug())
	    xx = unlink(fn);
	fn = _free(fn);
    }
    /*@=branchstate@*/

    return rc;
}

/**
 * Retrieve and run scriptlet from header.
 * @param psm		package state machine data
 * @return		rpmRC return code
 */
static rpmRC runInstScript(PSM_t psm)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies psm, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    TFI_t fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    void ** progArgv;
    int progArgc;
    const char ** argv;
    rpmTagType ptt, stt;
    const char * script;
    rpmRC rc = RPMRC_OK;
    int xx;

    /*
     * headerGetEntry() sets the data pointer to NULL if the entry does
     * not exist.
     */
    xx = hge(fi->h, psm->scriptTag, &stt, (void **) &script, NULL);
    xx = hge(fi->h, psm->progTag, &ptt, (void **) &progArgv, &progArgc);
    if (progArgv == NULL && script == NULL)
	goto exit;

    /*@-branchstate@*/
    if (progArgv && ptt == RPM_STRING_TYPE) {
	argv = alloca(sizeof(*argv));
	*argv = (const char *) progArgv;
    } else {
	argv = (const char **) progArgv;
    }
    /*@=branchstate@*/

    if (fi->h != NULL)	/* XXX can't happen */
    rc = runScript(psm, fi->h, tag2sln(psm->scriptTag), progArgc, argv,
		script, psm->scriptArg, -1);

exit:
    progArgv = hfd(progArgv, ptt);
    script = hfd(script, stt);
    return rc;
}

/**
 * @param psm		package state machine data
 * @param sourceH
 * @param triggeredH
 * @param arg2
 * @param triggersAlreadyRun
 * @return
 */
static int handleOneTrigger(PSM_t psm, Header sourceH, Header triggeredH,
			int arg2, unsigned char * triggersAlreadyRun)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState@*/
	/*@modifies psm, *triggersAlreadyRun, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    const char ** triggerNames;
    const char ** triggerEVR;
    const char ** triggerScripts;
    const char ** triggerProgs;
    int_32 * triggerFlags;
    int_32 * triggerIndices;
    rpmTagType tnt, tvt, tft;
    const char * triggerPackageName;
    const char * sourceName;
    int numTriggers;
    rpmRC rc = RPMRC_OK;
    int i, xx;
    int skip;

    if (!(	hge(triggeredH, RPMTAG_TRIGGERNAME, &tnt, 
			(void **) &triggerNames, &numTriggers) &&
		hge(triggeredH, RPMTAG_TRIGGERFLAGS, &tft,
			(void **) &triggerFlags, NULL) &&
		hge(triggeredH, RPMTAG_TRIGGERVERSION, &tvt,
			(void **) &triggerEVR, NULL))
	)
	return 0;

    xx = headerNVR(sourceH, &sourceName, NULL, NULL);

    for (i = 0; i < numTriggers; i++) {
	rpmTagType tit, tst, tpt;

	if (!(triggerFlags[i] & psm->sense)) continue;
	if (strcmp(triggerNames[i], sourceName)) continue;

	/*
	 * For some reason, the TRIGGERVERSION stuff includes the name of
	 * the package which the trigger is based on. We need to skip
	 * over that here. I suspect that we'll change our minds on this
	 * and remove that, so I'm going to just 'do the right thing'.
	 */
	skip = strlen(triggerNames[i]);
	if (!strncmp(triggerEVR[i], triggerNames[i], skip) &&
	    (triggerEVR[i][skip] == '-'))
	    skip++;
	else
	    skip = 0;

	if (!headerMatchesDepFlags(sourceH, triggerNames[i],
		triggerEVR[i] + skip, triggerFlags[i]))
	    continue;

	if (!(	hge(triggeredH, RPMTAG_TRIGGERINDEX, &tit,
		       (void **) &triggerIndices, NULL) &&
		hge(triggeredH, RPMTAG_TRIGGERSCRIPTS, &tst,
		       (void **) &triggerScripts, NULL) &&
		hge(triggeredH, RPMTAG_TRIGGERSCRIPTPROG, &tpt,
		       (void **) &triggerProgs, NULL))
	    )
	    continue;

	xx = headerNVR(triggeredH, &triggerPackageName, NULL, NULL);

	{   int arg1;
	    int index;

	    arg1 = rpmdbCountPackages(ts->rpmdb, triggerPackageName);
	    if (arg1 < 0) {
		/* XXX W2DO? same as "execution of script failed" */
		rc = RPMRC_FAIL;
	    } else {
		arg1 += psm->countCorrection;
		index = triggerIndices[i];
		if (triggersAlreadyRun == NULL ||
		    triggersAlreadyRun[index] == 0)
		{
		    rc = runScript(psm, triggeredH, "%trigger", 1,
			    triggerProgs + index, triggerScripts[index], 
			    arg1, arg2);
		    if (triggersAlreadyRun != NULL)
			triggersAlreadyRun[index] = 1;
		}
	    }
	}

	triggerIndices = hfd(triggerIndices, tit);
	triggerScripts = hfd(triggerScripts, tst);
	triggerProgs = hfd(triggerProgs, tpt);

	/*
	 * Each target/source header pair can only result in a single
	 * script being run.
	 */
	break;
    }

    triggerNames = hfd(triggerNames, tnt);
    triggerFlags = hfd(triggerFlags, tft);
    triggerEVR = hfd(triggerEVR, tvt);

    return rc;
}

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param psm		package state machine data
 * @return		0 on success, 1 on error
 */
static int runTriggers(PSM_t psm)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies psm, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    int numPackage;
    rpmRC rc = RPMRC_OK;

    numPackage = rpmdbCountPackages(ts->rpmdb, fi->name) + psm->countCorrection;
    if (numPackage < 0)
	return 1;

    if (fi->h != NULL)	/* XXX can't happen */
    {	Header triggeredH;
	rpmdbMatchIterator mi;
	int countCorrection = psm->countCorrection;

	psm->countCorrection = 0;
	mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, fi->name, 0);
	while((triggeredH = rpmdbNextIterator(mi)) != NULL) {
	    rc |= handleOneTrigger(psm, fi->h, triggeredH, numPackage, NULL);
	}

	mi = rpmdbFreeIterator(mi);
	psm->countCorrection = countCorrection;
    }

    return rc;
}

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @return		0 on success, 1 on error
 */
static int runImmedTriggers(PSM_t psm)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies psm, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    HGE_t hge = fi->hge;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    const char ** triggerNames;
    int numTriggers;
    int_32 * triggerIndices;
    rpmTagType tnt, tit;
    int numTriggerIndices;
    unsigned char * triggersRun;
    rpmRC rc = RPMRC_OK;

    if (fi->h == NULL)	return 0;	/* XXX can't happen */

    if (!(	hge(fi->h, RPMTAG_TRIGGERNAME, &tnt,
			(void **) &triggerNames, &numTriggers) &&
		hge(fi->h, RPMTAG_TRIGGERINDEX, &tit,
			(void **) &triggerIndices, &numTriggerIndices))
	)
	return 0;

    triggersRun = alloca(sizeof(*triggersRun) * numTriggerIndices);
    memset(triggersRun, 0, sizeof(*triggersRun) * numTriggerIndices);

    {	Header sourceH = NULL;
	int i;

	for (i = 0; i < numTriggers; i++) {
	    rpmdbMatchIterator mi;

	    if (triggersRun[triggerIndices[i]] != 0) continue;
	
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, triggerNames[i], 0);

	    while((sourceH = rpmdbNextIterator(mi)) != NULL) {
		rc |= handleOneTrigger(psm, sourceH, fi->h, 
				rpmdbGetIteratorCount(mi),
				triggersRun);
	    }

	    mi = rpmdbFreeIterator(mi);
	}
    }
    triggerIndices = hfd(triggerIndices, tit);
    triggerNames = hfd(triggerNames, tnt);
    return rc;
}

/*@observer@*/ static const char *const pkgStageString(pkgStage a)
	/*@*/
{
    switch(a) {
    case PSM_UNKNOWN:		return "unknown";

    case PSM_PKGINSTALL:	return "  install";
    case PSM_PKGERASE:		return "    erase";
    case PSM_PKGCOMMIT:		return "   commit";
    case PSM_PKGSAVE:		return "repackage";

    case PSM_INIT:		return "init";
    case PSM_PRE:		return "pre";
    case PSM_PROCESS:		return "process";
    case PSM_POST:		return "post";
    case PSM_UNDO:		return "undo";
    case PSM_FINI:		return "fini";

    case PSM_CREATE:		return "create";
    case PSM_NOTIFY:		return "notify";
    case PSM_DESTROY:		return "destroy";
    case PSM_COMMIT:		return "commit";

    case PSM_CHROOT_IN:		return "chrootin";
    case PSM_CHROOT_OUT:	return "chrootout";
    case PSM_SCRIPT:		return "script";
    case PSM_TRIGGERS:		return "triggers";
    case PSM_IMMED_TRIGGERS:	return "immedtriggers";

    case PSM_RPMIO_FLAGS:	return "rpmioflags";

    case PSM_RPMDB_LOAD:	return "rpmdbload";
    case PSM_RPMDB_ADD:		return "rpmdbadd";
    case PSM_RPMDB_REMOVE:	return "rpmdbremove";

    default:			return "???";
    }
    /*@noteached@*/
}

/**
 * @todo Packages w/o files never get a callback, hence don't get displayed
 * on install with -v.
 */
/*@-nullpass@*/ /* FIX: testing null annotation for fi->h */
int psmStage(PSM_t psm, pkgStage stage)
{
    const rpmTransactionSet ts = psm->ts;
    TFI_t fi = psm->fi;
    HGE_t hge = fi->hge;
    HME_t hme = fi->hme;
    HFD_t hfd = (fi->hfd ? fi->hfd : headerFreeData);
    rpmRC rc = psm->rc;
    int saveerrno;
    int xx;

    /*@-branchstate@*/
    switch (stage) {
    case PSM_UNKNOWN:
	break;
    case PSM_INIT:
	rpmMessage(RPMMESS_DEBUG, _("%s: %s-%s-%s has %d files, test = %d\n"),
		psm->stepName, fi->name, fi->version, fi->release,
		fi->fc, (ts->transFlags & RPMTRANS_FLAG_TEST));

	/*
	 * When we run scripts, we pass an argument which is the number of 
	 * versions of this package that will be installed when we are
	 * finished.
	 */
	psm->npkgs_installed = rpmdbCountPackages(ts->rpmdb, fi->name);
	if (psm->npkgs_installed < 0) {
	    rc = RPMRC_FAIL;
	    break;
	}

	if (psm->goal == PSM_PKGINSTALL) {
	    psm->scriptArg = psm->npkgs_installed + 1;

assert(psm->mi == NULL);
	    psm->mi = rpmtsInitIterator(ts, RPMTAG_NAME, fi->name, 0);
	    xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_VERSION,
			RPMMIRE_DEFAULT, fi->version);
	    xx = rpmdbSetIteratorRE(psm->mi, RPMTAG_RELEASE,
			RPMMIRE_DEFAULT, fi->release);

	    while ((psm->oh = rpmdbNextIterator(psm->mi))) {
		fi->record = rpmdbGetIteratorOffset(psm->mi);
		if (ts->transFlags & RPMTRANS_FLAG_MULTILIB)
		    psm->oh = headerCopy(psm->oh);
		else
		    psm->oh = NULL;
		/*@loopbreak@*/ break;
	    }
	    psm->mi = rpmdbFreeIterator(psm->mi);
	    rc = RPMRC_OK;

	    if (fi->fc > 0 && fi->fstates == NULL) {
		fi->fstates = xmalloc(sizeof(*fi->fstates) * fi->fc);
		memset(fi->fstates, RPMFILE_STATE_NORMAL, fi->fc);
	    }

	    if (fi->fc <= 0)				break;
	    if (ts->transFlags & RPMTRANS_FLAG_JUSTDB)	break;
	
	    /*
	     * Old format relocateable packages need the entire default
	     * prefix stripped to form the cpio list, while all other packages
	     * need the leading / stripped.
	     */
	    {   const char * p;
		rc = hge(fi->h, RPMTAG_DEFAULTPREFIX, NULL, (void **) &p, NULL);
		fi->striplen = (rc ? strlen(p) + 1 : 1); 
	    }
	    fi->mapflags =
		CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;
	
	    if (headerIsEntry(fi->h, RPMTAG_ORIGBASENAMES))
		buildOrigFileList(fi->h, &fi->apath, NULL);
	    else
		rpmBuildFileList(fi->h, &fi->apath, NULL);
	
	    if (fi->fuser == NULL)
		xx = hge(fi->h, RPMTAG_FILEUSERNAME, NULL,
				(void **) &fi->fuser, NULL);
	    if (fi->fgroup == NULL)
		xx = hge(fi->h, RPMTAG_FILEGROUPNAME, NULL,
				(void **) &fi->fgroup, NULL);
	    if (fi->fuids == NULL)
		fi->fuids = xcalloc(sizeof(*fi->fuids), fi->fc);
	    if (fi->fgids == NULL)
		fi->fgids = xcalloc(sizeof(*fi->fgids), fi->fc);
	    rc = RPMRC_OK;
	}
	if (psm->goal == PSM_PKGERASE || psm->goal == PSM_PKGSAVE) {
	    psm->scriptArg = psm->npkgs_installed - 1;
	
	    /* Retrieve installed header. */
	    rc = psmStage(psm, PSM_RPMDB_LOAD);
	}
	if (psm->goal == PSM_PKGSAVE) {
	    /* Open output package for writing. */
	    {	const char * bfmt = rpmGetPath("%{_repackage_name_fmt}", NULL);
		const char * pkgbn =
			headerSprintf(fi->h, bfmt, rpmTagTable, rpmHeaderFormats, NULL);

		bfmt = _free(bfmt);
		psm->pkgURL = rpmGenPath("%{?_repackage_root:%{_repackage_root}}",
					 "%{?_repackage_dir:%{_repackage_dir}}",
					pkgbn);
		pkgbn = _free(pkgbn);
		(void) urlPath(psm->pkgURL, &psm->pkgfn);
		psm->fd = Fopen(psm->pkgfn, "w.ufdio");
		if (psm->fd == NULL || Ferror(psm->fd)) {
		    rc = RPMRC_FAIL;
		    break;
		}
	    }
	}
	break;
    case PSM_PRE:
	if (ts->transFlags & RPMTRANS_FLAG_TEST)	break;

	/* Change root directory if requested and not already done. */
	rc = psmStage(psm, PSM_CHROOT_IN);

	if (psm->goal == PSM_PKGINSTALL) {
	    psm->scriptTag = RPMTAG_PREIN;
	    psm->progTag = RPMTAG_PREINPROG;

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERPREIN)) {
		/* XXX FIXME: implement %triggerprein. */
	    }

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOPRE)) {
		rc = psmStage(psm, PSM_SCRIPT);
		if (rc) {
		    rpmError(RPMERR_SCRIPT,
			_("%s: %s scriptlet failed (%d), skipping %s-%s-%s\n"),
			psm->stepName, tag2sln(psm->scriptTag), rc,
			fi->name, fi->version, fi->release);
		    break;
		}
	    }
	}

	if (psm->goal == PSM_PKGERASE) {
	    psm->scriptTag = RPMTAG_PREUN;
	    psm->progTag = RPMTAG_PREUNPROG;
	    psm->sense = RPMSENSE_TRIGGERUN;
	    psm->countCorrection = -1;

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERUN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = psmStage(psm, PSM_TRIGGERS);
		if (rc) break;

		/* Run triggers in this package other package(s) set off. */
		rc = psmStage(psm, PSM_IMMED_TRIGGERS);
		if (rc) break;
	    }

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOPREUN))
		rc = psmStage(psm, PSM_SCRIPT);
	}
	if (psm->goal == PSM_PKGSAVE) {
	    /* Regenerate original header. */
	    {	void * uh = NULL;
		int_32 uht, uhc;

		if (headerGetEntry(fi->h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)) {
		    psm->oh = headerCopyLoad(uh);
		    uh = hfd(uh, uht);
		} else {
		    psm->oh = headerLink(fi->h);
		}
	    }

	    /* Add remove transaction id to header. */
	    if (psm->oh)
	    {	int_32 tid = ts->id;
		xx = headerAddEntry(psm->oh, RPMTAG_REMOVETID,
			RPM_INT32_TYPE, &tid, 1);
	    }

	    /* Retrieve type of payload compression. */
	    /*@-nullstate@*/	/* FIX: psm->oh may be NULL */
	    rc = psmStage(psm, PSM_RPMIO_FLAGS);
	    /*@=nullstate@*/

	    /* Write the lead section into the package. */
	    {	int archnum = -1;
		int osnum = -1;
		struct rpmlead lead;

#ifndef	DYING
		rpmGetArchInfo(NULL, &archnum);
		rpmGetOsInfo(NULL, &osnum);
#endif

		memset(&lead, 0, sizeof(lead));
		/* XXX Set package version conditioned on noDirTokens. */
		lead.major = 4;
		lead.minor = 0;
		lead.type = RPMLEAD_BINARY;
		lead.archnum = archnum;
		lead.osnum = osnum;
		lead.signature_type = RPMSIGTYPE_HEADERSIG;

		{   char buf[256];
		    sprintf(buf, "%s-%s-%s", fi->name, fi->version, fi->release);
		    strncpy(lead.name, buf, sizeof(lead.name));
		}

		rc = writeLead(psm->fd, &lead);
		if (rc) {
		    rpmError(RPMERR_NOSPACE, _("Unable to write package: %s\n"),
			 Fstrerror(psm->fd));
		    rc = RPMRC_FAIL;
		    break;
		}
	    }

	    /* Write the signature section into the package. */
	    {	Header sig = headerRegenSigHeader(fi->h);
		rc = rpmWriteSignature(psm->fd, sig);
		sig = rpmFreeSignature(sig);
		if (rc) break;
	    }

	    /* Write the metadata section into the package. */
	    rc = headerWrite(psm->fd, psm->oh, HEADER_MAGIC_YES);
	    if (rc) break;
	}
	break;
    case PSM_PROCESS:
	if (ts->transFlags & RPMTRANS_FLAG_TEST)	break;

	if (psm->goal == PSM_PKGINSTALL) {
	    struct availablePackage * alp = fi->ap;
	    int i;

	    if (fi->fc <= 0)				break;
	    if (ts->transFlags & RPMTRANS_FLAG_JUSTDB)	break;

	    for (i = 0; i < fi->fc; i++) {
		uid_t uid;
		gid_t gid;

		uid = fi->uid;
		gid = fi->gid;
		if (fi->fuser && unameToUid(fi->fuser[i], &uid)) {
		    rpmMessage(RPMMESS_WARNING,
			_("user %s does not exist - using root\n"),
			fi->fuser[i]);
		    uid = 0;
		    /* XXX this diddles header memory. */
		    fi->fmodes[i] &= ~S_ISUID;	/* turn off the suid bit */
		}

		if (fi->fgroup && gnameToGid(fi->fgroup[i], &gid)) {
		    rpmMessage(RPMMESS_WARNING,
			_("group %s does not exist - using root\n"),
			fi->fgroup[i]);
		    gid = 0;
		    /* XXX this diddles header memory. */
		    fi->fmodes[i] &= ~S_ISGID;	/* turn off the sgid bit */
		}
		if (fi->fuids) fi->fuids[i] = uid;
		if (fi->fgids) fi->fgids[i] = gid;
	    }

	    /* Retrieve type of payload compression. */
	    rc = psmStage(psm, PSM_RPMIO_FLAGS);

	    if (alp->fd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }
	    /*@-nullpass@*/	/* LCL: alp->fd != NULL here. */
	    psm->cfd = Fdopen(fdDup(Fileno(alp->fd)), psm->rpmio_flags);
	    /*@=nullpass@*/
	    if (psm->cfd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }

	    rc = fsmSetup(fi->fsm, FSM_PKGINSTALL, ts, fi,
			psm->cfd, NULL, &psm->failedFile);
	    xx = fsmTeardown(fi->fsm);

	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->cfd);
	    psm->cfd = NULL;
	    /*@-mods@*/
	    errno = saveerrno; /* XXX FIXME: Fclose with libio destroys errno */
	    /*@=mods@*/

	    if (!rc)
		rc = psmStage(psm, PSM_COMMIT);

	    if (rc) {
		rpmError(RPMERR_CPIO,
			_("unpacking of archive failed%s%s: %s\n"),
			(psm->failedFile != NULL ? _(" on file ") : ""),
			(psm->failedFile != NULL ? psm->failedFile : ""),
			cpioStrerror(rc));
		rc = RPMRC_FAIL;
		break;
	    }
	    psm->what = RPMCALLBACK_INST_PROGRESS;
	    psm->amount = (fi->archiveSize ? fi->archiveSize : 100);
	    psm->total = psm->amount;
	    xx = psmStage(psm, PSM_NOTIFY);
	}
	if (psm->goal == PSM_PKGERASE) {

	    if (fi->fc <= 0)				break;
	    if (ts->transFlags & RPMTRANS_FLAG_JUSTDB)	break;
	    if (ts->transFlags & RPMTRANS_FLAG_APPLYONLY)	break;

	    psm->what = RPMCALLBACK_UNINST_START;
	    psm->amount = fi->fc;	/* XXX W2DO? looks wrong. */
	    psm->total = fi->fc;
	    xx = psmStage(psm, PSM_NOTIFY);

	    rc = fsmSetup(fi->fsm, FSM_PKGERASE, ts, fi,
			NULL, NULL, &psm->failedFile);
	    xx = fsmTeardown(fi->fsm);

	    psm->what = RPMCALLBACK_UNINST_STOP;
	    psm->amount = 0;		/* XXX W2DO? looks wrong. */
	    psm->total = fi->fc;
	    xx = psmStage(psm, PSM_NOTIFY);

	}
	if (psm->goal == PSM_PKGSAVE) {
	    fileAction * actions = fi->actions;
	    fileAction action = fi->action;

	    fi->action = FA_COPYOUT;
	    fi->actions = NULL;

	    if (psm->fd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }
	    /*@-nullpass@*/	/* LCL: psm->fd != NULL here. */
	    xx = Fflush(psm->fd);
	    psm->cfd = Fdopen(fdDup(Fileno(psm->fd)), psm->rpmio_flags);
	    /*@=nullpass@*/
	    if (psm->cfd == NULL) {	/* XXX can't happen */
		rc = RPMRC_FAIL;
		break;
	    }

	    rc = fsmSetup(fi->fsm, FSM_PKGBUILD, ts, fi, psm->cfd,
			NULL, &psm->failedFile);
	    xx = fsmTeardown(fi->fsm);

	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->cfd);
	    psm->cfd = NULL;
	    /*@-mods@*/
	    errno = saveerrno;
	    /*@=mods@*/

	    fi->action = action;
	    fi->actions = actions;
	}
	break;
    case PSM_POST:
	if (ts->transFlags & RPMTRANS_FLAG_TEST)	break;

	if (psm->goal == PSM_PKGINSTALL) {
	    int_32 installTime = (int_32) time(NULL);

	    if (fi->h == NULL) break;	/* XXX can't happen */
	    if (fi->fstates != NULL && fi->fc > 0)
		xx = headerAddEntry(fi->h, RPMTAG_FILESTATES, RPM_CHAR_TYPE,
				fi->fstates, fi->fc);

	    xx = headerAddEntry(fi->h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE,
				&installTime, 1);

	    if (ts->transFlags & RPMTRANS_FLAG_MULTILIB) {
		uint_32 multiLib, * newMultiLib, * p;

		if (hge(fi->h, RPMTAG_MULTILIBS, NULL,
				(void **) &newMultiLib, NULL) &&
		    hge(psm->oh, RPMTAG_MULTILIBS, NULL,
				(void **) &p, NULL))
		{
		    multiLib = *p;
		    multiLib |= *newMultiLib;
		    xx = hme(psm->oh, RPMTAG_MULTILIBS, RPM_INT32_TYPE,
				      &multiLib, 1);
		}
		rc = mergeFiles(fi, psm->oh, fi->h);
		if (rc) break;
	    }


	    /*
	     * If this package has already been installed, remove it from
	     * the database before adding the new one.
	     */
	    if (fi->record && !(ts->transFlags & RPMTRANS_FLAG_APPLYONLY)) {
		rc = psmStage(psm, PSM_RPMDB_REMOVE);
		if (rc) break;
	    }

	    rc = psmStage(psm, PSM_RPMDB_ADD);
	    if (rc) break;

	    psm->scriptTag = RPMTAG_POSTIN;
	    psm->progTag = RPMTAG_POSTINPROG;
	    psm->sense = RPMSENSE_TRIGGERIN;
	    psm->countCorrection = 0;

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOPOST)) {
		rc = psmStage(psm, PSM_SCRIPT);
		if (rc) break;
	    }
	    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERIN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = psmStage(psm, PSM_TRIGGERS);
		if (rc) break;

		/* Run triggers in this package other package(s) set off. */
		rc = psmStage(psm, PSM_IMMED_TRIGGERS);
		if (rc) break;
	    }

	    if (!(ts->transFlags & RPMTRANS_FLAG_APPLYONLY))
		rc = markReplacedFiles(psm);

	}
	if (psm->goal == PSM_PKGERASE) {

	    psm->scriptTag = RPMTAG_POSTUN;
	    psm->progTag = RPMTAG_POSTUNPROG;
	    psm->sense = RPMSENSE_TRIGGERPOSTUN;
	    psm->countCorrection = -1;

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOPOSTUN)) {
		rc = psmStage(psm, PSM_SCRIPT);
		/* XXX WTFO? postun failures don't cause erasure failure. */
	    }

	    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERPOSTUN)) {
		/* Run triggers in other package(s) this package sets off. */
		rc = psmStage(psm, PSM_TRIGGERS);
		if (rc) break;
	    }

	    if (!(ts->transFlags & RPMTRANS_FLAG_APPLYONLY))
		rc = psmStage(psm, PSM_RPMDB_REMOVE);
	}
	if (psm->goal == PSM_PKGSAVE) {
	}

	/* Restore root directory if changed. */
	xx = psmStage(psm, PSM_CHROOT_OUT);
	break;
    case PSM_UNDO:
	break;
    case PSM_FINI:
	/* Restore root directory if changed. */
	xx = psmStage(psm, PSM_CHROOT_OUT);

	if (psm->fd) {
	    saveerrno = errno; /* XXX FIXME: Fclose with libio destroys errno */
	    xx = Fclose(psm->fd);
	    psm->fd = NULL;
	    /*@-mods@*/
	    errno = saveerrno;
	    /*@=mods@*/
	}

	if (psm->goal == PSM_PKGSAVE) {
	    if (!rc) {
		rpmMessage(RPMMESS_VERBOSE, _("Wrote: %s\n"),
			(psm->pkgURL ? psm->pkgURL : "???"));
	    }
	}

	if (rc) {
	    if (psm->failedFile)
		rpmError(RPMERR_CPIO,
			_("%s failed on file %s: %s\n"),
			psm->stepName, psm->failedFile, cpioStrerror(rc));
	    else
		rpmError(RPMERR_CPIO, _("%s failed: %s\n"),
			psm->stepName, cpioStrerror(rc));
	}

	if (fi->h && (psm->goal == PSM_PKGERASE || psm->goal == PSM_PKGSAVE))
	    fi->h = headerFree(fi->h);
	psm->oh = headerFree(psm->oh);
	psm->pkgURL = _free(psm->pkgURL);
	psm->rpmio_flags = _free(psm->rpmio_flags);
	psm->failedFile = _free(psm->failedFile);

	fi->fgids = _free(fi->fgids);
	fi->fuids = _free(fi->fuids);
	fi->fgroup = hfd(fi->fgroup, -1);
	fi->fuser = hfd(fi->fuser, -1);
	fi->apath = _free(fi->apath);
	fi->fstates = _free(fi->fstates);
	break;

    case PSM_PKGINSTALL:
    case PSM_PKGERASE:
    case PSM_PKGSAVE:
	psm->goal = stage;
	psm->rc = RPMRC_OK;
	psm->stepName = pkgStageString(stage);

	rc = psmStage(psm, PSM_INIT);
	if (!rc) rc = psmStage(psm, PSM_PRE);
	if (!rc) rc = psmStage(psm, PSM_PROCESS);
	if (!rc) rc = psmStage(psm, PSM_POST);
	xx = psmStage(psm, PSM_FINI);
	break;
    case PSM_PKGCOMMIT:
	break;

    case PSM_CREATE:
	break;
    case PSM_NOTIFY:
	if (ts && ts->notify) {
	    /*@-noeffectuncon @*/ /* FIX: check rc */
	    (void) ts->notify(fi->h, psm->what, psm->amount, psm->total,
		(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	    /*@=noeffectuncon @*/
	}
	break;
    case PSM_DESTROY:
	break;
    case PSM_COMMIT:
	if (!(ts->transFlags & RPMTRANS_FLAG_PKGCOMMIT)) break;
	if (ts->transFlags & RPMTRANS_FLAG_APPLYONLY) break;

	rc = fsmSetup(fi->fsm, FSM_PKGCOMMIT, ts, fi,
			NULL, NULL, &psm->failedFile);
	xx = fsmTeardown(fi->fsm);
	break;

    case PSM_CHROOT_IN:
	/* Change root directory if requested and not already done. */
	if (ts->rootDir && !ts->chrootDone && !psm->chrootDone) {
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

	    xx = chdir("/");
	    /*@-superuser@*/
	    rc = chroot(ts->rootDir);
	    /*@=superuser@*/
	    psm->chrootDone = ts->chrootDone = 1;
	    if (ts->rpmdb != NULL) ts->rpmdb->db_chrootDone = 1;
	    /*@-onlytrans@*/
	    /*@-mods@*/
	    chroot_prefix = ts->rootDir;
	    /*@=mods@*/
	    /*@=onlytrans@*/
	}
	break;
    case PSM_CHROOT_OUT:
	/* Restore root directory if changed. */
	if (psm->chrootDone) {
	    /*@-superuser@*/
	    rc = chroot(".");
	    /*@=superuser@*/
	    psm->chrootDone = ts->chrootDone = 0;
	    if (ts->rpmdb != NULL) ts->rpmdb->db_chrootDone = 0;
	    /*@-mods@*/
	    chroot_prefix = NULL;
	    /*@=mods@*/
	    xx = chdir(ts->currDir);
	}
	break;
    case PSM_SCRIPT:	/* Run current package scriptlets. */
	rc = runInstScript(psm);
	break;
    case PSM_TRIGGERS:
	/* Run triggers in other package(s) this package sets off. */
	rc = runTriggers(psm);
	break;
    case PSM_IMMED_TRIGGERS:	
	/* Run triggers in this package other package(s) set off. */
	rc = runImmedTriggers(psm);
	break;

    case PSM_RPMIO_FLAGS:
    {	const char * payload_compressor = NULL;
	char * t;

	if (!hge(fi->h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	psm->rpmio_flags = t = xmalloc(sizeof("w9.gzdio"));
	*t = '\0';
	t = stpcpy(t, ((psm->goal == PSM_PKGSAVE) ? "w9" : "r"));
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
	rc = RPMRC_OK;
    }	break;

    case PSM_RPMDB_LOAD:
assert(psm->mi == NULL);
	psm->mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));

	fi->h = rpmdbNextIterator(psm->mi);
	if (fi->h)
	    fi->h = headerLink(fi->h);
else {
fprintf(stderr, "*** PSM_RDB_LOAD: header #%u not found\n", fi->record);
}
	psm->mi = rpmdbFreeIterator(psm->mi);
	rc = (fi->h ? RPMRC_OK : RPMRC_FAIL);
	break;
    case PSM_RPMDB_ADD:
	if (ts->transFlags & RPMTRANS_FLAG_TEST)	break;
	if (fi->h != NULL)	/* XXX can't happen */
	rc = rpmdbAdd(ts->rpmdb, ts->id, fi->h);
	break;
    case PSM_RPMDB_REMOVE:
	if (ts->transFlags & RPMTRANS_FLAG_TEST)	break;
	rc = rpmdbRemove(ts->rpmdb, ts->id, fi->record);
	break;

    default:
	break;
    }
    /*@=branchstate@*/

    /*@-nullstate@*/	/* FIX: psm->oh and psm->fi->h may be NULL. */
    return rc;
    /*@=nullstate@*/
}
/*@=nullpass@*/
