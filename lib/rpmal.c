/** \ingroup rpmdep
 * \file lib/rpmal.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rpmds.h"
#include "depends.h"
#include "rpmal.h"

#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */

typedef /*@abstract@*/ struct fileIndexEntry_s *	fileIndexEntry;
typedef /*@abstract@*/ struct dirInfo_s *		dirInfo;
typedef /*@abstract@*/ struct availableIndexEntry_s *	availableIndexEntry;
typedef /*@abstract@*/ struct availableIndex_s *	availableIndex;

/*@access availableIndexEntry@*/
/*@access availableIndex@*/
/*@access fileIndexEntry@*/
/*@access dirInfo@*/
/*@access availableList@*/

/*@access availablePackage@*/
/*@access tsortInfo@*/

/*@access alKey@*/
/*@access alNum@*/
/*@access rpmDepSet@*/

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
/*@refcounted@*/
    Header h;			/*!< Package header. */
/*@dependent@*/
    const char * name;		/*!< Header name. */
/*@dependent@*/
    const char * version;	/*!< Header version. */
/*@dependent@*/
    const char * release;	/*!< Header release. */

/*@owned@*/ /*@null@*/
    rpmDepSet provides;		/*!< Provides: dependencies. */
/*@owned@*/ /*@null@*/
    rpmDepSet requires;		/*!< Requires: dependencies. */
/*@owned@*//*@null@*/
    const char ** baseNames;	/*!< Header file basenames. */
/*@dependent@*//*@null@*/
    int_32 * epoch;		/*!< Header epoch (if any). */
    int filesCount;		/*!< No. of files in header. */

#ifdef	DYING
    uint_32 multiLib;	/* MULTILIB */
#endif

};

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
struct availableIndexEntry_s {
/*@dependent@*/
    availablePackage package;	/*!< Containing package. */
/*@dependent@*/
    const char * entry;		/*!< Available item name. */
    size_t entryLen;		/*!< No. of bytes in name. */
    enum indexEntryType {
	IET_PROVIDES=1			/*!< A Provides: dependency. */
    } type;			/*!< Type of available item. */
};

/** \ingroup rpmdep
 * Index of all available items.
 */
struct availableIndex_s {
/*@null@*/
    availableIndexEntry index;	/*!< Array of available items. */
    int size;			/*!< No. of available items. */
};

/** \ingroup rpmdep
 * A file to be installed/removed.
 */
struct fileIndexEntry_s {
    alNum pkgNum;		/*!< Containing package index. */
    int fileFlags;	/* MULTILIB */
/*@dependent@*/ /*@null@*/
    const char * baseName;	/*!< File basename. */
};

/** \ingroup rpmdep
 * A directory to be installed/removed.
 */
struct dirInfo_s {
/*@owned@*/
    const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;		/*!< No. bytes in directory path. */
/*@owned@*/
    fileIndexEntry files;	/*!< Array of files in directory. */
    int numFiles;		/*!< No. files in directory. */
};

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct availableList_s {
/*@owned@*/ /*@null@*/
    availablePackage list;	/*!< Set of packages. */
    struct availableIndex_s index;	/*!< Set of available items. */
    int delta;			/*!< Delta for pkg list reallocation. */
    int size;			/*!< No. of pkgs in list. */
    int alloced;		/*!< No. of pkgs allocated for list. */
    int numDirs;		/*!< No. of directories. */
/*@owned@*/ /*@null@*/
    dirInfo dirs;		/*!< Set of directories. */
};

/*@unchecked@*/
static int _al_debug = 0;

/**
 * Destroy available item index.
 * @param al		available list
 */
static void alFreeIndex(availableList al)
	/*@modifies al @*/
{
    if (al->index.size) {
	al->index.index = _free(al->index.index);
	al->index.size = 0;
    }
}

int alGetSize(const availableList al)
{
    return al->size;
}

static inline alNum alKey2Num(/*@unused@*/ /*@null@*/ const availableList al,
		/*@null@*/ alKey pkgKey)
	/*@*/
{
    /*@-nullret -temptrans -retalias @*/
    return ((alNum)pkgKey);
    /*@=nullret =temptrans =retalias @*/
}

static inline alKey alNum2Key(/*@unused@*/ /*@null@*/ const availableList al,
		/*@null@*/ alNum pkgNum)
	/*@*/
{
    /*@-nullret -temptrans -retalias @*/
    return ((alKey)pkgNum);
    /*@=nullret =temptrans =retalias @*/
}

availablePackage alGetPkg(const availableList al, alKey pkgKey)
{
    availablePackage alp = NULL;
    alNum pkgNum = alKey2Num(al, pkgKey);

    if (al != NULL && pkgNum >= 0 && pkgNum < al->size) {
	if (al->list != NULL)
	    alp = al->list + pkgNum;
    }
/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** alp[%d] %p\n", pkgNum, alp);
/*@=modfilesys@*/
    return alp;
}

#ifdef	DYING
int alGetMultiLib(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, alKey);
    return (alp != NULL ? alp->multiLib : 0);
}
#endif

int alGetFilesCount(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    return (alp != NULL ? alp->filesCount : 0);
}

rpmDepSet alGetProvides(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    /*@-retexpose@*/
    return (alp != NULL ? alp->provides : 0);
    /*@=retexpose@*/
}

rpmDepSet alGetRequires(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    /*@-retexpose@*/
    return (alp != NULL ? alp->requires : 0);
    /*@=retexpose@*/
}

Header alGetHeader(availableList al, alKey pkgKey, int unlink)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    Header h = NULL;

    if (alp != NULL && alp->h != NULL) {
	h = headerLink(alp->h, "alGetHeader");
	if (unlink) {
	    alp->h = headerFree(alp->h, "alGetHeader unlink");
	    alp->h = NULL;
	}
    }
    return h;
}

char * alGetNVR(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    char * pkgNVR = NULL;

    if (alp != NULL) {
	char * t;
	t = xcalloc(1,	strlen(alp->name) +
			strlen(alp->version) +
			strlen(alp->release) + sizeof("--"));
	pkgNVR = t;
	t = stpcpy(t, alp->name);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->version);
	t = stpcpy(t, "-");
	t = stpcpy(t, alp->release);
    }
    return pkgNVR;
}

availableList alCreate(int delta)
{
    availableList al = xcalloc(1, sizeof(*al));

    al->delta = delta;
    al->size = 0;
    al->list = xcalloc(al->delta, sizeof(*al->list));
    al->alloced = al->delta;

    al->index.index = NULL;
    al->index.size = 0;

    al->numDirs = 0;
    al->dirs = NULL;
    return al;
}

availableList alFree(availableList al)
{
    HFD_t hfd = headerFreeData;
    availablePackage p;
    int i;

    if (al == NULL)
	return NULL;

    if ((p = al->list) != NULL)
    for (i = 0; i < al->size; i++, p++) {

	p->provides = dsFree(p->provides);
	p->requires = dsFree(p->requires);

	p->baseNames = hfd(p->baseNames, -1);
	p->h = headerFree(p->h, "alFree");

    }

    if (al->dirs != NULL)
    for (i = 0; i < al->numDirs; i++) {
	al->dirs[i].dirName = _free(al->dirs[i].dirName);
	al->dirs[i].files = _free(al->dirs[i].files);
    }

    al->dirs = _free(al->dirs);
    al->numDirs = 0;
    al->list = _free(al->list);
    al->alloced = 0;
    alFreeIndex(al);
    return NULL;
}

/**
 * Compare two directory info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int dirInfoCompare(const void * one, const void * two)	/*@*/
{
    /*@-castexpose@*/
    const dirInfo a = (const dirInfo) one;
    const dirInfo b = (const dirInfo) two;
    /*@=castexpose@*/
    int lenchk = a->dirNameLen - b->dirNameLen;

    if (lenchk)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->dirName, b->dirName);
}

void alDelPackage(availableList al, alKey pkgKey)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    availablePackage alp;
    alNum pkgNum = alKey2Num(al, pkgKey);

    /*@-nullptrarith@*/ /* FIX: al->list might be NULL */
    alp = al->list + pkgNum;
    /*@=nullptrarith@*/

/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** del %p[%d] %s-%s-%s\n", al->list, pkgNum, alp->name, alp->version, alp->release);
/*@=modfilesys@*/

    if (alp->baseNames != NULL && alp->filesCount > 0) {
	int origNumDirs = al->numDirs;
	const char ** dirNames;
	int_32 numDirs;
	rpmTagType dnt;
	int dirNum;
	dirInfo dirNeedle =
		memset(alloca(sizeof(*dirNeedle)), 0, sizeof(*dirNeedle));
	dirInfo dirMatch;
	int last;
	int i, xx;

	xx = hge(alp->h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, &numDirs);

	/* XXX FIXME: We ought to relocate the directory list here */

	if (al->dirs != NULL)
	for (dirNum = al->numDirs - 1; dirNum >= 0; dirNum--) {
	    dirNeedle->dirName = (char *) dirNames[dirNum];
	    dirNeedle->dirNameLen = strlen(dirNames[dirNum]);
	    dirMatch = bsearch(dirNeedle, al->dirs, al->numDirs,
			       sizeof(*dirNeedle), dirInfoCompare);
	    if (dirMatch == NULL)
		continue;
	    last = dirMatch->numFiles;
	    for (i = dirMatch->numFiles - 1; i >= 0; i--) {
		if (dirMatch->files[i].pkgNum != pkgNum)
		    /*@innercontinue@*/ continue;
		dirMatch->numFiles--;
		if (i > dirMatch->numFiles)
		    /*@innercontinue@*/ continue;
		memmove(dirMatch->files, dirMatch->files+1,
			(dirMatch->numFiles - i));
	    }
	    if (dirMatch->numFiles > 0) {
		if (last > i)
		    dirMatch->files = xrealloc(dirMatch->files,
			dirMatch->numFiles * sizeof(*dirMatch->files));
		continue;
	    }
	    dirMatch->files = _free(dirMatch->files);
	    dirMatch->dirName = _free(dirMatch->dirName);
	    al->numDirs--;
	    if (dirNum > al->numDirs)
		continue;
	    memmove(dirMatch, dirMatch+1, (al->numDirs - dirNum));
	}

	if (origNumDirs > al->numDirs) {
	    if (al->numDirs > 0)
		al->dirs = xrealloc(al->dirs, al->numDirs * sizeof(*al->dirs));
	    else
		al->dirs = _free(al->dirs);
	}

	dirNames = hfd(dirNames, dnt);
    }
    alp->h = headerFree(alp->h, "alDelPackage");
    memset(alp, 0, sizeof(*alp));	/* XXX trash and burn */
    /*@-nullstate@*/ /* FIX: al->list->h may be NULL */
    return;
    /*@=nullstate@*/
}

alKey alAddPackage(availableList al, alKey pkgKey, Header h)
	/*@modifies al, h @*/
{
    int scareMem = 1;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType dnt, bnt;
    availablePackage alp;
    alNum pkgNum = alKey2Num(al, pkgKey);
    int xx;
#ifdef	DYING
    uint_32 multiLibMask = 0;
    uint_32 * pp = NULL;
#endif

    if (pkgNum >= 0 && pkgNum < al->size) {
	alDelPackage(al, pkgKey);
    } else {
	if (al->size == al->alloced) {
	    al->alloced += al->delta;
	    al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
	}
	pkgNum = al->size++;
    }

    /*@-nullptrarith@*/
    alp = al->list + pkgNum;
    /*@=nullptrarith@*/

    alp->h = headerLink(h, "alAddPackage");
#ifdef	DYING
    alp->multiLib = 0;	/* MULTILIB */
#endif

    xx = headerNVR(alp->h, &alp->name, &alp->version, &alp->release);

/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** add %p[%d] %s-%s-%s\n", al->list, pkgNum, alp->name, alp->version, alp->release);
/*@=modfilesys@*/

#ifdef	DYING
  { uint_32 *pp = NULL;
    /* XXX This should be added always so that packages look alike.
     * XXX However, there is logic in files.c/depends.c that checks for
     * XXX existence (rather than value) that will need to change as well.
     */
    if (hge(alp->h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
	multiLibMask = *pp;

    if (multiLibMask) {
	for (i = 0; i < pkgNum - 1; i++) {
	    if (!strcmp (alp->name, al->list[i].name)
		&& hge(al->list[i].h, RPMTAG_MULTILIBS, NULL,
				  (void **) &pp, NULL)
		&& !rpmVersionCompare(alp->h, al->list[i].h)
		&& *pp && !(*pp & multiLibMask))
		    alp->multiLib = multiLibMask;
	}
    }
  }
#endif

    if (!hge(h, RPMTAG_EPOCH, NULL, (void **) &alp->epoch, NULL))
	alp->epoch = NULL;

    alp->provides = dsNew(h, RPMTAG_PROVIDENAME, scareMem);
    alp->requires = dsNew(h, RPMTAG_REQUIRENAME, scareMem);

    if (!hge(h, RPMTAG_BASENAMES, &bnt, (void **)&alp->baseNames, &alp->filesCount))
    {
	alp->filesCount = 0;
	alp->baseNames = NULL;
    } else {
	int_32 * dirIndexes;
	const char ** dirNames;
	int_32 numDirs;
	uint_32 * fileFlags = NULL;
	int * dirMapping;
	dirInfo dirNeedle =
		memset(alloca(sizeof(*dirNeedle)), 0, sizeof(*dirNeedle));
	dirInfo dirMatch;
	int first, last, fileNum, dirNum;
	int origNumDirs;

	xx = hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, &numDirs);
	xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
	xx = hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL);

	/* XXX FIXME: We ought to relocate the directory list here */

	dirMapping = alloca(sizeof(*dirMapping) * numDirs);

	/* allocated enough space for all the directories we could possible
	   need to add */
	al->dirs = xrealloc(al->dirs,
			(al->numDirs + numDirs) * sizeof(*al->dirs));
	origNumDirs = al->numDirs;

	for (dirNum = 0; dirNum < numDirs; dirNum++) {
	    dirNeedle->dirName = (char *) dirNames[dirNum];
	    dirNeedle->dirNameLen = strlen(dirNames[dirNum]);
	    dirMatch = bsearch(dirNeedle, al->dirs, origNumDirs,
			       sizeof(*dirNeedle), dirInfoCompare);
	    if (dirMatch) {
		dirMapping[dirNum] = dirMatch - al->dirs;
	    } else {
		dirMapping[dirNum] = al->numDirs;
		dirMatch = al->dirs + al->numDirs;
		dirMatch->dirName = xstrdup(dirNames[dirNum]);
		dirMatch->dirNameLen = strlen(dirNames[dirNum]);
		dirMatch->files = NULL;
		dirMatch->numFiles = 0;
		al->numDirs++;
	    }
	}

	dirNames = hfd(dirNames, dnt);

	for (first = 0; first < alp->filesCount; first = last + 1) {
	    for (last = first; (last + 1) < alp->filesCount; last++) {
		if (dirIndexes[first] != dirIndexes[last + 1])
		    /*@innerbreak@*/ break;
	    }

	    dirMatch = al->dirs + dirMapping[dirIndexes[first]];
	    dirMatch->files = xrealloc(dirMatch->files,
		    (dirMatch->numFiles + last - first + 1) *
			sizeof(*dirMatch->files));
	    if (alp->baseNames != NULL)	/* XXX can't happen */
	    for (fileNum = first; fileNum <= last; fileNum++) {
		/*@-assignexpose@*/
		dirMatch->files[dirMatch->numFiles].baseName =
		    alp->baseNames[fileNum];
		/*@=assignexpose@*/
		dirMatch->files[dirMatch->numFiles].pkgNum = pkgNum;
		dirMatch->files[dirMatch->numFiles].fileFlags =
				fileFlags[fileNum];
		dirMatch->numFiles++;
	    }
	}

	/* XXX should we realloc to smaller size? */
	if (origNumDirs + al->numDirs)
	    qsort(al->dirs, al->numDirs, sizeof(*al->dirs), dirInfoCompare);

    }

    /*@-compdef@*/ /* FIX: al->list->relocs-?{oldPath,newPath} undefined */
    alFreeIndex(al);
    /*@=compdef@*/

assert(((alNum)(alp - al->list)) == pkgNum);
    return ((alKey)(alp - al->list));
}

/**
 * Compare two available index entries by name (qsort/bsearch).
 * @param one		1st available index entry
 * @param two		2nd available index entry
 * @return		result of comparison
 */
static int indexcmp(const void * one, const void * two)		/*@*/
{
    /*@-castexpose@*/
    const availableIndexEntry a = (const availableIndexEntry) one;
    const availableIndexEntry b = (const availableIndexEntry) two;
    /*@=castexpose@*/
    int lenchk = a->entryLen - b->entryLen;

    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

void alMakeIndex(availableList al)
{
    availableIndex ai = &al->index;
    int i, j, k;

    if (ai->size || al->list == NULL) return;

    for (i = 0; i < al->size; i++)
	if (al->list[i].provides != NULL)
	    ai->size += al->list[i].provides->Count;

    if (ai->size) {
	ai->index = xcalloc(ai->size, sizeof(*ai->index));

	k = 0;
	for (i = 0; i < al->size; i++) {
	    if (al->list[i].provides != NULL)
	    for (j = 0; j < al->list[i].provides->Count; j++) {

#ifdef	NOTNOW	/* XXX FIXME: multilib colored dependency search */
		/* If multilib install, skip non-multilib provides. */
		if (al->list[i].multiLib &&
		    !isDependsMULTILIB(al->list[i].provides->Flags[j])) {
			ai->size--;
			/*@innercontinue@*/ continue;
		}
#endif

		ai->index[k].package = al->list + i;
		/*@-assignexpose@*/
		ai->index[k].entry = al->list[i].provides->N[j];
		/*@=assignexpose@*/
		ai->index[k].entryLen = strlen(al->list[i].provides->N[j]);
		ai->index[k].type = IET_PROVIDES;
		k++;
	    }
	}

	qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
    }
}

alKey * alAllFileSatisfiesDepend(const availableList al, const rpmDepSet ds)
{
    int i, found = 0;
    const char * dirName;
    const char * baseName;
    dirInfo dirNeedle =
		memset(alloca(sizeof(*dirNeedle)), 0, sizeof(*dirNeedle));
    dirInfo dirMatch;
    alKey * ret = NULL;
    const char * fileName;

    if ((fileName = dsiGetN(ds)) == NULL || *fileName != '/')
	return NULL;

    /* Solaris 2.6 bsearch sucks down on this. */
    if (al->numDirs == 0 || al->dirs == NULL || al->list == NULL)
	return NULL;

    {	char * t;
	dirName = t = xstrdup(fileName);
	if ((t = strrchr(t, '/')) != NULL) {
	    t++;		/* leave the trailing '/' */
	    *t = '\0';
	}
    }

    dirNeedle->dirName = (char *) dirName;
    dirNeedle->dirNameLen = strlen(dirName);
    dirMatch = bsearch(dirNeedle, al->dirs, al->numDirs,
		       sizeof(*dirNeedle), dirInfoCompare);
    if (dirMatch == NULL)
	goto exit;

    /* rewind to the first match */
    while (dirMatch > al->dirs && dirInfoCompare(dirMatch-1, dirNeedle) == 0)
	dirMatch--;

    if ((baseName = strrchr(fileName, '/')) == NULL)
	goto exit;
    baseName++;

    for (found = 0, ret = NULL;
	 dirMatch <= al->dirs + al->numDirs &&
		dirInfoCompare(dirMatch, dirNeedle) == 0;
	 dirMatch++)
    {
	/* XXX FIXME: these file lists should be sorted and bsearched */
	for (i = 0; i < dirMatch->numFiles; i++) {
	    if (dirMatch->files[i].baseName == NULL ||
			strcmp(dirMatch->files[i].baseName, baseName))
		/*@innercontinue@*/ continue;

#ifdef	NOTNOW	/* XXX FIXME: multilib colored dependency search */
	    /*
	     * If a file dependency would be satisfied by a file
	     * we are not going to install, skip it.
	     */
	    if (al->list[dirMatch->files[i].pkgNum].multiLib &&
			!isFileMULTILIB(dirMatch->files[i].fileFlags))
	        /*@innercontinue@*/ continue;
#endif

	    dsiNotify(ds, _("(added files)"), 0);

	    ret = xrealloc(ret, (found+2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found++] = alNum2Key(al, dirMatch->files[i].pkgNum);
	    /*@innerbreak@*/ break;
	}
    }

exit:
    dirName = _free(dirName);
    /*@-mods@*/		/* AOK: al->list not modified through ret alias. */
    if (ret)
	ret[found] = NULL;
    /*@=mods@*/
    return ret;
}

#ifdef	DYING
/**
 * Check added package file lists for first package that provides a file.
 * @param al		available list
 * @param ds		dependency
 * @return		available package pointer
 */
/*@unused@*/ static /*@dependent@*/ /*@null@*/ alKey
alFileSatisfiesDepend(const availableList al, const rpmDepSet ds)
	/*@*/
{
    alKey ret = NULL;
    alKey * tmp = alAllFileSatisfiesDepend(al, ds);

    if (tmp) {
	ret = tmp[0];
	tmp = _free(tmp);
    }
    return ret;
}
#endif	/* DYING */

alKey *
alAllSatisfiesDepend(const availableList al, const rpmDepSet ds)
{
    availableIndexEntry needle =
		memset(alloca(sizeof(*needle)), 0, sizeof(*needle));
    availableIndexEntry match;
    alKey * ret = NULL;
    const char * KName;
    availablePackage alp;
    int rc, found;

    if ((KName = dsiGetN(ds)) == NULL)
	return ret;

    if (*KName == '/') {
	ret = alAllFileSatisfiesDepend(al, ds);
	/* XXX Provides: /path was broken with added packages (#52183). */
	if (ret != NULL && *ret != NULL)
	    return ret;
    }

    if (!al->index.size || al->index.index == NULL) return NULL;

    /*@-assignexpose@*/
    /*@-temptrans@*/
    needle->entry = KName;
    /*@=temptrans@*/
    needle->entryLen = strlen(KName);
    match = bsearch(needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), indexcmp);
    /*@=assignexpose@*/

    if (match == NULL) return NULL;

    /* rewind to the first match */
    while (match > al->index.index && indexcmp(match-1, needle) == 0)
	match--;

    for (ret = NULL, found = 0;
	 match <= al->index.index + al->index.size &&
		indexcmp(match, needle) == 0;
	 match++)
    {
	alp = match->package;
	rc = 0;
	switch (match->type) {
	case IET_PROVIDES:
	    if (alp->provides != NULL)
	    for (dsiInit(alp->provides) != NULL; dsiNext(alp->provides) >= 0;) {
		const char * PName;

		if ((PName = dsiGetN(alp->provides)) == NULL)
		    /*@innercontinue@*/ continue;

		/* Filter out provides that came along for the ride. */
		if (strcmp(PName, KName))
		    /*@innercontinue@*/ continue;

		rc = dsCompare(alp->provides, ds);
		if (rc)
		    /*@innerbreak@*/ break;
	    }
	    if (rc)
		dsiNotify(ds, _("(added provide)"), 0);
	    /*@switchbreak@*/ break;
	}

	/*@-branchstate@*/
	if (rc) {
	    ret = xrealloc(ret, (found + 2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found++] = ((alKey)(alp - al->list));
	}
	/*@=branchstate@*/
    }

    if (ret)
	ret[found] = NULL;

    return ret;
}

alKey alSatisfiesDepend(const availableList al, const rpmDepSet ds)
{
    alKey * tmp = alAllSatisfiesDepend(al, ds);

    if (tmp) {
	alKey ret = tmp[0];
	tmp = _free(tmp);
	return ret;
    }
    return RPMAL_NOMATCH;
}
