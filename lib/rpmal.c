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

/*@access rpmFNSet@*/
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
/*@dependent@*//*@null@*/
    int_32 * epoch;		/*!< Header epoch (if any). */

/*@owned@*/ /*@null@*/
    rpmDepSet provides;		/*!< Provides: dependencies. */
/*@owned@*/ /*@null@*/
    rpmDepSet requires;		/*!< Requires: dependencies. */
/*@owned@*//*@null@*/
    rpmFNSet fns;		/*!< File name set. */

#ifdef	DYING
    uint_32 multiLib;	/* MULTILIB */
#endif

};

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
struct availableIndexEntry_s {
/*@dependent@*/ /*@null@*/
    alKey pkgKey;		/*!< Containing package. */
/*@dependent@*/
    const char * entry;		/*!< Dependency name. */
    unsigned short entryLen;	/*!< No. of bytes in name. */
    unsigned short entryIx;	/*!< Dependency index. */
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
    int k;			/*!< Current index. */
};

/** \ingroup rpmdep
 * A file to be installed/removed.
 */
struct fileIndexEntry_s {
/*@dependent@*/
    const char * baseName;	/*!< File basename. */
    int baseNameLen;
    alNum pkgNum;		/*!< Containing package index. */
    int fileFlags;	/* MULTILIB */
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
    availableIndex ai = &al->index;
    if (ai->size > 0) {
	ai->index = _free(ai->index);
	ai->size = 0;
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

#ifndef	DYING
int alGetFilesCount(const availableList al, alKey pkgKey)
{
    availablePackage alp = alGetPkg(al, pkgKey);
    int_32 filesCount = 0;
    if (alp != NULL)
	if (alp->fns != NULL)
	    filesCount = alp->fns->Count;
    return filesCount;
}
#endif

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
    availableIndex ai = &al->index;

    al->delta = delta;
    al->size = 0;
    al->list = xcalloc(al->delta, sizeof(*al->list));
    al->alloced = al->delta;

    ai->index = NULL;
    ai->size = 0;

    al->numDirs = 0;
    al->dirs = NULL;
    return al;
}

availableList alFree(availableList al)
{
    availablePackage alp;
    dirInfo die;
    int i;

    if (al == NULL)
	return NULL;

    if ((alp = al->list) != NULL)
    for (i = 0; i < al->size; i++, alp++) {

	alp->provides = dsFree(alp->provides);
	alp->requires = dsFree(alp->requires);

	alp->fns = fnsFree(alp->fns);

	alp->h = headerFree(alp->h, "alFree");

    }

    if ((die = al->dirs) != NULL)
    for (i = 0; i < al->numDirs; i++, die++) {
	die->dirName = _free(die->dirName);
	die->files = _free(die->files);
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
static int dieCompare(const void * one, const void * two)
	/*@*/
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

/**
 * Compare two file info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int fieCompare(const void * one, const void * two)
	/*@*/
{
    /*@-castexpose@*/
    const fileIndexEntry a = (const fileIndexEntry) one;
    const fileIndexEntry b = (const fileIndexEntry) two;
    /*@=castexpose@*/
    int lenchk = a->baseNameLen - b->baseNameLen;

    if (lenchk)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->baseName, b->baseName);
}

void alDelPackage(availableList al, alKey pkgKey)
{
#ifdef	DYING
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
#endif
    availablePackage alp;
    rpmFNSet fns;
    alNum pkgNum = alKey2Num(al, pkgKey);

    /*@-nullptrarith@*/ /* FIX: al->list might be NULL */
    alp = al->list + pkgNum;
    /*@=nullptrarith@*/

/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** del %p[%d] %s-%s-%s\n", al->list, pkgNum, alp->name, alp->version, alp->release);
/*@=modfilesys@*/

    /* Delete directory/file info entries from added package list. */
    if ((fns = alp->fns) != NULL)
    if (fns->BN != NULL && fns->Count > 0) {
	int origNumDirs = al->numDirs;
#ifdef	DYING
	const char ** dirNames;
	int_32 numDirs;
	rpmTagType dnt;
#endif
	int dirNum;
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int last;
	int i;

#ifdef	DYING
	xx = hge(alp->h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, &numDirs);
#endif

	/* XXX FIXME: We ought to relocate the directory list here */

	if (al->dirs != NULL)
	if (fns->DN != NULL)
	for (dirNum = fns->DCount - 1; dirNum >= 0; dirNum--) {
	    fileIndexEntry fie;

	    /*@-assignexpose@*/
	    dieNeedle->dirName = (char *) fns->DN[dirNum];
	    /*@=assignexpose@*/
	    dieNeedle->dirNameLen = strlen(dieNeedle->dirName);
	    die = bsearch(dieNeedle, al->dirs, al->numDirs,
			       sizeof(*dieNeedle), dieCompare);
	    if (die == NULL)
		continue;

	    last = die->numFiles;
	    fie = die->files + last - 1;
	    for (i = last - 1; i >= 0; i--, fie--) {
		if (fie->pkgNum != pkgNum)
		    /*@innercontinue@*/ continue;
		die->numFiles--;
		if (i > die->numFiles)
		    /*@innercontinue@*/ continue;
		memmove(fie, fie+1, (die->numFiles - i));
	    }
	    if (die->numFiles > 0) {
		if (last > i)
		    die->files = xrealloc(die->files,
					die->numFiles * sizeof(*die->files));
		continue;
	    }
	    die->files = _free(die->files);
	    die->dirName = _free(die->dirName);
	    al->numDirs--;
	    if ((die - al->dirs) > al->numDirs)
		continue;
	    memmove(die, die+1, (al->numDirs - (die - al->dirs)));
	}

	if (origNumDirs > al->numDirs) {
	    if (al->numDirs > 0)
		al->dirs = xrealloc(al->dirs, al->numDirs * sizeof(*al->dirs));
	    else
		al->dirs = _free(al->dirs);
	}

#ifdef	DYING
	dirNames = hfd(dirNames, dnt);
#endif
    }

    alp->provides = dsFree(alp->provides);
    alp->requires = dsFree(alp->requires);
    alp->fns = fnsFree(alp->fns);
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
#ifdef	DYING
    HFD_t hfd = headerFreeData;
#endif
    availablePackage alp;
    alNum pkgNum = alKey2Num(al, pkgKey);
    int xx;
#ifdef	DYING
    uint_32 multiLibMask = 0;
    uint_32 * pp = NULL;
#endif
    rpmFNSet fns;

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

    alp->fns = fns = fnsNew(h, RPMTAG_BASENAMES, scareMem);

    if (fns && fns->Count > 0) {
	int * dirMapping;
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int first, last, dirNum;
	int origNumDirs;

	/* XXX FIXME: We ought to relocate the directory list here */

	dirMapping = alloca(sizeof(*dirMapping) * fns->DCount);

	/* allocated enough space for all the directories we could possible
	   need to add */
	al->dirs = xrealloc(al->dirs,
			(al->numDirs + fns->DCount) * sizeof(*al->dirs));
	origNumDirs = al->numDirs;

	if (fns->DN != NULL)
	for (dirNum = 0; dirNum < fns->DCount; dirNum++) {
	    /*@-assignexpose@*/
	    dieNeedle->dirName = (char *) fns->DN[dirNum];
	    /*@=assignexpose@*/
	    dieNeedle->dirNameLen = strlen(fns->DN[dirNum]);
	    die = bsearch(dieNeedle, al->dirs, origNumDirs,
			       sizeof(*dieNeedle), dieCompare);
	    if (die) {
		dirMapping[dirNum] = die - al->dirs;
	    } else {
		dirMapping[dirNum] = al->numDirs;
		die = al->dirs + al->numDirs;
		die->dirName = xstrdup(fns->DN[dirNum]);
		die->dirNameLen = strlen(die->dirName);
		die->files = NULL;
		die->numFiles = 0;
		al->numDirs++;
	    }
	}

#ifdef	DYING
	fns->DN = hfd(fns->DN, fns->DNt);
#endif

	last = 0;
	for (first = 0; first < fns->Count; first = last + 1) {
	    fileIndexEntry fie;

	    if (fns->DI == NULL)	/* XXX can't happen */
		continue;

	    for (last = first; (last + 1) < fns->Count; last++) {
		if (fns->DI[first] != fns->DI[last + 1])
		    /*@innerbreak@*/ break;
	    }

	    die = al->dirs + dirMapping[fns->DI[first]];
	    die->files = xrealloc(die->files,
		    (die->numFiles + last - first + 1) *
			sizeof(*die->files));

	    fie = die->files + die->numFiles;
	    for (fns->i = first; fns->i <= last; fns->i++) {
		if (fns->BN == NULL)	/* XXX can't happen */
		    /*@innercontinue@*/ continue;
		if (fns->Flags == NULL)	/* XXX can't happen */
		    /*@innercontinue@*/ continue;
		/*@-assignexpose@*/
		fie->baseName = fns->BN[fns->i];
		fie->baseNameLen = strlen(fns->BN[fns->i]);
		/*@=assignexpose@*/
		fie->pkgNum = pkgNum;
		fie->fileFlags = fns->Flags[fns->i];
		die->numFiles++;
		fie++;
	    }
	    qsort(die->files, die->numFiles, sizeof(*die->files), fieCompare);
	}

	/* XXX should we realloc al->dirs to actual size? */

	/* If any directories were added, resort the directory list. */
	if (origNumDirs != al->numDirs)
	    qsort(al->dirs, al->numDirs, sizeof(*al->dirs), dieCompare);

    }

    alFreeIndex(al);

assert(((alNum)(alp - al->list)) == pkgNum);
    return ((alKey)(alp - al->list));
}

/**
 * Compare two available index entries by name (qsort/bsearch).
 * @param one		1st available index entry
 * @param two		2nd available index entry
 * @return		result of comparison
 */
static int indexcmp(const void * one, const void * two)
	/*@*/
{
    /*@-castexpose@*/
    const availableIndexEntry a = (const availableIndexEntry) one;
    const availableIndexEntry b = (const availableIndexEntry) two;
    /*@=castexpose@*/
    int lenchk;

    lenchk = a->entryLen - b->entryLen;
    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

void alAddProvides(availableList al, alKey pkgKey, rpmDepSet provides)
{
    availableIndex ai = &al->index;
    int i = alKey2Num(al, pkgKey);

    if (provides == NULL || i < 0 || i >= al->size)
	return;
    if (ai->index == NULL || ai->k < 0 || ai->k >= ai->size)
	return;

    if (dsiInit(provides) != NULL)
    while (dsiNext(provides) >= 0) {
	const char * Name;

#ifdef	DYING	/* XXX FIXME: multilib colored dependency search */
	const int_32 Flags = dsiGetFlags(provides);

	/* If multilib install, skip non-multilib provides. */
	if (al->list[i].multiLib && !isDependsMULTILIB(Flags)) {
	    ai->size--;
	    /*@innercontinue@*/ continue;
	}
#endif

	if ((Name = dsiGetN(provides)) == NULL)
	    continue;	/* XXX can't happen */

	/*@-assignexpose@*/
	/*@-temptrans@*/
	ai->index[ai->k].pkgKey = pkgKey;
	/*@=temptrans@*/
	ai->index[ai->k].entry = Name;
	/*@=assignexpose@*/
	ai->index[ai->k].entryLen = strlen(Name);
assert(provides->i < 0x10000);
	ai->index[ai->k].entryIx = provides->i;
	ai->index[ai->k].type = IET_PROVIDES;
	ai->k++;
    }
}

void alMakeIndex(availableList al)
{
    availableIndex ai = &al->index;
    int i;

    if (ai->size || al->list == NULL) return;

    for (i = 0; i < al->size; i++)
	if (al->list[i].provides != NULL)
	    ai->size += al->list[i].provides->Count;

    if (ai->size) {
	ai->index = xcalloc(ai->size, sizeof(*ai->index));
	ai->k = 0;

	for (i = 0; i < al->size; i++)
	    alAddProvides(al, (alKey)i, al->list[i].provides);

	qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
    }
}

alKey * alAllFileSatisfiesDepend(const availableList al, const rpmDepSet ds)
{
    int found = 0;
    const char * dirName;
    const char * baseName;
    dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
    dirInfo die;
    fileIndexEntry fieNeedle =
		memset(alloca(sizeof(*fieNeedle)), 0, sizeof(*fieNeedle));
    fileIndexEntry fie;
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

    dieNeedle->dirName = (char *) dirName;
    dieNeedle->dirNameLen = strlen(dirName);
    die = bsearch(dieNeedle, al->dirs, al->numDirs,
		       sizeof(*dieNeedle), dieCompare);
    if (die == NULL)
	goto exit;

    /* rewind to the first match */
    while (die > al->dirs && dieCompare(die-1, dieNeedle) == 0)
	die--;

    if ((baseName = strrchr(fileName, '/')) == NULL)
	goto exit;
    baseName++;

    /*@-branchstate@*/ /* FIX: ret is a problem */
    for (found = 0, ret = NULL;
	 die <= al->dirs + al->numDirs && dieCompare(die, dieNeedle) == 0;
	 die++)
    {

	fieNeedle->baseName = baseName;
	fieNeedle->baseNameLen = strlen(fieNeedle->baseName);
	fie = bsearch(fieNeedle, die->files, die->numFiles,
		       sizeof(*fieNeedle), fieCompare);
	if (fie == NULL)
	    continue;	/* XXX shouldn't happen */

#ifdef	DYING	/* XXX FIXME: multilib colored dependency search */
	/*
	 * If a file dependency would be satisfied by a file
	 * we are not going to install, skip it.
	 */
	if (al->list[fie->pkgNum].multiLib && !isFileMULTILIB(fie->fileFlags))
	    continue;
#endif

	dsiNotify(ds, _("(added files)"), 0);

	ret = xrealloc(ret, (found+2) * sizeof(*ret));
	if (ret)	/* can't happen */
	    ret[found++] = alNum2Key(al, fie->pkgNum);
    }
    /*@=branchstate@*/

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
    availableIndex ai = &al->index;
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

    if (ai->index == NULL || ai->size <= 0)
	return NULL;

    /*@-assignexpose -temptrans@*/
    needle->entry = KName;
    /*@=assignexpose =temptrans@*/
    needle->entryLen = strlen(needle->entry);

    match = bsearch(needle, ai->index, ai->size, sizeof(*ai->index), indexcmp);
    if (match == NULL)
	return NULL;

    /* rewind to the first match */
    while (match > ai->index && indexcmp(match-1, needle) == 0)
	match--;

    for (ret = NULL, found = 0;
	 match <= ai->index + ai->size && indexcmp(match, needle) == 0;
	 match++)
    {
	/*@-nullptrarith@*/
	alp = al->list + alKey2Num(al, match->pkgKey);
	/*@=nullptrarith@*/

	rc = 0;
	if (alp->provides != NULL)	/* XXX can't happen */
	switch (match->type) {
	case IET_PROVIDES:
	    alp->provides->i = match->entryIx;

	    /* XXX single step on dsiNext to regenerate DNEVR string */
	    alp->provides->i--;
	    if (dsiNext(alp->provides) >= 0)
		rc = dsCompare(alp->provides, ds);

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
